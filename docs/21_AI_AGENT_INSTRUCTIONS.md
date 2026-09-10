Traffic Control Software
AI Agent Instructions
1. Purpose

본 문서는 Multi-Robot Traffic Control Software 프로젝트에서 AI Agent를 활용하여 소프트웨어를 개발할 때 반드시 따라야 하는 개발 지침을 정의한다.

AI Agent는 단순한 코드 생성기가 아니라 다음 역할을 수행한다.

Repository 분석
Architecture 분석
구현 계획 수립
코드 구현
Unit Test 작성
Integration Test 작성
Debugging
Performance 분석
Code Review 지원
문서화

최종적인 Architecture, Safety Policy 및 Production 적용 여부는 인간 개발자가 결정한다.

2. System Context

본 프로젝트는 다음 환경을 대상으로 한다.

Robot 규모
기본 운영 규모: 100~200대
확장 목표: 500대 이상
Environment
좁고 긴 통로가 많음
우회로가 제한적임
단방향 및 양방향 통로 존재
교차로 존재
Bottleneck 존재
사람과 Robot이 같은 공간에서 이동
Human에 의해 통로가 일시적으로 막힐 수 있음
Dynamic Behavior

Robot은 다음 상황에서 정지할 수 있다.

사람 통행
장애물
Navigation 실패
작업 대기
Traffic Conflict
Robot Failure
통신 문제

따라서 Traffic Control은 정적인 MAPF 문제만 해결하는 것이 아니라 Dynamic Environment를 처리할 수 있어야 한다.

3. Technology Policy

Production Core는 기본적으로 C++20을 사용한다.

Python은 다음 용도로 사용한다.

Algorithm Research
Simulation
Benchmark
Scenario Generation
Data Analysis
Visualization

기본 Build System:

CMake

기본 Test Framework:

GoogleTest

기본 Benchmark:

Google Benchmark

기본 Logging:

spdlog

필요한 경우 다음 기술을 사용한다.

Protocol Buffers
gRPC
ROS 2
DDS

Technology 선택은 Architecture Requirement를 우선한다.

4. Instruction Priority

AI Agent의 판단 우선순위는 다음과 같다.

Safety Requirement
System Specification
Architecture Decision
Coding Guidelines
Test Requirements
Existing Implementation
User Request
AI Agent Preference

상위 원칙과 충돌하는 요청이 있을 경우 AI Agent는 임의로 변경하지 않고 충돌 사항을 보고한다.

5. Before Coding

코드를 작성하기 전에 반드시 다음 작업을 수행한다.

Requirement 확인
관련 문서 확인
Repository 구조 확인
관련 코드 검색
Existing Interface 확인
Existing Test 확인
Dependency 확인
구현 범위 결정
Implementation Plan 작성

기존 코드를 확인하지 않고 새로운 Component를 생성하지 않는다.

6. Repository Inspection

작업 전에 다음을 확인한다.

Directory Structure
Build System
Existing Modules
Interfaces
Tests
Configuration
Logging
Error Handling
CI/CD
Simulation
Benchmark
External Dependencies

특히 동일하거나 유사한 기능이 이미 존재하는지 먼저 검색한다.

7. Reuse Existing Code

기존 기능이 있다면 우선 재사용한다.

예를 들어 기존에 ReservationManager가 존재한다면 별도의 ReservationManager2를 생성하지 않는다.

기존 Component의 확장으로 해결할 수 있는지 먼저 검토한다.

8. Smallest Change

요구사항을 만족하는 가장 작은 변경을 우선한다.

하나의 Feature를 구현하면서 관련 없는 Architecture까지 변경하지 않는다.

예:

작업:

Add reservation expiration

불필요한 변경:

Planner 전체 Rewrite
Logging System Rewrite
Database 변경
전체 Naming 변경
9. No Unrelated Refactoring

현재 작업과 직접 관련 없는 Refactoring은 별도의 작업으로 분리한다.

AI Agent가 더 좋은 구조라고 판단하더라도 기존 Architecture를 임의로 변경하지 않는다.

개선이 필요하다고 판단하면 다음 형식으로 제안한다.

Current Problem
Proposed Change
Expected Benefit
Risk
Migration Plan
10. Architecture First

새로운 Module이 필요한 경우 구현 전에 Architecture를 확인한다.

권장 구조:

TrafficController -> IRoutePlanner -> Planner

예:

TrafficController
IRoutePlanner
PIBTPlanner
WHCAStarPlanner
ECBSPlanner

TrafficController가 특정 Algorithm에 직접 의존하지 않도록 한다.

11. Interface First

Module 간 결합이 필요한 경우 Interface를 먼저 정의한다.

예:

class IRoutePlanner {
public:
    virtual PlanningResult plan(
        const PlanningRequest& request) = 0;
    virtual ~IRoutePlanner() = default;
};

Interface는 다음을 고려하여 설계한다.

Testability
Replaceability
Dependency Injection
Async Execution
Error Handling
Future Planner Replacement
12. Core Domain

다음 영역은 Traffic Control의 Core Domain으로 취급한다.

Robot
RobotState
Map
Graph
Node
Edge
Task
Route
Reservation
Conflict
Priority
Deadlock
TrafficEvent
TrafficDecision

Core Domain은 Database나 Vendor SDK에 직접 의존하지 않는다.

13. Traffic Controller

TrafficController의 책임은 Orchestration이다.

기본 흐름:

Event -> State Update -> Conflict Detection -> Priority -> Planning -> Reservation -> Decision

TrafficController 내부에 특정 MAPF Algorithm의 세부 구현을 넣지 않는다.

14. Planner

Planner의 책임은 Route 또는 Multi-Robot Planning이다.

Planner가 직접 수행해서는 안 되는 작업:

Robot Command 전송
Database Write
Vendor SDK 호출
Safety Stop
직접적인 Reservation 변경

Planner는 필요한 경우 Reservation 정보를 조회할 수 있지만 Reservation의 최종 Commit은 ReservationManager가 담당한다.

15. Planner Replacement

Planner는 교체 가능해야 한다.

예:

IRoutePlanner

구현:

AStarPlanner
PIBTPlanner
WHCAStarPlanner
ECBSPlanner

특정 Planner를 TrafficController에 Hard Coding하지 않는다.

16. MAPF Policy

MAPF Algorithm을 추가하거나 변경할 때 다음 항목을 반드시 고려한다.

Robot Count
Graph Size
Corridor Width
Alternative Route Count
Planning Horizon
Dynamic Obstacle
Human Blockage
Robot Stop
Deadlock
Planning Latency
Memory Usage
Throughput

Algorithm의 이론적인 Optimality만으로 선택하지 않는다.

17. Narrow Corridor

본 시스템에서는 좁은 Corridor가 핵심적인 문제 영역이다.

반드시 다음 상황을 고려한다.

Single Lane
Bidirectional Traffic
Head-on Conflict
Passing Impossible
Limited Passing
Corridor Entry Conflict
Corridor Exit Conflict
Blocking Robot
Human Blockage

특히 좁은 Corridor에서는 단순한 Shortest Path보다 Traffic Coordination이 중요할 수 있다.

18. Reservation

ReservationManager는 Traffic Resource의 점유를 관리한다.

주요 기능:

Check
Reserve
Release
Expire
Query
Conflict Detection

Reservation 대상은 필요에 따라 다음을 포함할 수 있다.

Node
Edge
Corridor
Intersection
Charging Area
Loading Area
19. Robot State

Robot State는 시스템의 중요한 Source of Truth이다.

권장 상태:

MOVING
IDLE
WAITING
TEMPORARILY_STOPPED
BLOCKED
FAILED
UNKNOWN

State에는 가능하면 다음 정보를 포함한다.

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
20. Temporary Stop

Robot의 일시적인 정지와 Failure를 구분한다.

TEMPORARILY_STOPPED != FAILED

예:

사람이 지나감
일시적인 장애물
Navigation 대기
Traffic 대기

이러한 상태를 Robot Failure로 처리하면 불필요한 Global Replanning이 발생할 수 있다.

21. Human Blockage

Human Blockage는 Dynamic Obstacle로 처리한다.

최소한 다음 상태를 구분할 수 있도록 한다.

FREE
TEMPORARILY_BLOCKED
LONG_BLOCKED
UNKNOWN

가능하다면 예상 Blockage Duration을 Planning에 반영할 수 있도록 설계한다.

22. Stale State

오래된 Robot State를 기반으로 Movement Permission을 승인하지 않는다.

예:

Planning State Version = 100

Current State Version = 105

이 경우 기존 Planning Result가 아직 유효한지 다시 확인한다.

23. Planning Result Validation

Planning Result를 생성했다고 바로 Commit하지 않는다.

다음 단계를 수행한다.

Planning Result -> Validate -> Version Check -> Reservation Check -> Commit

다음 정보가 있으면 좋다.

map_version
traffic_state_version
route_version
planning_request_id
24. Async Planning

비동기 Planner에서는 오래된 결과가 늦게 도착할 수 있다.

예:

Request A 실행

Request B 실행

B 완료

A 완료

이 경우 A의 결과가 현재 State에 유효하지 않다면 폐기한다.

25. Planning Timeout

Planner는 무한정 실행되어서는 안 된다.

Planning Timeout을 지원한다.

예:

PLANNING_TIMEOUT

Timeout 발생 시 명확한 정책을 정의한다.

예:

기존 Route 유지
WAIT
Local Replanning
다른 Planner 사용
Recovery Mode
26. Deadlock

Deadlock은 Normal Conflict와 구분한다.

예:

R01 -> R02 -> R03 -> R01

Wait-for Graph에 Cycle이 존재하면 Deadlock 가능성을 판단한다.

Deadlock 유형:

RESOURCE_DEADLOCK
CORRIDOR_DEADLOCK
HEAD_ON_DEADLOCK
CYCLE_DEADLOCK
27. Deadlock Recovery

Deadlock Recovery는 별도의 Module로 관리한다.

가능한 Recovery:

Replanning
Priority 변경
Backtracking
Resource Release
Holding Area 이동
Human Intervention

Recovery가 새로운 Deadlock을 만들지 않는지 검증한다.

28. Safety Boundary

Traffic Controller와 Safety Controller를 분리한다.

Traffic Controller:

Route
Priority
Reservation
WAIT
Traffic Decision
Replanning

Safety Controller:

Emergency Stop
Protective Stop
Safety Zone
Safety Interlock

AI Agent는 Traffic Control 개발 중 Safety Controller의 동작을 임의로 변경하지 않는다.

29. Fail Safe

불확실한 상태에서는 새로운 Movement Permission을 적극적으로 생성하지 않는다.

예:

UNKNOWN ROBOT STATE -> NO NEW MOVEMENT PERMISSION

Safety 관련 동작은 명시된 Safety Policy를 따른다.

30. Determinism

Random Algorithm을 사용할 경우 Seed를 외부에서 주입한다.

예:

Planner planner(seed);

가능하면 동일한 Input과 Seed에서 동일한 결과가 나오도록 한다.

31. Testing

모든 Feature 변경에는 관련 Test를 추가한다.

최소 다음을 고려한다.

Normal Case
Boundary Case
Failure Case
Regression Case
32. Planner Test

Planner는 최소 다음 Scenario를 테스트한다.

Single Robot
Two Robots
Multi Robot
Conflict
Head-on Conflict
Narrow Corridor
No Alternative Route
Temporary Blockage
Long Blockage
No Route
Timeout
33. Reservation Test

ReservationManager는 최소 다음을 테스트한다.

Reserve Success
Reservation Conflict
Release
Expiration
Overlap
Concurrent Request
Invalid Reservation
34. Deadlock Test

DeadlockManager는 최소 다음을 테스트한다.

No Deadlock
Two Robot Cycle
Three Robot Cycle
Corridor Deadlock
Resource Deadlock
False Positive
Recovery Success
Recovery Failure
35. Scale Test

다음 Robot 수를 기준으로 Scale Test를 수행한다.

10
50
100
150
200
500

100~200대가 핵심 Production Target이다.

500대는 Architecture의 확장성을 확인하기 위한 Test Target이다.

36. Performance

다음 Metric을 측정한다.

Planner Latency
Conflict Detection Latency
Reservation Latency
Decision Loop Latency
CPU
Memory
Throughput
Replanning Count
Deadlock Count
Average Wait Time

성능 최적화 전에 반드시 측정한다.

37. Performance Optimization

다음 순서를 따른다.

Measure -> Profile -> Identify Bottleneck -> Optimize -> Benchmark -> Regression Test

측정하지 않은 상태에서 성능이 개선되었다고 주장하지 않는다.

38. Concurrency

먼저 Sequential Correctness를 확보한다.

그 다음 병목을 측정한다.

그 후 필요한 부분에만 Parallelism을 적용한다.

권장 방식:

Single Owner
Message Passing
Immutable Snapshot
Lock 최소화
39. Database

Traffic Decision Loop에서 Database를 직접 호출하지 않는다.

권장 구조:

Robot Event -> In-memory State -> Planning

Persistence는 가능한 경우 비동기 처리한다.

40. External Robot SDK

Vendor SDK는 Adapter를 통해 격리한다.

예:

Traffic Core -> IRobotAdapter -> Vendor Adapter -> Robot

Traffic Core가 특정 Robot Vendor API에 직접 의존하지 않는다.

41. Configuration

Algorithm Parameter를 Source Code에 Hard Coding하지 않는다.

예:

traffic:
  max_robots: 200

planning:
  timeout_ms: 50
  horizon: 20

reservation:
  safety_margin_ms: 500

42. Logging

중요한 Traffic Decision은 재현 가능해야 한다.

예:

Robot: R01
Resource: C01
Decision: WAIT
Reason: RESERVATION_CONFLICT
Blocking Robot: R02
Priority: 54

43. Event Trace

Traffic Decision을 추적할 수 있도록 Event Trace를 유지한다.

예:

E001 R01 ENTERED C01
E002 R02 REQUESTED C01
E003 RESERVATION CONFLICT
E004 R02 WAIT
E005 R01 BLOCKED
E006 REPLANNING

44. Git Safety

AI Agent는 작업 시작 전에 Git 상태를 확인한다.

사용자 변경사항을 임의로 삭제하지 않는다.

다음 명령은 승인 없이 사용하지 않는다.

git reset --hard
git clean -fd
git checkout -- .
45. Dependency Change

Dependency 추가 또는 Version 변경 시 반드시 보고한다.

보고 내용:

Dependency
Version
Reason
Security Impact
Build Impact
46. Public API Change

Public API 변경 시 다음을 보고한다.

Before
After
Reason
Compatibility Impact
Migration Plan

Breaking Change는 승인 없이 수행하지 않는다.

47. Algorithm Documentation

Algorithm 구현에는 다음 내용을 기록한다.

Algorithm Name
Purpose
Input
Output
Complexity
Limitations
Timeout Behavior
Failure Behavior
Expected Scale
48. Benchmark Before Replacement

Planner를 교체할 경우 최소 다음을 비교한다.

Success Rate
Planning Latency
Average Wait Time
Deadlock Rate
Throughput
CPU
Memory

측정 결과 없이 Planner 교체를 결정하지 않는다.

49. Scenario Driven Development

다음 Scenario를 기반으로 개발한다.

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
50. Regression

Bug가 발견되면 가능하면 해당 Bug를 재현하는 Regression Test를 추가한다.

Bug -> Reproduce -> Add Test -> Fix -> Keep Test

51. Debugging

Bug 수정 순서:

Reproduce
Identify Root Cause
Add or Improve Test
Fix
Run Regression
Run Relevant Full Tests

단순히 Timeout을 증가시키거나 Error를 무시하는 방식으로 해결하지 않는다.

52. Never Fake Results

AI Agent는 다음 행동을 절대 하지 않는다.

실행하지 않은 Test를 Pass라고 보고
실행하지 않은 Benchmark 결과 작성
Build하지 않고 Compile 성공이라고 보고
성능 수치 임의 생성
존재하지 않는 Algorithm 결과 생성
확인하지 않은 Runtime Behavior를 사실처럼 보고
53. Code Review
Code Review 시 다음을 확인한다.

Correctness
Race Condition
Deadlock
Memory
Exception
Latency
Determinism
Logging
Test Coverage
Architecture
API Compatibility
Failure Handling
54. AI Agent Autonomy
AI Agent가 자유롭게 결정할 수 있는 범위:

Local Implementation
Private Helper
Variable Name
Test Data
Local Optimization
Internal Refactoring
단, Architecture와 Safety Policy를 변경하지 않는다.

55. Human Approval Required
다음 사항은 인간의 승인이 필요하다.

Architecture Change
Core Interface Change
Safety Policy Change
Planner Replacement
Reservation Semantics Change
Deadlock Policy Change
Breaking API Change
Major Dependency Change
Database Schema Change
Production Deployment Change
56. AI Agent Self Check
작업 완료 전에 다음을 확인한다.

Requirement을 만족하는가?
Architecture를 위반하지 않는가?
기존 코드를 재사용했는가?
불필요한 변경을 하지 않았는가?
Test를 추가했는가?
Test를 실제 실행했는가?
Race Condition 가능성이 없는가?
Deadlock 가능성이 없는가?
Stale State를 고려했는가?
Failure Behavior가 정의되어 있는가?
Logging이 충분한가?
Deterministic한가?
Performance를 측정했는가?
Safety Boundary를 침범하지 않았는가?
57. Definition of Done
작업은 다음 조건을 만족해야 완료로 간주한다.

Requirement Implemented
Code Compiles
Relevant Tests Pass
Regression Tests Pass
Architecture Preserved
No Unrelated Changes
Documentation Updated
성능 관련 작업은 Benchmark까지 완료해야 한다.

58. Final Principle
AI Agent는 다음 순서를 항상 따른다.

READ -> UNDERSTAND -> PLAN -> IMPLEMENT -> TEST -> MEASURE -> REPORT

다음 행동은 금지한다.

GUESS
HIDE FAILURE
BREAK ARCHITECTURE
CHANGE SAFETY SEMANTICS
CLAIM UNTESTED RESULTS
OPTIMIZE WITHOUT MEASUREMENT
REWRITE WITHOUT APPROVAL
최종 목표는 코드 생성이 아니라 다음 시스템을 구축하는 것이다.

Reliable + Deterministic + Scalable + Observable + Maintainable Traffic Control
