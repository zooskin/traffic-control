Traffic Control Software

Map & Traffic Graph Specification

1. 목적

본 문서는 실제 물리 공간을 Traffic Control Software가 사용할 수 있는 Graph 구조로 변환하고 관리하기 위한 데이터 모델과 기능을 정의한다.

대상 환경은 다음과 같다.

좁고 긴 corridor

단일 차선

양방향 corridor

교차로

대기 공간

작업장

충전소

사람이 이동하는 공간

Traffic Controller는 물리적인 좌표만을 사용하지 않고 Traffic Graph와 Resource Graph를 함께 사용한다.

2. 핵심 개념

전체 Map은 다음 구조로 표현한다.

Physical Map
     |
     v

Traffic Graph
     |
     +---- Node
     |
     +---- Edge
     |
     +---- Resource
     |
     +---- Conflict Zone
     |
     +---- Waiting Area

3. Graph 정의

기본 Graph:

G = (V, E)

V = Node

E = Edge

하지만 Traffic Control을 위해 다음 정보를 추가한다.

Traffic Graph
    |
    +-- Node
    +-- Edge
    +-- Resource
    +-- Conflict Zone
    +-- Waiting Area

4. Node

Node는 robot이 route에서 통과하거나 정지할 수 있는 logical point이다.

Node {
    node_id
    x
    y
    z
    type
    orientation
    capacity
    resource_id

}

5. Node Type

다음 type을 지원한다.

NORMAL

INTERSECTION

STATION

PICKUP

DROPOFF

CHARGER

WAITING_BAY

ENTRY

EXIT

6. Edge

Edge는 두 Node 사이의 이동 구간이다.

Edge {
    edge_id
    from_node
    to_node
    length
    width
    speed_limit
    direction
    capacity
    travel_time
    resource_id
    enabled

}

7. Edge Direction

FORWARD

REVERSE

BIDIRECTIONAL

예를 들어 좁은 단일 corridor가 양방향으로 사용되는 경우:

A ---------------- B
       Corridor

다음처럼 표현할 수 있다.

Edge C01

from = A

to = B

direction = BIDIRECTIONAL

capacity = 1

8. Narrow Corridor

본 프로젝트에서 가장 중요한 resource 중 하나이다.

예:

A ---- C01 ---- B

C01이 단일 차선이라면:

capacity = 1

로 설정한다.

Traffic Controller는 단순히 A*에서 C01을 통과하는 것뿐만 아니라 C01의 사용권을 관리해야 한다.

9. Corridor Resource

CorridorResource {
    resource_id
    type = CORRIDOR
    capacity
    allowed_direction
    entry_node
    exit_node
    min_travel_time
    max_travel_time
    current_occupants
    reservations

}

10. Intersection

Intersection은 여러 Edge가 만나는 지점이다.

예:
        B
        |
        |

A ------X------ C
        |
        |
        D

X를 Intersection Resource로 관리한다.

IntersectionResource {
    resource_id
    type = INTERSECTION
    conflict_groups
    capacity
    reservations

}

11. Conflict Group

Intersection에서는 단순 capacity보다 어떤 movement가 서로 충돌하는지가 중요하다.

예:

A → C

B → D

두 movement가 동시에 실행될 수 없다면:

ConflictGroup:

A-C

B-D

로 정의한다.

12. Movement

Intersection 내부 이동을 명시적으로 표현할 수 있어야 한다.

Movement {
    movement_id
    from_edge
    to_edge
    intersection_id
    conflict_group

}

예:

M01:

A → C

M02:

B → D

13. Waiting Bay

좁은 corridor에서 deadlock을 해결하기 위해 waiting bay를 사용할 수 있다.

WaitingBay {
    resource_id
    node_id
    capacity
    compatible_robot_types
    reservation

}

14. Resource

모든 traffic-critical 공간은 Resource로 표현할 수 있어야 한다.

Resource {
    resource_id
    type
    capacity
    current_occupancy
    reservations
    enabled

}

Resource Type:

CORRIDOR

INTERSECTION

WAITING_BAY

STATION

CHARGER

15. Resource와 Edge의 관계

Edge는 Resource를 참조할 수 있다.

Edge
 |
 +-- resource_id

예:

Edge E01

from = N01

to = N02

resource = C01

이렇게 하면 route가:

N01
 ↓

E01
 ↓

N02

인 경우 Traffic Controller는 E01을 이동하기 전에 C01 reservation을 확인할 수 있다.

16. Map Version

Map은 반드시 version을 가진다.

Map {
    map_id
    version
    created_at
    updated_at
    nodes
    edges
    resources

}

예:

warehouse_A
version = 12

17. Map Validation

Map Load 시 다음 항목을 검사해야 한다.

Node

중복 node_id

invalid coordinate

invalid type

Edge

존재하지 않는 node 참조

length <= 0

speed_limit <= 0

invalid direction

Resource

중복 resource_id

존재하지 않는 edge 참조

capacity <= 0

Graph

disconnected graph

unreachable node

orphan node

18. Reachability

다음 API를 제공한다.

is_reachable(
    start_node,
    goal_node

)

반환:

true

false

19. Neighbor Query

get_neighbors(node_id)

반환:

[
    edge_01,
    edge_02,
    edge_03

]

20. Resource Query

get_resource(resource_id)

get_resources_for_edge(edge_id)

get_resource_occupancy(resource_id)

21. Route와 Graph

Route는 다음 구조를 가진다.

Route {
    route_id
    robot_id
    start_node
    goal_node
    nodes
    edges
    total_distance
    estimated_time
    created_at

}

예:

N01
 ↓

E01
 ↓

N02
 ↓

E07
 ↓

N08
 ↓

E12
 ↓

N20

22. Traffic Graph와 Physical Map의 분리

Physical Map과 Traffic Graph를 분리한다.

Physical Map
    |
    v

Map Converter
    |
    v

Traffic Graph

Physical Map 변경이 Traffic Algorithm 코드에 직접 영향을 주지 않도록 한다.

23. Configuration

다음 값은 configuration으로 관리한다.

default_speed

corridor_capacity

intersection_capacity

robot_length

robot_width

safety_margin

waiting_bay_capacity

Algorithm 코드에 hard coding하지 않는다.

24. Acceptance Criteria

다음 기능이 구현되어야 한다.

Map Load

Map Save

Map Version

Node 조회

Edge 조회

Neighbor 조회

Resource 조회

Reachability

Map Validation

Corridor 정의

Intersection 정의

Conflict Group 정의

Waiting Bay 정의

25. Test Requirements

최소 다음 test를 구현한다.

test_map_load

test_map_validation

test_duplicate_node

test_invalid_edge

test_reachability

test_neighbor_query

test_resource_query

test_corridor

test_intersection

test_conflict_group

test_waiting_bay
