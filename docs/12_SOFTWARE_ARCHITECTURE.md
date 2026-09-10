Traffic Control Software

Software Architecture Specification

1. 목적

실제 구현을 위한 package/module 구조와 dependency rule을 정의한다.

2. Recommended Technology

초기 prototype:

Language:

C++20

Build:

CMake

Testing:

GoogleTest

Logging:

spdlog

Serialization:

Protocol Buffers / JSON

Metrics:

Prometheus compatible

Simulation:

Custom discrete-event simulator

Python은:

simulation analysis

algorithm prototyping
visualization

benchmark

용도로 사용할 수 있다.

3. Repository Structure

traffic-control/
│
├── docs/
│
├── src/
│   ├── core/
│   ├── map/
│   ├── robot/
│   ├── task/
│   ├── planning/
│   ├── reservation/
│   ├── priority/
│   ├── deadlock/
│   ├── replanning/
│   ├── controller/
│   ├── adapter/
│   └── infrastructure/
│
├── tests/
│   ├── unit/
│   ├── integration/
   ├── simulation/
│   └── stress/
│
├── simulator/
│
├── configs/
│
├── tools/
│
└── CMakeLists.txt

4. Core

core/
├── types/
├── time/
├── id/
├── state/
└── events/

공통 primitive만 포함한다.

Core는 상위 module에 dependency를 가지지 않는다.

5. Map Module

map/
├── Map
├── Node
├── Edge
├── Resource
├── Corridor
├── WaitingBay
└── MapLoader

6. Robot Module

robot/
├── Robot
├── RobotState
├── RobotManager
├── RobotCapability
└── RobotStatus

7. Task Module

task/
├── Task
├── TaskManager
├── TaskState
└── TaskPriority

8. Planning Module

planning/
├── GlobalPlanner
├── LocalReplanner
├── AStar
├── WHCAStar
├── PIBT
├── ECBS
└── CostModel

알고리즘은 interface 뒤에 숨긴다.

class IRoutePlanner {

public:
    virtual Route plan(
        const PlanningRequest& request) = 0;
    virtual ~IRoutePlanner() = default;

};

9. Reservation Module

reservation/
├── Reservation
├── ReservationTable
├── ReservationManager
├── ConflictDetector
└── ResourceLock

10. Priority Module

priority/
├── PriorityManager
├── PriorityPolicy
├── AgingPolicy
└── FairnessMonitor

11. Deadlock Module

deadlock/
├── WaitForGraph
├── CycleDetector
├── DeadlockManager
├── VictimSelector
└── RecoveryStrategy

12. Replanning Module

replanning/
├── ReplanningEngine
├── ReplanningRequest
├── ReplanningQueue
├── AffectedRobotDetector
└── RouteCommitter

13. Controller

controller/
├── TrafficController
├── RobotStateHandler
├── ReservationHandler
├── BlockageHandler
├── DeadlockHandler
└── CommandGenerator

14. Adapter

adapter/
├── RobotAdapter
├── RobotProtocol
├── SimulatorAdapter
└── ExternalSystemAdapter

실제 Robot과 simulator가 동일 interface를 사용하도록 한다.

15. Infrastructure

infrastructure/
├── EventBus
├── StateStore
├── Config
├── Logger
├── Metrics
└── Clock

16. Dependency Rule

권장 dependency:

core
 ↑

map / robot / task
 ↑

planning / reservation / priority
 ↑

replanning / deadlock
 ↑

controller
 ↑

adapter

하위 module이 상위 module을 참조하면 안 된다.

17. Dependency Injection

구현체를 직접 생성하지 않는다.

Bad:

TrafficController::TrafficController()

{
    planner = new AStar();

}

Good:

TrafficController::TrafficController(
    IRoutePlanner& planner,
    IReservationManager& reservation_manager,
    IPriorityManager& priority_manager)

18. Clock Abstraction

실제 시간과 simulation 시간을 분리한다.

class IClock {

public:
    virtual TimePoint now() const = 0;

};

이를 통해 동일 code를:

Simulation

Production

Replay

에서 사용할 수 있다.

19. Event Interface

struct Event {
    EventId id;
    TimePoint timestamp;
    EventType type;

};

모든 event는 immutable하게 취급한다.

20. State Machine

Robot state transition은 명시적으로 구현한다.

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

21. Threading Model

초기 구현은 과도한 multithreading을 피한다.

권장:

Event IO Thread
       |
       v

Decision Thread
       |
       +--> Planner Worker
       |
       +--> Metrics

Traffic state mutation은 하나의 logical owner에서 처리한다.

22. Planner Worker

Planning은 controller decision loop를 block하지 않도록 별도 worker에서 수행할 수 있다.

Controller
    |
    v

Planning Request
    |
    v

Planner Worker
    |
    v

Planning Result

23. Configuration

모든 주요 policy는 configuration으로 분리한다.

예:

fleet:
  max_robots: 200

planning:
  algorithm: pibt
  timeout_ms: 50

reservation:
  horizon_sec: 10

deadlock:
  confirmation_sec: 5

replanning:
  blockage_threshold_sec: 10

24. Determinism

동일 input에 대해 가능한 경우 동일 decision이 나와야 한다.

Randomization을 사용하는 알고리즘은:

seed

를 명시적으로 관리한다.

25. Testability

모든 module은 독립적으로 test 가능해야 한다.

예:

ReservationManager
→ mock Clock
→ mock Map
→ deterministic test

26. Definition of Done

각 module은 다음을 만족해야 한다.

Interface 정의

Unit test

Error handling

Logging

Metrics

Configuration

Documentation

Deterministic test
