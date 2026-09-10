#pragma once

/// \file
/// The identifier types used across the domain.
///
/// Entities are taken from docs/24_DOMAIN_MODEL.md. Adding a new identifier
/// here is cheap; reusing an existing one for a different entity is not — it
/// removes the type separation the whole file exists to provide.

#include "traffic/core/strong_id.h"

namespace traffic::core {

namespace tags {

// Fleet
struct Robot {};
struct Task {};

// Map — docs/24_DOMAIN_MODEL.md §7~10, docs/03_MAP_GRAPH.md
struct Node {};
struct Edge {};
struct Resource {};
struct Corridor {};
struct Intersection {};

/// One way through an intersection — docs/03_MAP_GRAPH.md §12.
struct Movement {};

/// A set of movements that exclude each other — docs/03_MAP_GRAPH.md §11.
struct ConflictGroup {};

// Traffic — docs/24_DOMAIN_MODEL.md §11~23
struct Route {};
struct Reservation {};
struct Conflict {};
struct Deadlock {};
struct HumanBlockage {};

// Control plane
struct Event {};
struct Command {};
struct PlanningRequest {};
struct TrafficDecision {};

/// Ties a log line, a metric and a decision back to the event that caused it.
/// docs/23_SYSTEM_ARCHITECTURE.md §24.
struct Correlation {};

}  // namespace tags

using RobotId = StrongId<tags::Robot>;
using TaskId = StrongId<tags::Task>;

using NodeId = StrongId<tags::Node>;
using EdgeId = StrongId<tags::Edge>;
using ResourceId = StrongId<tags::Resource>;
using CorridorId = StrongId<tags::Corridor>;
using IntersectionId = StrongId<tags::Intersection>;
using MovementId = StrongId<tags::Movement>;
using ConflictGroupId = StrongId<tags::ConflictGroup>;

using RouteId = StrongId<tags::Route>;
using ReservationId = StrongId<tags::Reservation>;
using ConflictId = StrongId<tags::Conflict>;
using DeadlockId = StrongId<tags::Deadlock>;
using HumanBlockageId = StrongId<tags::HumanBlockage>;

using EventId = StrongId<tags::Event>;
using CommandId = StrongId<tags::Command>;
using PlanningRequestId = StrongId<tags::PlanningRequest>;
using TrafficDecisionId = StrongId<tags::TrafficDecision>;
using CorrelationId = StrongId<tags::Correlation>;

}  // namespace traffic::core
