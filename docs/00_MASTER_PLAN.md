Traffic Control Software

Master Development Plan

1\. 프로젝트 목적



본 프로젝트는 100\~200대의 AMR/AGV가 동시에 운용되는 물류/제조 환경에서 로봇 간 교통을 제어하는 Traffic Control Software를 개발하는 것을 목적으로 한다.



주요 환경 특성:



100\~200대의 다중 로봇

좁고 긴 통로가 많음

단일 차선 또는 양방향 단일 통로 존재

우회로가 제한적

교차로가 존재

사람과 로봇이 동일 공간에서 이동

사람에 의해 로봇이 예상치 못하게 정지할 수 있음

로봇 고장 및 통신 장애 가능

작업이 지속적으로 생성/변경됨

높은 traffic congestion 가능

Deadlock 발생 가능

2\. 개발 목표



Traffic Control Software는 다음 목표를 만족해야 한다.



Safety

로봇 간 충돌 방지

동일 corridor에서의 충돌 방지

교차로 충돌 방지

reservation conflict 방지

장애물/사람으로 인한 정지 상황 처리

deadlock 발생 최소화

Performance

200대 규모 fleet 지원

높은 task throughput

낮은 평균 waiting time

낮은 planning latency

traffic congestion 최소화

Reliability

Robot failure 대응

Communication failure 대응

Traffic Controller restart 대응

Reservation 복구

상태 불일치 복구

3\. 전체 Architecture

WMS / MES

&#x20;   |

&#x20;   v

Task Manager

&#x20;   |

&#x20;   v

Robot Assignment

&#x20;   |

&#x20;   v

Global Route Planner

&#x20;   |

&#x20;   v

Traffic Control Engine

&#x20;   |

&#x20;   +-------------------+

&#x20;   |                   |

&#x20;   v                   v

Reservation Manager   Priority Manager

&#x20;   |                   |

&#x20;   +---------+---------+

&#x20;             |

&#x20;             v

&#x20;      Replanning Engine

&#x20;             |

&#x20;             v

&#x20;      Deadlock Manager

&#x20;             |

&#x20;             v

&#x20;       Robot Adapter

&#x20;             |

&#x20;             v

&#x20;        Robot Fleet



4\. 핵심 설계 원칙

4.1 Global Planning과 Traffic Control을 분리한다.



Global Planner:



"어떤 경로로 갈 것인가?"



Traffic Controller:



"언제 이동할 것인가?"



Robot Controller:



"어떻게 움직일 것인가?"



각 역할을 명확하게 분리한다.



4.2 Resource 기반 Traffic Control



좁은 corridor, intersection 등을 단순한 graph edge로만 취급하지 않고 Traffic Resource로 관리한다.



예:



Corridor C01

capacity = 1





Robot R01이 C01을 사용하고 있다면 다른 robot은 C01을 사용할 수 없다.



4.3 Reservation 기반 제어



Robot이 resource에 진입하기 전에 해당 resource를 reservation한다.



Robot

&#x20; |

&#x20; v

Request Reservation

&#x20; |

&#x20; +----> GRANT ----> Move

&#x20; |

&#x20; +----> WAIT



4.4 Event Driven



모든 robot을 매 순간 전체 재계획하지 않는다.



다음과 같은 event가 발생했을 때 필요한 robot만 재계획한다.



Human blockage

Robot blockage

Robot failure

Resource failure

Reservation conflict

Congestion 증가

Deadlock 발생

5\. 초기 알고리즘

Global Route Planning



1차:



A\*





Fallback:



Dijkstra



Traffic Control



기본:



Resource Reservation

\+

Priority Management

\+

Rolling Horizon



Dynamic Replanning



기본:



Affected Robot Replanning

\+

Rolling Horizon



Deadlock



기본:



Wait-for Graph

\+

Cycle Detection

\+

Recovery Policy



6\. 검토할 고급 알고리즘



기본 시스템이 안정화된 이후 다음 알고리즘을 benchmark한다.



WHCA\*

PIBT

ECBS

CBS





알고리즘을 처음부터 복잡하게 적용하지 않는다.



먼저:



A\*

\+

Reservation

\+

Priority

\+

Deadlock Recovery





를 구현하고 실제 simulation 결과를 기준으로 고급 알고리즘 도입 여부를 결정한다.



7\. 개발 Phase

Phase 1



Requirements



Phase 2



Traffic Simulator



Phase 3



Map / Graph



Phase 4



Robot / Task Model



Phase 5



Global Route Planner



Phase 6



Reservation Engine



Phase 7



Dynamic Replanning



Phase 8



Deadlock Manager



Phase 9



Large Scale Optimization



Phase 10



Real Robot Integration



Phase 11



Production Hardening



8\. Target Scale



반드시 다음 규모를 단계적으로 테스트한다.



10 robots

20 robots

50 robots

100 robots

150 robots

200 robots



9\. 주요 KPI



Traffic Controller의 성능은 다음 KPI로 평가한다.



Collision Count

Near Miss Count

Average Travel Time

Average Waiting Time

Maximum Waiting Time

Task Throughput

Task Completion Rate

Robot Utilization

Deadlock Count

Deadlock Recovery Time

Planning Latency P50

Planning Latency P95

Planning Latency P99

CPU Usage

Memory Usage



10\. Definition of Done



각 Phase는 다음 조건을 만족해야 완료한다.



구현 완료

Unit Test 완료

Integration Test 완료

Simulation Test 완료

Regression Test 완료

Logging 구현

KPI 측정 가능

문서 업데이트

알려진 문제 정리

11\. AI Agent 개발 원칙



AI Agent는 한 번에 전체 시스템을 구현하지 않는다.



반드시 다음 순서로 진행한다.



Requirement

&#x20;   ↓

Design

&#x20;   ↓

Implementation

&#x20;   ↓

Unit Test

&#x20;   ↓

Integration Test

&#x20;   ↓

Simulation

&#x20;   ↓

Benchmark

&#x20;   ↓

Review





현재 Phase의 요구사항을 충족하기 전에는 다음 Phase의 구현을 시작하지 않는다.

