# Traffic Control Software

100~200대(확장 목표 500대) AMR/AGV를 대상으로 하는 다중 로봇 교통 제어
소프트웨어. 좁고 긴 통로, 제한적인 우회로, 사람과 로봇이 공존하는 환경을
전제로 한다.

> **현재 상태: Phase 2 완료. 다음은 Phase 3 (Route Planning, A*).**
> 빌드는 CI에서 검증한다 — GCC 13 / Clang 18 / MSVC 2022 3종, 단위 테스트
> 254/254 통과, ASan·UBSan·TSan 포함. 로컬에서도 빌드하려면 아래 Setup을 따른다.

---

## 문서

사양은 `docs/` 에 있다. **`docs/00_INDEX.md` 가 정본 선언 파일이다.**
문서 간 내용이 충돌하면 그 파일의 확정 결정이 이긴다. 문서 번호는 우선순위가
아니다 — 후기 문서가 초기 문서를 재작성한 경우가 있다.

| 먼저 읽을 것 | |
|---|---|
| `docs/00_INDEX.md` | 정본 선언, 확정 결정, 읽는 순서 |
| `docs/27_PROJECT_KICKOFF.md` | 착수 계획과 마일스톤 |
| `CLAUDE.md` | 개발 시 상시 참조 규칙 |

---

## Setup

개발 머신에 다음이 필요하다. 현재 이 저장소가 만들어진 머신에는
**git 외에 아무것도 설치되어 있지 않다.**

### Windows

```powershell
winget install --id Kitware.CMake        -e
winget install --id Microsoft.VisualStudio.2022.BuildTools -e `
  --override "--quiet --wait --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
winget install --id LLVM.LLVM            -e   # clang-format / clang-tidy
```

설치 후 새 터미널을 열어 확인한다.

```powershell
cmake --version      # 3.25 이상
cl                   # "Developer Command Prompt" 또는 VS Developer PowerShell 에서
clang-format --version
```

MSVC는 일반 PowerShell에서 `cl`이 보이지 않는다. **x64 Native Tools Command
Prompt for VS 2022** 를 쓰거나, VS Code / CLion에서 열면 자동으로 잡힌다.

### Linux

```bash
sudo apt install -y build-essential cmake ninja-build clang-format clang-tidy
```

---

## Build

```bash
cmake --preset dev            # 구성 (첫 실행 시 의존성 다운로드)
cmake --build --preset dev    # 빌드
ctest --preset dev            # 전체 테스트
ctest --preset unit           # 단위 테스트만
```

첫 configure는 GoogleTest / spdlog / fmt를 GitHub에서 받아오므로 네트워크가
필요하고 수 분 걸린다. 이후에는 캐시된다.

### 프리셋

| 프리셋 | 용도 |
|---|---|
| `dev` | Debug + 테스트 + 시뮬레이터 |
| `release` | RelWithDebInfo + 벤치마크 |
| `asan` | Address/UB Sanitizer (Linux·macOS) |
| `tsan` | ThreadSanitizer (Linux·macOS) |

`asan`/`tsan`은 Windows에서 사용할 수 없다. **MSVC에는 TSan이 없으므로
data race 검증은 Linux CI에서만 이루어진다.**

### 시뮬레이터 실행

```bash
./build/dev/bin/tc_simulator --log-level=debug
```

Phase 2 시점에는 로깅과 결정론적 시간 주입만 확인하는 골격이다.
시뮬레이션 세계는 Phase 13에서 붙인다.

### 기준 맵

`configs/maps/reference_map.json` 이 `17_SIMULATION_SCENARIOS` §2의 기준
맵이다. 단일 차선 양방향 corridor, 교차로, Waiting Bay를 포함한다.

---

## 형식 검사

```bash
scripts/format.sh            # 정렬 적용
scripts/format.sh --check    # CI와 동일한 검사
```

---

## 저장소 구조

```
include/traffic/<module>/   public header
src/<module>/               구현
tests/{unit,integration,simulation,stress}/
simulation/                 시뮬레이터
benchmarks/                 마이크로 벤치마크
configs/                    런타임 설정
scripts/                    개발 스크립트
docs/                       사양 (docs/archive/ 는 대체된 문서)
```

의존성 방향은 한쪽이다. 하위 모듈이 상위 모듈을 참조할 수 없다.

```
core
 ^  domain / map / state / task
 ^  planning / reservation / priority
 ^  replanning / deadlock
 ^  controller
 ^  adapter
```

---

## 개발 순서

```
0  Repository Setup      6  Reservation           12  Dynamic Replanning
1  Core Domain           7  Conflict Detection    13  Simulation
2  Map & Graph           8  Priority              14  Integration
3  Route Planning (A*)   9  Deadlock Detection    15  Performance
4  Robot State          10  Deadlock Recovery     16  Scale Test
5  Task Model           11  Traffic Controller    17  Production Hardening
```

현재 Phase의 완료 조건을 채우기 전에 다음 Phase를 시작하지 않는다.
WHCA\*/PIBT/ECBS는 Phase 15 이후 벤치마크 결과로 도입을 결정한다.

---

## 범위 밖

Traffic Controller는 **Safety Controller를 대체하지 않는다.**
Emergency Stop, Protective Stop, Safety Zone, Safety Interlock은 별도
안전 시스템의 책임이다. 이 저장소는 전략적·전술적 교통 조정만 담당한다.
