# Handoff — 작업 인계

다른 워크스페이스에서 이어서 작업하기 위한 문서다.

**어디까지 했는지**, **무엇을 결정해야 하는지**, **어떤 순서로 이어갈지**
세 가지를 담는다. 사양 자체는 여기 없다 — `00_INDEX.md`가 정본 선언 파일이고
이 문서는 그 아래에 있다.

작성 시점: Phase 7 완료 직후.

---

## 1. 지금 상태

| | |
|---|---|
| 저장소 | `https://github.com/zooskin/traffic-control` (private) |
| 브랜치 | `main` |
| 마지막 커밋 | `5cd49fc fix(reservation): satisfy clang on the corridor-passage scan` |
| 완료 Phase | **0 ~ 7** |
| 단위 테스트 | **761개, 100% 통과** |
| 검증 환경 | linux-gcc(13), linux-clang(18), windows-msvc(2022), ASan+UBSan, TSan |

### 완료한 Phase

| Phase | 내용 | 모듈 | 마감 문서 |
|---|---|---|---|
| 0 | Repository Setup | — | `27 §Week 1` |
| 1 | Core Domain | `core`, `domain`, `infrastructure` | `27 §Week 2` |
| 2 | Map & Graph | `map` | `27 §Week 3` |
| 3 | Route Planning (A\*) | `planning` | `27 §Week 4` |
| 4 | Robot State | `state` | `27 §Week 5` |
| 5 | Task Model | `task` | 이 문서 §1.2 |
| 6 | Reservation | `reservation` | 이 문서 §1.2 |
| 7 | Conflict Detection | `reservation` | 이 문서 §1.2 |

Phase 5~7은 마감 문서를 `27_PROJECT_KICKOFF.md`에 아직 쓰지 않았다. 커밋
메시지(`624d86e`, `a7c6ea3`)에 판단 근거가 들어 있으니 그것을 옮겨 쓰면 된다.

### 모듈 현황

```
core            5 헤더 / 1 소스     완료
domain         14 헤더 / 14 소스    완료
map             3 헤더 / 3 소스     완료
state           6 헤더 / 6 소스     완료
task            4 헤더 / 3 소스     완료
planning       10 헤더 / 9 소스     완료 (A* / Dijkstra만. WHCA*/PIBT/ECBS는 Phase 15 이후)
reservation     6 헤더 / 6 소스     완료 (예약 + 충돌 탐지)
infrastructure  1 헤더 / 1 소스     로깅만

priority        비어 있음           Phase 8
deadlock        비어 있음           Phase 9~10
controller      비어 있음           Phase 11
replanning      비어 있음           Phase 12
adapter         비어 있음           Phase 14
```

`src/CMakeLists.txt`의 주석이 어느 Phase에 어느 모듈이 들어오는지 표시한다.

### 1.2 Phase 5~7에서 내린 판단 (마감 문서 대신)

**Phase 5 — Task Model**

- 모든 lifecycle 호출이 `domain::with_status` / `domain::assign_to`를 거친다.
  Phase 1의 상태 기계가 유일한 권위로 남고, 어긋날 두 번째 전이표가 없다.
- Task 버전 카운터는 `TaskRecord`에 있고 `domain::Task`에는 없다. `24 §28`이
  Task를 버전 대상에 넣지 않았고 `23 §22`는 TaskManager에게 staleness 응답
  책임을 준다. 카운터를 소유자에 두면 §28의 목록을 건드리지 않고 답할 수 있다.
- 반복된 status 설정은 수용하되 버전을 올리지 않는다. 텔레메트리와 달리
  같은 status를 다시 받는 것은 새 정보가 아니다.
- `TaskChange`는 status뿐 아니라 robot도 담는다. `04 §16`은 assignment
  엔진이 실행 전에 task를 다른 robot으로 옮기는 것을 허용하는데, status만
  보고하면 그 재배정이 "아무 변화 없음"으로 보인다.
- Assignment seam(`ITaskAssignment`)에는 정책이 없다. 후보 robot 목록을
  호출자가 준다 — 그래야 `task`가 `state`나 `planning`을 참조하지 않는다.

**Phase 6 — Reservation**

- Capacity는 겹치는 **예약 수**가 아니라 동시 **로봇 수**로 센다. `06 §7`을
  문자 그대로 읽으면 과도하게 거부한다: 두 예약이 각각 요청과 겹치면서
  서로는 겹치지 않을 수 있다(하나가 나가고 다음이 들어옴).
- Capacity는 매번 `Map`에서 읽고 캐시하지 않는다. corridor 폭의 사본이
  맵과 어긋나는 순간이 곧 단일 차선에 로봇 두 대다.
- **Grant는 선점하지 않는다.** 우선순위 비교(`06 §16`)는 결정 시점에
  `request_all`에서 한다. 이미 행동하라고 통보한 grant를 회수하는 것은 이
  모듈이 막으려는 "주인 둘" 실패 그 자체다. 기아는 aging(`§17`)으로 답한다.
- `ReservationId`가 멱등성 키다(`§35`). `core/ids.h`에 `RequestId` 태그가
  없고 임의로 추가하지 않았다.

**Phase 7 — Conflict Detection**

- 저장하지 않고 결정하지 않는 순수 탐지기다. `24 §15`가 conflict는 판단을
  담지 않는다고 명시한다. 해소는 Phase 8이다.
- Head-on을 edge 수준과 corridor 수준에서 이중으로 잡는다. 긴 corridor에서
  두 로봇이 공유하는 edge가 하나도 없으면서 서로 지나갈 수 없는 경우가 있다.
- Conflict id는 내용에서 파생한다(type, resource, 정렬된 robot 쌍, overlap
  시작). 같은 장면이면 어떤 인스턴스에서 어떤 순서로 돌려도 같은 id가 나오고,
  A-vs-B와 B-vs-A가 문자 그대로 같은 id가 된다.
- 정의되지 않은 movement는 충돌로 취급한다. 맵이 서술한 적 없는 통과 방법은
  안전함을 보일 수 없다.

---

## 2. 결정해야 할 것

### 2.1 승인이 필요한 확정 결정 후보 (D-010 ~ D-012)

세 건 모두 **안전에 직결되는 영역의 사양 모호성**이다. 지금은 코드 주석과
커밋 메시지에만 근거가 있고 `00_INDEX.md`의 확정 결정으로 올라가 있지 않다.
CLAUDE.md가 임의 판단을 금지하므로 승인 후 등재해야 한다.

**D-010 후보 — 교차로에서는 conflict group이 결정하고 capacity는 적용하지 않는다**

`03 §10`은 Intersection에 `capacity`를 준다. `03 §11`과 `24 §16`은 conflict
group이 결정한다고 한다. capacity 1인 교차로에서 두 규칙이 어긋난다 —
호환되는 movement 두 개는 capacity로는 거부되고 group으로는 허용된다.

`06 §8`이 한 줄로 정리한다: *"Intersection: conflict group 기준으로 판단한다."*

→ 현재 구현은 교차로에 capacity 검사를 적용하지 않는다. 결과적으로
`domain::Intersection::capacity` 필드는 **아무 데서도 쓰이지 않는다.**
`03 §10`만 읽은 사람은 검사될 것으로 기대한다. 등재하거나, 필드를 제거하는
문서 변경안을 내야 한다.

**D-011 후보 — corridor 예약 연장 시 충돌 처리**

`06 §18~20`의 "R02 conflict → R02 WAIT / REPLAN"은 두 가지로 읽힌다:
*연장을 거부한다* 또는 *R02를 밀어낸다*.

어느 쪽이든 corridor 안에 멈춘 로봇은 그것을 점유하고 있다. 연장 거부는
테이블과 현장을 어긋나게 할 뿐이다.

→ 현재 구현: 자리가 있으면 연장한다. 없으면 경합하는 **pending** grant를
취소하고 재계획 대상으로 보고한다. 경합자가 이미 `active`이거나
커밋되었으면(`24 §30`) 거부한다 — 그때는 로봇 두 대가 물리적으로 관여한다.

**D-012 후보 — 예약 만료는 진입하지 않은 것만 자동 처리한다**

`06 §22`는 "GRANTED → EXPIRED"를 말하면서 같은 절에서 *"실제 safety 상태를
확인하지 않고 자동 release하면 안 된다"* 고 경고한다.

→ 현재 구현: `sweep()`은 로봇이 진입한 적 없는 **pending** grant만 만료시킨다.
창을 넘긴 `active` 예약은 `overrunning`으로 **보고만** 하고 건드리지 않으며,
확인을 마친 호출자를 위해 명시적 `expire(id, now)`를 따로 둔다.

### 2.2 사양의 실제 구멍 — `task_failed` 이벤트 부재

`24 §18`의 `TrafficEventType`에 `task_created` / `task_completed` /
`task_cancelled`는 있는데 **`task_failed`가 없다.**

`TaskStatus::failed`는 종료 상태이고 로봇을 놓아준다. 대응 이벤트가 없으면
task 실패가 이벤트 스트림에 나타나지 않는다. `robot_failed`로 대신하면
"로봇이 고장났다"는 다른 주장이 된다 — 로봇은 멀쩡하고 일이 실패한 것이다.

→ 현재 `task::to_event_type()`은 실패한 task에 대해 빈 값을 반환하고, 헤더가
그 이유를 적어 둔다. Data Model 변경이므로 ADR이 필요하다.

### 2.3 답이 없는 질문 — 목표 종료 Phase

Phase 2 무렵 "구현하는데 phase 몇까지 진행해야 하는건가?"라는 질문이 있었고
아직 답이 없다. 진행을 막지는 않는다 — Phase 8~13은 어느 시나리오에서도
필요하다. 다만 **Phase 14 이후는 목표에 따라 달라진다:**

| 목표 | 필요 Phase | 의미 |
|---|---|---|
| 알고리즘 연구 / 시뮬레이션만 | ~13 | 실물 로봇 없음, 시뮬레이터 안에서 검증 |
| 파일럿 (실물 소수) | ~14 | Adapter로 실제 fleet 연결 |
| 운영 투입 | ~17 | 성능·규모·하드닝 포함 |

권장은 **Phase 16**이다. 100~200대 목표에서 Scale Test 없이 운영에 넣는 것은
검증되지 않은 전제 위에 서는 일이다.

### 2.4 명시적 승인을 받지 않은 기존 결정

기록만 되어 있고 확인받지 않은 것들이다. 이어서 작업하기 전에 훑어보는 편이
좋다.

- **D-005** Robot(정체성)과 RobotStateSnapshot(관측)의 분리
- **D-006** NodeType은 `03 §5`의 9종을 따른다(`24 §7`의 6종이 아님)
- **D-007** `Edge.travel_time`은 optional override, 기본은 `length/speed_limit`
- **D-008** Heuristic 기본값은 Euclidean, Dijkstra는 별도 알고리즘 아님
- **D-009** 로봇이 보고한 것과 우리가 결정한 것을 섞지 않는다

### 2.5 작은 기술 항목

막지는 않지만 언젠가 정리해야 한다.

| 항목 | 내용 |
|---|---|
| `RequestId` 태그 | `06 §35`의 `request_id`를 예약 id와 별도로 모델링하려면 `core/ids.h`에 태그가 필요하다. 현재는 `ReservationId`를 멱등성 키로 쓴다 |
| `tags::TaskVersion` 위치 | `traffic::task::tags`에 있다. `domain/values.h`의 `StateVersion`/`MapVersion` 옆이 더 자연스럽다 |
| `make_task` 우선순위 범위 | `04 §15`의 0~100 검사가 `TaskManager`에 있고 도메인 팩토리에는 없다. 도메인에 두려면 `DomainError`에 값이 필요하다 |
| 시각 주입 방식의 불일치 | `AStarPlanner`는 `IClock&`를 주입받고 `StateManager`/`ReservationManager`는 `TimePoint`를 인자로 받는다. 둘 다 결정론적이지만 하나로 통일할지 정해야 한다 |
| `Intersection::capacity` 미사용 | D-010과 함께 처리 |
| `StrongId`의 문자열 저장 | 200대 이상에서 예약 테이블 hot path에 나타난다. 숫자 핸들 interning이 예상되는 최적화이고, 아키텍처 결정이므로 의도적으로 미룬 상태다 (`core/strong_id.h` 주석 참조) |

---

## 3. 이어서 할 일

### 3.1 남은 Phase 순서 (D-003 확정)

```
Phase 8   Priority              ← 다음
Phase 9   Deadlock Detection
Phase 10  Deadlock Recovery
Phase 11  Traffic Controller
Phase 12  Dynamic Replanning
Phase 13  Simulation
Phase 14  Integration
Phase 15  Performance
Phase 16  Scale Test
Phase 17  Production Hardening
```

### 3.2 각 Phase의 핵심 판단 지점

바로 다음 세 개는 서로 맞물린다. **Phase 8~11은 한 덩어리로 보는 편이 낫다** —
우선순위 없이는 충돌을 해소할 수 없고, 해소 없이는 교착이 발생하며, 컨트롤러
없이는 셋 다 아무것도 구동하지 않는다.

**Phase 8 — Priority** (`docs/09_PRIORITY_MANAGER.md`, `25 §15~16`)

- 이미 자리가 마련되어 있다: `reservation/reservation_policy.h`의
  `effective_priority()`가 기본 계수 0.0의 free function으로 있고, 헤더에
  *Phase 8의 `IPriorityManager`가 대체할 seam*이라고 적혀 있다. 여기부터
  시작하면 된다.
- 핵심 위험은 **기아**다. `09`가 waiting time 기반 aging을 요구한다. 낮은
  우선순위 로봇이 영원히 대기하지 않음을 보이는 테스트가 반드시 필요하다.
- Tie-break은 결정론적이어야 한다. `25 §16`의 순서를 따르고, 같은 입력이 같은
  승자를 내는 테스트를 넣는다.

**Phase 9~10 — Deadlock** (`docs/08_DEADLOCK_MANAGER.md`, `25`)

- Wait-for graph는 이미 절반 있다: `ReservationManager::wait_for_edges()`가
  `06 §26`의 체인을 만든다. 순환 탐지는 없다 — 그것이 Phase 9다.
- 복구 전략에서 **Waiting Bay가 핵심**이다. 우회로 없는 단일 차선 corridor에서
  head-on 교착의 유일한 탈출구다(`03 §13`). 기준 맵에 이미 `BAY-01`이 있고
  `CommandAction::go_to_waiting_bay`도 이미 있다.
- 순환이 곧 교착은 아니다. 사이클을 발견한 뒤 *확정*하는 절차가 따로 있어야
  하고, 그 타이머는 `StallPolicy`의 T1/T2와 정합해야 한다.

**Phase 11 — Traffic Controller** (`docs/10_TRAFFIC_CONTROLLER.md`, `23 §15~16`)

여기서 지금까지의 모든 조각이 연결된다. 미리 남겨 둔 seam들:

| Seam | 위치 | 할 일 |
|---|---|---|
| `planning::ITrafficConditions` | `planning/cost_model.h` | 예약 테이블로 congestion / expected wait를 채운다. `reservation`이 `planning`을 참조하면 안 되므로(같은 tier) **컨트롤러가 구현한다** |
| `IFleetPlanner` | `planning/route_planner.h` | 컨트롤러는 배치 인터페이스로만 부른다. 그래야 PIBT/ECBS 도입 시 컨트롤러를 다시 쓰지 않는다 |
| `StallPolicy` 사다리 | `state/stall_policy.h` | `StateManager`는 보고만 한다. T1/T2를 적용해 상태를 올리는 것은 컨트롤러다 |
| `RobotStateChange` | `state/state_change.h` | 반환값으로 나온다. `TrafficEvent`로 만들지 로그로 남길지는 컨트롤러가 정한다 |
| `PlanningRecord` | `planning/planning_record.h` | `05 §23`의 로그를 실제로 기록하는 곳 |
| `ReservationTable::capacity_of` / `owners_of` / `next_available` | `reservation/reservation_table.h` | 위 `ITrafficConditions` 구현에 쓰라고 public으로 열어 둔 것. 주의: `reservation_manager.h` §Phase 11 주석은 이들이 매니저에 있다고 읽히지만 실제로는 **테이블**에 있다. 매니저가 여는 것은 `active_reservations()`다 |
| `06 §31` / `25 §23` 일관성 검사 | `ReservationRecord::last_transition_at` | 예약과 로봇 상태의 대조. `reservation`이 `state`를 참조할 수 없으므로 컨트롤러가 한다 |

**Phase 12 — Dynamic Replanning** (`docs/07_DYNAMIC_REPLANNING.md`)

- `AStarPlanner::replan_route()`와 `RouteStabilityPolicy`가 이미 있다.
  개선 임계값과 hold time 두 브레이크가 구현되어 있고 테스트도 있다.
- 남은 것은 *언제* 재계획을 트리거하는가다 — `05 §14`의 7가지 상황.

**Phase 13 — Simulation** (`docs/02_SIMULATOR.md`, `26_TEST_SCENARIOS.md`)

- 착수 시 Week 4 스케치에 있던 "최소 시뮬레이션 루프"가 여기로 미뤄져 있다.
- `SimulationClock`은 Phase 0부터 준비되어 있다. `TrafficClock`에 `now()`가
  없는 것이 이걸 위해서다.
- `26`의 SC-001~ 시나리오 카탈로그가 목록이다.

**Phase 15~16 — Performance / Scale**

- `15_ALGORITHM_BENCHMARK.md` 기준으로 WHCA\*/PIBT/ECBS 도입 여부를 결정한다.
  `IRoutePlanner` / `IFleetPlanner` 뒤에 숨겨 뒀으므로 교체 비용은 낮다.
- **여기서 처음으로 진짜 숫자가 나온다.** 지금 코드의 여러 기본값이
  "측정 대상이 없어서 정하지 않은" 상태로 남아 있다:
  - `CostWeights` — distance 1.0, 나머지 전부 0
  - `StallPolicy::blocked_timeout` (T2) — 비어 있음
  - `RouteStabilityPolicy::minimum_improvement` — `05 §16`의 예시값 0.10
  - `ConflictDetector`의 severity 임계 — 2초 / 10초
  - `AStarConfig::max_expansions` — 0(무제한)
- KPI 목표(`CLAUDE.md`): Controller P50 < 10ms, P95 < 50ms, P99 < 100ms.

### 3.3 작업 방식 (이 저장소에서 검증된 것)

**로컬에 C++ 툴체인이 없다. CI가 유일한 빌드 검증 수단이다.**

- CI 1회 ≈ 8~10분. 7개 job: format, build×3, sanitizer×2, clang-tidy.
- 그래서 **Phase당 3배치**로 나눠 커밋한다. 컴파일 오류가 나도 범위가
  좁아서 원인을 찾기 쉽다.
- 또는 **에이전트 병렬화**: Phase 5·6·7은 서로 독립하도록 경계를 잘라
  에이전트 셋에 동시에 맡기고 CI를 한 번만 태웠다(수정 1회 포함 총 2회).
  이 방식이 잘 통했다. 경계를 자를 때 지킨 규칙:
  - 각자 자기 모듈 밖 파일을 건드리지 않는다
  - 공유 `CMakeLists.txt`는 아무도 손대지 않고 마지막에 사람이 묶는다
  - `git`과 `scripts/format.sh`는 아무도 실행하지 않는다

**커밋 전 정적 사전 검증** (컴파일 못 하므로):

```bash
bash scripts/format.sh          # 적용
bash scripts/format.sh --check  # 확인
```

그 외에 사람이 직접 보는 것: 헤더/구현 시그니처 불일치, 누락된 `#include`,
구조체 멤버 기본 초기화(GCC `-Wmissing-field-initializers`가 fatal),
`std::optional` 접근 앞의 `has_value()` 가드
(clang-tidy `bugprone-unchecked-optional-access`는 `*opt`와 `opt.value()`
**둘 다** 잡는다).

**이미 겪은 함정들** — 다시 밟지 않도록:

- `bugprone-unchecked-optional-access`는 가드와 사용 사이에 루프가 끼면
  참조를 추적하지 못한다. 가드 직후 값을 꺼내라.
- defaulted `operator==`를 `std::map` 키에 붙이면 clang이
  `-Wunused-function`으로 **거부**한다(경고 아님). `<=>`만 남긴다.
- 클래스 안에 `map`이라는 멤버를 두면 클래스 스코프에서 `map::MapData`가
  네임스페이스가 아닌 멤버로 해석되어 컴파일이 깨진다.
- `readability-redundant-member-init`(clang-tidy)와
  `-Wmissing-field-initializers`(GCC)가 정면 충돌한다. 컴파일러가 이겼고
  해당 검사는 `.clang-tidy`에서 껐다.
- `cppcoreguidelines-avoid-const-or-ref-data-members`는 CLAUDE.md의 DI 규칙과
  충돌한다. 껐고 이유를 `.clang-tidy`에 적었다.

### 3.4 새 워크스페이스에서 시작하기

```bash
git clone https://github.com/zooskin/traffic-control.git
cd traffic-control
```

`gh` CLI 인증이 필요하다(CI 결과 확인용). 이 환경에서는 winget이
비대화형에서 죽어서 공식 릴리스 zip을 `%LOCALAPPDATA%\Programs\gh`에 직접
풀고, 비대화형 셸이 `.bashrc`를 읽지 않으므로 `~/bin/gh`에 shim을 뒀다.
새 환경에서는 보통 설치로 충분할 것이다.

읽는 순서:

```
CLAUDE.md  →  docs/00_INDEX.md  →  이 문서  →  해당 Phase의 사양  →  docs/20
```

커밋 메시지 끝에 붙이는 attribution 두 줄은 세션마다 다르다. 새 세션의
지시를 따른다.

---

## 4. 유지되고 있는 원칙 (깨뜨리지 말 것)

이어서 작업하는 사람이 가장 쉽게 무너뜨릴 수 있는 것들이다.

**Corridor는 Edge가 아니다.** 여러 Edge가 하나의 `resource_id`를 공유하고,
한 대만 통과시키는 주체는 Edge가 아니라 Resource다. Edge로 모델링하면 두
로봇이 같은 통로의 양쪽 끝을 각각 배정받는다. (`00_MASTER_PLAN §4.2`)

**교차로는 capacity만으로 부족하다.** 교차하지 않는 두 movement는 동시 통과가
가능하고 교차하는 둘은 불가능한데, 점유 수로는 둘 중 하나밖에 표현할 수 없다.

**시간을 직접 읽지 않는다.** `TrafficClock`에는 `now()`가 없다. 이것이
결정성(NFR-003)과 재현 가능한 시뮬레이션의 토대다.

**순회 순서가 결과에 영향을 주는 곳에 `unordered_*`를 쓰지 않는다.**

**Traffic Controller는 Safety Controller가 아니다.** Emergency Stop /
Protective Stop / Safety Zone / Safety Interlock은 별도 시스템의 책임이다.
`CommandAction`에 emergency stop이 없고, 이름이 파싱되지 않는다는 테스트가
있다 — 나중에 추가하려면 의도적인 행위가 되도록.

**로봇은 자기 교통 상태를 명명할 수 없다.** `reserving` / `waiting` /
`replanning`은 교통 제어가 내린 결정이다. (D-009)

**Grant는 선점하지 않는다.** 이미 행동하라고 통보한 예약을 회수하는 것은
"주인 둘" 실패다.
