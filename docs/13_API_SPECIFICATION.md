Traffic Control Software

API Specification

1\. 목적



Traffic Controller와 외부 시스템 간 API를 정의한다.



2\. API Architecture

WMS / MES

&#x20;  |

&#x20;  v

Task API

&#x20;  |

&#x20;  v

Traffic Controller

&#x20;  |

&#x20;  v

Robot Adapter



3\. Task API

POST /tasks



Task 생성.



Request:



{

&#x20; "task\_id": "T001",

&#x20; "robot\_id": "R001",

&#x20; "source": "A01",

&#x20; "destination": "B01",

&#x20; "priority": 50

}





Response:



{

&#x20; "task\_id": "T001",

&#x20; "status": "ACCEPTED"

}



4\. Cancel Task

POST /tasks/{task\_id}/cancel



5\. Robot State

POST /robots/{robot\_id}/state





Example:



{

&#x20; "timestamp": 1710000000,

&#x20; "node\_id": "C01",

&#x20; "status": "MOVING",

&#x20; "battery": 82

}



6\. Robot Command

POST /robots/{robot\_id}/commands





Request:



{

&#x20; "command\_id": "CMD001",

&#x20; "type": "MOVE",

&#x20; "target": "C02"

}



7\. Reservation API

POST /reservations





Request:



{

&#x20; "robot\_id": "R001",

&#x20; "resources": \[

&#x20;   {

&#x20;     "resource\_id": "C01",

&#x20;     "start": 1000,

&#x20;     "end": 1010

&#x20;   }

&#x20; ]

}



8\. Replanning API

POST /robots/{robot\_id}/replan





Request:



{

&#x20; "reason": "HUMAN\_BLOCKAGE",

&#x20; "blocked\_resources": \[

&#x20;   "C01"

&#x20; ]

}



9\. Deadlock API

GET /traffic/deadlocks





Response:



{

&#x20; "deadlocks": \[

&#x20;   {

&#x20;     "id": "DL001",

&#x20;     "robots": \["R01", "R02", "R03"],

&#x20;     "status": "CONFIRMED"

&#x20;   }

&#x20; ]

}



10\. Robot State API

GET /robots/{robot\_id}



11\. Traffic State

GET /traffic/state





Response:



{

&#x20; "active\_robots": 143,

&#x20; "waiting\_robots": 12,

&#x20; "blocked\_robots": 3,

&#x20; "active\_reservations": 281,

&#x20; "deadlocks": 0

}



12\. Map API

GET /map

GET /map/resources

GET /map/corridors



13\. Health

GET /health

GET /ready

GET /metrics



14\. Error Model

{

&#x20; "error\_code": "RESOURCE\_CONFLICT",

&#x20; "message": "Resource is already reserved",

&#x20; "request\_id": "REQ001"

}



15\. Error Codes

INVALID\_REQUEST

ROBOT\_NOT\_FOUND

TASK\_NOT\_FOUND

RESOURCE\_NOT\_FOUND



RESOURCE\_CONFLICT

RESERVATION\_EXPIRED



NO\_ROUTE

PLANNING\_TIMEOUT



DEADLOCK\_DETECTED

RECOVERY\_FAILED



STALE\_EVENT

DUPLICATE\_EVENT



INTERNAL\_ERROR



16\. Idempotency



Task 생성, reservation, command API는 idempotency를 지원한다.



Header:



Idempotency-Key



17\. Event API



External system으로 다음 event를 publish한다.



TaskAccepted

TaskStarted

TaskCompleted

TaskFailed



RobotBlocked

RobotRecovered

RobotFailed



DeadlockDetected

DeadlockResolved



ReservationGranted

ReservationReleased



ReplanningStarted

ReplanningCompleted



18\. API Version



초기:



/api/v1





Breaking change 발생 시:



/api/v2



19\. API Design Principle



API는 traffic algorithm을 외부에 노출하지 않는다.



외부 시스템:



"R01을 움직여라"





가 아니라:



"R01에 Task T01을 할당"





하도록 설계한다.



Traffic Controller가 실제 movement를 결정한다.

