#pragma once

/// \file
/// Where reservations live and what they refuse.
/// docs/06_TRAFFIC_RESERVATION.md §7~12, §23~24, docs/24_DOMAIN_MODEL.md §13~14.
///
/// `domain::conflicts_with` is the pairwise rule and is deliberately
/// capacity-blind. This is the class its comment hands the rest of the job to:
/// a resource with capacity N is refused only when N robots would already be
/// inside during the requested window.
///
/// **Counting is not "how many reservations overlap my window".** Two
/// reservations can each overlap the request without overlapping each other —
/// one robot leaves, the next arrives — and at no instant are both inside.
/// Counting them as two would refuse a resource that is free. So admission
/// looks for the busiest *instant* inside the requested window and counts
/// distinct robots there. The count can only rise at a window start, which is
/// why only those instants are examined.
///
/// **Capacity is read from the map, never stored here.** Two records of how
/// wide a corridor is will eventually disagree, and the moment they do, one of
/// them is granting a second robot entry to a single-lane passage. The map is
/// already the authority (docs/03_MAP_GRAPH.md §9~13) so it stays the only one.
/// A resource the map does not know gets capacity one — the most restrictive
/// answer, because an unknown resource is one nothing has checked.
///
/// **Lookups are indexed by resource.** At 200 robots asking continuously, a
/// scan of every live reservation to answer one question about one corridor is
/// the whole decision loop. Reaching a resource's own claims is `O(log n)`; the
/// scan after that covers only that resource, only its still-holding claims,
/// and stops at the first one starting after the window.
///
/// Ordered containers throughout, per CLAUDE.md: an admission decision that
/// depended on hash order would not reproduce, and reproducing a near miss is
/// how it gets fixed.

#include <compare>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string_view>
#include <vector>

#include "traffic/core/ids.h"
#include "traffic/core/result.h"
#include "traffic/core/time.h"
#include "traffic/domain/reservation.h"
#include "traffic/domain/reservation_state.h"
#include "traffic/domain/values.h"
#include "traffic/map/map.h"
#include "traffic/reservation/reservation_request.h"

namespace traffic::reservation {

/// Everything the table holds about one claim.
///
/// The reservation is the entity of docs/24_DOMAIN_MODEL.md §13 and is not
/// extended — its shape is frozen by CLAUDE.md. What Phase 6 needs alongside it
/// lives here, the same split `state::RobotRecord` makes between what was
/// observed and what we concluded about it.
struct ReservationRecord {
    domain::Reservation reservation;

    /// Direction and movement, for the refusals capacity cannot express.
    ResourceUsage usage;

    /// What the robot asked for, before the buffers of §13. Kept so a decision
    /// can be explained afterwards: a robot refused at 10.0s when it asked for
    /// 10.5s is otherwise an unreadable log line.
    core::TimePoint requested_start{core::kTimeOrigin};
    core::Duration requested_duration{core::Duration::zero()};

    /// When the grant was made. The clock the grant timeout of §22 runs on.
    core::TimePoint decided_at{core::kTimeOrigin};

    /// When the claim last changed state.
    ///
    /// docs/25_TRAFFIC_CONTROL_SPECIFICATION.md §23 asks whether a reservation
    /// and the robot's state still agree, and that question needs to know how
    /// old the reservation's side of the story is. Without it, an `active`
    /// claim from an hour ago and one from a second ago are the same record.
    core::TimePoint last_transition_at{core::kTimeOrigin};

    /// Past the point where cancelling is safe. §30: once a robot is committed
    /// to entering, taking the resource back does not stop it.
    bool committed{false};

    std::optional<core::RouteId> route_id;
    std::optional<core::TaskId> task_id;
};

/// Builds a record with every field set.
///
/// `domain::Reservation` holds a `TimeWindow`, which has no default value —
/// there is no sensible empty window — so a record cannot be
/// default-constructed and filled in afterwards. One factory means one place
/// lists the fields, and adding a field cannot leave it uninitialised at some
/// other call site.
[[nodiscard]] ReservationRecord make_record(domain::Reservation reservation,
                                            ResourceUsage usage,
                                            core::TimePoint requested_start,
                                            core::Duration requested_duration,
                                            core::TimePoint decided_at,
                                            std::optional<core::RouteId> route_id,
                                            std::optional<core::TaskId> task_id);

/// What an admission check concluded.
enum class AdmissionResult {
    admitted,

    /// Capacity many robots are already inside for part of the window. §7~9.
    capacity_exceeded,

    /// Someone holds the corridor the other way. §10.
    opposing_direction,

    /// Someone holds a movement in the same conflict group. §11.
    movement_conflict,
};

[[nodiscard]] std::string_view to_string(AdmissionResult result) noexcept;

/// The answer to "may this claim be added", with what is needed to explain it.
/// §36 wants the owner and the expected availability in the log line, not only
/// the verdict.
struct AdmissionVerdict {
    AdmissionResult result{AdmissionResult::admitted};

    /// The resource's capacity, as the map states it.
    std::uint32_t capacity{1};

    /// The most robots that would be inside at any one instant of the
    /// requested window, not counting the requester.
    std::uint32_t peak_holders{0};

    /// The claims in the way, in (start, id) order.
    std::vector<core::ReservationId> blockers;

    /// Who owns them, de-duplicated, in id order.
    std::vector<core::RobotId> blocking_robots;

    /// The earliest a blocker gives the resource back. §15.
    std::optional<core::TimePoint> next_available;
};

/// True when the verdict permits the claim.
[[nodiscard]] bool is_admitted(const AdmissionVerdict& verdict) noexcept;

/// Why a table operation was refused.
enum class TableError {
    /// A reservation with that id is already present. Re-inserting would
    /// silently replace a live claim.
    duplicate_id,

    unknown_reservation,

    /// The lifecycle forbids the move — including `released` -> `active`,
    /// which is the invariant of docs/24_DOMAIN_MODEL.md §30.
    invalid_transition,

    /// The new end is not after the start.
    invalid_window,
};

[[nodiscard]] std::string_view to_string(TableError error) noexcept;

/// The reservations of one map, indexed by resource.
///
/// It stores and it refuses. It does not decide *who* should get a contested
/// resource — that is the manager's, because deciding needs priority, waiting
/// time and a policy, none of which belong in an index.
class ReservationTable {
public:
    /// \p map must outlive the table. It is the authority on capacity, on
    /// which resources are corridors, and on which movements exclude each
    /// other.
    ///
    /// There is deliberately no `map()` accessor. A member of that name would
    /// hide the `traffic::map` namespace inside this class and every
    /// `map::Map` in it would stop naming a type. Callers that need the map
    /// already hold it — they had to, to build the table.
    explicit ReservationTable(const map::Map& map);

    // ----------------------------------------------------------- admission

    /// How many robots \p resource admits at once. One when the map does not
    /// know it.
    [[nodiscard]] std::uint32_t capacity_of(const core::ResourceId& resource) const;

    /// Whether a claim by \p requester on \p resource for \p window could be
    /// added without breaking capacity, direction or movement exclusion.
    ///
    /// Claims already held by \p requester are not counted against it: one
    /// robot occupies one place however many claims it holds, and counting
    /// itself would leave a robot queueing behind its own reservation.
    [[nodiscard]] AdmissionVerdict check(const core::ResourceId& resource,
                                         const core::RobotId& requester,
                                         const domain::TimeWindow& window,
                                         const ResourceUsage& usage) const;

    // --------------------------------------------------------------- write

    /// Adds a record. Fails on a duplicate id.
    ///
    /// Does *not* check admission. The caller checks and inserts as one step —
    /// §34 requires exactly that, and a table that re-checked here would not
    /// make the pair atomic, only slower.
    [[nodiscard]] core::Status<TableError> insert(ReservationRecord record);

    /// Moves a reservation to \p next through the domain lifecycle at \p at,
    /// keeping the indices in step.
    [[nodiscard]] core::Status<TableError> set_state(const core::ReservationId& id,
                                                     domain::ReservationState next,
                                                     core::TimePoint at);

    /// Moves the end of a reservation's window. §18.
    ///
    /// The start never moves: it is the index key, and a claim that could
    /// start earlier than it was granted is a claim on time nobody checked.
    [[nodiscard]] core::Status<TableError> set_window_end(const core::ReservationId& id,
                                                          core::TimePoint new_end);

    /// Marks a reservation as past the point of safe cancellation. §30.
    [[nodiscard]] core::Status<TableError> mark_committed(const core::ReservationId& id);

    /// Drops terminal records whose window ended before \p cutoff and returns
    /// how many went. History is kept for recovery and for explaining
    /// decisions, but not forever.
    std::size_t purge_terminal_before(core::TimePoint cutoff);

    // --------------------------------------------------------------- query

    [[nodiscard]] const ReservationRecord* find(const core::ReservationId& id) const;
    [[nodiscard]] bool contains(const core::ReservationId& id) const;

    /// Every record inserted and not purged, terminal ones included.
    [[nodiscard]] std::size_t size() const noexcept;

    /// Records that still withhold their resource — `pending` or `active`.
    [[nodiscard]] std::size_t holding_count() const noexcept;

    /// The ids of those records, in id order. Take this before a sweep that
    /// changes states, rather than iterating an index while mutating it.
    [[nodiscard]] std::vector<core::ReservationId> holding_ids() const;

    /// Claims on \p resource that overlap \p window and still hold it. §24.
    [[nodiscard]] std::vector<domain::Reservation> find_conflicts(
        const core::ResourceId& resource, const domain::TimeWindow& window) const;

    /// The same, excluding \p requester's own claims — the set
    /// `domain::conflicts_with` would flag for a claim by that robot.
    [[nodiscard]] std::vector<domain::Reservation> find_contenders(
        const core::ResourceId& resource,
        const domain::TimeWindow& window,
        const core::RobotId& requester) const;

    /// Everything currently holding \p resource, pending or active.
    [[nodiscard]] std::vector<domain::Reservation> holders(const core::ResourceId& resource) const;

    /// The `active` claims on \p resource. §23's `get_active_reservation` is
    /// the capacity-one case of this: a resource that admits several robots
    /// has several owners, and a singular answer would have to pick one of
    /// them to report.
    [[nodiscard]] std::vector<domain::Reservation> active_reservations(
        const core::ResourceId& resource) const;

    /// Every `active` claim in the table, in id order. §37.
    [[nodiscard]] std::vector<domain::Reservation> active_reservations() const;

    /// Who currently holds \p resource, in id order. §23.
    [[nodiscard]] std::vector<core::RobotId> owners_of(const core::ResourceId& resource) const;

    /// Every claim belonging to \p robot, in id order, terminal ones included.
    [[nodiscard]] std::vector<domain::Reservation> reservations_for(
        const core::RobotId& robot) const;

    /// Claims of \p robot that still hold their resource, in id order.
    [[nodiscard]] std::vector<core::ReservationId> holding_ids_for(
        const core::RobotId& robot) const;

    /// When \p resource next frees up for \p requester, if it is busy. §15.
    [[nodiscard]] std::optional<core::TimePoint> next_available(
        const core::ResourceId& resource,
        const core::RobotId& requester,
        const domain::TimeWindow& window) const;

private:
    /// Orders a resource's claims by when they start, then by id so that two
    /// claims starting at the same instant still have one fixed order.
    struct Key {
        core::TimePoint start{core::kTimeOrigin};
        core::ReservationId id;

        [[nodiscard]] friend std::strong_ordering operator<=>(const Key&, const Key&) = default;
        [[nodiscard]] friend bool operator==(const Key&, const Key&) = default;
    };

    [[nodiscard]] static Key key_of(const ReservationRecord& record);

    void index(const ReservationRecord& record);
    void unindex(const ReservationRecord& record);

    /// The holding claims on \p resource that overlap \p window and belong to
    /// someone other than \p requester, in (start, id) order.
    [[nodiscard]] std::vector<const ReservationRecord*> contenders(
        const core::ResourceId& resource,
        const domain::TimeWindow& window,
        const core::RobotId& requester) const;

    /// True when the two usages of \p resource cannot share it, whatever its
    /// capacity says.
    [[nodiscard]] bool opposes_direction(const core::ResourceId& resource,
                                         const ResourceUsage& lhs,
                                         const ResourceUsage& rhs) const;
    [[nodiscard]] bool opposes_movement(const core::ResourceId& resource,
                                        const ResourceUsage& lhs,
                                        const ResourceUsage& rhs) const;

    /// Fills in the blockers, the owners and the next-available hint.
    static void explain(AdmissionVerdict& verdict,
                        const std::vector<const ReservationRecord*>& blockers);

    const map::Map& map_;

    std::map<core::ReservationId, ReservationRecord> records_;

    /// resource -> its still-holding claims, ordered by start.
    std::map<core::ResourceId, std::map<Key, core::ReservationId>> holding_by_resource_;

    /// robot -> every claim it has made. Terminal ones stay until purged, so
    /// that "what did R01 hold" is answerable after the fact.
    std::map<core::RobotId, std::set<core::ReservationId>> by_robot_;

    std::size_t holding_count_{0};
};

}  // namespace traffic::reservation
