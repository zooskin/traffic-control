Traffic Control Software

Test Strategy

1. 목적

Traffic Control Software의 correctness, safety, performance, scalability를 검증한다.

2. Test Pyramid
              E2E
             /   \
        Simulation
          /       \
    Integration
       /           \
       Unit Tests

3. Unit Test

대상:

Map

Robot

Task

Reservation

Priority

Planner

Deadlock

Replanning

4. Reservation Test

필수:

test_grant

test_conflict

test_release

test_expire

test_cancel

test_overlap

5. Priority Test

test_task_priority

test_waiting_aging

test_deadlock_priority

test_tie_break

test_starvation_prevention

6. Deadlock Test

test_two_robot_cycle

test_three_robot_cycle

test_large_cycle

test_false_positive

test_recovery

test_recovery_failure

7. Replanning Test

test_human_blockage

test_robot_failure

test_no_route

test_retry

test_route_commit

test_route_hysteresis

8. Integration Test

다음 component 조합을 검증한다.

Planner
+

Reservation
+

Priority

그리고:

Reservation
+

Deadlock
+

Replanning

9. Simulation Test

실제 map을 simulator에 넣는다.

Map
+

Robot
+

Task
+

Human
+

Traffic Controller

10. Scenario Test

S01 Normal

100 robots normal operation.

S02 High Traffic

200 robots high task arrival.

S03 Human Blockage

Human이 corridor를 10~30초 점유.

S04 Robot Failure

Corridor 내부 Robot failure.

S05 Head-on

좁은 corridor에서 양방향 traffic.

S06 Deadlock

의도적인 cycle 생성.

S07 Controller Restart

운영 중 controller restart.

11. Stress Test

200 robots

1000 tasks/hour

High blockage

High replanning

12. Soak Test

최소:

8 hours

권장:

24 hours

검증:

Memory leak

State corruption

Queue growth

Reservation leak

13. Chaos Test

Random failure:

Robot disconnect

Network delay

Event duplication

Event loss

Planner timeout

Database restart

14. Deterministic Replay

모든 simulation event를 저장하여 동일 simulation을 재실행할 수 있어야 한다.

Scenario
+

Seed
+

Event Log

15. Safety Test

반드시 검증:

No collision

No unsafe command

No duplicate movement command

No invalid reservation

16. Performance Test

측정:

Event latency

Decision latency

Planning latency

Command latency

17. Load Test

Robot:

50

100

150

200

300

500

18. Regression Test

알고리즘 변경 시 전체 scenario를 자동 실행한다.

git commit
   |
   v

CI
   |
   v

Unit
   |
   v

Integration
   |
   v

Simulation
   |
   v

Benchmark

19. Test Result

각 test는 다음 정보를 기록한다.

test_id

commit_id

scenario

seed

robot_count

result

throughput

latency

deadlock

collision

duration

20. Release Gate

Production release 전:

Unit = PASS

Integration = PASS

Simulation = PASS

Safety = PASS

Stress = PASS

Regression = PASS

모두 통과해야 한다.
