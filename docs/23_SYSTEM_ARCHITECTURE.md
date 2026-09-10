Traffic Control Software

System Architecture Specification

1\. Purpose



본 문서는 Multi-Robot Traffic Control Software의 전체 System Architecture를 정의한다.



본 시스템은 좁고 긴 통로, 제한된 우회로, 높은 Robot 밀도, Human과 Robot의 혼재 환경을 대상으로 한다.



Primary Target:



100\~200 Robots

확장 목표 500+ Robots

Dynamic Environment

Frequent Temporary Stop

Limited Alternative Routes

Narrow Corridor

Shared Human/Robot Space



Architecture의 핵심 목표는 다음과 같다.



Scalable

Deterministic

Reliable

Observable

Testable

Planner Replaceable

Vendor Independent

2\. Architecture Principles

2.1 Separation of Concerns



각 Component는 하나의 명확한 책임을 가진다.



State Management

Traffic Decision

Planning

Reservation

Robot Command

Monitoring





을 가능한 한 분리한다.



2.2 Planner Independence



Traffic Controller는 특정 MAPF Algorithm에 직접 의존하지 않는다.



TrafficController

&#x20;       |

&#x20;       v

IRoutePlanner

&#x20;       |

&#x20;  ┌────┼───────────┐

&#x20;  v    v           v

&#x20;A\*   PIBT      WHCA\*/ECBS





Planner는 교체 가능해야 한다.



2.3 Safety Separation



Traffic Control과 Safety Control을 분리한다.



Traffic Control:



Route

Priority

Reservation

WAIT

Replanning



Safety Control:



Emergency Stop

Protective Stop

Safety Zone

Safety Interlock



Traffic Controller는 Safety Controller를 대체하지 않는다.



2.4 State Consistency



Traffic Decision은 최신 State를 기반으로 해야 한다.



모든 중요한 State에는 Version 또는 Timestamp를 유지한다.



State Version 100

&#x20;      |

Planning

&#x20;      |

Current State Version 105

&#x20;      |

&#x20;      X

Stale Result



3\. High Level Architecture

&#x20;                        ┌─────────────────────┐

&#x20;                        │     Task System     │

&#x20;                        └──────────┬──────────┘

&#x20;                                   │

&#x20;                                   ▼

&#x20;                        ┌─────────────────────┐

&#x20;                        │  Traffic Controller │

&#x20;                        └──────────┬──────────┘

&#x20;                                   │

&#x20;             ┌─────────────────────┼─────────────────────┐

&#x20;             │                     │                     │

&#x20;             ▼                     ▼                     ▼

&#x20;     ┌──────────────┐      ┌──────────────┐      ┌──────────────┐

&#x20;     │ State Manager│      │   Conflict   │      │   Priority   │

&#x20;     │              │      │   Detector   │      │   Manager    │

&#x20;     └──────────────┘      └──────────────┘      └──────────────┘

&#x20;                                   │

&#x20;                                   ▼

&#x20;                          ┌─────────────────┐

&#x20;                          │     Planner     │

&#x20;                          │  IRoutePlanner  │

&#x20;                          └────────┬────────┘

&#x20;                                   │

&#x20;                 ┌─────────────────┼─────────────────┐

&#x20;                 ▼                 ▼                 ▼

&#x20;               A\*                PIBT             WHCA\*

&#x20;                                  

&#x20;                                   │

&#x20;                                   ▼

&#x20;                          ┌─────────────────┐

&#x20;                          │   Reservation   │

&#x20;                          │     Manager     │

&#x20;                          └────────┬────────┘

&#x20;                                   │

&#x20;                                   ▼

&#x20;                          ┌─────────────────┐

&#x20;                          │ Traffic Decision│

&#x20;                          └────────┬────────┘

&#x20;                                   │

&#x20;                                   ▼

&#x20;                          ┌─────────────────┐

&#x20;                          │ Robot Adapter   │

&#x20;                          └────────┬────────┘

&#x20;                                   │

&#x20;                                   ▼

&#x20;                             Robot Fleet



4\. Major Components

4.1 TrafficController



System의 중앙 Orchestrator다.



책임:



Event 수신

State Update

Task Update

Conflict Detection 호출

Priority 계산

Planner 호출

Reservation Coordination

Traffic Decision 생성



TrafficController가 직접 수행하지 않는 것:



Path Search 내부 구현

Vendor SDK 호출

Database Access

Safety Stop

5\. StateManager



Robot의 최신 상태를 관리한다.



주요 기능:



Robot State Update

Robot State Query

State Version 관리

Timestamp 관리

Stale State Detection



예:



class IRobotStateManager {

public:

&#x20;   virtual RobotState getRobotState(

&#x20;       RobotId robotId) const = 0;



&#x20;   virtual void updateRobotState(

&#x20;       const RobotState\& state) = 0;

};



6\. MapManager



Traffic Map을 관리한다.



관리 대상:



Node

Edge

Corridor

Intersection

Resource

Speed Limit

Direction

Capacity



Map은 Version을 가진다.



map\_version = 102





Planning Result에는 가능하면 Map Version을 포함한다.



7\. TaskManager



Robot의 작업을 관리한다.



책임:



Task 생성

Task 할당

Task 시작

Task 완료

Task 취소

Task 실패



Traffic Controller는 Task의 이동 요구를 Planning Request로 변환한다.



8\. ConflictDetector



Robot 간 충돌 가능성을 탐지한다.



Conflict Type:



Node Conflict

Edge Conflict

Head-on Conflict

Crossing Conflict

Corridor Conflict

Resource Conflict

Temporal Conflict



Input:



Robot State

Route

Reservation

Map





Output:



Conflict List



9\. PriorityManager



Conflict가 발생했을 때 Robot의 진행 우선순위를 결정한다.



Input:



Task Priority

Waiting Time

Deadline

Robot State

Corridor Position

Direction

Blocking Impact



Priority는 Starvation을 방지할 수 있어야 한다.



10\. Planner



Planner는 Route 또는 Multi-Robot Planning을 수행한다.



Interface:



class IRoutePlanner {

public:

&#x20;   virtual PlanningResult plan(

&#x20;       const PlanningRequest\& request) = 0;



&#x20;   virtual \~IRoutePlanner() = default;

};





Planner 구현 예:



AStarPlanner

PIBTPlanner

WHCAStarPlanner

ECBSPlanner



11\. ReservationManager



Traffic Resource의 예약을 관리한다.



기능:



Check

Reserve

Release

Expire

Query

Conflict Detection



Reservation Resource:



Node

Edge

Corridor

Intersection

12\. DeadlockManager



Deadlock을 탐지하고 Recovery를 수행한다.



Detection:



Wait-for Graph





예:



R01 -> R02

R02 -> R03

R03 -> R01





Recovery:



Replanning

Priority Change

Backtracking

Resource Release

Holding Area

Human Intervention

13\. RobotAdapter



Traffic Core와 실제 Robot 시스템을 분리한다.



Traffic Core

&#x20;    |

IRobotAdapter

&#x20;    |

Vendor Adapter

&#x20;    |

Robot





Traffic Core는 특정 Robot Vendor API를 직접 호출하지 않는다.



14\. Event Architecture



주요 Event:



RobotStateUpdated

RobotStopped

RobotBlocked

RobotRecovered

TaskCreated

TaskCompleted

TaskCancelled

ReservationCreated

ReservationExpired

ConflictDetected

DeadlockDetected

MapUpdated





Event에는 가능하면 다음 정보를 포함한다.



event\_id

event\_type

timestamp

source

robot\_id

state\_version

map\_version



15\. Decision Flow



기본 Decision Loop:



Event

&#x20; |

&#x20; v

State Update

&#x20; |

&#x20; v

Affected Robot Detection

&#x20; |

&#x20; v

Conflict Detection

&#x20; |

&#x20; v

Priority Calculation

&#x20; |

&#x20; v

Planning

&#x20; |

&#x20; v

Validation

&#x20; |

&#x20; v

Reservation

&#x20; |

&#x20; v

Decision

&#x20; |

&#x20; v

Robot Command



16\. Planning Validation



Planning Result는 즉시 Commit하지 않는다.



Planning Result

&#x20;     |

&#x20;     v

Map Version Check

&#x20;     |

&#x20;     v

State Version Check

&#x20;     |

&#x20;     v

Reservation Check

&#x20;     |

&#x20;     v

Safety Validation

&#x20;     |

&#x20;     v

Commit



17\. Local vs Global Planning



기본적으로 Local Replanning을 우선한다.



Event

&#x20;|

&#x20;v

Affected Robots

&#x20;|

&#x20;v

Local Replanning





Global Replanning은 다음과 같은 경우 사용한다.



Large Scale Conflict

Deadlock

Map Change

Major Traffic Change

Local Planning Failure

18\. Narrow Corridor Architecture



좁은 Corridor는 일반 Edge와 동일하게 취급하지 않을 수 있다.



Node A

&#x20;  |

&#x20;  |  Corridor C01

&#x20;  |

Node B





Corridor Resource를 별도로 정의하여 다음을 관리할 수 있어야 한다.



Direction

Occupancy

Entry Permission

Exit Permission

Reservation

Capacity

19\. Human Interaction



Human은 Dynamic Obstacle로 처리한다.



상태:



FREE

TEMPORARILY\_BLOCKED

LONG\_BLOCKED

UNKNOWN





Human Blockage가 발생하면 영향을 받는 Robot만 우선 Replanning한다.



20\. Temporary Stop



Robot의 일시 정지는 Failure와 구분한다.



MOVING

&#x20;  |

&#x20;  v

TEMPORARILY\_STOPPED

&#x20;  |

&#x20;  v

MOVING





불필요한 Global Replanning을 방지한다.



21\. Concurrency Model



기본적으로 다음 구조를 권장한다.



Event Queue

&#x20;    |

&#x20;    v

Traffic Decision Loop

&#x20;    |

&#x20;    +---- State Snapshot

&#x20;    |

&#x20;    +---- Planner Workers

&#x20;    |

&#x20;    +---- Reservation





Core State의 소유권을 명확하게 유지한다.



22\. Data Ownership



각 Data의 Owner를 명확하게 한다.



RobotState      -> StateManager

Map             -> MapManager

Task            -> TaskManager

Reservation     -> ReservationManager

TrafficDecision -> TrafficController





여러 Component가 동일 State를 임의로 변경하지 않는다.



23\. Failure Handling



대표적인 Failure:



Planner Timeout

No Route

Robot State Timeout

Reservation Conflict

Communication Failure

Robot Failure

Deadlock



각 Failure에는 명확한 Recovery Policy가 필요하다.



24\. Observability



Production 환경에서는 다음 Metric을 수집한다.



Decision Latency

Planner Latency

Conflict Count

Deadlock Count

Replanning Count

Average Wait Time

Robot State Age

Reservation Count

Command Failure Count



Log와 Metric을 연결할 수 있도록 Correlation ID를 사용한다.



25\. Technology Architecture



Production Core:



C++20

CMake

GoogleTest

spdlog





Research / Simulation:



Python





Communication:



gRPC

Protocol Buffers

ROS 2 / DDS





필요한 경우에만 추가한다.



26\. Directory Architecture

traffic-control/

├── include/

│   ├── domain/

│   ├── map/

│   ├── state/

│   ├── task/

│   ├── traffic/

│   ├── planning/

│   ├── reservation/

│   ├── deadlock/

│   └── adapter/

│

├── src/

│   ├── domain/

│   ├── map/

│   ├── state/

│   ├── task/

│   ├── traffic/

│   ├── planning/

│   ├── reservation/

│   ├── deadlock/

│   └── adapter/

│

├── tests/

├── simulation/

├── benchmarks/

├── configs/

└── docs/



27\. Architecture Decision Rules



Architecture를 변경할 때 다음을 검토한다.



Responsibility

Coupling

Testability

Scalability

Failure Isolation

Performance

Maintainability



Architecture 변경은 인간의 승인을 필요로 한다.



28\. Target Scale



Primary:



100\~200 Robots





Secondary:



500+ Robots





Architecture는 100\~200 Robot에서 안정적으로 동작하면서 500 Robot까지 확장 가능한 구조를 목표로 한다.



29\. Final Architecture Principle



전체 시스템은 다음 구조를 유지한다.



Input

&#x20; |

State

&#x20; |

Traffic Intelligence

&#x20; |

Planning

&#x20; |

Reservation

&#x20; |

Decision

&#x20; |

Robot





핵심 원칙:



Traffic Controller != Planner

Planner != Robot Adapter

Traffic Control != Safety Control

Domain != Infrastructure





이 원칙을 전체 프로젝트의 기본 Architecture로 사용한다.

