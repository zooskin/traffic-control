Traffic Control Software

Priority Manager Specification

1. 목적

Priority Manager는 Traffic conflict가 발생했을 때 어떤 Robot이 먼저 resource를 사용할 것인지 결정한다.

Priority는 고정값 하나로 결정하지 않는다.

2. Priority 구성

기본:

Priority Score =
    Task Priority
  + Waiting Time
  + Deadlock Risk
  + Resource Cost
  + Aging

3. 기본 Priority

Task에서 제공되는 priority:

0 ~ 100

4. Waiting Aging

오래 기다린 Robot의 priority를 증가시킨다.

aging_score =

aging_factor * waiting_time

5. Deadlock Risk

Deadlock에 가까운 Robot은 우선적으로 처리할 수 있다.

deadlock_risk =

0 ~ 100

6. Final Score

score =
    w_task * task_priority
  + w_wait * waiting_score
  + w_deadlock * deadlock_risk
  + w_aging * aging_score

모든 weight는 configuration으로 관리한다.

7. Priority Ordering

높은 score가 먼저 처리된다.

동점인 경우 deterministic tie-breaker:

1. waiting_time

2. task_id

3. robot_id

8. Starvation Prevention

다음 조건을 반드시 만족한다.

waiting_time ↑
    =>

priority ↑

단, priority가 무한히 증가하지 않도록 upper bound를 둔다.

9. Priority Inversion

높은 priority robot이 낮은 priority robot이 점유한 resource를 기다리는 상황을 처리한다.

필요한 경우 temporary priority inheritance를 적용할 수 있다.

예:

High Priority R01
        |
        v

waits for
        |
        v

Low Priority R02

R02의 effective priority를 일시적으로 높인다.

10. Corridor Priority

좁은 corridor에서는 Robot 하나씩 판단하기보다 방향을 고려한다.

A → B : 5 robots waiting

B → A : 1 robot waiting

단순 priority가 아니라 batch efficiency를 고려할 수 있다.

11. Corridor Batch Score

direction_score =
    waiting_robot_count
  + average_priority
  + aging

12. Priority Override

Deadlock recovery에서는 일반 priority를 override할 수 있다.

NORMAL
    |
    v

DEADLOCK RECOVERY
    |
    v

RECOVERY PRIORITY

Recovery 종료 후 원래 priority policy로 돌아간다.

13. Fairness

Priority Manager는 다음 metric을 측정한다.

max_waiting_time

average_waiting_time

waiting_time_variance

특정 Robot이 지속적으로 불리한지 확인한다.

14. API

calculate_priority(robot)

compare_priority(robot_a, robot_b)

calculate_direction_priority(corridor)

apply_priority_override()

clear_priority_override()

15. Logging

각 decision:

robot_id

resource_id

task_priority

waiting_score

deadlock_score

aging_score

final_score

decision

을 기록한다.

16. Acceptance Criteria

다음 조건을 만족해야 한다.

높은 task priority가 우선 처리됨

장시간 waiting robot이 starvation되지 않음

deadlock recovery priority 지원

deterministic ordering

priority inversion 처리

corridor direction priority 지원
