Traffic Control Software

Simulation Scenario Specification

1. 목적

실제 현장의 traffic 특성을 simulation으로 재현하고 알고리즘을 검증한다.

2. Base Map

기본 map 구성:

Entrance
   |
   v

Long Corridor
   |

Intersection
 /    \

Area A Area B
   |

Waiting Bay

3. Robot Population

기본:

100 robots

Stress:

200 robots

4. Task Distribution

70% normal

20% high priority

10% urgent

초기값이며 실제 workload로 calibration한다.

5. Human Model

Human은 다음 행동을 random하게 수행한다.

Walk

Stop

Cross

Enter Corridor

Exit Corridor

6. Human Blockage

Blockage duration:

1 sec

5 sec

10 sec

30 sec

60 sec

7. Robot Failure

Failure 위치:

Node

Corridor

Intersection

Station

8. Scenario S01

Normal Traffic

100 robots

No blockage

No failure

목표:

baseline throughput 측정.

9. Scenario S02

High Traffic

200 robots

High task arrival

목표:

scalability.

10. Scenario S03

Human Blockage

100 robots

Human blockage every 60 sec

목표:

dynamic replanning.

11. Scenario S04

Long Blockage

Blockage = 60 sec

목표:

WAIT → REPLAN 동작 검증.

12. Scenario S05

Head-on

R01 → A

R02 → B

좁은 corridor에서 서로 반대 방향으로 접근.

목표:

corridor direction management.

13. Scenario S06

Corridor Batch

A → B:

R01

R02

R03

R04

반대 방향:

B → A:

R05

R06

목표:

batch optimization.

14. Scenario S07

Deadlock

3~5개 Robot이 cycle을 생성한다.

목표:

deadlock detection 및 recovery.

15. Scenario S08

Robot Failure

Corridor 중간 Robot이 정지한다.

목표:

affected robot detection.

16. Scenario S09

Multiple Blockage

동시에:

Human blockage
+

Robot failure
+

High traffic

목표:

cascade handling.

17. Scenario S10

Controller Restart

Traffic 중 controller restart.

목표:

state recovery.

18. Scenario S11

Network Delay

Robot state delay:

100 ms

500 ms

1 sec

목표:

stale state handling.

19. Scenario S12

Duplicate Event

동일 RobotStateUpdated event를 여러 번 전달한다.

목표:

idempotency.

20. Scenario S13

Event Reordering

event timestamp와 arrival order를 다르게 한다.

목표:

out-of-order handling.

21. Scenario S14

Maximum Load

200 robots

maximum task arrival

10% human blockage

5% robot failure

목표:

system limit 확인.

22. KPI

모든 scenario에서:

Throughput

Average travel time

P95 travel time

Average waiting

P95 waiting

Deadlock

Replanning

Collision

Safety violation

측정.

23. Success Criteria

Collision = 0

Safety violation = 0

그리고:

Deadlock recovery success > target

Planner timeout < target

Controller P99 < target

을 만족해야 한다.

24. Random Seed

모든 random simulation은 seed를 저장한다.

seed = 12345

동일 seed로 재현 가능해야 한다.

25. Scenario Configuration

각 scenario는 YAML로 정의한다.

scenario:
  name: human_blockage
  seed: 12345

fleet:
  robots: 100

traffic:
  task_rate: 20

human:
  blockage_probability: 0.1
  duration_sec:
    min: 5
    max: 30

26. Batch Benchmark

각 scenario를 여러 seed로 반복한다.

seed:

1

2

3

4

5

10

20

50

100

평균뿐 아니라 variance를 측정한다.
