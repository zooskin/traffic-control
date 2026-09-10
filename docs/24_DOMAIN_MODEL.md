Traffic Control Software

Domain Model Specification

1. Purpose

본 문서는 Multi-Robot Traffic Control Software에서 사용하는 핵심 Domain Entity와 Value Object를 정의한다.

Domain Model은 Infrastructure와 분리한다.

2. Domain Overview

Robot
  |
  +---- RobotState
  |
  +---- Task
  |
  +---- Route
  |
  +---- Reservation
  |
  +---- Conflict
  |
  +---- TrafficDecision

Map:

Map
 |
 +---- Node
 |
 +---- Edge
 |
 +---- Corridor
 |
 +---- Intersection

3. Robot

Robot은 실제 Fleet의 이동 주체다.

Robot
├── robot_id
├── state
├── current_task
├── current_route
└── capabilities

필수 식별자는 robot_id다.

Robot Entity는 Vendor SDK에 의존하지 않는다.

4. RobotState

Robot의 현재 상태를 표현한다.

robot_id

position
velocity

current_node

current_edge

current_task

current_route

state

timestamp

state_version

State:

MOVING

IDLE

WAITING

TEMPORARILY_STOPPED

BLOCKED

FAILED

UNKNOWN

5. RobotState Transition

권장 Transition:

IDLE
 |
 v

MOVING
 |
 +----> WAITING
 |
 +----> TEMPORARILY_STOPPED
 |
 +----> BLOCKED
 |
 +----> FAILED
 |
 v

IDLE

잘못된 Transition은 명시적으로 거부할 수 있어야 한다.

6. Task

Robot에게 주어진 작업이다.

task_id

robot_id

source

destination

priority

deadline

status

created_at

Task State:

CREATED

ASSIGNED

PLANNING

RUNNING

WAITING

COMPLETED

CANCELLED

FAILED

7. Node

Graph의 위치 단위다.

node_id

position

type

Node Type 예:

NORMAL

INTERSECTION

CHARGING

LOADING

UNLOADING

HOLDING

8. Edge

두 Node를 연결하는 이동 경로다.

edge_id

from_node

to_node

length

width

direction

speed_limit

capacity

resource_id

Direction:

FORWARD

REVERSE

BIDIRECTIONAL

9. Corridor

좁고 긴 이동 영역이다.

Corridor는 Traffic Resource로 취급할 수 있다.

corridor_id

entry_node

exit_node

length

width

capacity

direction

필요한 경우 내부 Edge 여러 개를 하나의 Corridor Resource로 묶을 수 있다.

10. Intersection

여러 이동 경로가 만나는 영역이다.

intersection_id

connected_nodes

connected_edges

capacity

Intersection은 Conflict Detection과 Reservation의 주요 대상이다.

11. Route

Robot이 이동할 경로다.

route_id

robot_id

map_version

nodes

edges

created_at

expires_at

Route에는 Version을 부여할 수 있다.

12. Route Segment

Route의 개별 이동 단위다.

edge_id

sequence

expected_entry_time

expected_exit_time

이를 기반으로 Temporal Conflict를 검출할 수 있다.

13. Reservation

Robot이 특정 Resource를 사용할 권리를 예약한 것이다.

reservation_id

robot_id

resource_id

start_time

end_time

priority

state

State:

PENDING

ACTIVE

RELEASED

EXPIRED

CANCELLED

14. Resource

Traffic Control에서 경쟁적으로 사용되는 자원이다.

예:

NODE

EDGE

CORRIDOR

INTERSECTION

CHARGING_AREA

LOADING_AREA

Resource에는 고유한 resource_id가 있어야 한다.

15. Conflict

둘 이상의 Robot이 동시에 특정 Resource를 사용할 수 없는 상황이다.

Conflict
├── conflict_id
├── robot_a
├── robot_b
├── resource
├── conflict_type
├── severity
└── detected_at

16. Conflict Type

NODE_CONFLICT

EDGE_CONFLICT

HEAD_ON

CROSSING

CORRIDOR_CONFLICT

RESOURCE_CONFLICT

TEMPORAL_CONFLICT

17. Priority

Traffic Decision에 사용되는 우선순위다.

Input:

base_priority

waiting_time

deadline

blocking_impact

task_priority

Output:

effective_priority

Priority는 가능한 경우 deterministic해야 한다.

18. TrafficEvent

Traffic Control을 동작시키는 입력이다.

event_id

event_type

timestamp

robot_id

state_version

map_version

payload

Event Type:

ROBOT_STATE_UPDATED

ROBOT_STOPPED

ROBOT_BLOCKED

ROBOT_RECOVERED

TASK_CREATED

TASK_COMPLETED

TASK_CANCELLED

RESERVATION_EXPIRED

MAP_UPDATED

19. TrafficDecision

Traffic Controller가 생성하는 최종 Traffic 명령이다.

decision_id

robot_id

action

route

reservation

reason

created_at

state_version

Action:

GO

WAIT

STOP

REPLAN

HOLD

RECOVER

20. PlanningRequest

Planner에 전달되는 입력이다.

request_id

robot_ids

start_states

goals

map_version

traffic_state_version

reservations

constraints

deadline

timeout

21. PlanningResult

Planner가 반환하는 결과다.

request_id

status

routes

map_version

traffic_state_version

planning_time

Status:

SUCCESS

NO_PATH

TIMEOUT

CANCELLED

FAILED

22. Deadlock

둘 이상의 Robot이 서로의 진행을 막아 영구적으로 진행하지 못하는 상태다.

deadlock_id

robots

resources

type

detected_at

status

Type:

RESOURCE_DEADLOCK

CORRIDOR_DEADLOCK

HEAD_ON_DEADLOCK

CYCLE_DEADLOCK

23. DeadlockRecovery

Deadlock 해소 작업이다.

recovery_id

deadlock_id

strategy

target_robots

status

created_at

Strategy:

REPLAN

CHANGE_PRIORITY

BACKTRACK

RELEASE_RESOURCE

MOVE_TO_HOLDING

HUMAN_INTERVENTION

24. HumanBlockage

Human 또는 Human Activity에 의해 Traffic Resource가 막힌 상태다.

blockage_id

resource_id

detected_at

estimated_duration

confidence

status

Status:

FREE

TEMPORARILY_BLOCKED

LONG_BLOCKED

UNKNOWN

25. Value Objects

가능한 경우 다음을 Value Object로 정의한다.

RobotId

TaskId

NodeId

EdgeId

RouteId

ReservationId

ResourceId

Position

Velocity

Timestamp

Duration

Priority

26. ID Rules

ID는 Domain 내에서 고유해야 한다.

예:

R001

TASK-00001

EDGE-001

CORRIDOR-01

RES-00001

실제 형식은 Implementation 단계에서 결정한다.

27. Time Model

Traffic Planning에서는 시간 정보가 중요하다.

가능하면 다음을 구분한다.

Timestamp

Duration

ExpectedEntryTime

ExpectedExitTime

Deadline

모든 시간은 동일한 기준을 사용한다.

28. Version Model

다음 Entity에는 Version을 사용할 수 있어야 한다.

RobotState

Map

Route

TrafficDecision

PlanningRequest

PlanningResult

Version은 Stale Result를 방지하는 데 사용한다.

29. Entity Relationship

Robot
 |
 +---- Task
 |
 +---- RobotState
 |
 +---- Route
 |       |
 |       +---- Edge
 |
 +---- Reservation
         |
         +---- Resource

Robot + Route + Reservation
          |
          v
       Conflict
          |
          v
       Priority
          |
          v
    TrafficDecision

30. Domain Invariants

다음 조건은 항상 만족해야 한다.

Robot

robot_id는 unique

Robot은 하나의 current state를 가진다.

Task

Task는 하나의 Robot에 할당될 수 있다.

완료된 Task는 다시 RUNNING으로 전환하지 않는다.

Reservation

동일 Resource의 충돌하는 Reservation은 동시에 ACTIVE일 수 없다.

RELEASED Reservation은 다시 ACTIVE로 변경하지 않는다.

Route

Route는 유효한 Map Version을 참조한다.

Route의 Node/Edge 연결이 유효해야 한다.

31. Domain vs Infrastructure

Domain에서 직접 의존하지 않는다.

Database

Network

ROS

gRPC

Vendor SDK

Filesystem

예:

나쁜 구조:

Robot
 |
 +-- ROS API
 +-- Database

좋은 구조:

Robot
 |

Robot Domain
 |

Adapter / Repository
 |

Infrastructure

32. Serialization

Domain Object 자체에 Serialization 책임을 과도하게 넣지 않는다.

권장:

Domain Object
      |

Mapper
      |

DTO / Proto
      |

Network

33. Equality

ID가 있는 Entity는 기본적으로 Identity를 기준으로 비교한다.

Value Object는 Value Equality를 사용한다.

예:

RobotId("R01") == RobotId("R01")

34. Domain Validation

Invalid State는 생성 단계에서 가능한 한 방지한다.

예:

Reservation:

start_time < end_time

Route:

nodes != empty

Task:

source != destination

실제 규칙은 Traffic Specification과 일치해야 한다.

35. Domain Evolution

Domain Model 변경은 기존 데이터와 API에 영향을 줄 수 있다.

변경 시:

Compatibility 확인

Serialization 확인

Test 수정

Migration 필요성 확인

36. Final Principle

Domain Model은 Traffic Control의 공통 언어다.

다음 Component들이 동일한 Domain 의미를 사용해야 한다.

Traffic Controller

Planner

Reservation Manager

Conflict Detector

Deadlock Manager

Simulation

Monitoring

Domain의 의미를 Component마다 다르게 정의하지 않는다.
