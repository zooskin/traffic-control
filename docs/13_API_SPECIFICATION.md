Traffic Control Software

API Specification

1. 목적

Traffic Controller와 외부 시스템 간 API를 정의한다.

2. API Architecture

WMS / MES
   |
   v

Task API
   |
   v

Traffic Controller
   |
   v

Robot Adapter

3. Task API

POST /tasks

Task 생성.

Request:

{
  "task_id": "T001",
  "robot_id": "R001",
  "source": "A01",
  "destination": "B01",
  "priority": 50

}

Response:

{
  "task_id": "T001",
  "status": "ACCEPTED"

}

4. Cancel Task

POST /tasks/{task_id}/cancel

5. Robot State

POST /robots/{robot_id}/state

Example:

{
  "timestamp": 1710000000,
  "node_id": "C01",
  "status": "MOVING",
  "battery": 82

}

6. Robot Command

POST /robots/{robot_id}/commands

Request:

{
  "command_id": "CMD001",
  "type": "MOVE",
  "target": "C02"

}

7. Reservation API

POST /reservations

Request:

{
  "robot_id": "R001",
  "resources": [
    {
      "resource_id": "C01",
      "start": 1000,
      "end": 1010
    }
  ]

}

8. Replanning API

POST /robots/{robot_id}/replan

Request:

{
  "reason": "HUMAN_BLOCKAGE",
  "blocked_resources": [
    "C01"
  ]

}

9. Deadlock API

GET /traffic/deadlocks

Response:

{
  "deadlocks": [
    {
      "id": "DL001",
      "robots": ["R01", "R02", "R03"],
      "status": "CONFIRMED"
    }
  ]

}

10. Robot State API

GET /robots/{robot_id}

11. Traffic State

GET /traffic/state

Response:

{
  "active_robots": 143,
  "waiting_robots": 12,
  "blocked_robots": 3,
  "active_reservations": 281,
  "deadlocks": 0

}

12. Map API

GET /map

GET /map/resources

GET /map/corridors

13. Health

GET /health

GET /ready

GET /metrics

14. Error Model

{
  "error_code": "RESOURCE_CONFLICT",
  "message": "Resource is already reserved",
  "request_id": "REQ001"

}

15. Error Codes

INVALID_REQUEST

ROBOT_NOT_FOUND

TASK_NOT_FOUND

RESOURCE_NOT_FOUND

RESOURCE_CONFLICT

RESERVATION_EXPIRED

NO_ROUTE

PLANNING_TIMEOUT

DEADLOCK_DETECTED

RECOVERY_FAILED

STALE_EVENT

DUPLICATE_EVENT

INTERNAL_ERROR

16. Idempotency

Task 생성, reservation, command API는 idempotency를 지원한다.

Header:

Idempotency-Key

17. Event API

External system으로 다음 event를 publish한다.

TaskAccepted

TaskStarted

TaskCompleted

TaskFailed

RobotBlocked

RobotRecovered

RobotFailed

DeadlockDetected

DeadlockResolved

ReservationGranted

ReservationReleased

ReplanningStarted

ReplanningCompleted

18. API Version

초기:
/api/v1

Breaking change 발생 시:
/api/v2

19. API Design Principle

API는 traffic algorithm을 외부에 노출하지 않는다.

외부 시스템:

"R01을 움직여라"

가 아니라:

"R01에 Task T01을 할당"

하도록 설계한다.

Traffic Controller가 실제 movement를 결정한다.
