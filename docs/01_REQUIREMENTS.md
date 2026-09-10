Traffic Control Software

System Requirements

1. 목적

본 문서는 Traffic Control Software의 기능적/비기능적 요구사항을 정의한다.

대상 시스템은 약 100~200대의 AMR/AGV를 동시에 관리한다.

2. 환경

시스템은 다음과 같은 환경을 대상으로 한다.

Narrow corridor

Single lane corridor

Bidirectional corridor

Limited alternative route

Multiple intersection

Human / Robot mixed traffic

Dynamic blockage

Robot failure

Communication failure

3. Robot 요구사항

Traffic Controller는 다음 Robot 정보를 받을 수 있어야 한다.

robot_id

timestamp

position
velocity

heading

current_node

current_edge

current_task

battery

status

Robot 상태:

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

4. Task 요구사항

Task는 최소 다음 정보를 가진다.

task_id

pickup_location

delivery_location

priority

created_at

deadline

assigned_robot

status

Task 상태:

CREATED

ASSIGNED

PLANNED

EXECUTING

COMPLETED

FAILED

CANCELLED

5. Traffic Resource

Traffic Controller는 다음 resource를 관리한다.

CORRIDOR

INTERSECTION

WAITING_BAY

STATION

CHARGER

Resource:

resource_id

resource_type

capacity

direction

occupancy

reservation

6. Functional Requirements

FR-001 Fleet Management

최소 200대의 robot state를 동시에 관리할 수 있어야 한다.

FR-002 Route Planning

Robot의 출발지에서 목적지까지 route를 계산해야 한다.

FR-003 Corridor Control

좁은 corridor의 진입을 제어해야 한다.

FR-004 Intersection Control

Intersection의 충돌 movement를 제어해야 한다.

FR-005 Reservation

Robot이 resource를 사용하기 전에 reservation을 획득할 수 있어야 한다.

FR-006 Waiting

Reservation을 획득하지 못한 robot은 안전하게 대기할 수 있어야 한다.

FR-007 Dynamic Replanning

환경 변화가 발생하면 영향을 받는 robot을 재계획할 수 있어야 한다.

FR-008 Human Blockage

사람으로 인해 robot이 정지하는 상황을 처리해야 한다.

FR-009 Robot Failure

Robot이 고장난 경우 영향을 받는 robot을 처리해야 한다.

FR-010 Deadlock Detection

Traffic deadlock을 감지할 수 있어야 한다.

FR-011 Deadlock Recovery

Deadlock을 해소할 수 있어야 한다.

FR-012 Traffic Logging

Traffic decision을 추적할 수 있어야 한다.

7. Non-Functional Requirements

NFR-001 Scale

200 robot을 지원해야 한다.

NFR-002 Performance

초기 목표:

Planning P95 < 200 ms

Planning P99 < 500 ms

실제 production 목표는 benchmark 이후 결정한다.

NFR-003 Determinism

동일한 input과 동일한 seed에서는 동일한 simulation 결과를 생성할 수 있어야 한다.

NFR-004 Observability

모든 주요 traffic decision을 log로 추적할 수 있어야 한다.

NFR-005 Recovery

다음 장애 상황을 처리할 수 있어야 한다.

Robot failure

Communication failure

Controller restart

Reservation state inconsistency

8. Safety Requirement

Traffic Controller는 safety controller를 대체하지 않는다.

다음 기능은 별도의 safety system에서 처리한다.

Emergency Stop

Safety PLC

Safety Scanner

Protective Stop

Collision Safety

Traffic Controller는 strategic/tactical traffic coordination을 담당한다.

9. Acceptance Criteria

각 requirement에는 최소 하나 이상의 자동화된 test가 존재해야 한다.

예:

FR-003
    ↓

Corridor Reservation Test

FR-010
    ↓

Deadlock Detection Test

FR-011
    ↓

Deadlock Recovery Test
