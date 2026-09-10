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
| 27 | `27_PROJECT_KICKOFF.md` | 착수 계획, 마일스톤 |

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
