Traffic Control Software

Data Model Specification

1. 목적

Traffic Control System에서 사용하는 핵심 domain model을 정의한다.

2. Robot

Robot {
    robot_id
    status
    position
    current_node
    current_edge
    destination
    current_task
    route
    battery
    capability
    last_update

}

3. Robot Status

IDLE

ASSIGNED

PLANNING

RESERVING

MOVING

WAITING

BLOCKED

REPLANNING

RECOVERY

FAILED

COMPLETED

4. Node

Node {
    node_id
    x
    y
    type
    capacity

}

Node Type:

NORMAL

INTERSECTION

CORRIDOR_ENTRY

CORRIDOR_EXIT

WAITING_BAY

CHARGER

STATION

5. Edge

Edge {
    edge_id
    from
    to
    length
    speed
    capacity
    direction
    enabled

}

6. Resource

Traffic control에서 실제 conflict를 판단하는 기본 단위.

Resource {
    resource_id
    type
    capacity
    conflict_set

}

7. Resource Type

EDGE

NODE

CORRIDOR

INTERSECTION

WAITING_BAY

STATION

8. Corridor

Corridor {
    corridor_id
    entry_node
    exit_node
    edges
    capacity
    direction_mode
    current_direction
    direction_lock_until

}

9. Reservation

Reservation {
    reservation_id
    robot_id
    resource_id
    start_time
    end_time
    status
    priority

}

10. Reservation Status

REQUESTED

GRANTED

ACTIVE

RELEASED

EXPIRED

CANCELLED

11. Task

Task {
    task_id
    robot_id
    source
    destination
    priority
    created_at
    started_at
    completed_at
    status

}

12. Task Status

CREATED

ASSIGNED

RUNNING

WAITING

COMPLETED

FAILED

CANCELLED

13. Route

Route {
    route_id
    robot_id
    nodes
    edges
    estimated_cost
    estimated_time
    created_at
    planner

}

14. Replanning Request

ReplanningRequest {
    request_id
    robot_id
    reason
    blocked_resources
    current_route
    priority
    created_at

}

15. Deadlock

Deadlock {
    deadlock_id
    robots
    resources
    wait_edges
    detected_at
    confirmed_at
    resolved_at
    status
    recovery_attempts

}

16. Wait Edge

WaitEdge {
    waiting_robot
    blocking_robot
    resource_id
    created_at

}

17. Human Blockage

HumanBlockage {
    blockage_id
    resource_id
    detected_at
    cleared_at
    confidence
    status

}

18. Event

Event {
    event_id
    type
    timestamp
    source
    payload

}

19. Event Type

ROBOT_STATE_UPDATED

ROBOT_BLOCKED

ROBOT_FAILED

ROBOT_RECOVERED

HUMAN_DETECTED

HUMAN_CLEARED

TASK_CREATED

TASK_CANCELLED

TASK_COMPLETED

RESERVATION_GRANTED

RESERVATION_RELEASED

REPLANNING_REQUESTED

REPLANNING_COMPLETED

DEADLOCK_DETECTED

DEADLOCK_RESOLVED

MAP_CHANGED

20. Historical Data

운영 분석을 위해 다음 데이터를 저장한다.

Robot trajectory

Reservation history

Task history

Route changes

Deadlock history

Replanning history

Waiting history

Traffic decisions

21. Time Representation

모든 timestamp는 UTC 기반 Unix epoch 또는 ISO-8601 중 하나로 통일한다.

Runtime 내부에서는 monotonic clock을 사용할 수 있다.

22. ID 규칙

ID는 사람이 읽을 수 있어야 한다.

예:

R001

T000123

C012

RES000123

DL00012

Production에서는 collision-free unique ID를 보장한다.

23. Serialization

외부 API:

JSON

내부 high-performance communication:

Protocol Buffers

를 사용할 수 있다.

24. Schema Evolution

Data schema 변경 시 backward compatibility를 고려한다.

필드 삭제보다:

deprecated

처리를 우선한다.
