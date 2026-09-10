Traffic Control Software

Traffic Controller Core Specification

1. 목적

Traffic Controller는 전체 Traffic Control System의 orchestration을 담당한다.

다음 component를 통합한다.

Map

Robot Manager

Task Manager

Global Planner

Reservation Manager

Priority Manager

Replanning Engine

Deadlock Manager

2. 전체 구조
                 Task Manager
                      |
                      v
                Route Planner
                      |
                      v
              Traffic Controller
                      |
        +-------------+-------------+
        |             |             |
        v             v             v
 Reservation      Priority      Replanning
 Manager          Manager        Engine
        |             |             |
        +-------------+-------------+
                      |
                      v
                Deadlock Manager
                      |
                      v
                 Robot Adapter

3. Traffic Controller Loop

기본적으로 Event Driven Architecture를 사용한다.

Event
  |
  v

State Update
  |
  v

Traffic Decision
  |
  v

Reservation
  |
  v

Command

4. 주요 Event

RobotStateUpdated

RobotArrived

RobotBlocked

RobotFailed

HumanDetected

HumanCleared

TaskCreated

TaskCancelled

TaskCompleted

ReservationRequested

ReservationGranted

ReservationExpired

DeadlockDetected

MapChanged

5. Event Processing

Event 수신:

Event Bus
   |
   v

Traffic Controller

Event 종류에 따라 handler를 호출한다.

예:

RobotBlocked
    |
    v

handle_robot_blocked()

6. Robot State Update

Robot 상태가 들어오면:

1. Validate state

2. Update Robot State

3. Update Resource Occupancy

4. Update Reservation

5. Detect Events

7. Robot Movement

Robot이 route를 따라 이동하면:

Current Edge
     |
     v

Next Resource
     |
     v

Reservation Check

Reservation이 있으면:

GRANTED
    |
    v

MOVE

없으면:

WAIT

8. Next Resource Planning

Robot이 현재 resource를 사용 중일 때 다음 resource를 미리 예약한다.

Current
   |
   +-- Next Resource
   |
   +-- Next+1 Resource

Rolling Horizon을 사용한다.

9. Traffic Decision

Traffic Controller는 다음 순서로 판단한다.

1. Safety condition

2. Deadlock condition

3. Resource conflict

4. Priority

5. Reservation

6. Route validity

7. Movement command

10. Safety Priority

Safety 관련 상태는 일반 traffic priority보다 항상 우선한다.

예:

Emergency Stop

Protective Stop

Safety Scanner

이들은 Traffic Controller의 일반 command와 별도의 safety layer에서 처리한다.

11. Reservation Decision

예:

R01 requests C01

R02 owns C01

Traffic Controller:

Conflict
   |
   v

Priority Manager
   |
   v

Decision

12. Decision

가능한 결과:

GRANT

WAIT

REPLAN

RECOVERY

ESCALATE

13. Human Blockage

HumanDetected
      |
      v

Robot Stop
      |
      v

Estimate Blockage Duration
      |
      +---- short ----> WAIT
      |
      +---- long -----> REPLAN

14. Human Clear

HumanCleared
     |
     v

Validate Reservation
     |
     +---- valid ----> RESUME
     |
     +---- invalid --> REPLAN

15. Robot Failure

RobotFailed
    |
    v

Mark Robot Failed
    |
    v

Identify Occupied Resources
    |
    v

Identify Affected Robots
    |
    v

Replanning

16. Deadlock

Traffic State
     |
     v

Wait-for Graph
     |
     v

Cycle?
     |
   YES
     |
     v

Deadlock Manager
     |
     v

Recovery

17. Command Generation

Traffic Controller는 Robot Adapter에 command를 전달한다.

Traffic Controller
        |
        v

RobotCommand
        |
        v

Robot Adapter
        |
        v

Robot

18. Command 종류

MOVE

WAIT

STOP

RESUME

REROUTE

GO_TO_WAITING_BAY

19. Command Idempotency

모든 command는 unique ID를 가진다.

command_id

Robot Adapter는 동일 command가 반복 전달되어도 중복 실행되지 않도록 한다.

20. State Machine

Traffic Controller의 Robot lifecycle:

IDLE
 |
 v

ASSIGNED
 |
 v

PLANNING
 |
 v

RESERVING
 |
 v

MOVING
 |
 +----> WAITING
 |          |
 |          v
 |       MOVING
 |
 +----> BLOCKED
            |
            v
        REPLANNING
            |
            v
          MOVING

21. Event Ordering

Event timestamp를 관리한다.

event_timestamp

controller_timestamp

오래된 event는 현재 상태를 덮어쓰지 않도록 한다.

22. Out-of-Order Event

예:

Event A timestamp = 100

Event B timestamp = 90

B가 늦게 도착하면 이미 처리된 최신 state를 덮어쓰지 않는다.

23. Duplicate Event

같은 event가 여러 번 들어올 수 있다.

event_id

를 사용하여 duplicate를 제거한다.

24. Controller State

Traffic Controller는 전체 system state를 유지한다.

TrafficState {
    robots
    resources
    reservations
    tasks
    events

}

25. Snapshot

주기적으로 system state snapshot을 저장할 수 있어야 한다.

snapshot_id

timestamp

robots

resources

reservations

tasks

Controller restart 시 recovery에 사용한다.

26. Recovery

Controller가 재시작하면:

Load Snapshot
     |
     v

Load Reservation State
     |
     v

Query Robot States
     |
     v

Reconcile
     |
     v

Resume Traffic Control

27. Reconciliation

Controller state와 실제 Robot state가 다를 경우:

Controller State
       vs

Robot State

차이를 검출한다.

예:

Controller:

R01 = C01

Robot:

R01 = C02

이 경우 reservation과 route를 재검증한다.

28. Metrics

Traffic Controller는 다음 metric을 제공한다.

active_robot_count

active_reservation_count

waiting_robot_count

blocked_robot_count

deadlock_count

replanning_count

reservation_conflict_count

command_latency

planning_latency

29. API

submit_task()

cancel_task()

update_robot_state()

request_route()

request_reservation()

get_robot_state()

get_resource_state()

get_traffic_state()

trigger_replanning()

trigger_recovery()

30. Observability

다음 log level을 지원한다.

ERROR

WARN

INFO

DEBUG

TRACE

Production에서는 기본적으로:

INFO

WARN

ERROR

를 사용한다.

31. Decision Trace

Traffic decision을 사후에 재현할 수 있어야 한다.

예:

10:00:00 R01 requests C01

10:00:00 C01 occupied by R02

10:00:00 priority calculation

10:00:00 R01 WAIT

10:00:03 R02 exits C01

10:00:03 R01 GRANT

32. Performance Target

초기 목표:

200 robots

이벤트 처리:

P95 < 50 ms

P99 < 100 ms

Route planning은 별도의 latency budget으로 관리한다.

33. Concurrency

Traffic Controller는 여러 event를 동시에 처리할 수 있어야 한다.

하지만 동일 resource에 대한 결정은 serialization 또는 atomic transaction을 보장해야 한다.

34. Failure Isolation

하나의 Robot 오류가 전체 Traffic Controller를 crash시키면 안 된다.

예:

R01 malformed message

이어도:

R02 ~ R200

의 traffic control은 계속 동작해야 한다.

35. Acceptance Criteria

Traffic Controller는 다음 scenario를 성공적으로 처리해야 한다.

10 robots

50 robots

100 robots

200 robots

그리고:

Normal Traffic

Human Blockage

Robot Failure

Congestion

Reservation Conflict

Deadlock

Controller Restart

Communication Delay

를 모두 simulation으로 검증한다.

36. Definition of Done

Traffic Controller Phase는 다음 조건을 만족해야 한다.

Event-driven architecture

Robot state management

Reservation integration

Priority integration

Replanning integration

Deadlock integration

Command generation

Duplicate event handling

Out-of-order event handling

Controller recovery

State reconciliation

Metrics

Decision logging

200 robot simulation
