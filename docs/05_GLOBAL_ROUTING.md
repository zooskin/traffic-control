Traffic Control Software

Global Route Planner Specification

1. 목적

Global Route Planner는 Robot의 현재 위치에서 Task 목적지까지 이동하기 위한 기본 경로를 계산한다.

본 모듈은 다음 문제를 해결한다.

"Robot이 어느 경로를 통해 목적지까지 이동할 것인가?"

반면 실제 resource를 언제 사용할지는 Traffic Controller와 Reservation Manager가 결정한다.

2. 역할 분리

Global Planner
    |
    | "어디로?"
    v

Route

Traffic Controller
    |
    | "언제?"
    v

Reservation

Robot Controller
    |
    | "어떻게?"
    v

Motion

3. Initial Algorithm

기본 알고리즘:

A*

Fallback:

Dijkstra

4. A*

Graph:

G = (V, E)

A*:

f(n) = g(n) + h(n)

where:

g(n) = start에서 n까지의 실제 비용

h(n) = n에서 goal까지의 heuristic

5. Heuristic

기본:

Manhattan Distance

또는 map 특성에 따라:

Euclidean Distance

를 사용한다.

Heuristic은 admissible하도록 설계한다.

6. 기본 Cost

초기 버전에서는 다음 cost를 사용한다.

Cost =
    DistanceCost
  + TravelTimeCost
  + CongestionCost
  + WaitingCost

각 weight는 configuration으로 관리한다.

cost =
    w_distance * distance
  + w_time * travel_time
  + w_congestion * congestion
  + w_wait * expected_wait

7. Distance Cost

distance_cost = edge.length

8. Travel Time

travel_time =

edge.length / edge.speed_limit

9. Congestion

Resource의 현재 상태를 이용한다.

congestion =

occupancy / capacity

예:

capacity = 1

occupancy = 1

congestion = 1.0

10. Expected Waiting

Reservation 정보를 기반으로 예상 대기 시간을 계산할 수 있다.

예:

expected_wait =

next_available_time - current_time

11. Route Request

Planner 입력:

RouteRequest {
    robot_id
    start_node
    goal_node
    current_time
    priority
    constraints

}

12. Route Response

RouteResponse {
    route_id
    robot_id
    nodes
    edges
    total_distance
    estimated_time
    cost
    planner
    created_at

}

13. Route Failure

목적지까지 경로가 존재하지 않으면:

NO_ROUTE

를 반환한다.

Traffic Controller는 다음 정책을 수행할 수 있다.

WAIT

RETRY

ALTERNATIVE_GOAL

TASK_FAILED

14. Dynamic Replanning

다음 상황에서 route를 재계산할 수 있다.

Human blockage

Robot blockage

Resource blocked

Robot failure

Congestion

Reservation conflict

Deadlock recovery

15. Replanning 범위

전체 fleet을 동시에 replanning하지 않는다.

우선 affected robot만 대상으로 한다.

Blocked Resource
      |
      v

Affected Robots
      |
      v

Replanning

16. Route Stability

Traffic이 조금 변했다고 매번 route를 변경하면 route oscillation이 발생할 수 있다.

따라서 route 변경에는 최소 개선 조건을 둔다.

예:

new_cost < old_cost * (1 - minimum_improvement)

예:

minimum_improvement = 0.10

즉 10% 이상 개선되는 경우에만 route 변경을 허용하는 정책을 사용할 수 있다.

17. Route Hysteresis

짧은 시간 안에:

Route A
→ Route B
→ Route A

가 반복되지 않도록 한다.

관리 값:

minimum_route_hold_time

18. Route Constraint

Planner는 다음 constraint를 받을 수 있어야 한다.

blocked_edges

blocked_nodes

blocked_resources

forbidden_resources

preferred_resources

예:

blocked_edges = [E12, E13]

19. Reservation-Aware Planning

초기 버전에서는 A*와 Reservation을 분리한다.

A*
 ↓

Route
 ↓

Reservation

이후 필요할 경우:

Reservation-aware A*

로 확장한다.

20. Planning API

plan_route(request)

replan_route(request)

estimate_route_cost(route)

is_route_valid(route)

21. Performance

Planning latency를 측정한다.

P50

P95

P99

Robot scale:

10

50

100

150

200

22. Determinism

동일한:

Map

Start

Goal

Cost

Constraints

를 입력하면 동일한 route가 생성되어야 한다.

동일 cost의 route가 여러 개일 경우 deterministic tie-breaker를 사용한다.

예:

edge_id

node_id

순으로 비교한다.

23. Logging

모든 planning request는 다음을 기록한다.

robot_id

start

goal

planner

old_route

new_route

cost

planning_latency

constraints

reason

24. Acceptance Criteria

A* 구현

Dijkstra fallback

Dynamic edge cost

Congestion cost

Expected waiting cost

Blocked edge 지원

Route validation

Replanning

Deterministic result

Performance benchmark

Planning logging

25. Test Requirements

test_astar_basic

test_astar_shortest_path

test_dijkstra

test_unreachable_goal

test_blocked_edge

test_blocked_node

test_congestion_cost

test_waiting_cost

test_route_validation

test_deterministic_route

test_replanning

test_route_hysteresis
