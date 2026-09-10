Traffic Control Software

Test Scenario Specification

1\. Purpose



본 문서는 Multi-Robot Traffic Control Software의 검증을 위한 표준 Test Scenario를 정의한다.



Unit Test와 별도로 Simulation 및 Integration Test에서 동일한 Scenario를 반복 실행할 수 있도록 한다.



2\. Test Levels

Unit Test

&#x20;   |

&#x20;   v

Component Test

&#x20;   |

&#x20;   v

Integration Test

&#x20;   |

&#x20;   v

Simulation

&#x20;   |

&#x20;   v

Scale Test

&#x20;   |

&#x20;   v

Hardware Integration



3\. Test Categories

Functional

Safety

Traffic

Deadlock

Recovery

Performance

Scalability

Failure

Regression

Determinism



4\. Scenario Format



각 Scenario는 다음 정보를 가진다.



Scenario ID

Name

Environment

Robot Count

Initial State

Trigger

Expected Behavior

Expected Result

Metrics



5\. SC-001 Single Robot

Environment

A ---- B ---- C



Robots



1



Goal



R01: A -> C



Expected

A -> B -> C



Verify

Route generated

Robot moves

Task completed

No conflict

6\. SC-002 Two Robots Same Direction

Environment

A -------- B -------- C



Robots



2



Initial

R01 -> C

R02 -> C



Expected



Traffic Control이 정상적인 순서로 두 Robot을 통과시킨다.



7\. SC-003 Head-on Corridor

Environment

A ================= B



Robots



2



R01 A -> B

R02 B -> A



Expected

Head-on Conflict Detection

Priority Calculation

One Robot WAIT

One Robot GO

Corridor Exit

Other Robot GO

No Deadlock

8\. SC-004 Three Robot Corridor

Robots



3



R01 A -> B

R02 B -> A

R03 A -> B



Expected

Conflict Resolution

Deterministic Priority

No Deadlock

All Robot eventually complete

9\. SC-005 Narrow Corridor Bottleneck

Environment

A ----\\

&#x20;      \\==== C ====/

B ----/



Robots



10



Expected



Bottleneck Resource에 대한 Reservation이 정상적으로 수행된다.



10\. SC-006 Intersection

Environment

&#x20;     A

&#x20;     |

B ----X---- C

&#x20;     |

&#x20;     D



Robots



4



Expected

Intersection Conflict Detection

Reservation

Sequential Crossing

No Collision

11\. SC-007 Multiple Intersections

Robots



20



여러 Intersection을 동시에 사용하는 Scenario다.



Verify

Resource Reservation

Concurrent Traffic

Throughput

Deadlock

12\. SC-008 Robot Temporary Stop

Setup



R01이 이동 중 정지한다.



R01 -> TEMPORARILY\_STOPPED



Expected

R01 State Update

Following Robot WAIT

불필요한 Global Replanning 방지

R01 Recovery 후 Traffic 재개

13\. SC-009 Robot Blocked



R01이 Corridor 내부에서 장시간 정지한다.



Expected

MOVING

&#x20;  |

BLOCKED

&#x20;  |

Affected Robots

&#x20;  |

REPLAN



14\. SC-010 Robot Failure



R01이 Corridor를 점유한 상태에서 Failure 발생.



Expected

R01 FAILED

Occupied Resource 식별

영향 Robot 식별

Replanning

Deadlock 방지

15\. SC-011 Human Blockage



Human이 Corridor를 일시적으로 막는다.



Expected

Human Blockage

&#x20;     |

&#x20;     v

TEMPORARILY\_BLOCKED

&#x20;     |

&#x20;     v

Robot WAIT





Blockage가 해제되면 Traffic이 정상적으로 재개된다.



16\. SC-012 Long Human Blockage



Human Blockage가 장시간 지속된다.



Expected

Blockage

&#x20;  |

Long Duration

&#x20;  |

Alternative Route





Alternative Route가 없으면 WAIT 상태를 유지한다.



17\. SC-013 No Alternative Route



모든 Route가 Blocked 상태다.



Expected

No Path

WAIT

No unsafe movement

No false Success

18\. SC-014 Planner Timeout



Planner가 Timeout된다.



Expected

Planning

&#x20;  |

Timeout

&#x20;  |

Fallback / WAIT





System이 Hang되지 않아야 한다.



19\. SC-015 Reservation Conflict



두 Robot이 동일 Resource를 요청한다.



Expected

Conflict Detection

Priority

One Reservation Granted

Other WAIT

20\. SC-016 Reservation Expiration



Reservation의 유효시간이 종료된다.



Expected

ACTIVE

&#x20;|

&#x20;v

EXPIRED





Expired Reservation이 다른 Robot을 영구적으로 막지 않아야 한다.



21\. SC-017 Stale State



Planning 중 Robot State가 변경된다.



Planning State = 100

Current State = 101



Expected



기존 Planning Result가 검증되고 필요하면 폐기된다.



22\. SC-018 Stale Planning Result



Request A가 Request B보다 늦게 완료된다.



A started

B started

B completed

A completed



Expected



현재 State에 유효하지 않은 A 결과를 Commit하지 않는다.



23\. SC-019 Two Robot Deadlock

R01 -> R02

R02 -> R01



Expected

Deadlock Detection

Recovery

Both Robot eventually progress

24\. SC-020 Three Robot Cycle

R01 -> R02

R02 -> R03

R03 -> R01



Expected



Cycle Detection 및 Recovery.



25\. SC-021 Corridor Deadlock



여러 Robot이 좁은 Corridor에서 서로 진입하여 빠져나오지 못하는 상황을 만든다.



Expected

Deadlock Detection

Corridor Resource 분석

Recovery

No Permanent Block

26\. SC-022 Priority Starvation



R01이 반복적으로 Priority를 잃도록 Scenario를 구성한다.



Expected



Waiting Time에 따라 R01의 Priority가 상승한다.



R01이 영구적으로 WAIT하지 않아야 한다.



27\. SC-023 Priority Tie



두 Robot이 동일한 Priority를 가진다.



Expected



동일 Input에서 항상 동일한 Tie-breaking 결과를 생성한다.



28\. SC-024 Task Burst



짧은 시간에 많은 Task가 생성된다.



예:



100 Tasks

10 seconds



Expected

Queue 안정성

Planning 안정성

CPU 안정성

Deadlock 없음

29\. SC-025 High Traffic



100 Robots가 동시에 이동한다.



Metrics

Decision Latency

Planner Latency

Waiting Time

Throughput

Deadlock

30\. SC-026 150 Robot Scale



150 Robot 환경.



Expected



Traffic Controller가 정상적으로 동작한다.



측정:



P50 Latency

P95 Latency

P99 Latency

CPU

Memory

Throughput

31\. SC-027 200 Robot Scale



Production Target Scenario.



Expected



100\~200 Robot 규모에서 안정적인 Traffic Control.



필수 Metric:



Planning Latency

Decision Latency

Average Wait

P95 Wait

Deadlock Count

Replanning Count

CPU

Memory

Throughput



32\. SC-028 500 Robot Stress



Architecture 확장성 검증을 위한 Stress Test.



Production Target은 아니며 Scalability Test 목적이다.



33\. SC-029 Map Update



Traffic 중 Map이 변경된다.



Expected

Map v10

&#x20;  |

Map v11

&#x20;  |

Route Validation

&#x20;  |

Replanning





유효하지 않은 Route는 실행하지 않는다.



34\. SC-030 Communication Delay



Robot State 전달이 지연된다.



Expected

State Age 증가

Stale State 감지

안전한 Traffic Decision

Unknown Robot에 대한 신규 Permission 제한

35\. SC-031 Communication Loss



Robot과 통신이 완전히 끊긴다.



Expected

CONNECTED

&#x20;  |

STALE

&#x20;  |

UNKNOWN





System이 해당 Robot을 무시한 채 다른 Robot을 위험하게 통과시키지 않아야 한다.



36\. SC-032 Robot Recovery



Failure Robot이 복구된다.



Expected

FAILED

&#x20;  |

RECOVERING

&#x20;  |

IDLE

&#x20;  |

MOVING





Recovery 후 기존 Reservation과 Route를 재검증한다.



37\. SC-033 Replanning Storm



많은 Robot에서 동시에 State Change가 발생한다.



Expected

Replanning 폭증 방지

중복 Planning 최소화

CPU 안정성

Traffic Stability

38\. SC-034 Route Oscillation



두 Route의 비용 차이가 작은 상태에서 반복적인 Replanning을 발생시킨다.



Expected



Hysteresis에 의해 불필요한 Route 변경을 방지한다.



39\. SC-035 Planner Comparison



동일 Scenario를 여러 Planner로 실행한다.



예:



A\*

PIBT

WHCA\*

ECBS



Metrics

Success Rate

Planning Latency

Travel Time

Waiting Time

Deadlock

CPU

Memory

40\. SC-036 Determinism



동일 Input과 동일 Seed로 Scenario를 반복 실행한다.



Expected



동일한 결과를 생성한다.



비교:



Route

Priority

Reservation

Decision



41\. SC-037 Recovery Regression



과거 Deadlock Bug를 재현한다.



Expected



현재 Version에서 Bug가 재발하지 않는다.



42\. SC-038 Reservation Race



동시에 여러 Robot이 동일 Resource를 요청한다.



Expected



하나의 일관된 Reservation 결과를 생성한다.



Race Condition이 없어야 한다.



43\. SC-039 Concurrent Planning



여러 Planner Request가 동시에 실행된다.



Expected

Thread Safety

Correct Result

Stale Result Rejection

No Shared State Corruption

44\. SC-040 Full Traffic Scenario



실제 환경을 최대한 근접하게 구성한다.



구성:



200 Robots

Multiple Corridors

Multiple Intersections

Human Blockage

Robot Stops

Robot Failures

Task Burst

Dynamic Replanning



Expected



장시간 안정적으로 동작한다.



45\. Long Running Test



최소 수 시간 이상 지속적으로 실행한다.



목적:



Memory Leak

Resource Leak

Queue Growth

Reservation Leak

State Leak

Performance Degradation



확인.



46\. Performance Targets



초기 Engineering Target:



Decision Loop



P50 < 10 ms

P95 < 50 ms

P99 < 100 ms





실제 Production Target은 Benchmark 결과를 통해 확정한다.



47\. Traffic KPIs



필수 KPI:



Throughput

Average Travel Time

Average Waiting Time

P95 Waiting Time

Deadlock Rate

Replanning Rate

Planner Success Rate

Planner Latency

Decision Latency



48\. Resource KPIs



System:



CPU

Memory

Thread Count

Queue Depth

Event Rate

Network Rate



49\. Test Result Format



각 Scenario 실행 결과:



\## Scenario



SC-003 Head-on Corridor



\## Environment



10 Robots

Single Corridor



\## Result



PASS



\## Metrics



Planning P95: XX ms

Decision P95: XX ms

Average Wait: XX sec

Deadlock: 0



\## Notes



<additional observations>





실제 측정하지 않은 값은 작성하지 않는다.



50\. Regression Policy



Bug 발생 시:



Bug

&#x20;|

&#x20;v

Reproduce

&#x20;|

&#x20;v

Scenario 생성

&#x20;|

&#x20;v

Regression Test

&#x20;|

&#x20;v

Fix

&#x20;|

&#x20;v

Regression





기존 Scenario는 특별한 이유 없이 삭제하지 않는다.



51\. Test Data Version



Scenario와 Map은 Version을 가진다.



예:



Map: v1.4

Scenario: v2.1

Planner: PIBT

Config: v1.3





Test 결과 재현성을 확보한다.



52\. Scenario Automation



가능하면 모든 주요 Scenario는 자동 실행할 수 있어야 한다.



예:



scenario\_runner

&#x20;   |

&#x20;   +-- SC-001

&#x20;   +-- SC-002

&#x20;   +-- SC-003

&#x20;   +-- ...





CI에서 빠른 Regression Scenario를 자동 실행한다.



53\. CI Test Levels



Commit:



Unit Test





Pull Request:



Unit

Component

Selected Integration





Nightly:



Full Integration

Simulation

Deadlock

Performance





Release:



Full Regression

Scale Test

Long Running Test



54\. Test Failure Policy



Test가 실패하면 단순히 Retry해서 통과시키지 않는다.



먼저:



Failure

&#x20;|

Reproduce

&#x20;|

Analyze

&#x20;|

Fix





Flaky Test는 별도로 관리한다.



55\. Final Acceptance



Production Candidate는 최소 다음을 통과해야 한다.



\[ ] Unit Tests

\[ ] Integration Tests

\[ ] Core Traffic Scenarios

\[ ] Corridor Scenarios

\[ ] Intersection Scenarios

\[ ] Human Blockage

\[ ] Robot Stop

\[ ] Robot Failure

\[ ] Deadlock

\[ ] Deadlock Recovery

\[ ] Planner Timeout

\[ ] Stale State

\[ ] 100 Robot Scale

\[ ] 150 Robot Scale

\[ ] 200 Robot Scale

\[ ] Long Running Test

\[ ] Determinism Test

\[ ] Regression Test



56\. Final Principle



Test의 목적은 단순히 "코드가 동작한다"를 확인하는 것이 아니다.



다음을 증명해야 한다.



Correctness

\+

Safety

\+

Stability

\+

Scalability

\+

Determinism

\+

Recoverability





최종적으로 실제 Robot 환경에 투입하기 전에 Simulation과 Scale Test를 통해 Traffic Control Policy를 검증한다.

