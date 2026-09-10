# Specification Index — Single Source of Truth

이 파일은 사양 문서의 **정본(Source of Truth)** 을 선언한다.

문서 간에 내용이 충돌하면 아래 "확정 결정" 절이 이긴다. 확정 결정에 없는
충돌을 발견하면 임의로 판단하지 말고 이 파일에 항목을 추가하는 변경안을
먼저 제안한다.

문서는 세 차례에 걸쳐 작성되었고 후기 문서가 초기 문서의 일부를 재작성했다.
따라서 **번호 순서는 우선순위가 아니다.** 아래 표를 기준으로 읽는다.

---

## 1. 문서 상태

### 정본 (Active)

| # | 문서 | 범위 |
|---|---|---|
| 00 | `00_MASTER_PLAN.md` | 프로젝트 목적, 개발 원칙, KPI, Phase 개요 |
| 01 | `01_REQUIREMENTS.md` | 기능/비기능 요구사항 (FR-001~012, NFR-001~005) |
| 02 | `02_SIMULATOR.md` | 시뮬레이터 구조 |
| 03 | `03_MAP_GRAPH.md` | Traffic Graph / Resource / Conflict Zone |
| 04 | `04_ROBOT_TASK_MODEL.md` | Robot·Task 모델 (→ 24가 상위 정본) |
| 05 | `05_GLOBAL_ROUTING.md` | 전역 경로 계획 |
| 06 | `06_TRAFFIC_RESERVATION.md` | 예약 시맨틱 |
| 07 | `07_DYNAMIC_REPLANNING.md` | 동적 재계획 |
| 08 | `08_DEADLOCK_MANAGER.md` | 교착 탐지·복구 |
| 09 | `09_PRIORITY_MANAGER.md` | 우선순위 |
| 10 | `10_TRAFFIC_CONTROLLER.md` | **제어 루프 구현**: 이벤트 처리, 커맨드 생성, 스냅샷, 복구, 재조정 |
| 12 | `12_SOFTWARE_ARCHITECTURE.md` | 모듈 의존성 규칙, DI, 스레딩 모델 (§3 디렉토리 구조는 23 §26이 대체) |
| 13 | `13_API_SPECIFICATION.md` | 외부 REST API |
| 15 | `15_ALGORITHM_BENCHMARK.md` | 알고리즘 선정 벤치마크 |
| 16 | `16_TEST_STRATEGY.md` | **테스트 전략**: 피라미드, soak/chaos, 릴리스 게이트 |
| 17 | `17_SIMULATION_SCENARIOS.md` | 기준 맵, 로봇 개체군 |
| 19 | `19_TECHNOLOGY_DECISION.md` | 기술 스택 ADR |
| 20 | `20_CODING_GUIDELINES.md` | C++ 코딩 규칙 56개 항목 |
| 21 | `21_AI_AGENT_INSTRUCTIONS.md` | AI Agent 개발 지침 |
| 22 | `22_IMPLEMENTATION_WORKFLOW.md` | **Phase 순서 정본** (Phase 0~17) |
| 23 | `23_SYSTEM_ARCHITECTURE.md` | **시스템 아키텍처 정본** |
| 24 | `24_DOMAIN_MODEL.md` | **도메인 모델 정본** |
| 25 | `25_TRAFFIC_CONTROL_SPECIFICATION.md` | **교통 제어 정책 정본**: corridor/교차로/대기/기아 방지 |
| 26 | `26_TEST_SCENARIOS.md` | **시나리오 카탈로그 정본** (SC-001~) |
| 27 | `27_PROJECT_KICKOFF.md` | 착수 계획, 마일스톤, Phase 0~4 마감 기록 |
| 28 | `28_HANDOFF.md` | **작업 인계**: 현재 상태, 미결 결정, 다음 계획 |

### 대체됨 (Superseded) — `docs/archive/`

| # | 문서 | 대체 문서 | 사유 |
|---|---|---|---|
| 11 | `11_SYSTEM_ARCHITECTURE.md` | **23** | 동일 제목의 재작성판. 23이 29절로 더 넓다 |
| 14 | `14_DATA_MODEL.md` | **24** | 24(36절)가 14(24절)의 상위집합 |
| 18 | `18_IMPLEMENTATION_PLAN.md` | **22** | Phase 순서 충돌. 아래 결정 D-003 참조 |

> 아카이브 문서는 **구현 근거로 인용하지 않는다.** 이력 보존 목적으로만 남긴다.
> 단 11에는 Layer Architecture / Transaction Boundary / Resource Lock 절이 있고
> 23에 대응 절이 없다. 해당 주제를 구현할 때는 11을 참고하되, 채택하려면
> 23에 반영하는 변경안을 먼저 제안한다.

### 10 vs 25, 16 vs 26 — 중복 아님

혼동하기 쉬우므로 명시한다.

- **10**은 *어떻게 동작하는가*(제어 루프, 이벤트 순서, 커맨드 멱등성, 스냅샷/복구).
  **25**는 *무엇을 허용하는가*(corridor 정책, head-on 규칙, 기아 방지, tie-break).
  정책이 충돌하면 **25**가 이긴다.
- **16**은 테스트 *전략*(피라미드, soak, chaos, 릴리스 게이트).
  **26**은 테스트 *시나리오 목록*(SC-001~). 서로를 대체하지 않는다.

---

## 2. 확정 결정

문서 간 충돌에 대한 구속력 있는 판정이다. 변경하려면 ADR을 추가한다
(`19_TECHNOLOGY_DECISION.md` §32의 절차를 따른다).

### D-001. RobotState는 24를 따른다

세 문서가 서로 다른 상태 집합을 정의했다.

```
archive/01 (참고)  IDLE ASSIGNED PLANNING MOVING WAITING BLOCKED
                   REPLANNING RECOVERY ARRIVED FAILED EMERGENCY_STOP
archive/14         IDLE ASSIGNED PLANNING RESERVING MOVING WAITING BLOCKED
                   REPLANNING RECOVERY FAILED COMPLETED
24 (정본)          MOVING IDLE WAITING TEMPORARILY_STOPPED BLOCKED
                   FAILED UNKNOWN
```

**결정: 24의 7개 상태를 정본으로 한다.**

근거: `TEMPORARILY_STOPPED`가 있어야 "일시정지 ≠ 고장"을 구분할 수 있고,
이는 23 §20과 25 §17~18의 핵심 설계다. 이 구분이 없으면 사람이 잠깐
가로막을 때마다 불필요한 전역 재계획이 발생한다.

**단, 다음 두 상태를 추가한다:**

| 상태 | 사유 |
|---|---|
| `RESERVING` | 예약 요청 후 GRANT 대기 구간을 관측 가능하게 한다 (14에 있었음) |
| `REPLANNING` | 재계획 중 중복 재계획 요청을 막는 데 필요하다 (07 참조) |

`ARRIVED`/`COMPLETED`는 **Task 상태**이지 Robot 상태가 아니므로 채택하지
않는다. `EMERGENCY_STOP`은 Safety Controller 영역이므로 Traffic Core의
Robot 상태로 두지 않는다 (01 §8, 19 §30, 23 §2.3).

최종 RobotState (9개):

```
IDLE  MOVING  WAITING  RESERVING  REPLANNING
TEMPORARILY_STOPPED  BLOCKED  FAILED  UNKNOWN
```

### D-002. 디렉토리 구조는 23 §26을 따른다

`include/traffic/<module>/` + `src/<module>/` 로 public header를 분리한다.
12 §3의 `src/<module>/` 통합 구조는 채택하지 않는다.

근거: 19가 gRPC / ROS 2 어댑터 분리를 예고하므로 public API 경계가 물리적으로
필요하다. 12의 나머지 절(의존성 규칙, DI, 스레딩 모델)은 그대로 유효하다.

### D-003. Phase 순서는 22를 따르되 A*를 앞으로 당긴다

18과 22의 Phase 순서가 충돌한다. 특히 18은 Phase 6~7에서 이미 WHCA\*/PIBT를
구현하라고 하는데, 이는 00 §6("알고리즘을 처음부터 복잡하게 적용하지 않는다.
기본 시스템 안정화 이후 benchmark한다")과 정면으로 충돌한다.

**결정: 22를 정본으로 하고, Route Planning(A\*)만 Phase 3으로 당긴다.**

근거: Reservation과 Conflict Detection을 테스트하려면 Route가 필요하다.
A*가 없으면 하드코딩 route로 테스트를 짜야 하고 그 테스트는 나중에 전부
버려진다.

확정 Phase 순서:

```
Phase 0   Repository Setup
Phase 1   Core Domain
Phase 2   Map & Graph
Phase 3   Route Planning (A*)        <- 22의 Phase 8에서 당김
Phase 4   Robot State
Phase 5   Task Model
Phase 6   Reservation
Phase 7   Conflict Detection
Phase 8   Priority
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

WHCA\*/PIBT/ECBS는 Phase 15 이후 15_ALGORITHM_BENCHMARK 기준으로 도입 여부를
결정한다. `IRoutePlanner` 뒤에 숨기므로 교체 비용은 낮다.

### D-005. Robot과 RobotState를 분리한다

`24 §3`은 `Robot`이 `state` / `current_task` / `current_route`를 갖는다고
하고, `§4`의 `RobotState`도 같은 필드를 갖는다. 두 곳에 두면 어느 쪽이
진실인지 알 수 없다.

한편 `23 §22`는 소유권을 명시한다.

```
RobotState -> StateManager
Task       -> TaskManager
```

**결정: 다음과 같이 분리한다.**

| 타입 | 내용 | 성격 |
|---|---|---|
| `Robot` | `robot_id`, `capabilities` | 정적 등록 정보 |
| `RobotStateSnapshot` | `§4`의 전 필드 (position, velocity, current_node, current_edge, current_task, current_route, state, timestamp, state_version) | 시점 관측값, StateManager 소유 |

근거: 로봇의 동적 상태는 초당 수십 회 갱신된다. 같은 필드를 두 객체에
두면 반드시 갈라지고, 그 시점에 어느 쪽으로 교통 판단을 했는지 추적할 수
없게 된다.

**명명:** `§4`의 엔티티 이름이 `RobotState`인데 그 안의 `state` 필드 타입도
`RobotState`라 충돌한다. enum이 `RobotState`(D-001, CLAUDE.md 확정),
스냅샷 엔티티가 `RobotStateSnapshot`이다.

### D-006. NodeType은 03을 따른다

`03 §5`와 `24 §7`이 서로 다른 Node Type 집합을 정의한다.

```
03 §5 (9종)  NORMAL INTERSECTION STATION PICKUP DROPOFF
             CHARGER WAITING_BAY ENTRY EXIT
24 §7 (6종)  NORMAL INTERSECTION CHARGING LOADING UNLOADING HOLDING
```

**결정: `03 §5`의 9종을 정본으로 한다.**

근거는 어휘 사용 빈도다. `03`의 용어는 요구사항과 예약 사양에서도 쓰인다.

| 용어 | 등장 문서 |
|---|---|
| `WAITING_BAY` / `CHARGER` / `STATION` | **01, 03, 06** |
| `HOLDING` / `CHARGING` / `LOADING` / `UNLOADING` | 24 |

`24`가 도메인 모델의 정본이지만 이 항목에서는 나머지 사양 전체와 어긋난
고립된 표기다. 또한 Node Type은 맵의 관심사이고 `03`이 맵 사양이다.

Phase 1에서 구현한 이름을 다음과 같이 옮긴다.

```
charging  -> charger
loading   -> pickup
unloading -> dropoff
holding   -> waiting_bay
추가:        station, entry, exit
```

### D-007. Edge의 travel_time은 저장하되 파생값을 기본으로 한다

`03 §6`은 Edge에 `travel_time` 필드를 둔다. 그러나 대부분의 Edge에서
`travel_time = length / speed_limit`이므로 저장하면 두 값이 갈라진다.

**결정: `travel_time`을 optional override로 두고, 비어 있으면
`length / speed_limit`을 계산한다.**

리프트나 자동문처럼 통과 시간이 거리에 비례하지 않는 구간이 실제로 있으므로
필드 자체는 유지한다. 다만 기본은 파생값이다.

### D-008. Heuristic 기본값은 Euclidean이다

`05 §5`는 한 절 안에서 두 가지를 말한다.

```
기본: Manhattan Distance
또는 map 특성에 따라: Euclidean Distance
Heuristic은 admissible하도록 설계한다.
```

이 두 문장은 이 프로젝트의 맵에서 서로 충돌한다.

Manhattan 거리는 로봇이 축 방향으로만 움직이는 격자 맵에서만 실제 이동
거리와 같다. `03_MAP_GRAPH`가 정의하는 맵은 일반 그래프이고 Edge는 임의의
각도로 놓인다. 대각선 Edge에서 `|dx| + |dy|`는 직선거리를 초과하므로
heuristic이 실제 남은 비용을 과대평가한다.

과대평가하는 A\*는 **실패하지 않는다.** 최단 경로가 아닌 경로를 아무 신호
없이 반환한다. 그래서 잘못을 알아차릴 방법이 없다.

**결정: `EuclideanHeuristic`을 기본으로 하고, `ManhattanHeuristic`은
격자형 맵을 위해 남긴다.**

`05 §5`의 "map 특성에 따라"가 이 선택을 이미 허용한다. admissibility는
같은 절이 명시적으로 요구하는 성질이므로, 충돌 시 그쪽이 이긴다
(D-004의 2번 System Specification 안에서 더 구체적인 요구가 우선).

관련 구현 사실 두 가지를 함께 기록한다.

**Dijkstra는 별도 알고리즘으로 구현하지 않는다.** `05 §3`이 fallback으로
지정한 Dijkstra는 heuristic이 0인 A\*와 같다. `ZeroHeuristic`을 주입하면
된다. 정확성을 유지해야 할 구현이 하나로 줄고, A\* 테스트가 자기 답을
검증할 기준을 얻는다.

**Heuristic은 거리가 아니라 비용을 반환한다.** `g`와 단위가 같아야 하므로
`min_cost_per_metre`(= `preferred_factor * (w_distance + w_time / max_speed)`)로
스케일한다. 이 값을 위로 잘못 잡으면 admissibility가 깨지므로
`make_admissible_heuristic(map, weights)`이 맵에서 직접 계산한다.

전제 하나가 남는다. 직선거리 기반 heuristic은 **모든 Edge의 `length`가 양
끝점 좌표 사이 직선거리 이상**이어야 성립한다. 맵 포맷은 이를 강제하지
않는다(좌표를 명목값으로 쓰고 실제 거리를 `length`에 담는 현장이 있을 수
있으므로 맵 검증 오류로 두지 않았다). `geometry_supports_distance_heuristic`
으로 확인하고, 실패하는 맵은 `ZeroHeuristic`으로 계획한다.

### D-009. 로봇이 보고한 것과 우리가 결정한 것을 섞지 않는다

`04 §7`은 WAITING 상태의 Robot이 `waiting_since` / `waiting_resource` /
`waiting_reason`을 저장한다고 하고, `§20`의 `RobotStateUpdate`는 Robot이
`status`를 보낸다고 한다. 두 문장을 그대로 구현하면 Robot이 자기 교통 상태를
스스로 선언하고 그 근거까지 관측값에 섞인다.

**결정 1: Robot은 자기 교통 상태를 명명할 수 없다.**

D-001의 9개 상태 중 `reserving` / `waiting` / `replanning`은 교통 제어가
**내린 결정**이지 Robot이 관측할 수 있는 사실이 아니다. Robot이 `waiting`을
보고한다는 것은 우리가 무언가를 승인했다고 주장하는 것이다. 이를 받아들이면
오작동하거나 재전송된 메시지가 발급된 적 없는 예약을 컨트롤러에 믿게 만들 수
있다.

`is_robot_reportable()`이 이 세 상태를 거부한다. 조용히 무시하거나 다른
값으로 고치지 않고 **거부**한다 — 말이 안 되는 값을 보내는 fleet을 숨기면
안 된다.

**결정 2: `waiting_*`는 `RobotStateSnapshot`에 넣지 않는다.**

Snapshot은 *관측된 것*이다. Waiting reason은 우리가 *결론 내린 것*이다.
둘을 한 구조체에 넣으면

- 동일한 관측 두 개가 우리가 그것에 대해 내린 판단 때문에 서로 다르게
  비교된다(`operator==`가 defaulted이고 version 비교에 쓰인다).
- fleet이 말한 것과 우리가 추론한 것을 사후에 구분할 방법이 없어진다.

`state::WaitingContext`로 분리하고 StateManager가 보유한다. `23 §22`가
RobotState의 소유자를 StateManager로 지정한 것과 같은 자리다.

**결정 3: `04 §5`의 11개 status는 D-001이 대체한다.**

`04 §5`는 IDLE / ASSIGNED / PLANNING / MOVING / WAITING / BLOCKED /
REPLANNING / RECOVERY / ARRIVED / FAILED / EMERGENCY_STOP를 나열한다.
D-001이 이미 9개로 확정했고, `EMERGENCY_STOP`은 CLAUDE.md의 Safety 경계
바깥이다(Emergency Stop / Protective Stop / Safety Zone / Safety Interlock은
별도 시스템의 책임). `ASSIGNED`/`PLANNING`/`ARRIVED`는 Task 상태이지
Robot의 교통 상태가 아니므로 Phase 5의 TaskStatus가 담는다.

다만 안전 시스템이 Robot을 멈춘 사실 자체는 기록한다 —
`WaitingReason::safety_stop`. 우리 타이머가 그 정지를 교통 문제로 오인하지
않게 하기 위해서다. 그 이상은 하지 않는다.

### D-004. 정책 충돌 시 우선순위

21 §4의 우선순위를 문서에 대응시킨다.

```
1. Safety Requirement          01 §8,  19 §30, 23 §2.3
2. System Specification        01, 25
3. Architecture Decision       19, 23, 12, 이 파일의 확정 결정
4. Coding Guidelines           20
5. Test Requirements           16, 26
6. Existing Implementation     저장소 코드
7. User Request
8. AI Agent Preference
```

---

## 3. 읽는 순서

새로 합류한 사람 또는 에이전트는 이 순서로 읽는다.

**최초 1회 (전체 맥락)**
```
00 -> 01 -> 23 -> 24 -> 19 -> 22
```

**이어서 작업을 맡는 경우 (워크스페이스 이관 등)**
```
CLAUDE.md  ->  00  ->  28 (인계)  ->  해당 Phase의 사양
```

**코드를 쓰기 전 매번**
```
CLAUDE.md  ->  해당 Phase의 관련 사양  ->  20 (코딩 규칙)
```

**모듈별 참조**

| 작업 | 읽을 문서 |
|---|---|
| 도메인 타입 | 24, 03 |
| 맵/그래프 | 03, 24 §7~10 |
| 경로 계획 | 05, 15, 24 §20~21 |
| 예약 | 06, 25 §10, 24 §13~14 |
| 우선순위 | 09, 25 §15~16 |
| 교착 | 08, 25, 24 §22~23 |
| 재계획 | 07, 25 §24~26 |
| 컨트롤러 | 10, 23 §15~16, 25 |
| 어댑터 | 13, 23 §13 |
| 테스트 | 16 (전략), 26 (시나리오) |

---

## 4. 변경 규칙

- 이 파일의 확정 결정을 바꾸려면 사유·영향 범위·마이그레이션 계획을 포함한
  변경안을 먼저 제시한다. 코드부터 바꾸지 않는다.
- 새 문서를 추가하면 이 표에 등록한다. 등록되지 않은 문서는 정본이 아니다.
- 문서를 대체할 때는 `docs/archive/` 로 옮기고 대체 문서를 명기한다. 삭제하지 않는다.
