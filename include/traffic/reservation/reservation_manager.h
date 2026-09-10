#pragma once

/// \file
/// Who gets the resource. docs/06_TRAFFIC_RESERVATION.md §6, §14~22, §25~26,
/// §31~37, docs/25_TRAFFIC_CONTROL_SPECIFICATION.md §7~12, §15~16, §22~23.
///
/// docs/06_TRAFFIC_RESERVATION.md §40 is the brief: this is not a reservation
/// database. It is where a claim is checked, decided, recorded and explained,
/// and §40's last line — no other module changes reservation state — is why
/// every mutation on the table goes through here.
///
/// Four decisions in this file are worth more than the code that implements
/// them.
///
/// **A grant is never taken back to satisfy a competing request.** §16 says to
/// compare priority when claims conflict, and
/// docs/25_TRAFFIC_CONTROL_SPECIFICATION.md §8 says to choose one robot and
/// make the other wait. That choice is made *at decision time*, which is what
/// `request_all` is for: requests arriving together are ordered by effective
/// priority and granted in that order, within capacity. Revoking a grant a
/// robot has already been told to act on would put two robots in one corridor
/// with nothing but timing between them, and that is the failure this module
/// exists to prevent. Starvation is answered by aging (§17), not by preemption.
///
/// **An extension may cancel other robots' *pending* claims, and never an
/// active one.** §18~20: a robot stopped inside a corridor holds it whether we
/// like it or not, so refusing the extension does not free the corridor — it
/// only makes the table disagree with the world. So the claims that would have
/// followed it in are cancelled and reported, and the controller replans them
/// (§20's "R02 WAIT / REPLAN"). If one of them is already `active`, or
/// committed and so past being called back (§30), the extension is refused
/// instead: two robots are then physically involved and the answer is not a
/// bookkeeping one.
///
/// **Expiry does not release an active claim by itself.** §22 is explicit —
/// nothing is auto-released without checking the real state. A *pending* grant
/// that timed out is safe to reclaim: the robot never entered. An `active` one
/// past its window is reported as an overrun and left alone, for the controller
/// to extend or escalate.
///
/// **No clock.** Every operation takes `now`, like `state::StateManager`, so
/// the same call sequence replays identically and a decision can be dated to
/// the event that caused it rather than to when the code happened to run
/// (docs/01_REQUIREMENTS.md NFR-003).
///
/// Two seams are deliberately left open:
///   * `planning::ITrafficConditions` (congestion and expected wait for the
///     cost model) is *not* implemented here. `planning` and `reservation` sit
///     at the same tier of docs/12_SOFTWARE_ARCHITECTURE.md §16, so an adapter
///     wiring this table to that interface belongs to whatever composes them —
///     Phase 11. Everything it needs is already public: `capacity_of`,
///     `holders` and `next_available`.
///   * `wait_for_edges` publishes the dependency chain of §26. Deciding that a
///     cycle in it is a deadlock is Phase 9 and is not done here.

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "traffic/core/ids.h"
#include "traffic/core/result.h"
#include "traffic/core/time.h"
#include "traffic/domain/reservation.h"
#include "traffic/domain/values.h"
#include "traffic/reservation/reservation_policy.h"
#include "traffic/reservation/reservation_request.h"
#include "traffic/reservation/reservation_table.h"

namespace traffic::reservation {

/// Why an operation on an existing reservation failed.
enum class ReservationError {
    /// Not an error. Present so an outcome struct can say "nothing went wrong"
    /// without an optional. `core::Status<ReservationError>` never carries it.
    none,

    unknown_reservation,

    /// The reservation has already ended. docs/24_DOMAIN_MODEL.md §30: a
    /// released claim is never revived.
    already_terminal,

    /// The lifecycle refused the move.
    invalid_transition,

    /// The proposed end is not later than the current one. §18 is about
    /// holding a resource for longer; giving it back early is `release`.
    not_an_extension,

    /// Another robot is already inside for part of the extended window. §20.
    extension_blocked,

    /// The robot is committed to entering and cancelling would not stop it.
    /// §30.
    committed,
};

[[nodiscard]] std::string_view to_string(ReservationError error) noexcept;

/// One robot waiting on another. §26.
///
/// The edges of a wait-for graph, published and not interpreted: a cycle here
/// is what docs/08_DEADLOCK_MANAGER.md calls a deadlock, and finding one is
/// Phase 9's.
struct WaitEdge {
    core::RobotId waiting_robot;
    core::RobotId holding_robot;
    core::ResourceId resource;

    /// When the wait began — the same instant the aging of §17 counts from.
    core::TimePoint since{core::kTimeOrigin};

    [[nodiscard]] friend bool operator==(const WaitEdge&, const WaitEdge&) = default;
};

/// What an extension request produced. §18~20.
struct ExtensionOutcome {
    bool granted{false};

    core::ReservationId reservation_id;

    /// The end the reservation now has. Unchanged when the extension failed.
    core::TimePoint new_end{core::kTimeOrigin};

    ReservationError error{ReservationError::none};

    /// Pending claims cancelled to make room, in id order. Their robots must
    /// be replanned — §20.
    std::vector<core::ReservationId> revoked;

    /// Robots affected by the extension, in id order. §20's "영향받는 robot을
    /// 자동으로 찾을 수 있어야 한다", and §25's `get_affected_robots`.
    std::vector<core::RobotId> affected_robots;

    core::TimePoint decided_at{core::kTimeOrigin};
};

/// What a sweep found. §22, docs/25_TRAFFIC_CONTROL_SPECIFICATION.md §22~23.
struct SweepReport {
    /// Grants that were never used and have been taken back. Safe to reclaim
    /// without asking anyone: the robot never entered.
    std::vector<core::ReservationId> expired;

    /// Active claims past their window. **Not** expired here. The robot may
    /// still be inside, and §22 forbids releasing on a timer alone.
    std::vector<core::ReservationId> overrunning;
};

/// What came back after a restart. §32.
struct RecoveryReport {
    /// Restored as they were.
    std::vector<core::ReservationId> restored;

    /// Restored, then found to be past their window and expired.
    std::vector<core::ReservationId> expired;

    /// Refused because restoring them would have put two robots in one place.
    /// The controller has to re-derive these from where the robots actually
    /// are — §32's "실제 Robot 위치와 비교하여 재검증한다".
    std::vector<core::ReservationId> rejected;
};

/// Counters for the KPI table in CLAUDE.md.
struct ReservationMetrics {
    std::uint64_t granted{0};
    std::uint64_t waited{0};
    std::uint64_t rejected{0};
    std::uint64_t released{0};
    std::uint64_t expired{0};
    std::uint64_t cancelled{0};
    std::uint64_t extended{0};

    /// Pending claims cancelled to let an extension through. A rising number
    /// means robots are routinely overstaying, which is a traffic problem and
    /// not a reservation one.
    std::uint64_t revoked{0};

    /// Requests answered from an existing reservation. §35.
    std::uint64_t replayed{0};
};

/// The only thing that changes reservation state. §40.
class IReservationManager {
public:
    IReservationManager() = default;
    virtual ~IReservationManager() = default;

    IReservationManager(const IReservationManager&) = delete;
    IReservationManager& operator=(const IReservationManager&) = delete;
    IReservationManager(IReservationManager&&) = delete;
    IReservationManager& operator=(IReservationManager&&) = delete;

    // ------------------------------------------------------------- decide

    /// Checks and, if the resource allows it, grants — as one step, so that
    /// nothing can be granted between the check and the write (§34).
    [[nodiscard]] virtual ReservationDecision request(const ReservationRequest& request,
                                                      core::TimePoint now) = 0;

    /// Decides a set of requests that arrived together. §33.
    ///
    /// They are ordered by effective priority, then by how long each robot has
    /// waited, then by task id, then by robot id, then by reservation id —
    /// docs/25_TRAFFIC_CONTROL_SPECIFICATION.md §16 — and granted in that
    /// order until the resource is full. The tie-break runs all the way to an
    /// identifier so the order is total: a partial one would leave the outcome
    /// depending on the order the requests happened to arrive in.
    ///
    /// Decisions come back in the order they were decided, not the order they
    /// were passed in. That order is the arbitration, and hiding it would make
    /// a contested resource impossible to explain.
    [[nodiscard]] virtual std::vector<ReservationDecision> request_all(
        std::vector<ReservationRequest> requests, core::TimePoint now) = 0;

    // ----------------------------------------------------------- lifecycle

    /// The robot has entered: `pending` -> `active`.
    /// docs/25_TRAFFIC_CONTROL_SPECIFICATION.md §11.
    [[nodiscard]] virtual core::Status<ReservationError> activate(const core::ReservationId& id,
                                                                  core::TimePoint now) = 0;

    /// Holds a resource for longer than granted. §18~20.
    [[nodiscard]] virtual ExtensionOutcome extend(const core::ReservationId& id,
                                                  core::TimePoint new_end,
                                                  core::TimePoint now) = 0;

    /// The robot is out. §21.
    ///
    /// Whether it is really out is the caller's determination — §21 measures
    /// it against the robot's actual position, which this module cannot see.
    ///
    /// Only an `active` claim can be released; a grant the robot never entered
    /// is withdrawn with `cancel`. The lifecycle draws that line
    /// (docs/24_DOMAIN_MODEL.md §13) and the distinction is worth keeping:
    /// a release says a resource was used and given back, and counting a grant
    /// nobody took up as one hides how often robots are told to go and do not.
    [[nodiscard]] virtual core::Status<ReservationError> release(const core::ReservationId& id,
                                                                 core::TimePoint now) = 0;

    /// Releases everything \p robot still holds, in id order, and returns what
    /// went. For a robot that has failed or left the fleet — a claim nobody
    /// will ever release blocks the resource for good.
    [[nodiscard]] virtual std::vector<core::ReservationId> release_all_for(
        const core::RobotId& robot, core::TimePoint now) = 0;

    /// Withdraws a claim the robot never used. Refused once committed (§30).
    [[nodiscard]] virtual core::Status<ReservationError> cancel(const core::ReservationId& id,
                                                                core::TimePoint now) = 0;

    /// Marks the claim as past the point where cancelling would help. §30.
    [[nodiscard]] virtual core::Status<ReservationError> commit(const core::ReservationId& id) = 0;

    /// Expires one claim explicitly — the path §22 allows for an `active`
    /// claim, once the caller has established that the robot is not inside.
    [[nodiscard]] virtual core::Status<ReservationError> expire(const core::ReservationId& id,
                                                                core::TimePoint now) = 0;

    /// Expires unused grants and reports overruns. §22.
    [[nodiscard]] virtual SweepReport sweep(core::TimePoint now) = 0;

    /// Rebuilds state after a restart. §32.
    [[nodiscard]] virtual RecoveryReport recover(const std::vector<ReservationRecord>& records,
                                                 core::TimePoint now) = 0;

    // --------------------------------------------------------------- query

    [[nodiscard]] virtual const ReservationRecord* get_reservation(
        const core::ReservationId& id) const = 0;

    /// §23, §37.
    [[nodiscard]] virtual std::vector<domain::Reservation> active_reservations() const = 0;
    [[nodiscard]] virtual std::vector<domain::Reservation> active_reservations(
        const core::ResourceId& resource) const = 0;

    /// §24.
    [[nodiscard]] virtual std::vector<domain::Reservation> find_conflicts(
        const core::ResourceId& resource, const domain::TimeWindow& window) const = 0;

    /// Robots whose claims on \p resource overlap \p id's window, excluding
    /// its owner. §25.
    [[nodiscard]] virtual std::vector<core::RobotId> affected_robots(
        const core::ResourceId& resource, const core::ReservationId& id) const = 0;

    /// The wait-for edges of §26, in (waiting robot, holding robot) order.
    [[nodiscard]] virtual std::vector<WaitEdge> wait_for_edges() const = 0;

    /// The decisions taken, oldest first, capped by the policy. §36.
    [[nodiscard]] virtual std::span<const ReservationDecision> decision_log() const noexcept = 0;

    [[nodiscard]] virtual const ReservationMetrics& metrics() const noexcept = 0;
    [[nodiscard]] virtual const ReservationTable& table() const noexcept = 0;
};

/// The one implementation.
class ReservationManager final : public IReservationManager {
public:
    /// \p table must outlive the manager. Injected rather than owned so that a
    /// simulation can hold one table and inspect it directly, and so the
    /// manager never has to know how a table is built (CLAUDE.md, DI).
    ReservationManager(ReservationTable& table, ReservationPolicy policy);

    [[nodiscard]] const ReservationPolicy& policy() const noexcept { return policy_; }

    [[nodiscard]] ReservationDecision request(const ReservationRequest& request,
                                              core::TimePoint now) override;
    [[nodiscard]] std::vector<ReservationDecision> request_all(
        std::vector<ReservationRequest> requests, core::TimePoint now) override;

    [[nodiscard]] core::Status<ReservationError> activate(const core::ReservationId& id,
                                                          core::TimePoint now) override;
    [[nodiscard]] ExtensionOutcome extend(const core::ReservationId& id,
                                          core::TimePoint new_end,
                                          core::TimePoint now) override;
    [[nodiscard]] core::Status<ReservationError> release(const core::ReservationId& id,
                                                         core::TimePoint now) override;
    [[nodiscard]] std::vector<core::ReservationId> release_all_for(const core::RobotId& robot,
                                                                   core::TimePoint now) override;
    [[nodiscard]] core::Status<ReservationError> cancel(const core::ReservationId& id,
                                                        core::TimePoint now) override;
    [[nodiscard]] core::Status<ReservationError> commit(const core::ReservationId& id) override;
    [[nodiscard]] core::Status<ReservationError> expire(const core::ReservationId& id,
                                                        core::TimePoint now) override;
    [[nodiscard]] SweepReport sweep(core::TimePoint now) override;
    [[nodiscard]] RecoveryReport recover(const std::vector<ReservationRecord>& records,
                                         core::TimePoint now) override;

    [[nodiscard]] const ReservationRecord* get_reservation(
        const core::ReservationId& id) const override;
    [[nodiscard]] std::vector<domain::Reservation> active_reservations() const override;
    [[nodiscard]] std::vector<domain::Reservation> active_reservations(
        const core::ResourceId& resource) const override;
    [[nodiscard]] std::vector<domain::Reservation> find_conflicts(
        const core::ResourceId& resource, const domain::TimeWindow& window) const override;
    [[nodiscard]] std::vector<core::RobotId> affected_robots(
        const core::ResourceId& resource, const core::ReservationId& id) const override;
    [[nodiscard]] std::vector<WaitEdge> wait_for_edges() const override;
    [[nodiscard]] std::span<const ReservationDecision> decision_log() const noexcept override;
    [[nodiscard]] const ReservationMetrics& metrics() const noexcept override;
    [[nodiscard]] const ReservationTable& table() const noexcept override;

    /// How long \p robot has been asking for \p resource without getting it.
    /// Zero when it is not waiting for that resource. §17.
    [[nodiscard]] core::Duration waited_for(const core::RobotId& robot,
                                            const core::ResourceId& resource,
                                            core::TimePoint now) const;

private:
    /// What a robot is currently blocked on. One per robot: a robot moves
    /// through one resource at a time, so a second wait replaces the first
    /// rather than accumulating.
    struct WaitEntry {
        core::ResourceId resource;
        core::TimePoint since{core::kTimeOrigin};
        std::vector<core::RobotId> blocked_by;
    };

    /// Appends to the §36 log and hands the decision back, so a decision can
    /// be recorded and returned in one expression.
    [[nodiscard]] ReservationDecision journal(ReservationDecision decision);

    /// §35: what to answer when the reservation id is already known.
    [[nodiscard]] ReservationDecision replay(const ReservationRecord& existing,
                                             const ReservationRequest& request,
                                             ReservationDecision decision);

    void note_wait(const ReservationRequest& request,
                   const AdmissionVerdict& verdict,
                   core::TimePoint now);
    void clear_wait(const core::RobotId& robot);

    ReservationTable& table_;
    ReservationPolicy policy_;

    std::map<core::RobotId, WaitEntry> waits_;

    std::vector<ReservationDecision> journal_;

    ReservationMetrics metrics_;
};

}  // namespace traffic::reservation
