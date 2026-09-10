Traffic Control Software

Domain Model Specification

1\. Purpose



본 문서는 Multi-Robot Traffic Control Software에서 사용하는 핵심 Domain Entity와 Value Object를 정의한다.



Domain Model은 Infrastructure와 분리한다.



2\. Domain Overview

Robot

&#x20; |

&#x20; +---- RobotState

&#x20; |

&#x20; +---- Task

&#x20; |

&#x20; +---- Route

&#x20; |

&#x20; +---- Reservation

&#x20; |

&#x20; +---- Conflict

&#x20; |

&#x20; +---- TrafficDecision





Map:



Map

&#x20;|

&#x20;+---- Node

&#x20;|

&#x20;+---- Edge

&#x20;|

&#x20;+---- Corridor

&#x20;|

&#x20;+---- Intersection



3\. Robot



Robot은 실제 Fleet의 이동 주체다.



Robot

├── robot\_id

├── state

├── current\_task

├── current\_route

└── capabilities





필수 식별자는 robot\_id다.



Robot Entity는 Vendor SDK에 의존하지 않는다.



4\. RobotState



Robot의 현재 상태를 표현한다.



robot\_id

position

velocity

current\_node

current\_edge

current\_task

current\_route

state

timestamp

state\_version





State:



MOVING

IDLE

WAITING

TEMPORARILY\_STOPPED

BLOCKED

FAILED

UNKNOWN



5\. RobotState Transition



권장 Transition:



IDLE

&#x20;|

&#x20;v

MOVING

&#x20;|

&#x20;+----> WAITING

&#x20;|

&#x20;+----> TEMPORARILY\_STOPPED

&#x20;|

&#x20;+----> BLOCKED

&#x20;|

&#x20;+----> FAILED

&#x20;|

&#x20;v

IDLE





잘못된 Transition은 명시적으로 거부할 수 있어야 한다.



6\. Task



Robot에게 주어진 작업이다.



task\_id

robot\_id

source

destination

priority

deadline

status

created\_at





Task State:



CREATED

ASSIGNED

PLANNING

RUNNING

WAITING

COMPLETED

CANCELLED

FAILED



7\. Node



Graph의 위치 단위다.



node\_id

position

type





Node Type 예:



NORMAL

INTERSECTION

CHARGING

LOADING

UNLOADING

HOLDING



8\. Edge



두 Node를 연결하는 이동 경로다.



edge\_id

from\_node

to\_node

length

width

direction

speed\_limit

capacity

resource\_id





Direction:



FORWARD

REVERSE

BIDIRECTIONAL



9\. Corridor



좁고 긴 이동 영역이다.



Corridor는 Traffic Resource로 취급할 수 있다.



corridor\_id

entry\_node

exit\_node

length

width

capacity

direction





필요한 경우 내부 Edge 여러 개를 하나의 Corridor Resource로 묶을 수 있다.



10\. Intersection



여러 이동 경로가 만나는 영역이다.



intersection\_id

connected\_nodes

connected\_edges

capacity





Intersection은 Conflict Detection과 Reservation의 주요 대상이다.



11\. Route



Robot이 이동할 경로다.



route\_id

robot\_id

map\_version

nodes

edges

created\_at

expires\_at





Route에는 Version을 부여할 수 있다.



12\. Route Segment



Route의 개별 이동 단위다.



edge\_id

sequence

expected\_entry\_time

expected\_exit\_time





이를 기반으로 Temporal Conflict를 검출할 수 있다.



13\. Reservation



Robot이 특정 Resource를 사용할 권리를 예약한 것이다.



reservation\_id

robot\_id

resource\_id

start\_time

end\_time

priority

state





State:



PENDING

ACTIVE

RELEASED

EXPIRED

CANCELLED



14\. Resource



Traffic Control에서 경쟁적으로 사용되는 자원이다.



예:



NODE

EDGE

CORRIDOR

INTERSECTION

CHARGING\_AREA

LOADING\_AREA





Resource에는 고유한 resource\_id가 있어야 한다.



15\. Conflict



둘 이상의 Robot이 동시에 특정 Resource를 사용할 수 없는 상황이다.



Conflict

├── conflict\_id

├── robot\_a

├── robot\_b

├── resource

├── conflict\_type

├── severity

└── detected\_at



16\. Conflict Type

NODE\_CONFLICT

EDGE\_CONFLICT

HEAD\_ON

CROSSING

CORRIDOR\_CONFLICT

RESOURCE\_CONFLICT

TEMPORAL\_CONFLICT



17\. Priority



Traffic Decision에 사용되는 우선순위다.



Input:



base\_priority

waiting\_time

deadline

blocking\_impact

task\_priority





Output:



effective\_priority





Priority는 가능한 경우 deterministic해야 한다.



18\. TrafficEvent



Traffic Control을 동작시키는 입력이다.



event\_id

event\_type

timestamp

robot\_id

state\_version

map\_version

payload





Event Type:



ROBOT\_STATE\_UPDATED

ROBOT\_STOPPED

ROBOT\_BLOCKED

ROBOT\_RECOVERED

TASK\_CREATED

TASK\_COMPLETED

TASK\_CANCELLED

RESERVATION\_EXPIRED

MAP\_UPDATED



19\. TrafficDecision



Traffic Controller가 생성하는 최종 Traffic 명령이다.



decision\_id

robot\_id

action

route

reservation

reason

created\_at

state\_version





Action:



GO

WAIT

STOP

REPLAN

HOLD

RECOVER



20\. PlanningRequest



Planner에 전달되는 입력이다.



request\_id

robot\_ids

start\_states

goals

map\_version

traffic\_state\_version

reservations

constraints

deadline

timeout



21\. PlanningResult



Planner가 반환하는 결과다.



request\_id

status

routes

map\_version

traffic\_state\_version

planning\_time





Status:



SUCCESS

NO\_PATH

TIMEOUT

CANCELLED

FAILED



22\. Deadlock



둘 이상의 Robot이 서로의 진행을 막아 영구적으로 진행하지 못하는 상태다.



deadlock\_id

robots

resources

type

detected\_at

status





Type:



RESOURCE\_DEADLOCK

CORRIDOR\_DEADLOCK

HEAD\_ON\_DEADLOCK

CYCLE\_DEADLOCK



23\. DeadlockRecovery



Deadlock 해소 작업이다.



recovery\_id

deadlock\_id

strategy

target\_robots

status

created\_at





Strategy:



REPLAN

CHANGE\_PRIORITY

BACKTRACK

RELEASE\_RESOURCE

MOVE\_TO\_HOLDING

HUMAN\_INTERVENTION



24\. HumanBlockage



Human 또는 Human Activity에 의해 Traffic Resource가 막힌 상태다.



blockage\_id

resource\_id

detected\_at

estimated\_duration

confidence

status





Status:



FREE

TEMPORARILY\_BLOCKED

LONG\_BLOCKED

UNKNOWN



25\. Value Objects



가능한 경우 다음을 Value Object로 정의한다.



RobotId

TaskId

NodeId

EdgeId

RouteId

ReservationId

ResourceId

Position

Velocity

Timestamp

Duration

Priority

26\. ID Rules



ID는 Domain 내에서 고유해야 한다.



예:



R001

TASK-00001

EDGE-001

CORRIDOR-01

RES-00001





실제 형식은 Implementation 단계에서 결정한다.



27\. Time Model



Traffic Planning에서는 시간 정보가 중요하다.



가능하면 다음을 구분한다.



Timestamp

Duration

ExpectedEntryTime

ExpectedExitTime

Deadline





모든 시간은 동일한 기준을 사용한다.



28\. Version Model



다음 Entity에는 Version을 사용할 수 있어야 한다.



RobotState

Map

Route

TrafficDecision

PlanningRequest

PlanningResult





Version은 Stale Result를 방지하는 데 사용한다.



29\. Entity Relationship

Robot

&#x20;|

&#x20;+---- Task

&#x20;|

&#x20;+---- RobotState

&#x20;|

&#x20;+---- Route

&#x20;|       |

&#x20;|       +---- Edge

&#x20;|

&#x20;+---- Reservation

&#x20;        |

&#x20;        +---- Resource



Robot + Route + Reservation

&#x20;         |

&#x20;         v

&#x20;      Conflict

&#x20;         |

&#x20;         v

&#x20;      Priority

&#x20;         |

&#x20;         v

&#x20;   TrafficDecision



30\. Domain Invariants



다음 조건은 항상 만족해야 한다.



Robot

robot\_id는 unique

Robot은 하나의 current state를 가진다.

Task

Task는 하나의 Robot에 할당될 수 있다.

완료된 Task는 다시 RUNNING으로 전환하지 않는다.

Reservation

동일 Resource의 충돌하는 Reservation은 동시에 ACTIVE일 수 없다.

RELEASED Reservation은 다시 ACTIVE로 변경하지 않는다.

Route

Route는 유효한 Map Version을 참조한다.

Route의 Node/Edge 연결이 유효해야 한다.

31\. Domain vs Infrastructure



Domain에서 직접 의존하지 않는다.



Database

Network

ROS

gRPC

Vendor SDK

Filesystem





예:



나쁜 구조:



Robot

&#x20;|

&#x20;+-- ROS API

&#x20;+-- Database





좋은 구조:



Robot

&#x20;|

Robot Domain

&#x20;|

Adapter / Repository

&#x20;|

Infrastructure



32\. Serialization



Domain Object 자체에 Serialization 책임을 과도하게 넣지 않는다.



권장:



Domain Object

&#x20;     |

Mapper

&#x20;     |

DTO / Proto

&#x20;     |

Network



33\. Equality



ID가 있는 Entity는 기본적으로 Identity를 기준으로 비교한다.



Value Object는 Value Equality를 사용한다.



예:



RobotId("R01") == RobotId("R01")



34\. Domain Validation



Invalid State는 생성 단계에서 가능한 한 방지한다.



예:



Reservation:

start\_time < end\_time



Route:

nodes != empty



Task:

source != destination





실제 규칙은 Traffic Specification과 일치해야 한다.



35\. Domain Evolution



Domain Model 변경은 기존 데이터와 API에 영향을 줄 수 있다.



변경 시:



Compatibility 확인

Serialization 확인

Test 수정

Migration 필요성 확인

36\. Final Principle



Domain Model은 Traffic Control의 공통 언어다.



다음 Component들이 동일한 Domain 의미를 사용해야 한다.



Traffic Controller

Planner

Reservation Manager

Conflict Detector

Deadlock Manager

Simulation

Monitoring





Domain의 의미를 Component마다 다르게 정의하지 않는다.

