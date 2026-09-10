/// Cost model and route constraints. docs/05_GLOBAL_ROUTING.md §6~10, §18.
///
/// The interesting tests here are not "does the arithmetic work" but the two
/// properties A* depends on: that no edge can cost less than nothing, and that
/// `min_cost_per_metre` never exceeds what a metre actually costs. The second
/// is the admissibility of the heuristic, expressed as a number.

#include "traffic/planning/cost_model.h"

#include <chrono>

#include <gtest/gtest.h>

#include "traffic/core/ids.h"
#include "traffic/core/time.h"
#include "traffic/domain/graph.h"
#include "traffic/planning/route_request.h"

namespace traffic::planning {
namespace {

using core::EdgeId;
using core::NodeId;
using core::ResourceId;

domain::Edge make_test_edge(double length, double speed_limit) {
    domain::Edge edge;
    edge.id = EdgeId{"E1"};
    edge.from_node = NodeId{"N1"};
    edge.to_node = NodeId{"N2"};
    edge.length = length;
    edge.speed_limit = speed_limit;
    edge.width = 1.5;
    return edge;
}

/// Conditions with fixed numbers, so a test can say what the traffic is.
class FixedConditions final : public ITrafficConditions {
public:
    FixedConditions(double congestion, core::Duration wait)
        : congestion_(congestion), wait_(wait) {}

    [[nodiscard]] double congestion(const ResourceId& /*resource*/) const override {
        return congestion_;
    }

    [[nodiscard]] core::Duration expected_wait(const ResourceId& /*resource*/,
                                               core::TimePoint /*at*/) const override {
        return wait_;
    }

private:
    double congestion_;
    core::Duration wait_;
};

// ------------------------------------------------------------------ weights

TEST(CostWeights, cost_weights_default_to_shortest_distance) {
    const CostWeights weights;

    EXPECT_DOUBLE_EQ(weights.distance, 1.0);
    EXPECT_DOUBLE_EQ(weights.travel_time, 0.0);
    EXPECT_DOUBLE_EQ(weights.congestion, 0.0);
    EXPECT_DOUBLE_EQ(weights.waiting, 0.0);
    EXPECT_DOUBLE_EQ(weights.preferred_factor, 1.0);
}

TEST(CostWeights, cost_weights_defaults_are_valid) {
    EXPECT_TRUE(make_cost_weights(CostWeights{}).has_value());
}

TEST(CostWeights, cost_weights_negative_weight_is_rejected) {
    CostWeights weights;
    weights.congestion = -1.0;

    const auto result = make_cost_weights(weights);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), domain::DomainError::negative_value);
}

TEST(CostWeights, cost_weights_preferred_factor_above_one_is_rejected) {
    // Above 1 would mean "prefer this resource by making it more expensive".
    CostWeights weights;
    weights.preferred_factor = 1.5;

    EXPECT_FALSE(make_cost_weights(weights).has_value());
}

TEST(CostWeights, cost_weights_zero_preferred_factor_is_rejected) {
    // A free edge lets a route loop through it at no cost.
    CostWeights weights;
    weights.preferred_factor = 0.0;

    EXPECT_FALSE(make_cost_weights(weights).has_value());
}

// --------------------------------------------------------------- edge cost

TEST(EdgeCost, edge_cost_default_weights_use_distance_only) {
    const domain::Edge edge = make_test_edge(10.0, 2.0);
    const FreeFlowConditions conditions;

    const double cost = edge_cost(
        edge, ResourceId{"R1"}, CostWeights{}, RouteConstraints{}, conditions, core::kTimeOrigin);

    EXPECT_DOUBLE_EQ(cost, 10.0);
}

TEST(EdgeCost, edge_cost_travel_time_is_length_over_speed) {
    const domain::Edge edge = make_test_edge(10.0, 2.0);  // 5 seconds
    const FreeFlowConditions conditions;

    CostWeights weights;
    weights.distance = 0.0;
    weights.travel_time = 1.0;

    const double cost = edge_cost(
        edge, ResourceId{"R1"}, weights, RouteConstraints{}, conditions, core::kTimeOrigin);

    EXPECT_DOUBLE_EQ(cost, 5.0);
}

TEST(EdgeCost, edge_cost_travel_time_uses_override_when_set) {
    // docs/00_INDEX.md D-007: the override exists for a lift, where the time
    // spent has nothing to do with the distance covered.
    domain::Edge edge = make_test_edge(10.0, 2.0);
    edge.travel_time_override = std::chrono::duration_cast<core::Duration>(core::Seconds{30});
    const FreeFlowConditions conditions;

    CostWeights weights;
    weights.distance = 0.0;
    weights.travel_time = 1.0;

    const double cost = edge_cost(
        edge, ResourceId{"R1"}, weights, RouteConstraints{}, conditions, core::kTimeOrigin);

    EXPECT_DOUBLE_EQ(cost, 30.0);
}

TEST(EdgeCost, edge_cost_congestion_adds_to_cost) {
    const domain::Edge edge = make_test_edge(10.0, 2.0);
    const FixedConditions conditions{1.0, core::Duration::zero()};

    CostWeights weights;
    weights.congestion = 4.0;

    const double cost = edge_cost(
        edge, ResourceId{"R1"}, weights, RouteConstraints{}, conditions, core::kTimeOrigin);

    EXPECT_DOUBLE_EQ(cost, 14.0);
}

TEST(EdgeCost, edge_cost_expected_wait_adds_to_cost) {
    const domain::Edge edge = make_test_edge(10.0, 2.0);
    const FixedConditions conditions{0.0,
                                     std::chrono::duration_cast<core::Duration>(core::Seconds{3})};

    CostWeights weights;
    weights.waiting = 2.0;

    const double cost = edge_cost(
        edge, ResourceId{"R1"}, weights, RouteConstraints{}, conditions, core::kTimeOrigin);

    EXPECT_DOUBLE_EQ(cost, 16.0);
}

TEST(EdgeCost, edge_cost_preferred_resource_is_cheaper) {
    const domain::Edge edge = make_test_edge(10.0, 2.0);
    const FreeFlowConditions conditions;

    CostWeights weights;
    weights.preferred_factor = 0.5;

    RouteConstraints constraints;
    constraints.preferred_resources.insert(ResourceId{"R1"});

    const double preferred =
        edge_cost(edge, ResourceId{"R1"}, weights, constraints, conditions, core::kTimeOrigin);
    const double ordinary =
        edge_cost(edge, ResourceId{"R2"}, weights, constraints, conditions, core::kTimeOrigin);

    EXPECT_DOUBLE_EQ(preferred, 5.0);
    EXPECT_DOUBLE_EQ(ordinary, 10.0);
    EXPECT_LT(preferred, ordinary);
}

TEST(EdgeCost, edge_cost_is_never_negative) {
    // A* stays correct only while every edge cost is non-negative. With
    // validated weights there is no combination of inputs that produces one.
    const domain::Edge edge = make_test_edge(10.0, 2.0);
    const FixedConditions conditions{2.0,
                                     std::chrono::duration_cast<core::Duration>(core::Seconds{5})};

    CostWeights weights;
    weights.travel_time = 1.0;
    weights.congestion = 1.0;
    weights.waiting = 1.0;
    weights.preferred_factor = 0.1;

    RouteConstraints constraints;
    constraints.preferred_resources.insert(ResourceId{"R1"});

    ASSERT_TRUE(make_cost_weights(weights).has_value());
    EXPECT_GE(
        edge_cost(edge, ResourceId{"R1"}, weights, constraints, conditions, core::kTimeOrigin),
        0.0);
}

// --------------------------------------------------------- heuristic bound

TEST(MinCostPerMetre, min_cost_per_metre_never_exceeds_real_edge_cost) {
    // The admissibility argument, as a test: for every edge, the bound times
    // the edge's length must not exceed the edge's real cost. If this fails,
    // A* can return a route that is not the cheapest.
    CostWeights weights;
    weights.distance = 2.0;
    weights.travel_time = 3.0;
    weights.congestion = 5.0;
    weights.waiting = 7.0;

    constexpr double kMaxSpeed = 4.0;
    const double bound = min_cost_per_metre(weights, kMaxSpeed);

    const FixedConditions conditions{0.5,
                                     std::chrono::duration_cast<core::Duration>(core::Seconds{2})};

    for (const double speed : {1.0, 2.0, 3.5, kMaxSpeed}) {
        for (const double length : {0.5, 1.0, 12.0, 100.0}) {
            const domain::Edge edge = make_test_edge(length, speed);
            const double actual = edge_cost(
                edge, ResourceId{"R1"}, weights, RouteConstraints{}, conditions, core::kTimeOrigin);
            EXPECT_LE(bound * length, actual) << "speed=" << speed << " length=" << length;
        }
    }
}

TEST(MinCostPerMetre, min_cost_per_metre_accounts_for_preference_discount) {
    // A discounted edge is the cheapest an edge can be, so the bound has to
    // include the discount or it will overestimate exactly on the routes a
    // preference was meant to encourage.
    CostWeights weights;
    weights.distance = 1.0;
    weights.preferred_factor = 0.5;

    const double bound = min_cost_per_metre(weights, 1.0);

    RouteConstraints constraints;
    constraints.preferred_resources.insert(ResourceId{"R1"});
    const FreeFlowConditions conditions;
    const domain::Edge edge = make_test_edge(10.0, 1.0);

    const double actual =
        edge_cost(edge, ResourceId{"R1"}, weights, constraints, conditions, core::kTimeOrigin);

    EXPECT_LE(bound * edge.length, actual);
    EXPECT_DOUBLE_EQ(bound, 0.5);
}

TEST(MinCostPerMetre, min_cost_per_metre_without_positive_speed_is_zero) {
    CostWeights weights;
    weights.travel_time = 1.0;

    EXPECT_DOUBLE_EQ(min_cost_per_metre(weights, 0.0), 0.0);
    EXPECT_DOUBLE_EQ(min_cost_per_metre(weights, -1.0), 0.0);
}

// --------------------------------------------------------------- durations

TEST(ToSeconds, to_seconds_converts_nanoseconds_to_real_seconds) {
    EXPECT_DOUBLE_EQ(to_seconds(core::Duration::zero()), 0.0);
    EXPECT_DOUBLE_EQ(
        to_seconds(std::chrono::duration_cast<core::Duration>(core::Milliseconds{1500})), 1.5);
}

TEST(FreeFlowConditions, free_flow_conditions_report_no_traffic) {
    const FreeFlowConditions conditions;

    EXPECT_DOUBLE_EQ(conditions.congestion(ResourceId{"CORRIDOR-01"}), 0.0);
    EXPECT_EQ(conditions.expected_wait(ResourceId{"CORRIDOR-01"}, core::kTimeOrigin),
              core::Duration::zero());
}

}  // namespace
}  // namespace traffic::planning
