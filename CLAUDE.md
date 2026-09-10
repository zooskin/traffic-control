# CLAUDE.md

Multi-Robot Traffic Control Software. 100~200대(확장 목표 500대) AMR/AGV가
좁고 긴 통로에서 사람과 함께 운용되는 환경의 교통 제어 소프트웨어.

사양은 `docs/` 에 있다. **`docs/00_INDEX.md` 가 정본 선언 파일이다.** 문서 간
내용이 충돌하면 그 파일의 "확정 결정"이 이긴다. 문서 번호는 우선순위가 아니다.

---

## 절대 금지

다음을 **임의로 변경하지 않는다.** 변경이 필요하면 코드를 고치기 전에
문서 변경안을 먼저 제시하고 승인을 받는다.

- Architecture
- Public API / Interface 시그니처
- Data Model
- Algorithm Selection
- Safety Policy
- Reservation Semantics

(출처: `docs/archive/18_IMPLEMENTATION_PLAN.md` §25, `docs/21_AI_AGENT_INSTRUCTIONS.md` §4)

## 판단 우선순위

충돌하는 지시를 받으면 이 순서로 판단하고, 상위 원칙과 충돌하는 요청은
임의로 수행하지 말고 충돌 사실을 보고한다.

```
1. Safety Requirement        5. Test Requirements
2. System Specification      6. Existing Implementation
3. Architecture Decision     7. User Request
4. Coding Guidelines         8. AI Agent Preference
```

---

## 핵심 아키텍처 원칙

책임 분리가 이 프로젝트의 전부다. 이것만은 무너뜨리지 않는다.

| 컴포넌트 | 답하는 질문 |
|---|---|
| Global Planner | **어디로** 갈 것인가 |
| Traffic Controller | **언제** 갈 수 있는가 |
| Reservation Manager | 그 자원을 **누가** 소유하는가 |
| Deadlock Manager | 진행이 멈췄을 때 **어떻게** 복구하는가 |
| Robot Controller | **어떻게** 움직이는가 (우리 범위 밖) |

```
Traffic Controller != Planner
Planner            != Robot Adapter
Traffic Control    != Safety Control
Domain             != Infrastructure
```

**Safety 경계**: Traffic Controller는 Safety Controller를 대체하지 않는다.
Emergency Stop / Protective Stop / Safety Zone / Safety Interlock은 별도
시스템의 책임이다. Traffic Core에 이런 상태나 로직을 넣지 않는다.

---

## 기술 스택 (변경 시 ADR 필수)

| | |
|---|---|
| 언어 | **C++20** (Production Core) |
| 빌드 | **CMake >= 3.25**, FetchContent로 의존성 관리 |
| 테스트 | **GoogleTest** |
| 벤치마크 | Google Benchmark |
| 로깅 | **spdlog** |
| 직렬화 | 외부 API는 JSON, 내부 고성능 경로는 Protobuf |
| RPC | gRPC (필요할 때만) |
| Python | Research / Benchmark / Analysis / Visualization **전용**. Core에 쓰지 않는다 |

로컬은 MSVC 2022, CI는 GCC/Clang. **TSan은 MSVC에서 지원되지 않으므로
sanitizer 검증은 Linux CI에서 한다.**

---

## 디렉토리 구조

```
include/traffic/<module>/     public header
src/<module>/                 구현
tests/{unit,integration,simulation,stress}/
simulation/  benchmarks/  configs/  tools/  scripts/  docs/
```

모듈: `core domain map state task planning reservation priority
deadlock replanning controller adapter infrastructure`

**의존성 방향** (하위가 상위를 참조하면 안 된다):

```
core
 ^  domain / map / state / task
 ^  planning / reservation / priority
 ^  replanning / deadlock
 ^  controller
 ^  adapter
```

---

## 반드시 지키는 코딩 규칙

`docs/20_CODING_GUIDELINES.md` 가 56개 항목의 정본이다. 그중 위반이 잦고
비용이 큰 것만 여기 둔다.

**Strong Types.** ID를 `int`/`std::string` 그대로 쓰지 않는다.
`RobotId`, `NodeId`, `EdgeId`, `TaskId`, `ResourceId`는 각각 별개 타입이다.

**IClock.** 시간을 직접 읽지 않는다. `std::chrono::system_clock::now()`를
도메인/로직 코드에서 호출하는 것은 금지다. 항상 주입된 `IClock`을 쓴다.
시뮬레이션·재현·테스트가 전부 여기에 걸려 있다.

**Dependency Injection.** 구현체를 내부에서 `new` 하지 않는다.
인터페이스 참조를 생성자로 받는다.

```cpp
// 금지
TrafficController::TrafficController() { planner_ = new AStar(); }

// 필수
TrafficController::TrafficController(IRoutePlanner& planner,
                                     IReservationManager& reservations,
                                     IPriorityManager& priority);
```

**결정성.** 동일 입력 + 동일 seed = 동일 결과. 난수는 명시적으로 seed를
주입받는다. 순회 순서가 결과에 영향을 주는 곳에 `unordered_*`를 쓰지 않는다.

**Algorithm Isolation.** 알고리즘은 인터페이스 뒤에 숨긴다. Controller가
A\*·PIBT·WHCA\*를 구분해서 알면 안 된다.

**Domain 순수성.** 도메인 타입은 DB / Network / Vendor SDK / UI에 의존하지
않는다. 직렬화는 도메인 바깥에 둔다.

**State Machine.** 상태 전이는 명시적 테이블로 구현하고 잘못된 전이는
거부한다. 조용히 무시하지 않는다.

**Warnings = Error.** 경고를 남긴 채 커밋하지 않는다.

---

## RobotState (확정)

```
IDLE  MOVING  WAITING  RESERVING  REPLANNING
TEMPORARILY_STOPPED  BLOCKED  FAILED  UNKNOWN
```

`TEMPORARILY_STOPPED`와 `FAILED`/`BLOCKED`의 구분이 핵심이다. 사람이 잠깐
지나가는 상황을 고장으로 취급하면 불필요한 전역 재계획이 폭증한다.
근거와 폐기된 대안은 `docs/00_INDEX.md` D-001 참조.

---

## Phase 순서 (확정)

```
0  Repository Setup      6  Reservation           12  Dynamic Replanning
1  Core Domain           7  Conflict Detection    13  Simulation
2  Map & Graph           8  Priority              14  Integration
3  Route Planning (A*)   9  Deadlock Detection    15  Performance
4  Robot State          10  Deadlock Recovery     16  Scale Test
5  Task Model           11  Traffic Controller    17  Production Hardening
```

**현재 Phase의 완료 조건을 채우기 전에 다음 Phase 구현을 시작하지 않는다.**

WHCA\*/PIBT/ECBS는 Phase 15 이후 벤치마크 결과로 도입을 결정한다.
지금 구현하지 않는다.

---

## 작업 절차

코드를 쓰기 전에:

1. 요구사항 확인 → 2. 관련 문서 확인 → 3. 저장소 구조 확인 →
4. 기존 코드 검색 → 5. 기존 인터페이스·테스트 확인 → 6. 구현 범위 결정

**기존 코드를 확인하지 않고 새 컴포넌트를 만들지 않는다.**

한 번에 전체 시스템을 구현하지 않는다. 작업 단위는 이 형식을 따른다:

```
TASK:         Implement ReservationTable
READ:         docs/06_TRAFFIC_RESERVATION.md, docs/24_DOMAIN_MODEL.md
REQUIREMENTS: 시간창 기반 overlap 검출, O(log n) 조회
ACCEPTANCE:   겹치는 비호환 예약이 동시에 GRANT되지 않음
TEST:         test_grant / test_conflict / test_release / test_expire / test_overlap
DO NOT:       Reservation semantics 변경, IReservationManager 시그니처 변경
```

## Definition of Done

구현 하나가 끝났다고 말하려면 전부 충족해야 한다.

```
Code + Unit Test + (필요시) Integration Test
+ Error Handling + Logging + Metrics + Configuration
+ Deterministic Test + Documentation
+ Build PASS + Test PASS
```

## 커밋

작은 단위로 나눈다. `<type>(<module>): <설명>`

```
feat(core): add strong id types
feat(map): add traffic graph model
feat(planning): add astar planner
test(reservation): add overlap conflict tests
```

---

## 빌드

```bash
cmake --preset dev          # 구성
cmake --build --preset dev  # 빌드
ctest --preset dev          # 테스트
```

## KPI

성능 판단은 감이 아니라 이 지표로 한다.

```
Collision Count / Near Miss           Deadlock Count / Recovery Time
Average & Max Waiting Time            Planning Latency P50 / P95 / P99
Task Throughput / Completion Rate     CPU / Memory
```

초기 목표: Controller P50 < 10ms, P95 < 50ms, P99 < 100ms.
실제 목표는 벤치마크 후 조정한다.
