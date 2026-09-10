Traffic Control Software
Implementation Workflow
1. Purpose

본 문서는 Multi-Robot Traffic Control Software를 실제로 개발하기 위한 단계별 Implementation Workflow를 정의한다.

개발은 한 번에 전체 시스템을 구현하지 않고 작은 단위로 진행한다.

기본 원칙:

작은 구현 -> 테스트 -> 검증 -> 측정 -> 문서화 -> 다음 단계

2. Overall Development Flow

전체 프로젝트는 다음 순서로 진행한다.

Phase 0  Repository Setup
Phase 1  Core Domain
Phase 2  Map & Graph
Phase 3  Robot State
Phase 4  Task Model
Phase 5  Reservation
Phase 6  Conflict Detection
Phase 7  Priority
Phase 8  Route Planning
Phase 9  Deadlock Detection
Phase 10 Deadlock Recovery
Phase 11 Traffic Controller
Phase 12 Dynamic Replanning
Phase 13 Simulation
Phase 14 Integration
Phase 15 Performance
Phase 16 Scale Test
Phase 17 Production Hardening

Phase 0. Repository Setup
Goal

개발 환경과 기본 Repository를 구성한다.

Tasks
Git Repository 구성
CMake 구성
C++20 설정
GoogleTest 구성
Logging 구성
Formatting 구성
Static Analysis 구성
CI 구성
Directory
traffic-control/
├── CMakeLists.txt
├── README.md
├── docs/
├── include/
├── src/
├── tests/
├── simulation/
├── benchmarks/
├── tools/
├── configs/
└── scripts/

완료 조건
Build 성공
Unit Test 실행 가능
Logging 동작
CI 동작
기본 Example 실행
Phase 1. Core Domain
Goal

Traffic Control에 필요한 기본 Domain Model을 만든다.

주요 Model
Robot
RobotState
Task
Node
Edge
Route
Reservation
Conflict
Priority
TrafficEvent
TrafficDecision
원칙

Domain Model은 다음에 직접 의존하지 않는다.

Database
Network
Robot Vendor SDK
UI
External Service
완료 조건
Domain 객체 정의
State Transition 정의
Unit Test 작성
Serialization과 Domain 분리
Phase 2. Map & Graph
Goal

Traffic Planning에 사용할 Map Graph를 구현한다.

기본 구조
Node
 |
Edge
 |
Node

Edge Attribute

최소 다음 정보를 고려한다.

length
width
direction
speed_limit
capacity
resource_id
Corridor

좁은 Corridor는 독립적인 Traffic Resource로 표현할 수 있도록 한다.

예:

Node A
   |
Corridor C01
   |
Node B

완료 조건
Graph 생성
Node 연결
Edge 연결
Direction 처리
Corridor 표현
Map Load
Map Validation
Phase 3. Robot State
Goal

100~200대 Robot의 상태를 일관되게 관리한다.

State
MOVING
IDLE
WAITING
TEMPORARILY_STOPPED
BLOCKED
FAILED
UNKNOWN

State Information
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

완료 조건
State Update
State Query
State Version
State Transition Test
Stale State Detection
Phase 4. Task Model
Goal

동적으로 생성되는 Task를 관리한다.

Task
task_id
robot_id
source
destination
priority
deadline
status
created_at

Task State
CREATED
ASSIGNED
PLANNING
RUNNING
WAITING
COMPLETED
CANCELLED
FAILED

완료 조건
Task 생성
Task 할당
Task 시작
Task 완료
Task 취소
Task 실패
Priority 처리
Phase 5. Reservation System
Goal

Traffic Resource의 점유를 관리한다.

Reservation
reservation_id
robot_id
resource_id
start_time
end_time
priority
state

Operation
Check
Reserve
Release
Expire
Query
Resource

필요에 따라 다음을 지원한다.

Node
Edge
Corridor
Intersection
Charging Area
Loading Area
완료 조건
Reservation 생성
Conflict Detection
Release
Expiration
Overlap Test
Concurrent Access Test
Phase 6. Conflict Detection
Goal

Robot 간 Traffic Conflict를 탐지한다.

Conflict Type
Node Conflict
Edge Conflict
Head-on Conflict
Crossing Conflict
Corridor Conflict
Resource Conflict
Temporal Conflict
Flow
Robot State
     +
Route
     +
Reservation
     |
     v
Conflict Detector
     |
     v
Conflict List

완료 조건
Two Robot Conflict
Multi Robot Conflict
Temporal Conflict
Corridor Conflict
Regression Test
Phase 7. Priority System
Goal

Conflict 상황에서 어떤 Robot이 우선적으로 진행할지 결정한다.

Priority Input
Task Priority
Waiting Time
Deadline
Robot State
Corridor Position
Direction
Blocking Impact
Starvation Prevention

특정 Robot이 영원히 대기하지 않도록 Waiting Time을 Priority에 반영할 수 있어야 한다.

예:

Effective Priority =
Base Priority
+
Waiting Time Bonus
+
Deadline Urgency
+
Blocking Impact

실제 수식은 Simulation과 Benchmark를 통해 결정한다.

완료 조건
Priority Calculation
Tie Breaking
Starvation Test
Fairness Test
Regression Test
Phase 8. Route Planning
Goal

Robot의 Route를 계산한다.

Planner Interface
IRoutePlanner

초기 Algorithm 후보
A*
PIBT
WHCA*
ECBS
권장 개발 순서
Single Robot A*
        |
        v
Multi Robot Planning
        |
        v
Reservation-aware Planning
        |
        v
Dynamic Replanning

개발 우선순위

본 환경에서는 초기 단계에서 다음을 우선한다.

Reliability
Predictable Latency
Deadlock Behavior
Dynamic Replanning
Optimality
Phase 9. Deadlock Detection
Goal

Traffic Deadlock을 탐지한다.

Wait-for Graph

예:

R01 -> R02
R02 -> R03
R03 -> R01

Cycle이 발생하면 Deadlock 가능성을 평가한다.

Deadlock Type
RESOURCE_DEADLOCK
CORRIDOR_DEADLOCK
HEAD_ON_DEADLOCK
CYCLE_DEADLOCK
완료 조건
Two Robot Deadlock
Three Robot Deadlock
Corridor Deadlock
Resource Deadlock
False Positive Test
Phase 10. Deadlock Recovery
Goal

탐지된 Deadlock을 해소한다.

Recovery Strategy

우선 다음 순서로 고려한다.

Replanning
Priority Change
Backtracking
Resource Release
Safe Holding Area
Human Intervention
중요한 원칙

Recovery 자체가 새로운 Deadlock을 만들지 않는지 검증한다.

완료 조건
Recovery Success
Recovery Failure
Recovery Timeout
Recovery Regression
Phase 11. Traffic Controller
Goal

Traffic Control Component를 하나의 Decision Loop으로 통합한다.

기본 Flow
Event
  |
  v
State Update
  |
  v
Task Update
  |
  v
Conflict Detection
  |
  v
Priority
  |
  v
Planning
  |
  v
Reservation
  |
  v
Decision
  |
  v
Command

TrafficController 책임
Event 처리
State Coordination
Planner 호출
Reservation Coordination
Decision 생성

TrafficController가 특정 Planner를 직접 구현하지 않는다.

Phase 12. Dynamic Replanning
Goal

Dynamic Environment 변화에 대응한다.

Replanning Trigger
Robot Stop
Human Blockage
Robot Failure
Task Change
Reservation Conflict
Map Change
Deadlock
Local Replanning

가능하면 전체 Robot을 Replanning하지 않는다.

Event
 |
 v
Affected Robots
 |
 v
Local Replanning

필요한 경우:

Local Replanning
 |
 v
Global Replanning

으로 확장한다.

Phase 13. Simulation
Goal

실제 Robot 적용 전에 Traffic Algorithm을 검증한다.

Scenario

최소 다음 Scenario를 구현한다.

Single Corridor
Two-way Corridor
Intersection
Bottleneck
Dead End
Human Blockage
Robot Stop
Robot Failure
High Traffic
Low Traffic
Task Burst
Deadlock
Recovery
Robot Scale
10
50
100
150
200
500
Simulation Metrics
Success Rate
Average Travel Time
Average Wait Time
Deadlock Count
Replanning Count
Throughput
Planner Latency
CPU
Memory
Phase 14. Integration
Goal

실제 Robot System과 연결한다.

Architecture
Traffic Core
     |
     v
IRobotAdapter
     |
     v
Vendor Adapter
     |
     v
Robot Fleet

Integration 대상
Robot Fleet Manager
Navigation System
Task System
Map System
Monitoring System
Robot Vendor SDK

Core Domain은 외부 시스템과 직접 연결하지 않는다.

Phase 15. Performance
Goal

100~200대 환경에서 안정적인 Decision Loop을 확보한다.

측정 항목
Planner Latency
Conflict Detection Latency
Reservation Latency
Decision Latency
CPU
Memory
Throughput
Replanning Rate
Deadlock Rate
초기 Performance Target

초기 Engineering Target:

P50 < 10 ms
P95 < 50 ms
P99 < 100 ms

이 값은 실제 Benchmark 결과를 기반으로 조정한다.

Phase 16. Scale Test
Goal

Robot 증가에 따른 Scaling 특성을 확인한다.

Test
10 Robots
50 Robots
100 Robots
150 Robots
200 Robots
500 Robots

비교
Planning Latency
Decision Latency
Deadlock Rate
Average Wait Time
CPU
Memory
Throughput

단일 평균값보다 Robot 수 증가에 따른 Scaling Curve를 분석한다.

Phase 17. Production Hardening
Goal

실제 운영 환경에서 사용할 수 있는 수준으로 안정화한다.

Failure Scenario
Robot Failure
Network Failure
Planner Timeout
Reservation Expiration
State Desynchronization
Process Restart
Recovery
Task Burst
Human Blockage
Production Checklist
[ ] Configuration Validation
[ ] Health Check
[ ] Metrics
[ ] Logging
[ ] Alerting
[ ] Error Recovery
[ ] Restart Recovery
[ ] State Recovery
[ ] Simulation Regression
[ ] Scale Test
[ ] Integration Test

18. AI Agent Work Unit

AI Agent에게 작업을 요청할 때는 하나의 Logical Objective만 전달한다.

좋은 예:

Implement Reservation expiration.

좋지 않은 예:

Implement Reservation,
rewrite Planner,
add Deadlock Recovery,
optimize entire system.

19. AI Agent Task Template

AI Agent에게 작업을 전달할 때 다음 Template을 사용한다.

Task:
<작업 내용>

Context:
<관련 배경>

Requirements:
<요구사항>

Constraints:
<제약조건>

Relevant Files:
<관련 파일>

Expected Result:
<완료 결과>

Tests:
<필요한 테스트>

20. Implementation Workflow

각 작업은 다음 순서를 따른다.

1. Inspect
2. Understand
3. Plan
4. Implement
5. Build
6. Unit Test
7. Integration Test
8. Benchmark if required
9. Review
10. Document

21. Completion Criteria

Feature는 다음 조건을 만족해야 완료로 간주한다.

Requirement
    +
Implementation
    +
Build Success
    +
Tests Pass
    +
Regression Pass
    +
Documentation

Performance 관련 Feature:
+
Benchmark

22. Change Report

AI Agent는 작업 완료 후 다음 형식으로 보고한다.

## Summary

무엇을 구현했는가?

## Changed Files

어떤 파일을 변경했는가?

## Architecture Impact

Architecture에 어떤 영향이 있는가?

## Tests Added

어떤 Test를 추가했는가?

## Tests Executed

어떤 Test를 실제 실행했는가?

## Benchmark

성능 측정을 했는가?

## Risks

남은 Risk는 무엇인가?

## Follow-up

다음 작업은 무엇인가?

23. Debugging Workflow

Bug 발생 시 다음 순서를 따른다.

Bug
 |
 v
Reproduce
 |
 v
Collect Logs
 |
 v
Identify Root Cause
 |
 v
Add Regression Test
 |
 v
Fix
 |
 v
Run Regression
 |
 v
Run Full Relevant Tests

Timeout을 무작정 증가시키거나 Error를 무시해서는 안 된다.

24. Performance Workflow

성능 문제는 다음 순서로 해결한다.

Measure
 |
 v
Profile
 |
 v
Find Bottleneck
 |
 v
Optimize
 |
 v
Benchmark
 |
 v
Regression Test

성능 수치는 반드시 실제 측정 결과를 사용한다.

25. Architecture Change Workflow

Architecture 변경이 필요한 경우:

Problem
 |
 v
Current Architecture
 |
 v
Proposed Architecture
 |
 v
Trade-off
 |
 v
Risk
 |
 v
Migration Plan
 |
 v
Human Approval
 |
 v
Implementation

AI Agent가 Architecture를 독단적으로 변경하지 않는다.

26. Real Robot Deployment Workflow

실제 Robot에 적용하기 전에 다음 단계를 거친다.

Unit Test
    |
    v
Integration Test
    |
    v
Simulation
    |
    v
100~200 Robot Scale Test
    |
    v
Hardware-in-the-loop
    |
    v
Controlled Robot Test
    |
    v
Production

실제 Robot에서 처음 검증하지 않는다.

27. Final Development Principle

본 프로젝트의 개발 원칙은 다음과 같다.

작게 구현한다.
자주 테스트한다.
실패를 숨기지 않는다.
측정하고 최적화한다.
Architecture를 임의로 변경하지 않는다.
Simulation을 적극적으로 사용한다.
실제 Robot 적용 전에 충분히 검증한다.

최종 Workflow:

Requirement
     |
     v
Specification
     |
     v
Architecture
     |
     v
Implementation
     |
     v
Unit Test
     |
     v
Integration Test
     |
     v
Simulation
     |
     v
Benchmark
     |
     v
Code Review
     |
     v
Human Review
     |
     v
Merge

이 Workflow를 프로젝트 전체의 기본 개발 절차로 사용한다.
