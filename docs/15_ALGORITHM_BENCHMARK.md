Traffic Control Software

Multi-Robot Algorithm Benchmark Specification

1\. 목적



100\~200 Robot Traffic Control 환경에 적합한 Multi-Agent Path Finding 및 Traffic Coordination 알고리즘을 실험적으로 선정한다.



알고리즘을 처음부터 하나로 확정하지 않는다.



Benchmark 결과를 기준으로 선택한다.



2\. 후보 알고리즘



최소 다음을 비교한다.



A\*

WHCA\*

PIBT

ECBS

CBS

Prioritized Planning

Reservation-based A\*



3\. 평가 환경

Scenario A

20 robots

Low traffic



Scenario B

50 robots

Medium traffic



Scenario C

100 robots

High traffic



Scenario D

200 robots

Very high traffic



4\. Map Characteristics



실제 환경을 최대한 반영한다.



Long corridor

Narrow corridor

Few alternative paths

Intersections

Waiting bays

Shared human areas



5\. Dynamic Obstacle Scenario



Robot 외에 다음 장애물을 넣는다.



Human

Stopped Robot

Temporary Blockage

Permanent Blockage



6\. Task Generation



Task는 random뿐 아니라 실제 workload distribution을 사용한다.



Source distribution

Destination distribution

Priority distribution

Task arrival rate



7\. Metrics

Planning

Planning latency

P50

P95

P99



Traffic

Throughput

Average travel time

Average waiting time

P95 waiting time



Stability

Route change count

Replanning count



Safety

Collision count

Conflict count

Deadlock count

Recovery failure



8\. Computational Metrics

CPU usage

Memory usage

Planner queue length

Event processing latency



9\. Scalability Test



다음 Robot count를 테스트한다.



10

20

50

100

150

200

300

500





300\~500은 architecture scalability 검증용이다.



10\. Algorithm Expectations

A\*



장점:



단순

안정적

구현 용이



단점:



multi-robot coordination 부족



사용:



Baseline

Single Robot

Fallback Planner



11\. Prioritized Planning



장점:



구현 간단

빠름



단점:



priority ordering에 따라 결과가 크게 달라짐

deadlock 가능



사용:



Baseline

Low traffic



12\. WHCA\*



장점:



multi-agent

rolling horizon

실시간 환경에 비교적 적합



단점:



window 크기 tuning 필요



Priority:



HIGH CANDIDATE



13\. PIBT



장점:



빠른 online MAPF

많은 agent 처리에 유리

decentralized 성격



단점:



환경 특성에 따른 tuning 필요



Priority:



HIGH CANDIDATE



14\. CBS



장점:



높은 solution quality

conflict resolution 명확



단점:



agent 증가 시 computational cost 증가



사용:



Benchmark

Small/medium fleet

Offline comparison



15\. ECBS



CBS의 bounded-suboptimal variant.



장점:



CBS보다 빠를 수 있음

solution quality 제어 가능



사용:



Benchmark

Medium fleet

Selective replanning



16\. 권장 초기 구조



실제 production에서는 하나의 planner만 사용하는 것을 목표로 하지 않는다.



권장:



&#x20;                 Traffic Controller

&#x20;                        |

&#x20;                +-------+-------+

&#x20;                |               |

&#x20;         Online Planner    Recovery Planner

&#x20;                |               |

&#x20;              PIBT/WHCA\*      ECBS/A\*



17\. Hybrid Planning



권장 초기 실험 구조:



Global Route

&#x20;    |

&#x20;    v

A\* / weighted A\*

&#x20;    |

&#x20;    v

Traffic Coordination

&#x20;    |

&#x20;    v

PIBT / WHCA\*

&#x20;    |

&#x20;    v

Reservation



18\. Planner Selection Rule



초기에는 다음 후보를 우선 benchmark한다.



1\. PIBT

2\. WHCA\*

3\. Prioritized Planning

4\. ECBS

5\. CBS



19\. Dynamic Environment Test



Human blockage를 random하게 발생시킨다.



예:



Blockage probability:

1%

5%

10%

20%



20\. Long Corridor Test



Corridor length:



10 cells

20 cells

50 cells

100 cells



21\. Low Detour Test



Alternative route 수:



0

1

2



22\. Deadlock Test



의도적으로 conflict를 생성한다.



Head-on

3-way

4-way

Chain

Cycle



23\. Stress Test

200 robots

High task arrival rate

10% human blockage

5% robot failure



24\. Acceptance Target



초기 목표:



200 robots

P95 controller decision < 100 ms



Deadlock rate:

as close to 0 as possible



Collision:

0



Planner timeout:

< 1%





실제 target은 benchmark 결과를 기준으로 조정한다.



25\. Selection Score



알고리즘 종합 점수:



score =

&#x20;   0.30 \* throughput

&#x20; + 0.20 \* latency

&#x20; + 0.20 \* waiting\_time

&#x20; + 0.15 \* deadlock\_performance

&#x20; + 0.10 \* route\_stability

&#x20; + 0.05 \* resource\_usage





Safety violation이 발생한 알고리즘은 종합 점수와 무관하게 탈락시킨다.



26\. Benchmark Output



각 알고리즘에 대해:



algorithm

robot\_count

scenario



throughput

travel\_time

waiting\_time



planning\_latency

CPU

memory



deadlock\_count

replanning\_count

route\_change\_count





을 저장한다.



27\. 최종 선택 기준



최종 알고리즘은 단순히 평균 성능이 가장 좋은 것을 선택하지 않는다.



다음을 모두 고려한다.



Performance

Scalability

Stability

Deadlock behavior

Dynamic blockage response

Implementation complexity

Operational predictability



28\. 중요한 원칙



Benchmark에서 좋은 결과가 나온 알고리즘도 실제 현장에서는 반드시 재검증한다.



Simulation:



Model





Production:



Reality





이므로 simulation-to-real validation을 별도 수행한다.

