Traffic Control Software

Robot & Task Model Specification

1. 목적

본 문서는 Traffic Controller에서 사용하는 Robot과 Task의 상태 및 lifecycle을 정의한다.

Traffic Controller의 모든 알고리즘은 Robot의 현재 상태를 기준으로 동작해야 한다.

2. Robot 기본 모델

Robot {
    robot_id
    pose
    velocity
    heading
    footprint
    status
    current_node
    current_edge
    current_task
    route
    reservation
    battery
    last_update
    created_at

}

3. Robot Pose

Pose {
    x
    y
    theta

}

4. Robot Footprint

Traffic Controller에서 corridor 및 intersection 점유 판단을 위해 robot 크기를 관리한다.

Footprint {
    length
    width
    safety_margin

}

5. Robot Status

IDLE

ASSIGNED

PLANNING

MOVING

WAITING

BLOCKED

REPLANNING

RECOVERY

ARRIVED

FAILED

EMERGENCY_STOP

6. Robot State Machine

정상 lifecycle:

IDLE
  |
  v

ASSIGNED
  |
  v

PLANNING
  |
  v

MOVING
  |
  v

ARRIVED
  |
  v

IDLE

7. Waiting

Robot이 traffic resource를 사용하지 못하면 WAITING 상태가 된다.

MOVING
   |
   v

WAITING
   |
   +----> MOVING

Waiting 상태에서는 다음 정보를 저장한다.

waiting_since

waiting_resource

waiting_reason

8. Waiting Reason

RESOURCE_OCCUPIED

HIGHER_PRIORITY_ROBOT

HUMAN_BLOCKAGE

RESERVATION_DENIED

CONGESTION

DEADLOCK_RECOVERY

SAFETY_STOP

9. Blocked

Robot이 예상 경로에서 일정 시간 이상 진행하지 못하면 BLOCKED 상태가 될 수 있다.

MOVING
   |
   v

BLOCKED

Blocked 판단은 configuration으로 관리한다.

예:

blocked_timeout = 5 sec

minimum_progress = 0.1 m

10. Replanning

BLOCKED
   |
   v

REPLANNING
   |
   +----> MOVING
   |
   +----> WAITING
   |
   +----> RECOVERY

11. Failed

Robot failure:

MOVING
   |
   v

FAILED

Failed robot이 점유하고 있던 resource는 상황에 따라 유지하거나 강제 해제할 수 있어야 한다.

안전한 resource release 여부는 별도의 정책으로 결정한다.

12. Task

Task {
    task_id
    pickup_location
    delivery_location
    priority
    created_at
    deadline
    assigned_robot
    status

}

13. Task State

CREATED

ASSIGNED

PLANNED

EXECUTING

COMPLETED

FAILED

CANCELLED

14. Task State Machine

CREATED
   |
   v

ASSIGNED
   |
   v

PLANNED
   |
   v

EXECUTING
   |
   v

COMPLETED

Failure:

EXECUTING
   |
   v

FAILED

Cancellation:

CREATED / ASSIGNED / PLANNED
   |
   v

CANCELLED

15. Task Priority

Priority는 숫자로 표현한다.

priority = 0 ~ 100

높은 숫자가 높은 우선순위를 의미한다.

단순 priority만 사용하지 않고 waiting time 등을 Traffic Priority에서 별도로 고려한다.

16. Task Assignment

Task와 Robot assignment는 별도 모듈에서 담당한다.

Task Manager
      |
      v

Assignment Engine
      |
      v

Robot

Traffic Controller는 assignment 결과를 받아 route planning을 수행한다.

17. Robot Route

Robot은 현재 route를 가진다.

Robot
 |
 +-- current_route
 |
 +-- current_route_index

예:

Route:

N01

N05

N07

N12

N20

현재 위치:

current_route_index = 2

18. Robot Reservation

Robot은 여러 reservation을 가질 수 있다.

예:

R01
 |
 +-- C01
 +-- I03
 +-- C07

Reservation은 시간 순서대로 관리한다.

19. Robot Event

Robot 상태 변경은 event를 생성한다.

RobotStateChanged

예:

robot_id = R01

old_state = MOVING

new_state = WAITING

reason = RESOURCE_OCCUPIED

resource_id = C01

20. State Update

Robot은 주기적으로 상태를 전송한다.

예:

RobotStateUpdate {
    robot_id
    timestamp
    x
    y
    theta
    velocity
    current_node
    current_edge
    battery
    status

}

21. State Timeout

일정 시간 동안 Robot 상태가 업데이트되지 않으면 timeout 상태를 감지한다.

last_update
        |
        v

timeout?
        |
        +---- YES ---> COMMUNICATION_LOST

COMMUNICATION_LOST는 Robot 내부 상태와 별도로 system event로 관리할 수 있다.

22. State Consistency

Traffic Controller가 가진 Robot 상태와 실제 Robot 상태가 다를 수 있다.

따라서 다음 정보를 관리한다.

controller_timestamp

robot_timestamp

last_command_id

last_ack_command_id

23. Command

Traffic Controller가 Robot에 보내는 command:

RobotCommand {
    command_id
    robot_id
    action
    route
    target_node
    created_at

}

Action:

MOVE

WAIT

STOP

RESUME

REROUTE

GO_TO_WAITING_BAY

Emergency Stop은 일반 Traffic Command와 분리한다.

24. Acceptance Criteria

다음 기능을 구현해야 한다.

Robot lifecycle

Task lifecycle

Robot state update

Robot state timeout

Task assignment interface

Route association

Reservation association

Robot command

State transition validation

Event generation

25. Test Requirements

test_robot_state_transition

test_invalid_robot_transition

test_task_state_transition

test_waiting_state

test_blocked_state

test_replanning_state

test_robot_timeout

test_command_generation

test_route_assignment

test_reservation_assignment
