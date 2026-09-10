# Project Kickoff Plan

착수 시점의 계획과 근거를 기록한다. 진행하면서 갱신한다.

---

## 1. 현재 상태

| | |
|---|---|
| 사양 문서 | 27개 + 인덱스, `docs/` |
| 코드 | Phase 0 스캐폴딩만 존재 |
| 저장소 | git 초기화 완료 |
| 로컬 툴체인 | **C++ 컴파일러·CMake 미설치** (§6 참조) |

---

## 2. 전략: Walking Skeleton 우선

27개 문서는 완결된 설계를 담고 있지만, 아직 **한 줄도 실행된 적이 없다.**
문서상의 설계 가정 — corridor를 별도 Resource로 다루는 것, 예약 기반 제어,
일시정지와 고장의 분리 — 은 모두 합리적으로 보이지만 검증된 바 없다.

따라서 모듈을 완성도 순으로 하나씩 쌓아 올리는 대신, **가장 얇은 수직
슬라이스를 먼저 관통시킨다.**

### M0 — Walking Skeleton (목표 4주)

> 로봇 10대가 단일 corridor 맵에서 A\* 경로를 받아 이동하고,
> 동일 seed로 재실행하면 동일한 결과가 나오는 최소 시뮬레이션 루프.

M0에는 **예약도, 우선순위도, 교착 관리도 없다.** 당연히 충돌이 발생한다.
그것이 목적이다.

M0을 먼저 만드는 이유:

1. **설계 가정을 4주 안에 실물로 검증한다.** 문서를 더 쓰는 것으로는
   알 수 없는 것들이 있다.
2. **측정 가능한 진전이 생긴다.** 이후 Reservation을 넣었을 때
   "충돌 N건 → 0건"으로 효과를 숫자로 말할 수 있다. 기준선 없이 만든
   교통 제어는 좋아졌는지 알 수 없다.
3. **KPI 수집 파이프라인을 초기에 확보한다.** `00_MASTER_PLAN` §9의 지표를
   나중에 붙이면 이미 늦다.
4. **결정성(NFR-003)을 처음부터 강제한다.** `IClock`과 seed 주입을 나중에
   끼워 넣으면 전체 코드를 다시 손대야 한다.

---

## 3. 마일스톤

| | 내용 | 완료 판정 |
|---|---|---|
| **M0** | Walking Skeleton (Phase 0~3) | 10대 결정론적 시뮬레이션 재현 |
| **M1** | Core + Map 완성 (Phase 1~2 심화) | 맵 로드·검증, 도메인 불변식 테스트 |
| **M2** | A\* + Reservation (Phase 3, 6) | 비호환 예약 동시 GRANT 0건 |
| **M3** | Multi-Robot Baseline (Phase 7~8) | 50대·100대 충돌 0건 |
| **M4** | Deadlock (Phase 9~10) | SC-019~021 통과 |
| **M5** | Traffic Controller 통합 (Phase 11) | 단일 결정 루프 동작 |
| **M6** | Dynamic Replanning (Phase 12) | SC-011~013 통과 |
| **M7** | Simulator 완성 (Phase 13) | 26_TEST_SCENARIOS 전 시나리오 자동 실행 |
| **M8** | 200대 Stress (Phase 15~16) | P95 < 50ms, 24h soak PASS |
| **M9** | Shadow Mode | 실측과 모델의 차이 정량화 |
| **M10** | Limited Production | 10 → 30 → 50 → 100 → 200 단계 적용 |

Production Gate는 `docs/archive/18_IMPLEMENTATION_PLAN.md` §30의 조건을
그대로 적용한다 (Collision 0, Safety violation 0, deterministic replay PASS,
24h soak PASS 등).

---

## 4. 4주 상세 계획 (M0)

### Week 0 — 문서 정합화 ✅ 완료

- [x] `docs/` 재구성, 대체 문서 `docs/archive/` 분리
- [x] 마크다운 escape 정리 (`scripts/normalize_docs.sh`)
- [x] `docs/00_INDEX.md` — 정본 선언 및 확정 결정 D-001~D-004
- [x] `CLAUDE.md` — 에이전트 상시 참조 규칙
- [x] git 초기화 및 baseline 커밋

### Week 1 — Phase 0: Repository Setup

- [x] CMake 골격, C++20, warnings-as-error
- [x] FetchContent로 GoogleTest / spdlog / fmt
- [x] CMakePresets (dev / release / asan / tsan)
- [x] `.clang-format`, `.clang-tidy`, `.gitignore`
- [x] GitHub Actions CI (Linux GCC/Clang + Windows MSVC + sanitizer job)
- [x] Strong ID 타입 + `IClock` / `SystemClock` / `SimulationClock`
- [x] 로깅 초기화
- [x] 정적 검증 (프리셋·워크플로 파싱, 경로 정합성, 코드 리뷰)
- [x] **CI 빌드 통과** — 7개 잡 전부 green

완료 조건 (22 Phase 0) 충족 결과:

| 조건 | 결과 |
|---|---|
| Build 성공 | GCC 13 / Clang 18 / MSVC 2022 3종 통과 |
| Unit Test 실행 | **32/32 통과** (5개 환경 전부) |
| Logging 동작 | 시뮬레이터 스모크 테스트에서 결정 레코드 출력 확인 |
| CI 동작 | format / build×3 / sanitizers×2 / clang-tidy |

Sanitizer(ASan+UBSan, TSan)에서도 32/32 통과했다.

**Phase 0 완료.**

### Week 2 — Phase 1: Core Domain ✅ 완료

`24_DOMAIN_MODEL` 기준. 단위 테스트 **174개, 5개 환경 전부 통과**.

3개 배치로 나눠 각각 CI 검증했다. 로컬 컴파일러가 없으므로 한 번에 밀어넣으면
오류 위치를 좁히는 데 왕복이 더 든다.

| 배치 | 내용 | CI |
|---|---|---|
| 1 | `Result<T,E>`, 값 객체(Position/Velocity/Priority/Version/TimeWindow), DomainError | 1회 통과 |
| 2 | 상태 기계 3종 (RobotState / TaskStatus / ReservationState) | 1회 통과 |
| 3 | 엔티티 (Robot, Task, Node, Edge, Route, Reservation, Conflict, TrafficEvent, TrafficDecision) | 2회 |

**결정 사항**

- `Result<T,E>` — `20 §18`은 `std::expected`를 예로 들지만 그건 C++23이고
  `19 §10`이 C++20으로 고정한다. 같은 절이 허용하는 project-defined
  result type으로 구현했다. 인터페이스를 `std::expected`와 맞춰뒀으므로
  나중에 C++23으로 옮기면 대부분 이름만 바뀐다.
- `TimeWindow`는 반개구간 `[start, end)`. 맞닿은 구간이 겹치지 않아야
  한 로봇이 다음 로봇에게 corridor를 넘길 때 인위적인 간격이 생기지 않는다.
- 상태 기계는 if 연쇄가 아니라 **전이 테이블**. 정책 전체가 한눈에 보이고,
  `static_assert`가 enum과 테이블 정렬을 컴파일 타임에 묶는다.
- 엔티티는 전부 `Result`를 반환하는 팩토리로만 생성. `24 §34`의 검증을
  생성자 우회로 건너뛸 수 없다.

**사양에 없지만 도출한 규칙 2가지**

- `failed -> moving` 거부. 복구는 먼저 자원을 반납해야 하고, 건너뛰면
  예약 테이블에 주인 없는 항목이 남는다. 복구는 `idle`을 경유한다.
- `active -> cancelled` 거부. 로봇이 이미 자원 안에 있는데 취소하면
  점유 사실을 기록한 유일한 근거가 사라진다. 반납하거나 만료된다.

**연기한 것** — Phase 경계에 맞춤: Corridor/Intersection → Phase 2,
PlanningRequest/Result → Phase 3, Deadlock/HumanBlockage → Phase 9.

### Week 3 — Phase 2: Map & Graph ✅ 완료

`03_MAP_GRAPH` 기준. 단위 테스트 **254개, 5개 환경 전부 통과**.

| 배치 | 내용 | CI |
|---|---|---|
| 1 | Corridor / Intersection / Movement / ConflictGroup / WaitingBay, NodeType 정렬 | 1회 통과 |
| 2 | Map 컨테이너 + 조회 API + 검증기 | 1회 통과 |
| 3 | JSON 로드/저장 + 기준 맵 | 1회 통과 |

**`03 §24` Acceptance Criteria 대비**

| 항목 | 상태 |
|---|---|
| Map Load / Save / Version | ✅ |
| Node / Edge / Neighbor / Resource 조회 | ✅ |
| Reachability | ✅ |
| Map Validation | ✅ 16종 검사 |
| Corridor / Intersection / Conflict Group / Waiting Bay 정의 | ✅ |

`§25` 요구 테스트 11종 전부 존재한다.

**설계 판단**

- **Corridor는 Edge의 속성이 아니라 독립 Resource다.** 여러 Edge가 하나의
  `resource_id`를 공유하고, 한 대만 통과시키는 주체는 Edge가 아니라 Corridor다.
  이것이 `00_MASTER_PLAN §4.2`의 요구이며 여기서 타협하면 프로젝트 전제가
  무너진다. 기준 맵에서 `E-C1`/`E-C2`/`E-C3`가 `CORRIDOR-01` 하나를 공유한다.
- **Intersection은 capacity만으로 부족하다.** 교차하지 않는 두 movement는
  동시 통과가 가능하고 교차하는 둘은 불가능한데, 점유 수만으로는 둘 중
  하나밖에 표현하지 못한다. ConflictGroup이 그 차이를 담는다.
- **`traversable_edges`와 `incident_edges`를 분리했다.** incident를 확장하는
  플래너는 일방통행 corridor를 역주행하는 경로를 만들고 예약이 거부될 때에야
  알게 된다.
- **인접 목록은 Edge 순서로 구성한다.** 해시 순서에 의존하면 같은 입력에서
  같은 경로가 나오지 않는다 (NFR-003).
- **검증은 첫 오류에서 멈추지 않는다.** 현장 맵을 한 번에 하나씩 고치는 것은
  작업 흐름이 아니다. 연결성 문제는 error가 아니라 **warning** — 도달 불가능한
  정비용 지선 때문에 현장 전체를 막을 이유가 없다.

**기준 맵** — `configs/maps/reference_map.json`

`17_SIMULATION_SCENARIOS §2`의 구조를 실제 파일로 만들었다. Corridor는
의도적으로 단일 차선 양방향이고(head-on 케이스), 교차로 옆에 Waiting Bay가
있다(우회로 없는 corridor에서 head-on 교착의 유일한 탈출구).

**기록한 결정** — D-006 (NodeType은 `03` 어휘), D-007 (travel_time은 파생값).

### Week 4 — Phase 3: Route Planning (A\*) ✅ 완료

3개 배치, 단위 테스트 254 → **407개**. 5개 환경(linux-gcc, linux-clang,
windows-msvc, asan, tsan) 전부 통과.

| 배치 | 커밋 | 내용 |
|---|---|---|
| 1 | `a016b45` | RouteRequest/Response, PlanningRequest/Result, Constraints, Cost Model |
| 2 | `03d7e47` | Heuristic, Route Validation, Route Stability |
| 3 | `380008e` | AStarPlanner, SequentialFleetPlanner, Planning Record |

**`05 §24` Acceptance Criteria**

| 항목 | 상태 |
|---|---|
| A\* 구현 | ✅ `AStarPlanner` |
| Dijkstra fallback | ✅ `ZeroHeuristic` 주입 |
| Dynamic edge cost | ✅ `edge_cost` + `CostWeights` |
| Congestion cost | ✅ `ITrafficConditions::congestion` |
| Expected waiting cost | ✅ `ITrafficConditions::expected_wait` |
| Blocked edge 지원 | ✅ Edge / Node / Resource 3종 |
| Route validation | ✅ `validate_route`, 11종 defect |
| Replanning | ✅ `replan_route` + 안정화 정책 |
| Deterministic result | ✅ tie-break 3단 + 결정성 테스트 |
| Performance benchmark | ⏳ Phase 15 (`max_expansions`로 상한만 확보) |
| Planning logging | ✅ `PlanningRecord` (§23의 10개 필드) |

`§25`가 요구한 테스트 12종 중 11종 구현. `test_route_hysteresis`까지 포함하며
`test_dijkstra`는 A\*와의 비용 일치 검증으로 구현했다.

**두 계층의 인터페이스**

`05 §20`은 `plan_route(request)`(단일 로봇)를, `24 §20~21`은
`PlanningRequest`/`PlanningResult`(배치)를 정의한다. 충돌이 아니라 호출자가
다르다. `IRoutePlanner`와 `IFleetPlanner`로 나누고
`SequentialFleetPlanner`가 다리를 놓는다.

이유: PIBT/ECBS는 배치를 동시에 푸는 알고리즘이라 단일 요청의 루프로
표현할 수 없다. Traffic Controller가 단일 인터페이스를 직접 호출하면
알고리즘 교체 시 Controller를 다시 써야 하고, 그건 CLAUDE.md의 Algorithm
Isolation이 금지하는 상태다.

**A\*가 책임지는 세 가지**

*주행 가능한 경로만 반환한다.* 확장은 `incident_edges`가 아니라
`traversable_edges`로 한다. 차이는 일방통행 corridor를 역주행하는 경로다 —
연결되어 있고, 그럴듯하고, 첫 예약에서 거부된다.

*같은 입력이면 같은 경로다*(`§22`). 깨질 수 있는 지점 셋을 각각 고정했다:
인접 목록은 Edge 순서, open set 동점은 node_id, 같은 비용의 경로는 edge_id.
unordered 컨테이너를 순회하는 곳이 없다. 대칭 테스트 맵은 양쪽 경로가 정확히
40으로 같아서 tie-breaker 외에는 아무것도 결과를 결정하지 않는다.

*지금이 언제인지 모른다.* 모든 시각은 요청으로 들어온다. 주입된 clock은
경로에 시각을 찍고 탐색 시간을 재는 데만 쓴다. `SimulationClock` 아래서
그 측정은 0이고, 그게 맞다 — 빠른 기계와 느린 기계에서 시나리오가 똑같이
재생되어야 한다.

**Heuristic — `05 §5`의 내부 충돌**

`§5`는 Manhattan을 기본으로 제시하면서 같은 절에서 admissible을 요구한다.
일반 그래프에서 두 요구는 양립하지 않는다. Euclidean을 기본으로 하고
Manhattan은 격자 맵용으로 남겼다. 근거는 D-008.

과대평가하는 heuristic은 A\*를 실패시키지 않는다. 최단이 아닌 경로를 아무
신호 없이 반환한다. 그래서 `min_cost_per_metre`가 실제 Edge 비용을 절대
넘지 않는다는 것을 테스트로 직접 단언한다.

전제 하나가 남아 있다. 확장이 끝난 노드를 다시 열지 않는 최적화는
heuristic이 admissible한 것만으로는 부족하고 **consistent**해야 한다.
거리 기반 heuristic은 모든 Edge의 `length`가 양 끝점 사이 직선거리 이상일 때
consistent하다. `geometry_supports_distance_heuristic`이 이를 확인하고,
실패하는 맵은 `ZeroHeuristic`으로 계획한다.

**Cost weight 기본값**

`distance = 1.0`, 나머지 전부 0. `§6`이 weight를 configuration이라 했고
`15_ALGORITHM_BENCHMARK`는 Phase 15다. 측정 대상이 없는 상태에서 만든
숫자는 나중의 튜닝이 상대해야 할 근거 없는 값이 된다.

**Congestion / Waiting이 Reservation을 참조하지 않는 이유**

두 값 모두 예약 테이블에서 나오는데 Reservation은 Phase 6이고 `12 §16`의
의존 순서에서 planning보다 위다. `ITrafficConditions` 인터페이스로 받아서
방향을 뒤집지 않는다. `§19`가 예고한 reservation-aware planning이 도착할
자리도 여기다.

두 값은 요청의 `current_time`에 한 번만 읽는다(`§10`의 정의 그대로).
탐색 중 Edge 비용이 고정되므로 A\*의 최적성이 유지되고, 같은 입력이 같은
경로를 낳는다.

**Route Stability — 브레이크가 둘인 이유**(`§16~17`)

개선 임계값은 반올림 수준의 변화로 경로가 바뀌는 것을 막는다.
Hold time은 진동을 막는다 — 그리고 진동을 볼 수 있는 것은 hold time뿐이다.
A → B → A 순환의 매 단계는 그 시점에서 진짜 개선이기 때문이다.

현재 경로가 주행 불가능해지면 둘 다 우회한다. 폐쇄된 corridor를 지나는
경로보다 대안이 10% 싼지 묻는 것은 잘못된 질문이다.

**`max_expansions`는 시간이 아니라 작업량 상한**

탐색 중간에 실제 시계를 읽으면 같은 시나리오의 두 실행이 갈라진다. 고정된
맵에서 확장 수 상한이 P99 지연 KPI를 탐색에 강제할 수 있는 형태다.
`NO_ROUTE`와 구분해서 보고한다 — 경로가 존재할 수도 있고, 다만 planner가
충분히 멀리 볼 수 없었을 뿐이다.

**Logging은 데이터로 남긴다**(`§23`)

`PlanningRecord`는 포맷 문자열이 아니라 구조체다. planning이 infrastructure에
의존하면 안 되고(CLAUDE.md), "옛 경로를 유지한 이유를 기록했는가"는 테스트가
되지만 "올바른 줄을 출력했는가"는 되지 않는다.

**테스트가 잡은 버그**

Batch 2 테스트가 `is_traversable_from`이 비활성 Edge에도 false를 반환한다는
사실을 드러냈다. 검증기가 폐쇄된 corridor를 전부 일방통행 위반으로
보고하고 있었다 — 읽는 사람을 있지도 않은 방향 버그로 보내는 진단이다.
Batch 3에서 고쳤다.

**기록한 결정** — D-008 (Heuristic 기본값은 Euclidean, Dijkstra는 별도
알고리즘이 아님).

**남긴 것** — 착수 시점의 Week 4 스케치에는 "최소 시뮬레이션 루프(10대,
단일 corridor)"와 "KPI 수집"이 있었다. 둘 다 확정 Phase 순서(D-003)에서는
Phase 13(Simulation)과 Phase 15(Performance)에 속한다. 로봇을 움직이려면
Robot State(Phase 4)와 Reservation(Phase 6)이 먼저 있어야 하므로 지금
만들면 두 번 만들게 된다. `05 §24`의 Performance benchmark 항목도 같은
이유로 Phase 15에 남긴다.

### Week 5 — Phase 4: Robot State ✅ 완료

3개 배치, 단위 테스트 407 → **525개**. 5개 환경 전부 통과.

| 배치 | 커밋 | 내용 |
|---|---|---|
| 1 | `03b3032` | RobotStateUpdate, WaitingReason, StallPolicy |
| 2 | `6d3ed6f` | StateManager |
| 3 | `a539f58` | RobotCommand, RobotStateChange, 명령 추적 |

**`22` Phase 3 완료 조건**

| 항목 | 상태 |
|---|---|
| State Update | ✅ `StateManager::apply` |
| State Query | ✅ snapshot / snapshots / robots_in_state |
| State Version | ✅ fleet-wide `StateVersion` |
| State Transition Test | ✅ Phase 1 전이표 + 관리자 경유 검증 |
| Stale State Detection | ✅ `stale_robots` |

`04 §24`의 나머지 항목 중 Robot 쪽은 전부 구현했다. Task lifecycle과 Task
assignment interface는 Phase 5, Reservation association은 Phase 6이다.

**로봇은 자기 교통 상태를 명명할 수 없다** — D-009

D-001의 9개 상태 중 `reserving` / `waiting` / `replanning`은 교통 제어가
내린 결정이지 Robot이 관측할 수 있는 사실이 아니다. Robot이 `waiting`을
보고한다는 것은 우리가 무언가를 승인했다고 주장하는 것이고, 이를 믿으면
발급된 적 없는 예약이 컨트롤러 안에 생긴다.

`StateManager`에 문이 둘인 이유가 이것이다. `apply`는 Robot이 자기에 대해
말한 것을, `assign_state`는 교통 제어가 Robot에 대해 결정한 것을 받는다.
세 상태는 두 번째 문으로만 들어온다.

거부하되 고치지 않는다. 조용히 다른 값으로 바꾸면 말이 안 되는 값을 보내는
fleet이 숨는다.

**관측과 결론을 한 구조체에 넣지 않는다** — D-009

`04 §7`은 WAITING 상태의 Robot이 `waiting_since` / `waiting_resource` /
`waiting_reason`을 저장한다고 한다. 그러나 Snapshot은 *관측된 것*이고
waiting reason은 우리가 *결론 내린 것*이다. 함께 넣으면

- 동일한 관측 두 개가 우리가 내린 판단 때문에 다르게 비교된다
  (`operator==`가 defaulted이고 version 비교에 쓰인다),
- fleet이 말한 것과 우리가 추론한 것을 사후에 구분할 수 없다.

`state::WaitingContext`로 분리했다.

**`waiting`은 이유 없이 설정할 수 없고, 이유는 `waiting`에만 붙는다**

양방향 모두 거부한다. 이유 없는 hold는 나중에 설명할 수 없고, 재개한 Robot에
남은 이유는 현재 상태로 읽힌다. 조용히 정리하면 호출자가 잘못 알고 있었다는
사실이 사라진다. Robot이 다시 움직인다고 보고하면 이유는 즉시 지운다.

**State Version은 fleet 단위다**

`23 §2.4`가 묻는 것은 "이 계획이 이미 움직인 세계를 상대로 계산되었는가"이고,
그건 fleet에 대한 하나의 질문이다. 그래서 카운터는 하나다. 각 Snapshot에는
마지막으로 갱신된 시점의 fleet version을 찍어서 "계획 이후 어떤 Robot이
바뀌었는가"도 답할 수 있게 했다. 거부된 갱신과 조회는 version을 움직이지
않는다.

**정지 판정 사다리** — `04 §9` + `25 §17~18`

```
0  .. T1     TEMPORARILY_STOPPED
T1 .. T2     BLOCKED
>  T2        FAILURE / RECOVERY
```

T1은 `04 §9`가 준 유일한 숫자인 5초를 기본값으로 넣었다. **T2는 기본값이
없다.** `failed`로 올리면 Robot의 자원이 해제되고 Robot이 서 있는 자리로
교통이 흘러간다. 아무도 측정하지 않은 숫자로 할 일이 아니다 — 그런 기본값은
사고 중에 발견된다.

사다리는 나쁜 쪽부터 검사한다. 반대로 하면 한 시간 갇힌 Robot이 영원히
blocked로만 보고되고 절대 올라가지 않는다.

**진행은 속도가 아니라 이동 거리로 잰다.** 팔레트에 밀착해 미는 Robot은
motion을 보고하면서 아무 데도 가지 않는다. Progress mark는 실제로 움직였을
때만 전진한다 — 매 관측마다 전진시키면 그 Robot이 방금 출발한 것처럼 보이는
일이 100ms마다 영원히 반복된다.

**StateManager는 보고하고 판단하지 않는다.** Robot이 얼마나 오래 못 움직였는지
알지만, 그래서 blocked라고 결정하지는 않는다. 사다리는 Traffic Controller가
적용한다. Staleness도 같다 — `04 §21`이 COMMUNICATION_LOST를 Robot 상태와
별개의 system event로 두므로, 보고만 하고 아무도 `unknown`으로 옮기지 않는다.

**Emergency Stop은 없고 앞으로도 없다**

`04 §23`이 Emergency Stop을 일반 Traffic Command와 분리하라고 하고 CLAUDE.md는
아예 다른 시스템의 책임으로 둔다. `CommandAction`에 해당 값이 없으며, 이름이
파싱되지 않는다는 테스트를 두었다 — 나중에 추가하려면 의도적인 행위가 되도록.

Emergency Stop을 낼 수 있는 Traffic Controller는 사람들이 안전을 위해
의지하기 시작하는 Traffic Controller이고, 이 시스템은 그 기준으로 만들어지지
않았다. 판단이 낡을 수 있는 상태, 시간 초과할 수 있는 planner, 침묵할 수 있는
fleet에 의존한다.

다만 안전 시스템이 Robot을 멈춘 사실은 `WaitingReason::safety_stop`으로
기록한다. 우리 타이머가 그 정지를 교통 문제로 오인하지 않게 하기 위해서다.

**상태 변경은 발행하지 않고 반환한다**

`apply`와 `assign_state`가 변경이 있었을 때 `RobotStateChange`를, 상태가
움직이지 않았을 때 빈 값을 돌려준다(대부분의 텔레메트리가 후자다). Listener
registry를 두면 listener 실행 순서가 시스템 동작의 일부가 되어버린다.
TrafficEvent로 만들지, 로그로 남길지, 버릴지는 호출자가 정한다.

`is_stall_onset`은 진행하던 Robot이 멈춘 전이만 센다. waiting에서 blocked로
간 Robot은 방금 막힌 것이 아니며, 세면 아무것도 변하지 않는 동안 경보가 계속
올라간다. 고장은 대응이 다른 별개의 경보이므로 섞지 않는다.

**명령 추적** — `04 §22`

`last_command`와 `last_ack_command`의 짝이 "아직 시작하지 않았다"와 "못
들었다"를 구분한다. 밖에서 보면 같은 그림이고 대응은 반대다. 보낸 적이 없는
것은 무시된 것이 아니므로, 명령받은 적 없는 Robot에는 미확인 명령이 없다.

**테스트가 잡은 것 하나, 도구가 잡은 것 하나**

`04 §5`의 11개 status(IDLE/ASSIGNED/PLANNING/MOVING/WAITING/BLOCKED/
REPLANNING/RECOVERY/ARRIVED/FAILED/EMERGENCY_STOP)는 D-001의 9개가 대체한다.
`ASSIGNED`/`PLANNING`/`ARRIVED`는 Task 상태이지 Robot의 교통 상태가 아니므로
Phase 5의 `TaskStatus`가 담는다.

clang-tidy의 `bugprone-unchecked-optional-access`가 `Result<void, E>::error()`
에 도달했고, 지적이 옳았다. `assert`는 NDEBUG에서 사라지므로 릴리스에서
전제 위반이 undefined behaviour였다 — 다른 것을 망가뜨리고 몇 시간 뒤 무관한
곳에서 진단되는 종류의 결함이다. 기본 템플릿과 같은 `std::variant`로 바꿨다.
저장 방식이 하나로 줄고, 잘못된 alternative 접근은 noexcept 함수 안에서
terminate로 끝난다. 결함 지점에서의 crash가 떨어진 곳에서의 조용한 손상보다
낫다.

**기록한 결정** — D-009 (보고한 것과 결정한 것의 분리, `04 §5`의 status 대체).

---

## 5. 초기 위험

| 위험 | 영향 | 대응 |
|---|---|---|
| `IClock`/seed 주입 누락 | 결정성(NFR-003) 확보 불가, 전면 재작업 | Phase 0에서 먼저 넣는다 (완료) |
| Corridor를 일반 Edge로 축약 | 좁은 통로 제어 불가 — 프로젝트 전제 붕괴 | Phase 2에서 Resource로 분리 고정 |
| 고급 MAPF 조기 도입 | 복잡도 폭증, 기준선 없음 | D-003으로 Phase 15 이후로 고정 |
| 기준선 없는 최적화 | 개선 여부 판단 불가 | M0에서 KPI 파이프라인 확보 |
| 문서-코드 괴리 | 에이전트가 낡은 사양으로 구현 | `00_INDEX.md` 정본 선언 + 아카이브 |
| MSVC의 TSan 미지원 | data race 미검출 | Linux CI에 sanitizer job 분리 |

---

## 6. 미결 사항: 빌드 검증

착수 시점에 개발 머신에 C++ 툴체인이 없다.

```
git            2.55.0     OK
node           24.20.0    OK
cmake          없음
컴파일러        없음 (MSVC / GCC / Clang 모두)
clang-format   없음
python         없음 (Store 스텁만 존재)
```

**결정: 로컬에 툴체인을 설치하지 않고 CI에서 검증한다.**

따라서 `.github/workflows/ci.yml` 이 이 프로젝트를 컴파일하는 유일한
수단이다. 이 제약이 두 가지를 바꾼다.

1. **CI는 첫 push에서 통과해야 한다.** 로컬에서 고쳐보고 push하는 반복이
   불가능하므로, CI 설정 자체의 오류가 실제 컴파일 오류를 가리면 진단이
   어렵다. clang-tidy의 `bugprone-exception-escape` / `performance-enum-size`
   를 끈 이유가 이것이다 — 스타일 문제로 빨간 빌드를 만들면 안 된다.
2. **정적 검증을 최대한 당겨서 했다.** 컴파일러 없이 가능한 확인:

| 확인 | 결과 |
|---|---|
| `CMakePresets.json` 파싱 | OK (configure 5, build 4, test 6) |
| `ci.yml` YAML 파싱 | OK (job 4개) |
| `bash -n` 셸 스크립트 | OK |
| CMake가 참조하는 소스·헤더 존재 | 11개 전부 존재 |
| 수동 코드 리뷰 | 결함 4건 발견·수정 (커밋 `e01661f`) |

### 결과

저장소: https://github.com/zooskin/traffic-control (private)

CI를 green으로 만드는 데 4회 반복이 필요했다. 로컬 컴파일 없이 작성한
코드가 실제로 어디서 깨지는지 기록해 둔다.

| 회차 | 실패 | 원인 |
|---|---|---|
| 1 | 7/7 | `requires { TrafficClock::now(); }` — 한정된 이름 조회는 SFINAE 문맥이 아니라 하드 에러. GCC/Clang/MSVC **3종 모두 동일한 오류 하나뿐**이었다 |
| 2 | 5/7 | clang-tidy 수정이 GCC `-Wmissing-field-initializers`와 충돌. `format.sh`가 `100644`로 커밋되어 실행 불가 |
| 3 | 1/7 | npm 패키지가 win32/linux/darwin 바이너리를 **모두 실행 비트와 함께** 배포해서, "첫 실행 가능 파일" 탐색이 Linux에서 `.exe`를 골랐다 |
| 4 | 0/7 | 통과 |

### 얻은 교훈

- **툴 간 충돌은 명시적으로 판정한다.** clang-tidy의
  `readability-redundant-member-init`과 GCC의 `-Wmissing-field-initializers`는
  정반대를 요구한다. 컴파일러 경고가 이긴다는 원칙을 `.clang-tidy`에
  기록했다.
- **포매터 버전을 고정한다.** 배포판 clang-format에 의존하면 로컬에서
  재현되지 않는 실패가 난다. `package.json`에 핀으로 박고 로컬과 CI가
  같은 바이너리를 쓴다.
- **Windows에서 커밋한 셸 스크립트는 실행 비트를 잃는다.**
  `git update-index --chmod=+x` 로 기록하고, CI는 `bash <script>` 로 호출한다.
