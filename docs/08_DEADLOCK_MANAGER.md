Traffic Control Software

Deadlock Manager Specification

1. 목적

Deadlock Manager는 여러 Robot이 서로의 이동을 기다리면서 영원히 진행하지 못하는 상황을 감지하고 recovery한다.

좁은 corridor와 우회로가 부족한 환경에서는 Deadlock이 주요 failure mode가 될 수 있다.

2. Deadlock 예시

R01 waits for R02

R02 waits for R03

R03 waits for R01

Graph:

R01 → R02
 ↑       ↓
 └── R03

Cycle이 존재하므로 deadlock 가능성이 높다.

3. Wait-for Graph

Deadlock detection의 핵심 구조:

Wait-for Graph

Robot
  |
  v

Resource
  |
  v

Owner Robot

간단한 표현:

R01 → R02

의미:

R01은 R02가 resource를 release하기를 기다린다.

4. Edge 생성

Robot R01이 C01을 요청했지만 R02가 C01을 점유한다면:

R01 → R02

edge를 생성한다.

5. Cycle Detection

Graph에서 cycle을 찾는다.

R01 → R02

R02 → R03

R03 → R01

cycle 발견:

R01

R02

R03

6. Deadlock State

SUSPECTED

CONFIRMED

RECOVERING

RESOLVED

7. False Positive

일시적인 waiting을 deadlock으로 판단하면 안 된다.

따라서 다음 조건을 함께 사용한다.

cycle exists

AND

minimum_cycle_duration exceeded

예:

deadlock_confirmation_time = 5 sec

8. Deadlock Score

필요할 경우 score 기반으로 판단한다.

deadlock_score =
    cycle_score
  + waiting_time_score
  + no_progress_score
  + resource_dependency_score

9. Recovery Strategy

Recovery는 최소 비용으로 deadlock을 해소해야 한다.

우선순위:

1. WAIT adjustment

2. Reservation rollback

3. Backward movement

4. Waiting Bay

5. Route replanning

6. Task reassignment

실제 robot이 후진 가능한지는 Robot Capability로 판단한다.

10. Priority Reversal

Deadlock을 해결하기 위해 특정 Robot의 우선순위를 임시로 변경할 수 있다.

예:

R01 priority = 90

R02 priority = 50

R03 priority = 30

하지만 R01이 계속 resource를 독점하지 않도록 제한한다.

11. Victim Selection

Deadlock recovery에서 어느 Robot을 양보시킬지 결정한다.

기본 score:
victim_cost =
    w_progress * remaining_distance
  + w_task * task_priority
  + w_wait * waiting_time
  + w_reverse * reverse_cost

일반적으로 recovery cost가 낮은 robot을 선택한다.

12. Waiting Bay Recovery

가능한 경우 robot을 waiting bay로 이동시킨다.

Deadlock
   |
   v

Select Victim
   |
   v

Waiting Bay
   |
   v

Release Corridor
   |
   v

Other Robots Move

13. Corridor Deadlock

예:

A ---------------- B

R01:

A → B

R02:

B → A

둘이 corridor 안에서 서로 마주친 경우:

R01 ←→ R02

deadlock recovery가 필요하다.

14. Corridor 정책

좁은 corridor에서는 가능한 경우:

Corridor Entry

전에 traffic 방향을 결정한다.

즉:

Enter Corridor

와:

Pass Corridor

를 별도로 관리한다.

15. Corridor Batch

같은 방향의 robot을 batch로 처리할 수 있다.

예:

A → B:

R01

R02

R03

그 다음:

B → A:

R04

R05

이런 방식으로 corridor utilization을 높일 수 있다.

16. Direction Lock

Corridor가 단일 차선이면 일정 시간 동안 방향을 lock할 수 있다.

direction = A_TO_B

반대 방향 Robot:

WAIT

17. Direction Lock 해제

다음 조건 중 하나가 발생하면 방향을 변경할 수 있다.

corridor empty

OR

maximum_batch_size reached

OR

maximum_direction_hold_time reached

OR

opposite_priority exceeds threshold

18. Starvation Prevention

한 방향만 계속 통과하지 않도록 aging을 사용한다.

opposite_waiting_time

이 증가하면 반대 방향 priority가 증가한다.

19. Deadlock Recovery State Machine

NORMAL
  |
  v

SUSPECTED
  |
  v

CONFIRMED
  |
  v

RECOVERING
  |
  +---- success ---> RESOLVED
  |
  +---- failure ---> ESCALATE

20. Escalation

자동 recovery가 실패하면:

ESCALATE

상태가 된다.

상위 시스템에:

Deadlock Alert

를 발생시킨다.

21. Recovery 제한

자동 recovery는 무한 반복하지 않는다.

Configuration:

max_recovery_attempts

예:

max_recovery_attempts = 3

초과 시 operator intervention을 요청한다.

22. Deadlock Dependency Snapshot

Deadlock 발생 시 당시 graph를 저장한다.

timestamp

robots

resources

wait_edges

reservations

routes

states

이를 통해 사후 분석이 가능해야 한다.

23. API

detect_deadlock()

get_wait_for_graph()

confirm_deadlock()

select_victim()

select_recovery_strategy()

execute_recovery()

get_deadlock_status()

24. Acceptance Criteria

다음 simulation을 반드시 통과해야 한다.

Scenario 1

2 Robot cycle.

Scenario 2

3 Robot cycle.

Scenario 3

5 Robot chain.

Scenario 4

Narrow corridor head-on deadlock.

Scenario 5

Waiting bay를 이용한 recovery.

Scenario 6

Recovery 실패 및 escalation.

25. Test Requirements

test_wait_for_graph

test_cycle_detection

test_deadlock_confirmation

test_two_robot_deadlock

test_multi_robot_deadlock

test_victim_selection

test_waiting_bay_recovery

test_direction_lock

test_starvation_prevention

test_recovery_retry

test_escalation
