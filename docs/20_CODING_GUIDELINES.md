Traffic Control Software

C++ Coding Guidelines

1. 목적

본 문서는 Traffic Control Software의 C++ 구현 규칙을 정의한다.

목표:

안전한 코드

예측 가능한 동작

deterministic behavior

testable architecture

AI Agent가 일관된 코드를 생성할 수 있는 구조

장기 유지보수 가능성

성능과 가독성의 균형

2. Language

기본:

C++20

컴파일러가 지원하는 범위 내에서 C++20 standard library를 사용한다.

3. General Principle

다음 우선순위를 따른다.

Correctness
    >

Safety
    >

Maintainability
    >

Determinism
    >

Performance
    >

Code Brevity

짧은 코드보다 명확한 코드를 우선한다.

4. Naming

Class

PascalCase:

class ReservationManager;

class TrafficController;

class DeadlockDetector;

Function

camelCase:

reserveResource();

detectDeadlock();

calculatePriority();

Variable

snake_case:

robot_id

current_route

reservation_table

Constants

k + PascalCase:

constexpr int kMaxRobots = 200;

constexpr auto kDefaultTimeout = 100ms;

5. File Naming

파일 이름은 snake_case를 사용한다.

robot.h

robot.cpp

reservation_manager.h

reservation_manager.cpp

deadlock_detector.h

deadlock_detector.cpp

6. Header Structure

권장:

#pragma once

#include <string>

#include <vector>

namespace traffic {

class Robot {

};

}

7. Namespace

전체 프로젝트 namespace:

namespace traffic {

}

하위 domain:

namespace traffic::planning {

}

namespace traffic::reservation {

}

namespace traffic::deadlock {

}

8. Include Rule

필요한 header만 include한다.

Bad:

#include "traffic/all.h"

Good:

#include "reservation/reservation.h"

9. Include Order

권장 순서:

1. Corresponding header

2. C++ standard library

3. Third-party library

4. Project headers

10. Ownership

Raw pointer를 ownership 표현에 사용하지 않는다.

가능하면:

std::unique_ptr

std::shared_ptr

std::weak_ptr

중 의미에 맞는 것을 사용한다.

기본 선택은:

unique_ptr

이다.

11. References

Ownership이 없는 객체는 reference 또는 pointer를 사용한다.

필수 객체:

ReservationManager& manager;

optional:

ReservationManager* manager;

12. Shared Pointer

shared_ptr는 기본 선택이 아니다.

다음 경우에만 사용한다.

Shared lifetime

Shared ownership

Async callback lifetime

단순히 편하다는 이유로 사용하지 않는다.

13. const Correctness

읽기 전용 값에는 const를 사용한다.

Route plan(
    const PlanningRequest& request);

14. Pass-by-value

작은 immutable object:

RobotId robot_id

큰 object:

const Route& route

를 사용한다.

15. Strong Types

서로 다른 ID를 단순 string으로 취급하지 않는다.

Bad:

std::string robot_id;

std::string task_id;

가능하면:

struct RobotId {
    std::string value;

};

struct TaskId {
    std::string value;

};

처럼 strong type을 사용한다.

16. Time

시간은 반드시 명시적인 type을 사용한다.

권장:

std::chrono::steady_clock

std::chrono::system_clock

std::chrono::milliseconds

std::chrono::seconds

가능하면 raw integer timestamp를 core logic에 사용하지 않는다.

17. Clock Abstraction

Production code에서 직접:

std::chrono::steady_clock::now();

를 남발하지 않는다.

다음 abstraction을 사용한다.

class IClock {

public:
    virtual TimePoint now() const = 0;

};

이를 통해 simulation/replay가 가능하도록 한다.

18. Error Handling

예외를 무분별하게 사용하지 않는다.

Domain failure는 명시적인 result type을 우선한다.

예:

std::expected<Route, PlanningError>

또는 project-defined result type을 사용한다.

19. Assertions

assert는 programming invariant 검증에 사용한다.

외부 입력 validation을 assert로 처리하지 않는다.

Bad:

assert(robot_id != "");

Good:

if (robot_id.empty()) {
    return Error::InvalidRobotId;

}

20. Logging

Domain decision은 logging한다.

예:

Robot R01 granted corridor C01

Robot R02 waiting for C01

Deadlock DL001 detected

Replanning R03 reason=HUMAN_BLOCKAGE

21. Logging Rule

Log에 반드시 가능하면 포함:

timestamp

robot_id

task_id

resource_id

event_id

decision

reason

22. No Logging in Tight Loop

고빈도 loop에서 INFO log를 남발하지 않는다.

Bad:

for (...) {
    LOG_INFO("checking robot");

}

필요하면:

DEBUG

TRACE

Metrics

를 사용한다.

23. Determinism

Algorithm이 randomization을 사용하는 경우 seed를 외부에서 주입한다.

Bad:

std::random_device rd;

Good:

Planner planner(seed);

24. Global State

Global mutable state를 금지한다.

Bad:

static ReservationTable g_table;

Good:

ReservationManager manager;

25. Singleton

Singleton을 기본적으로 사용하지 않는다.

Dependency Injection을 사용한다.

26. Thread Safety

공유 mutable state는 최소화한다.

Traffic state의 mutation은 가능한 한 하나의 logical owner가 담당한다.

Event
 ↓

Decision Thread
 ↓

State Mutation

27. Mutex

Mutex는 필요한 경우에만 사용한다.

Traffic decision loop에서 큰 critical section을 만들지 않는다.

28. Atomic

Atomic은 실제 concurrent state가 필요한 경우에만 사용한다.

단순히 thread-safe하게 보이기 위한 용도로 사용하지 않는다.

29. Deadlock Avoidance

여러 mutex를 사용해야 하는 경우 lock ordering을 명확히 정의한다.

예:

Map Lock
    ↓

Reservation Lock
    ↓

Robot Lock

반대 순서로 lock을 획득하지 않는다.

30. Planner Interface

Planner는 Traffic Controller와 직접 결합하지 않는다.

class IRoutePlanner {

public:
    virtual PlanningResult plan(
        const PlanningRequest& request) = 0;
    virtual ~IRoutePlanner() = default;

};

31. Reservation Interface

class IReservationManager {

public:
    virtual ReservationResult reserve(
        const ReservationRequest& request) = 0;
    virtual void release(
        const ReservationId& id) = 0;
    virtual ~IReservationManager() = default;

};

32. Deadlock Interface

class IDeadlockManager {

public:
    virtual DeadlockStatus update(
        const TrafficSnapshot& snapshot) = 0;
    virtual RecoveryPlan recover(
        const DeadlockId& id) = 0;
    virtual ~IDeadlockManager() = default;

};

33. Robot Adapter

Traffic Core는 Robot vendor API를 직접 호출하지 않는다.

Traffic Controller
        |

IRobotAdapter
        |
+-------+--------+
|                |

Simulator      Vendor SDK

34. Algorithm Isolation

알고리즘 implementation은 독립적으로 교체 가능해야 한다.

IRoutePlanner
    |
    +-- AStarPlanner
    +-- PIBTPlanner
    +-- WHCAStarPlanner
    +-- ECBSPlanner

TrafficController는 구현체 이름을 알지 못해야 한다.

35. Configuration

Algorithm parameter를 source code에 하드코딩하지 않는다.

Bad:

constexpr int kWindowSize = 10;

Good:

planning:
  window_size: 10

단, compile-time invariant는 constexpr를 사용한다.

36. Magic Numbers

금지:

if (wait_time > 30) {

}

허용:

if (wait_time > config.max_wait_time) {

}

37. State Machine

Robot state 변경은 명시적인 transition을 거친다.

MOVING
  ↓

BLOCKED
  ↓

REPLANNING
  ↓

MOVING

임의의 state 변경을 허용하지 않는다.

38. Event Handling

Event handler는 가능한 한 작게 유지한다.

Bad:

onRobotStateUpdated()
    ├─ planning
    ├─ reservation
    ├─ deadlock
    ├─ database
    └─ network

Good:

onRobotStateUpdated()
    ↓

Update State
    ↓

Emit Event
    ↓

Decision Pipeline

39. Immutability

Event와 PlanningRequest는 가능하면 immutable object로 취급한다.

40. Copy Policy

대용량 route/map data를 불필요하게 복사하지 않는다.

가능하면:

const Route&

std::span<const Node>

등을 사용한다.

단, lifetime 문제가 생기면 명확한 ownership을 선택한다.

41. Performance Optimization

측정 없이 최적화하지 않는다.

순서:

Profile
 ↓

Identify Bottleneck
 ↓

Optimize
 ↓

Benchmark
 ↓

Regression Test

42. Data Structure Selection

자료구조는 algorithm complexity와 실제 workload를 기준으로 선택한다.

예:

unordered_map

map
vector

deque

priority_queue

단순히 익숙하다는 이유로 선택하지 않는다.

43. Allocation

고빈도 traffic loop에서 반복적인 heap allocation을 최소화한다.

특히:

Event

Reservation

Planning

Robot State

경로를 profiling한다.

44. API Stability

Public interface를 임의로 변경하지 않는다.

변경이 필요하면:

Specification
→ Review
→ Migration
→ Implementation

순서로 진행한다.

45. Unit Test Naming

형식:
<component>_<condition>_<expected>

예:

reservation_conflict_is_rejected

deadlock_cycle_is_detected

planner_no_route_returns_error

46. Test Determinism

Random test라도 seed를 고정하고 실패 시 seed를 출력한다.

FAILED

seed=123456

47. Test Isolation

Unit test는 외부 DB나 실제 Robot network에 의존하지 않는다.

48. Integration Test

실제 module 간 interaction을 검증한다.

예:

Planner
+

Reservation
+

Priority

49. Simulation Test

Simulation은 실제 Traffic Controller와 동일한 decision path를 최대한 사용한다.

50. Safety Rule

다음은 절대 허용하지 않는다.

Collision

Invalid Reservation

Unsafe Command

Unknown Robot Command

Stale State 기반의 무조건적인 Move

51. Fail Safe

불확실한 상태에서는:

WAIT

또는 시스템에서 정의한 safe behavior를 선택한다.

예:

Unknown Robot State
        ↓

Do not grant new movement

52. Comments

코드가 무엇을 하는지보다 왜 그렇게 하는지를 설명한다.

Bad:
// increment i

i++;

Good:
// Keep the reservation horizon ahead of the robot
// to prevent starvation at corridor entry.

53. TODO

TODO에는 반드시 issue 또는 명확한 설명을 남긴다.

Bad:
// TODO

Good:
// TODO(#123): Replace heuristic with learned cost model

54. No Hidden Behavior

함수 이름과 실제 동작이 일치해야 한다.

Bad:

calculateRoute()

가 내부에서 reservation까지 변경하는 것.

Good:

calculateRoute()
    → route만 계산

reserveRoute()
    → reservation만 변경

55. Separation of Responsibility

각 module은 하나의 주요 책임을 가진다.

Planner
    → Route

ReservationManager
    → Reservation

PriorityManager
    → Priority

DeadlockManager
    → Deadlock

TrafficController
    → Decision orchestration

56. No Algorithm Leakage

ReservationManager가 PIBT 내부 구현을 알아서는 안 된다.

Planner가 DeadlockManager를 직접 호출해서도 안 된다.

Orchestration은 TrafficController가 담당한다.

57. Decision Trace

Traffic decision은 추적 가능해야 한다.

예:

Robot R01

Requested C01

Rejected

Reason:

C01 reserved by R02

Priority:

R02 = 72

R01 = 54

58. Reproducibility

Bug report에는 가능하면:

Scenario

Map Version

Software Version

Commit

Seed

Robot State

Task State

Event Log

를 포함한다.

59. Formatting

clang-format을 사용한다.

모든 PR은 formatting check를 통과해야 한다.

60. Compiler Warnings

가능한 강한 warning을 사용한다.

예:
-Wall
-Wextra
-Wpedantic

Warnings는 CI에서 error로 취급한다.

61. Sanitizer

CI에서 가능한 경우:

AddressSanitizer

UndefinedBehaviorSanitizer

ThreadSanitizer

를 사용한다.

62. Dependency Rule

Core는 infrastructure에 의존하지 않도록 한다.

Bad:

core
 ↓

database

Good:

core
 ↑

adapter/infrastructure

63. Database Rule

Traffic decision loop에서 직접 SQL을 실행하지 않는다.

Bad:

onRobotState()

{
    database.query(...);

}

Good:

Decision
 ↓

Event
 ↓

Async Persistence

64. Network Rule

Network latency가 Traffic Decision을 blocking하지 않도록 설계한다.

65. Async Rule

Async code에서는 lifetime과 cancellation을 명확히 정의한다.

특히 Planner Worker에서 오래된 planning result가 들어오는 경우:

Planning Request A

Planning Request B
        ↓

B completes first
        ↓

A completes later

A의 오래된 결과를 그대로 commit하지 않는다.

66. Versioning

Planning request/result에는 가능하면:

map_version

traffic_state_version

route_version

등을 포함한다.

이를 통해 stale result를 검출한다.

67. Route Commit

Planning result가 생성되었다고 바로 적용하지 않는다.

Planning Result
       ↓

Validation
       ↓

Version Check
       ↓

Reservation Check
       ↓

Commit

순서를 따른다.

68. Reservation Atomicity

Reservation 변경은 가능한 한 atomic operation으로 처리한다.

Check
+

Grant

사이에 다른 decision이 끼어들어서는 안 된다.

69. Deadlock Recovery

Deadlock recovery는 기존 route를 단순히 다시 planning하는 것과 구분한다.

Normal Replanning
    ≠

Deadlock Recovery

Recovery는 명시적인 policy를 가진다.

70. Final Rule

모든 개발자는 다음 질문에 답할 수 있어야 한다.

이 코드는 무엇을 하는가?

왜 필요한가?

어떤 state를 변경하는가?

thread-safe한가?

deterministic한가?

실패하면 어떻게 되는가?

어떻게 test하는가?

AI Agent가 생성한 코드도 동일한 기준을 적용한다.

71. AI Agent Coding Rule

AI Agent가 코드를 작성할 경우:

Read Specification
        ↓

Read Architecture
        ↓

Read Coding Guidelines
        ↓

Inspect Existing Code
        ↓

Implement Small Change
        ↓

Write Tests
        ↓

Run Tests
        ↓

Review Diff

순서를 반드시 따른다.

72. Final Principle

이 프로젝트의 코드는 "빠르게 동작하는 코드"보다:

Predictable

Testable

Deterministic

Observable

Recoverable

Maintainable

한 코드를 우선한다.
