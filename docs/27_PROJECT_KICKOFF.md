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

### Week 3 — Phase 2: Map & Graph

`03_MAP_GRAPH` + `24 §7~10` 기준.

- Node / Edge / Resource / Corridor / Intersection / Conflict Zone
- **Corridor를 독립 Traffic Resource로 표현** (23 §18 — 이 설계가 프로젝트의
  핵심이므로 여기서 타협하지 않는다)
- MapLoader (YAML 또는 JSON) + Map validation
- 17_SIMULATION_SCENARIOS의 기준 맵을 실제 파일로 작성

### Week 4 — Phase 3: A\* + 수직 슬라이스

- `IRoutePlanner` 인터페이스 + `AStarPlanner`
- 결정론적 tie-breaking (같은 비용이면 항상 같은 경로)
- 최소 시뮬레이션 루프: 10대, 단일 corridor, 예약 없음
- KPI 수집: 충돌 수, 평균 이동 시간, 계획 지연
- 동일 seed 재실행 시 동일 결과 확인

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
