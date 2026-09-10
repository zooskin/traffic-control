Traffic Control Software

Traffic Control Specification

1\. Purpose



본 문서는 Multi-Robot Traffic Control Software의 실제 Traffic Policy와 동작 규칙을 정의한다.



이 문서는 단순한 Algorithm 설명이 아니라 다음 질문에 답하는 것을 목표로 한다.



"특정 Traffic 상황에서 Robot Fleet은 정확히 어떻게 행동해야 하는가?"



2\. Target Environment

Robot

100\~200 Robots

확장 목표 500+ Robots

Environment

Narrow Corridor

Long Corridor

Limited Alternative Route

Intersection

Bottleneck

Dead End

Human Shared Space

Dynamic Condition

Robot Temporary Stop

Robot Failure

Human Blockage

Navigation Delay

Communication Delay

3\. Traffic Control Objectives



우선순위:



1\. Safety

2\. Deadlock Avoidance

3\. System Stability

4\. Throughput

5\. Waiting Time

6\. Travel Time

7\. Route Optimality





Shortest Path가 항상 최우선은 아니다.



4\. Basic Traffic Flow

Task

&#x20;|

&#x20;v

Route Planning

&#x20;|

&#x20;v

Conflict Detection

&#x20;|

&#x20;v

Priority

&#x20;|

&#x20;v

Reservation

&#x20;|

&#x20;v

Movement Permission

&#x20;|

&#x20;v

Robot



5\. Movement Permission



Robot은 Route를 가지고 있는 것만으로 이동할 수 있다고 간주하지 않는다.



다음 조건을 만족해야 한다.



Valid Route

\+

Valid Reservation

\+

Valid Robot State

\+

No Safety Conflict



6\. Corridor Policy



좁은 Corridor는 중요한 Traffic Resource다.



예:



A ===================== B





Corridor 진입 전에 Entry Permission을 확인한다.



7\. Single Lane Corridor



단일 Robot만 통과 가능한 Corridor에서는 동시에 반대 방향 Robot이 진입하지 않도록 한다.



R01 ----->



<----- R02





한 Robot이 Corridor를 점유한 경우 다른 방향 Robot은 WAIT한다.



8\. Head-on Conflict



예:



R01 -----> <----- R02





정책:



Conflict Detection

Priority Calculation

한 Robot 선택

선택된 Robot GO

다른 Robot WAIT

Corridor Exit

Reservation Release

대기 Robot GO

9\. Corridor Entry



Robot은 Corridor 전체를 안전하게 통과할 수 있는지 확인한 후 진입하는 것을 기본 정책으로 한다.



가능하면:



Check Corridor

&#x20;     |

&#x20;     v

Reserve Corridor

&#x20;     |

&#x20;     v

Enter



10\. Corridor Reservation



Corridor Reservation은 다음 정보를 포함할 수 있다.



corridor\_id

robot\_id

direction

entry\_time

expected\_exit\_time

priority



11\. Intersection Policy



Intersection도 Resource로 취급한다.



기본 원칙:



Reserve

&#x20;  |

Enter

&#x20;  |

Cross

&#x20;  |

Exit

&#x20;  |

Release



12\. Intersection Conflict



두 Robot이 동일 Intersection을 동시에 사용할 수 없는 경우:



R01 ---> X <--- R02





Priority를 계산하여 한 Robot만 진입시킨다.



13\. Waiting Policy



WAIT 상태는 정상적인 Traffic Control 상태다.



WAIT Robot은 다음 정보를 기록할 수 있다.



robot\_id

waiting\_since

reason

blocking\_robot

blocking\_resource



14\. Waiting Reason



예:



RESERVATION\_CONFLICT

CORRIDOR\_OCCUPIED

INTERSECTION\_OCCUPIED

HUMAN\_BLOCKAGE

ROBOT\_BLOCKAGE

PLANNING

DEADLOCK\_RECOVERY



15\. Starvation Prevention



특정 Robot이 지속적으로 Priority를 잃지 않도록 한다.



Waiting Time을 Priority에 반영한다.



예:



EffectivePriority =

BasePriority

\+

WaitingBonus

\+

DeadlineUrgency





실제 계수는 Benchmark로 결정한다.



16\. Priority Tie Breaking



동일 Priority인 경우 deterministic한 Tie Breaking을 사용한다.



예:



Priority

&#x20;   |

Waiting Time

&#x20;   |

Task ID

&#x20;   |

Robot ID





최종 Tie Break는 항상 동일한 결과를 만들어야 한다.



17\. Temporary Stop



Robot이 잠시 정지한 경우 즉시 Failure로 처리하지 않는다.



MOVING

&#x20;  |

&#x20;  v

TEMPORARILY\_STOPPED

&#x20;  |

&#x20;  v

MOVING



18\. Temporary Stop Timeout



Temporary Stop이 일정 시간 이상 지속되면 상태를 재평가한다.



예:



0\~T1       TEMPORARILY\_STOPPED

T1\~T2      BLOCKED

>T2        FAILURE / RECOVERY





실제 T 값은 운영 환경 Benchmark로 결정한다.



19\. Human Blockage



Human이 Corridor를 막는 경우:



Corridor

&#x20;  |

&#x20;  v

HUMAN BLOCKED





영향받는 Robot은 WAIT한다.



가능한 경우 Alternative Route를 탐색한다.



20\. Human Blockage Duration



Blockage 예상 시간이 짧으면 WAIT가 유리할 수 있다.



Blockage가 장기화되면 Replanning을 고려한다.



Short Blockage

&#x20;     |

&#x20;     v

WAIT



Long Blockage

&#x20;     |

&#x20;     v

REPLAN



21\. Robot Failure



Robot Failure가 발생하면 해당 Robot이 점유한 Resource를 재평가한다.



Robot Failure

&#x20;     |

&#x20;     v

Identify Occupied Resources

&#x20;     |

&#x20;     v

Mark Blocked

&#x20;     |

&#x20;     v

Affected Robots

&#x20;     |

&#x20;     v

Replanning



22\. Reservation Expiration



Reservation은 무한정 유지하지 않는다.



ACTIVE

&#x20;|

&#x20;v

EXPIRED





Robot이 예상 시간보다 늦어지는 경우 Reservation을 갱신하거나 재계획한다.



23\. Stale Reservation



Robot State가 오래된 경우 기존 Reservation을 그대로 신뢰하지 않는다.



State와 Reservation의 일관성을 확인한다.



24\. Planning Trigger



Replanning Trigger:



Robot Stopped

Robot Blocked

Human Blockage

Robot Failure

Reservation Conflict

Map Update

Deadlock

Task Change



25\. Local Replanning



가능하면 영향을 받은 Robot만 Replanning한다.



100 Robots

&#x20;  |

Event

&#x20;  |

Affected 5 Robots

&#x20;  |

Replan 5 Robots





전체 100 Robot을 항상 Replanning하지 않는다.



26\. Global Replanning



다음 경우 Global Replanning을 고려한다.



Large-scale Traffic Collapse

Multiple Deadlocks

Major Map Change

Local Planning Failure

System Recovery

27\. Deadlock Detection



Wait-for Graph를 사용한다.



R01 -> R02

R02 -> R03

R03 -> R01





Cycle이 발생하면 Deadlock 후보로 판단한다.



28\. Deadlock Recovery Priority



기본 Recovery 순서:



1\. Local Replanning

2\. Priority Change

3\. Backtracking

4\. Resource Release

5\. Holding Area

6\. Human Intervention



29\. Deadlock Recovery Safety



Recovery 과정에서 다음을 금지한다.



Safety Boundary 침범

충돌 가능 Route 강제

Unknown Robot 위로 이동

검증되지 않은 Reservation 강제 삭제

30\. No Path



Planner가 Route를 찾지 못한 경우:



NO\_PATH





즉시 Failure로 처리하지 않는다.



가능한 처리:



WAIT

RETRY

ALTERNATIVE\_PLANNER

REPLAN

HUMAN\_INTERVENTION



31\. Planner Timeout



Planner Timeout:



TIMEOUT





정책:



Current Safe Route 유지

또는

WAIT

또는

Fallback Planner





실제 정책은 상황별로 정의한다.



32\. Planner Fallback



Planner 교체 구조:



Primary Planner

&#x20;     |

&#x20;     X

&#x20;     |

Fallback Planner





예:



PIBT

&#x20;|

Timeout

&#x20;|

WHCA\*





Fallback 조건은 명확하게 정의한다.



33\. Traffic Decision State



Decision:



PROPOSED

VALIDATING

COMMITTED

EXECUTING

COMPLETED

CANCELLED

FAILED



34\. Decision Validation



Commit 전에:



Map Version

\+

State Version

\+

Reservation

\+

Safety





를 확인한다.



35\. Command Policy



Traffic Controller는 다음 Command를 사용할 수 있다.



GO

WAIT

STOP

REPLAN

HOLD

RECOVER





Safety Stop은 Safety Controller의 책임이다.



36\. Command Idempotency



가능하면 동일 Command를 반복해서 전송해도 시스템 상태가 비정상적으로 변하지 않도록 한다.



예:



WAIT

WAIT

WAIT





은 하나의 상태로 처리 가능해야 한다.



37\. Communication Failure



Robot과 통신이 끊긴 경우:



CONNECTED

&#x20;   |

&#x20;   v

STALE

&#x20;   |

&#x20;   v

UNKNOWN





Unknown Robot에 대해 새로운 Traffic Permission을 임의로 생성하지 않는다.



38\. Map Update



Map 변경 시 기존 Route의 유효성을 확인한다.



Map v10

Route v10

&#x20;    |

Map Update

&#x20;    |

Map v11

&#x20;    |

Route Validation





유효하지 않으면 Replanning한다.



39\. Traffic Priority Classes



필요한 경우 다음 Priority Class를 사용할 수 있다.



EMERGENCY

HIGH

NORMAL

LOW





Priority Class가 Safety Rule을 override해서는 안 된다.



40\. Throughput Policy



Throughput 향상을 위해 다음을 고려한다.



Corridor Utilization

Intersection Utilization

Waiting Time

Batch Planning

Local Replanning

Reservation Horizon



단, Safety와 Deadlock Prevention보다 우선하지 않는다.



41\. Traffic Stability



지속적인 Replanning을 방지한다.



예:



Plan A

&#x20;  |

Minor State Change

&#x20;  |

Plan A 유지





작은 변화마다 Route를 바꾸면 Traffic Oscillation이 발생할 수 있다.



42\. Hysteresis



Decision 변경에는 일정한 Threshold를 둘 수 있다.



예:



Current Route

&#x20;    |

Small improvement

&#x20;    |

Keep Route





충분한 이득이 있는 경우에만 Route를 변경한다.



43\. Traffic Metrics



필수 Metric:



Average Travel Time

Average Waiting Time

P95 Waiting Time

Deadlock Count

Replanning Count

Throughput

Planner Latency

Decision Latency



44\. Acceptance Criteria



초기 시스템의 기본 Acceptance Criteria:



No Collision in Simulation

No Unresolved Deadlock in Defined Scenarios

Deterministic Decision

100\~200 Robot Scale Test

Planner Timeout Handling

Robot Failure Handling

Human Blockage Handling





실제 수치 목표는 Benchmark 단계에서 확정한다.



45\. Final Policy



Traffic Control의 기본 원칙:



Reserve Before Enter

Wait Before Conflict

Detect Before Commit

Validate Before Execute

Replan When Necessary

Recover From Deadlock

Never Ignore Unknown State





최종 우선순위:



Safety

&#x20;>

Deadlock Prevention

&#x20;>

Stability

&#x20;>

Throughput

&#x20;>

Waiting Time

&#x20;>

Travel Time

&#x20;>

Optimality

