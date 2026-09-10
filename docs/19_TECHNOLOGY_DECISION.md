Traffic Control Software

Technology Decision Record

1\. 목적



본 문서는 Multi-Robot Traffic Control Software의 기술 스택을 결정하고, 향후 개발 과정에서 기술 선택이 임의로 변경되는 것을 방지하기 위한 Architecture Decision Record(ADR)이다.



본 프로젝트의 핵심 환경:



100\~200대 Robot

500대까지 확장 가능한 architecture

좁고 긴 Corridor

제한적인 우회로

사람과 Robot이 공존

Human blockage 빈번

Robot failure 가능

Dynamic replanning 필요

Deadlock detection/recovery 필요

실시간 Traffic Decision 필요

2\. 핵심 결정

Primary Language



C++20



Production Traffic Control Core는 C++20을 기준으로 개발한다.



향후 C++23으로 migration할 수 있으나 초기 개발에서는 C++20을 기준으로 한다.



3\. Secondary Language

Python



Python은 Production Traffic Control Core의 기본 언어로 사용하지 않는다.



다음 용도로 사용한다.



Algorithm Research

Simulation Analysis

Benchmark

Data Analysis

Visualization

Test Scenario Generation

Offline Optimization





예:



Python

&#x20;  |

&#x20;  +-- Benchmark Runner

&#x20;  +-- Result Analyzer

&#x20;  +-- Plot

&#x20;  +-- Scenario Generator

&#x20;  +-- Algorithm Research



4\. Language Independence



Traffic Control Architecture 자체는 특정 programming language에 종속되지 않도록 설계한다.



특히 다음 interface를 명확하게 정의한다.



IRoutePlanner

IReservationManager

IPriorityManager

IDeadlockManager

IReplanningEngine

IRobotAdapter

IMapProvider

IClock

IEventBus





구현:



C++ implementation





을 기본으로 하지만 interface 수준에서는 특정 algorithm 또는 implementation에 종속되지 않는다.



5\. Why C++?



C++을 선택하는 주요 이유:



5.1 Runtime Performance



Traffic Controller는 지속적으로 다음 작업을 수행한다.



Robot State Update

&#x20;       ↓

Conflict Detection

&#x20;       ↓

Reservation Update

&#x20;       ↓

Planning

&#x20;       ↓

Deadlock Detection

&#x20;       ↓

Traffic Decision

&#x20;       ↓

Command





낮은 latency와 높은 처리량이 중요하다.



5.2 Memory Control



핵심 자료구조:



Graph

Priority Queue

Reservation Table

Time Window

Wait-for Graph

Robot State Table

Event Queue

Spatial Index





에 대해 memory layout과 allocation을 세밀하게 제어할 필요가 있다.



5.3 Deterministic Behavior



동일한 입력에 대해 가능한 한 동일한 결과를 얻어야 한다.



특히:



Seed

Input State

Map

Task Set

Robot State





가 동일하면 동일한 decision을 재현할 수 있어야 한다.



5.4 Robotics / Industrial Integration



향후 다음 시스템과 integration할 가능성을 고려한다.



ROS 2

DDS

OPC UA

Ethernet/IP

TCP/UDP

gRPC

Protobuf

WMS

MES

PLC

Robot Vendor SDK





C++은 이러한 robotics/industrial 환경과의 integration에 적합하다.



6\. Why Not Python for Core?



Python은 빠른 prototype과 알고리즘 연구에 매우 적합하다.



그러나 Production Core에서는 다음 이유로 사용하지 않는다.



Latency predictability

High-frequency event processing

Memory control

Concurrency control

Robot SDK integration

Long-running runtime stability





단, 특정 알고리즘을 Python으로 먼저 검증할 수 있다.



검증 후 production implementation은 C++로 옮길 수 있다.



7\. Rust Decision



Rust는 후보 기술로 인정한다.



Rust의 장점:



Memory Safety

High Performance

Concurrency Safety





그러나 현재 프로젝트에서는 C++을 선택한다.



주요 이유:



Robotics ecosystem

ROS 2 ecosystem

Industrial SDK

Existing C/C++ libraries

Graph/MAPF libraries

Team familiarity





향후 새로운 module을 Rust로 개발할 필요가 발생하면 별도의 ADR을 작성한다.



8\. Java / Go Decision



Java와 Go는 다음 영역에서는 좋은 선택이다.



API Server

Backend

Distributed Services

Monitoring

Management System





그러나 본 프로젝트의 핵심 Traffic Control Core는 C++로 구현한다.



필요하면 향후 다음과 같이 분리할 수 있다.



&#x20;            Backend

&#x20;         Java / Go

&#x20;              |

&#x20;            gRPC

&#x20;              |

&#x20;              v

&#x20;      C++ Traffic Core



9\. Build System

CMake



CMake를 사용한다.



CMake >= 3.25





권장.



목적:



Cross Platform Build

Dependency Management

CI Integration

IDE Integration



10\. C++ Standard



기본:



C++20





사용 가능한 경우 다음 기능을 적극 활용한다.



std::span

std::chrono

std::optional

std::variant

std::expected

concepts

ranges

coroutines





단, 복잡도를 증가시키는 경우 무조건 최신 기능을 사용하는 것은 금지한다.



11\. Unit Testing

GoogleTest



기본 Unit Test Framework:



GoogleTest





Test:



Unit

Integration

Algorithm

Reservation

Deadlock

Replanning



12\. Benchmark



C++ 내부 micro benchmark에는 필요에 따라:



Google Benchmark





을 사용한다.



System-level benchmark는 별도 benchmark runner를 사용한다.



13\. Logging

spdlog



Production logging:



spdlog





권장.



Logging level:



TRACE

DEBUG

INFO

WARN

ERROR

CRITICAL





Production에서는 DEBUG/TRACE logging을 기본적으로 제한한다.



14\. Serialization

Protobuf



Internal communication 및 고성능 event/message serialization에:



Protocol Buffers





를 사용한다.



15\. External API



외부 API는 기본적으로:



REST

JSON





을 사용한다.



예:



WMS

MES

Management UI

Monitoring



16\. RPC



Service-to-service communication이 필요한 경우:



gRPC





를 사용한다.



예:



Traffic Core

&#x20;     |

&#x20;    gRPC

&#x20;     |

Robot Gateway



17\. Robotics Middleware



실제 Robot system과 integration할 경우 필요에 따라:



ROS 2

DDS





를 사용한다.



그러나 Traffic Controller Core가 ROS 2에 강하게 결합되지 않도록 한다.



권장:



Traffic Core

&#x20;    |

IRobotAdapter

&#x20;    |

ROS2 Adapter



18\. Database



Traffic Controller의 real-time decision state를 Database에 직접 의존시키지 않는다.



Runtime state:



Memory





Historical data:



Database





로 분리한다.



19\. State Storage



Runtime:



In-memory State Store





Persistent:



PostgreSQL





등을 사용할 수 있다.



Database는 Traffic Decision Loop의 critical path에서 제외한다.



20\. Cache



필요한 경우:



Redis





등을 사용할 수 있다.



그러나 처음부터 Redis를 필수 dependency로 만들지 않는다.



21\. Metrics



Metrics는 Prometheus-compatible format을 사용한다.



주요 metrics:



controller\_decision\_latency

planner\_latency

reservation\_conflict\_count

deadlock\_count

replanning\_count

robot\_waiting\_time

task\_completion\_time



22\. Visualization



초기 분석:



Python

Matplotlib

Pandas

Jupyter





등을 사용할 수 있다.



Production Monitoring UI는 별도 Web application으로 구성한다.



23\. Simulation



Simulator Core:



C++





를 기본으로 한다.



이유:



Production Core와 동일한 model과 algorithm을 최대한 공유하기 위함이다.



구조:



&#x20;            Traffic Core

&#x20;                |

&#x20;       +--------+--------+

&#x20;       |                 |

&#x20;Simulator Adapter   Real Adapter

&#x20;       |                 |

&#x20;   Simulator          Robot



24\. Python Simulation



Python simulation은 허용한다.



단, 목적은:



Research

Algorithm Prototype

Data Analysis





로 제한한다.



Production Traffic Controller 검증용 기준 simulator는 C++ 기반으로 유지한다.



25\. Container



기본 실행 환경:



Docker





를 지원한다.



Production deployment 환경은 향후:



Docker Compose

Kubernetes

Bare Metal





중 실제 운영 환경에 맞춰 결정한다.



초기에는 Kubernetes를 필수로 하지 않는다.



26\. CI/CD



CI에서 최소 다음을 수행한다.



Build

Unit Test

Integration Test

Static Analysis

Formatting Check

Simulation Smoke Test



27\. Static Analysis



권장:



clang-tidy

clang-format

AddressSanitizer

UndefinedBehaviorSanitizer

ThreadSanitizer





단, Sanitizer는 runtime 환경에 따라 별도 CI job으로 구성한다.



28\. Code Quality



권장 기준:



Warnings = Error

No undefined behavior

No data race

No memory leak

No unchecked error



29\. Real-Time Strategy



본 시스템은 hard real-time safety controller가 아니다.



따라서:



Soft Real-Time





을 목표로 한다.



그러나 traffic decision latency는 명확하게 관리한다.



초기 목표:



Controller P50 < 10 ms

Controller P95 < 50 ms

Controller P99 < 100 ms





실제 목표는 benchmark 결과에 따라 조정한다.



30\. Safety Boundary



Traffic Control Software는 Robot Safety Controller를 대체하지 않는다.



Safety Controller

&#x20;      |

&#x20;      v

Emergency Stop

&#x20;      |

&#x20;      v

Robot



Traffic Controller

&#x20;      |

&#x20;      v

Traffic Permission

Route

Wait

Resume





Safety-critical stop은 별도의 safety system이 담당한다.



31\. Architecture Principle



다음 원칙을 유지한다.



Language Independent Architecture

C++ Production Core

Python Research Layer

Simulation/Production Core Reuse

Explicit Interfaces

Deterministic Decision

Observable Behavior

Fail Safe



32\. Technology Change Policy



다음 기술은 임의로 변경하지 않는다.



Primary Language

Build System

Serialization

Core API

Planner Interface

Reservation Semantics

Safety Boundary





변경이 필요한 경우:



1\. ADR 작성

2\. 영향 분석

3\. Benchmark

4\. Migration Plan

5\. Review

6\. Implementation





순서로 진행한다.



33\. Final Technology Stack

Language

&#x20;   C++20



Research

&#x20;   Python



Build

&#x20;   CMake



Testing

&#x20;   GoogleTest



Benchmark

&#x20;   Google Benchmark



Logging

&#x20;   spdlog



Serialization

&#x20;   Protobuf



RPC

&#x20;   gRPC



API

&#x20;   REST / JSON



Robotics

&#x20;   ROS 2 / DDS when required



Runtime State

&#x20;   In-memory



Persistent Data

&#x20;   PostgreSQL



Metrics

&#x20;   Prometheus



Simulation

&#x20;   C++



Container

&#x20;   Docker



Static Analysis

&#x20;   clang-tidy



Formatting

&#x20;   clang-format



34\. Final Decision



본 프로젝트의 기본 기술 방향은 다음과 같다.



&#x20;               Production

&#x20;                   |

&#x20;                 C++20

&#x20;                   |

&#x20;       +-----------+-----------+

&#x20;       |           |           |

&#x20;     MAPF     Reservation   Deadlock

&#x20;       |           |           |

&#x20;       +-----------+-----------+

&#x20;                   |

&#x20;             Traffic Core

&#x20;                   |

&#x20;            Robot Adapter

&#x20;                   |

&#x20;         +---------+---------+

&#x20;         |                   |

&#x20;      Simulator          Real Robot





Python은 Production Core가 아니라:



Research

Benchmark

Analysis

Visualization





영역에서 사용한다.



이 결정은 초기 개발의 기본값이며, 변경 시 반드시 ADR을 추가한다.

