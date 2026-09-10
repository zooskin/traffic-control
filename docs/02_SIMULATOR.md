Traffic Control Software

Traffic Simulator Specification

1\. 목적



실제 robot 없이 100\~200대 규모의 fleet traffic을 재현하고 Traffic Control 알고리즘을 검증할 수 있는 simulator를 개발한다.



2\. Simulator Architecture

Simulation World

&#x20;   |

&#x20;   +-- Map

&#x20;   |

&#x20;   +-- Robot Simulator

&#x20;   |

&#x20;   +-- Human Simulator

&#x20;   |

&#x20;   +-- Task Generator

&#x20;   |

&#x20;   +-- Traffic Controller

&#x20;   |

&#x20;   +-- Event Engine

&#x20;   |

&#x20;   +-- Metrics

&#x20;   |

&#x20;   +-- Replay



3\. Simulation Loop



기본 simulation cycle:



Update World

&#x20;   ↓

Update Human

&#x20;   ↓

Update Robot

&#x20;   ↓

Detect Events

&#x20;   ↓

Traffic Controller

&#x20;   ↓

Planner

&#x20;   ↓

Reservation

&#x20;   ↓

Generate Robot Commands

&#x20;   ↓

Execute Commands

&#x20;   ↓

Collect Metrics



4\. Robot Simulator



Robot은 다음 동작을 지원해야 한다.



MOVE

STOP

WAIT

RESUME

REROUTE

ARRIVE

FAIL





Robot의 실제 위치는 route와 velocity를 기반으로 simulation한다.



5\. Human Simulator



Human은 다음 동작을 지원한다.



MOVE

STOP

ENTER\_CORRIDOR

EXIT\_CORRIDOR

RANDOM\_WALK





Human blockage duration은 configurable해야 한다.



예:



min = 2 sec

max = 30 sec



6\. Task Generator



Task arrival rate를 설정할 수 있어야 한다.



예:



0.1 task/sec

0.5 task/sec

1 task/sec

2 task/sec





Task는 random seed 기반으로 생성할 수 있어야 한다.



7\. Scenario



다음 scenario를 반드시 지원한다.



S01 Normal



50 robots normal traffic.



S02 Narrow Corridor



100 robots가 좁은 corridor를 사용한다.



S03 Human Stop



100 robots + random human blockage.



S04 Heavy Human



200 robots + 빈번한 human blockage.



S05 Robot Failure



Corridor 내부 robot failure.



S06 Deadlock



의도적인 cyclic deadlock 발생.



S07 Communication Delay



Robot state update delay.



S08 Heavy Congestion



높은 task arrival rate.



8\. Deterministic Simulation



Simulator는 seed를 지원해야 한다.



seed = 12345





같은:



Map

Robot State

Task Sequence

Human Sequence

Seed





를 사용하면 동일한 결과가 나와야 한다.



9\. Replay



Simulation 결과를 저장하고 동일 simulation을 replay할 수 있어야 한다.



저장 정보:



timestamp

robot states

human states

tasks

events

reservations

traffic decisions



10\. Metrics



Simulation 종료 후 다음 값을 자동 계산한다.



Collision Count

Near Miss

Travel Time

Waiting Time

Task Throughput

Deadlock Count

Recovery Time

Planning Latency

CPU

Memory



11\. Acceptance Criteria



최소 다음 simulation이 성공해야 한다.



10 robots

50 robots

100 robots

200 robots





200 robots 기준 최소 1시간 simulation을 안정적으로 수행할 수 있어야 한다.

