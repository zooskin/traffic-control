Traffic Control Software

System Architecture Specification

1. 목적

본 문서는 100~200대의 AMR/AGV가 동시에 운용되는 환경에서 Robot Traffic Control Software의 전체 시스템 구조를 정의한다.

환경 특성:

Robot 100~200대

좁고 긴 Corridor 다수

우회로가 제한적

사람과 Robot이 동일 공간에서 이동

Human에 의한 일시적인 blockage 빈번

Robot failure 가능

높은 traffic density

실시간 route 변경 필요

Deadlock 방지 및 recovery 필요

2. 설계 목표

Primary goals:

Safety

Deadlock prevention

Deadlock recovery

High throughput

Low waiting time

Route stability

Scalability

Deterministic behavior

Observability

Fault tolerance

3. High-Level Architecture
                    +----------------------+
                    |      Task Manager     |
                    +----------+-----------+
                               |
                               v
                    +----------------------+
                    |   Traffic Controller  |
                    +----------+-----------+
                               |
          +--------------------+--------------------+
          |                    |                    |
          v                    v                    v
+----------------+    +----------------+    +----------------+
| Global Planner |    | Reservation    |    | Priority       |
|                |    | Manager        |    | Manager        |
+----------------+    +----------------+    +----------------+
          |                    |                    |
          +--------------------+--------------------+
                               |
                               v
                    +----------------------+
                    | Dynamic Replanning   |
                    +----------+-----------+
                               |
                               v
                    +----------------------+
                    |  Deadlock Manager    |
                    +----------+-----------+
                               |
                               v
                    +----------------------+
                    |    Robot Adapter     |
                    +----------+-----------+
                               |
                  +------------+------------+
                  |            |            |
                 R01          R02         R200

4. Layer Architecture

Layer 1: Robot Interface

책임:

Robot state 수신

Robot command 전달

communication monitoring

heartbeat

Layer 2: Traffic State

책임:

Robot state

Resource state

Reservation state

Task state

Blockage state

Layer 3: Decision

책임:

Priority

Reservation

Conflict resolution

Replanning

Deadlock

Layer 4: Planning

책임:

Global routing

Local rerouting

Corridor planning

Layer 5: Infrastructure

책임:

Event Bus

Database

Cache

Metrics

Logging

5. Component List

TaskManager

RobotManager

MapManager

GlobalPlanner

LocalReplanner

ReservationManager

PriorityManager

DeadlockManager

TrafficController

RobotAdapter

EventBus

StateStore

Metrics

Logger

6. Event Driven Architecture

시스템의 주요 동작은 Event 기반으로 처리한다.

RobotStateUpdated
        |
        v

EventBus
        |
        v

TrafficController
        |
        +--> Reservation
        +--> Priority
        +--> Replanning
        +--> Deadlock

7. State Ownership

각 state의 owner:

State	Owner

Robot State	RobotManager

Task State	TaskManager

Map State	MapManager

Reservation	ReservationManager

Priority	PriorityManager

Deadlock	DeadlockManager

Traffic Decision	TrafficController

8. Source of Truth

Runtime traffic state는 중앙 Traffic Controller를 source of truth로 한다.

Robot은 실제 물리 상태를 제공한다.

Robot
  = Physical State

Controller
  = Logical Traffic State

두 상태의 차이는 reconciliation 대상으로 처리한다.

9. Safety Boundary

Traffic Control Software는 Safety PLC 또는 Robot Safety Controller를 대체하지 않는다.

Safety System
      |
      | Emergency Stop
      v

Robot
      ^
      |

Traffic Controller

Traffic Controller는:

STOP

WAIT

RESUME

route

traffic permission

등을 제어한다.

10. Scalability

초기 목표:

200 robots

Architecture는 최소:

500 robots

까지 확장 가능한 구조로 설계한다.

11. Processing Model

권장 구조:

Event Receiver
      |
      v

Event Queue
      |
      v

Traffic Decision Worker
      |
      v

State Transaction
      |
      v

Command Dispatcher

12. Resource Lock

동일 resource에 대한 concurrent decision을 방지한다.

예:

C01

R01 request

R02 request

R03 request

하나의 atomic decision cycle에서 순서를 결정한다.

13. Transaction Boundary

다음 작업은 atomic하게 처리한다.

Reservation Grant

Reservation Release

Route Commit

Priority Update

14. Failure Model

다음 failure를 고려한다.

Robot Failure

Network Failure

Controller Failure

Database Failure

Planner Failure

Sensor Failure

Stale State

Duplicate Event

Out-of-order Event

15. Recovery Strategy

Controller restart:

Snapshot
   |
   v

Load State
   |
   v

Query Robot
   |
   v

Reconcile
   |
   v

Resume

16. Observability

필수 observability:

Metrics

Logs

Decision Trace

Event Trace

Reservation Trace

Deadlock Trace

Planning Trace

17. 주요 KPI

Throughput

Average Travel Time

Average Waiting Time

P95 Waiting Time

Deadlock Rate

Replanning Rate

Route Change Rate

Reservation Conflict Rate

Planning Latency

Controller Latency

18. Architecture Principle

다음 원칙을 지킨다.

Safety over throughput

Local replanning over global replanning

Reservation over reactive collision avoidance

Deterministic decision

Explicit state machine

Observable decisions

Fail safe

No hidden global mutable state
