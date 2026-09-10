Traffic Control Software

Implementation Plan

1\. 목적



Traffic Control Software를 실제 개발하기 위한 단계별 implementation plan을 정의한다.



AI Agent를 활용하여 개발하되, 각 단계에서 명확한 artifact와 acceptance criteria를 갖도록 한다.



2\. 개발 원칙



AI Agent에게 전체 프로젝트를 한 번에 구현하도록 지시하지 않는다.



반드시:



Specification

&#x20;   |

&#x20;   v

Design

&#x20;   |

&#x20;   v

Implementation

&#x20;   |

&#x20;   v

Unit Test

&#x20;   |

&#x20;   v

Integration

&#x20;   |

&#x20;   v

Simulation





순서로 진행한다.



3\. Phase 0 - Repository



목표:



Buildable repository





작업:



Create CMake

Create src

Create tests

Create simulator

Create CI





Acceptance:



cmake build PASS

unit test PASS



4\. Phase 1 - Core Model



구현:



Robot

Task

Node

Edge

Resource

Route

Reservation

Event





Acceptance:



All model unit tests PASS



5\. Phase 2 - Map



구현:



Map

MapLoader

Resource

Corridor

WaitingBay





Acceptance:



Load real-like map

Validate topology



6\. Phase 3 - Single Robot Planner



먼저 A\*를 구현한다.



A\*





목표:



Correctness

Determinism

Performance





Acceptance:



Known map

Known start

Known goal

Expected route



7\. Phase 4 - Reservation



구현:



ReservationManager

ReservationTable

ConflictDetector





Acceptance:



No overlapping incompatible reservations



8\. Phase 5 - Multi Robot Baseline



Prioritized Planning 구현.



목표:



Multi robot baseline





Acceptance:



50 robots

100 robots



9\. Phase 6 - WHCA\*



WHCA\* 구현.



비교:



Prioritized Planning

vs

WHCA\*



10\. Phase 7 - PIBT



PIBT 구현.



비교:



Prioritized

WHCA\*

PIBT



11\. Phase 8 - Benchmark Framework



자동 benchmark runner 구현.



Algorithm

Scenario

Seed

Robot Count





조합을 자동 실행한다.



12\. Phase 9 - Priority Manager



구현:



Task Priority

Waiting Aging

Fairness

Priority Override



13\. Phase 10 - Deadlock Manager



구현:



Wait-for Graph

Cycle Detection

Deadlock Confirmation

Victim Selection

Recovery



14\. Phase 11 - Dynamic Replanning



구현:



Affected Robot Detection

Local Replanning

Route Commit

Retry

Hysteresis



15\. Phase 12 - Traffic Controller



모든 module 통합.



Task

&#x20;|

Planner

&#x20;|

Reservation

&#x20;|

Priority

&#x20;|

Robot Command



16\. Phase 13 - Human Interaction



구현:



Human Detected

Human Cleared

Temporary Blockage

Long Blockage



17\. Phase 14 - Robot Failure



구현:



Robot Failure

Resource Release

Affected Robot Detection

Replanning

Recovery



18\. Phase 15 - Simulator



Simulator에:



Robot

Human

Task

Failure

Network Delay





를 추가한다.



19\. Phase 16 - Integration



실제 Traffic Controller와 simulator 연결.



Simulator

&#x20;  |

Robot Adapter

&#x20;  |

Traffic Controller



20\. Phase 17 - Stress Test



목표:



200 robots





검증:



CPU

Memory

Latency

Deadlock

Throughput



21\. Phase 18 - Production Adapter



실제 Robot protocol adapter를 구현한다.



Simulator와 동일 interface를 유지한다.



IRobotAdapter

&#x20;  |

&#x20;  +-- SimulatorAdapter

&#x20;  |

&#x20;  +-- ProductionAdapter



22\. Phase 19 - Shadow Mode



실제 현장에서 controller가 decision만 계산한다.



실제 Robot command는 보내지 않는다.



Real Robot

&#x20;   |

&#x20;   v

Traffic Controller

&#x20;   |

&#x20;   v

Decision

&#x20;   |

&#x20;   X

No command





목표:



실제 traffic과 simulation/model의 차이를 확인한다.



23\. Phase 20 - Limited Production



소수 Robot부터 적용한다.



10 robots





다음:



30

50

100

200





단계적으로 증가시킨다.



24\. AI Agent Workflow



AI Agent에게 작업을 줄 때:



1\. Read relevant specification

2\. Inspect repository

3\. Identify dependencies

4\. Implement smallest unit

5\. Write tests

6\. Run tests

7\. Fix failures

8\. Report changes





순서를 강제한다.



25\. AI Agent 금지사항



AI Agent가 임의로 다음을 변경하지 않도록 한다.



Architecture

Public API

Data Model

Algorithm Selection

Safety Policy

Reservation Semantics





변경이 필요하면 먼저 문서 수정안을 제안하도록 한다.



26\. AI Agent Task Format



각 task는 다음 형태를 사용한다.



TASK:

Implement ReservationManager.



READ:

06\_TRAFFIC\_RESERVATION.md

14\_DATA\_MODEL.md

12\_SOFTWARE\_ARCHITECTURE.md



REQUIREMENTS:

...



ACCEPTANCE:

...



TEST:

...



DO NOT:

...



27\. Commit Strategy



작은 단위로 commit한다.



예:



feat(core): add robot model

feat(map): add graph model

feat(planning): add astar

feat(reservation): add reservation table

test(reservation): add conflict tests



28\. Definition of Done



각 implementation task:



Code

\+

Unit Test

\+

Integration Test if required

\+

Documentation

\+

Build

\+

Test Pass



29\. Milestone

M1



Core + Map



M2



A\* + Reservation



M3



Multi Robot Baseline



M4



WHCA\* / PIBT



M5



Deadlock



M6



Replanning



M7



Traffic Controller



M8



Simulator



M9



200 Robot Stress



M10



Shadow Mode



M11



Pilot



30\. 최종 Production Gate



다음 조건을 만족해야 Production Pilot로 이동한다.



Collision = 0

Safety violation = 0



200 robot simulation PASS



Deadlock recovery PASS



Human blockage PASS



Robot failure PASS



Controller restart PASS



Network delay PASS



Deterministic replay PASS



24h soak test PASS



31\. 최종 Architecture



최종 시스템:



&#x20;                WMS / MES

&#x20;                    |

&#x20;                    v

&#x20;               Task Manager

&#x20;                    |

&#x20;                    v

&#x20;             Traffic Controller

&#x20;                    |

&#x20;      +-------------+-------------+

&#x20;      |             |             |

&#x20;      v             v             v

&#x20;  Planner     Reservation     Priority

&#x20;      |             |             |

&#x20;      +-------------+-------------+

&#x20;                    |

&#x20;            +-------+-------+

&#x20;            |               |

&#x20;            v               v

&#x20;      Replanning       Deadlock

&#x20;            |               |

&#x20;            +-------+-------+

&#x20;                    |

&#x20;                    v

&#x20;               Robot Adapter

&#x20;                    |

&#x20;             +------+------+

&#x20;             |             |

&#x20;        Simulator       Real Robot



32\. 핵심 개발 전략



이 프로젝트에서 가장 중요한 것은 "좋은 MAPF 알고리즘 하나를 선택하는 것"이 아니다.



전체 시스템을:



MAPF

\+

Reservation

\+

Priority

\+

Dynamic Replanning

\+

Deadlock Recovery

\+

Human-aware Blocking

\+

Simulation





의 결합 시스템으로 개발하는 것이다.



특히 100\~200대의 Robot이 좁은 corridor에서 운용되는 환경에서는 planner가 모든 문제를 해결하도록 설계하지 않는다.



Planner는:



Where should the robot go?





를 결정하고,



Traffic Controller는:



When can the robot go?





를 결정하며,



Reservation Manager는:



Which robot owns the resource?





를 결정하고,



Deadlock Manager는:



How do we recover when progress stops?





를 결정한다.



이 책임 분리를 유지하는 것이 본 프로젝트의 핵심 architecture principle이다.

