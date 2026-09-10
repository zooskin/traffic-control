Traffic Control Software

Data Model Specification

1\. 목적



Traffic Control System에서 사용하는 핵심 domain model을 정의한다.



2\. Robot

Robot {

&#x20;   robot\_id

&#x20;   status



&#x20;   position

&#x20;   current\_node



&#x20;   current\_edge



&#x20;   destination



&#x20;   current\_task



&#x20;   route



&#x20;   battery



&#x20;   capability



&#x20;   last\_update

}



3\. Robot Status

IDLE

ASSIGNED

PLANNING

RESERVING

MOVING

WAITING

BLOCKED

REPLANNING

RECOVERY

FAILED

COMPLETED



4\. Node

Node {

&#x20;   node\_id

&#x20;   x

&#x20;   y



&#x20;   type



&#x20;   capacity

}





Node Type:



NORMAL

INTERSECTION

CORRIDOR\_ENTRY

CORRIDOR\_EXIT

WAITING\_BAY

CHARGER

STATION



5\. Edge

Edge {

&#x20;   edge\_id



&#x20;   from

&#x20;   to



&#x20;   length

&#x20;   speed



&#x20;   capacity



&#x20;   direction



&#x20;   enabled

}



6\. Resource



Traffic control에서 실제 conflict를 판단하는 기본 단위.



Resource {

&#x20;   resource\_id



&#x20;   type



&#x20;   capacity



&#x20;   conflict\_set

}



7\. Resource Type

EDGE

NODE

CORRIDOR

INTERSECTION

WAITING\_BAY

STATION



8\. Corridor

Corridor {

&#x20;   corridor\_id



&#x20;   entry\_node

&#x20;   exit\_node



&#x20;   edges



&#x20;   capacity



&#x20;   direction\_mode



&#x20;   current\_direction



&#x20;   direction\_lock\_until

}



9\. Reservation

Reservation {

&#x20;   reservation\_id



&#x20;   robot\_id

&#x20;   resource\_id



&#x20;   start\_time

&#x20;   end\_time



&#x20;   status



&#x20;   priority

}



10\. Reservation Status

REQUESTED

GRANTED

ACTIVE

RELEASED

EXPIRED

CANCELLED



11\. Task

Task {

&#x20;   task\_id



&#x20;   robot\_id



&#x20;   source

&#x20;   destination



&#x20;   priority



&#x20;   created\_at

&#x20;   started\_at

&#x20;   completed\_at



&#x20;   status

}



12\. Task Status

CREATED

ASSIGNED

RUNNING

WAITING

COMPLETED

FAILED

CANCELLED



13\. Route

Route {

&#x20;   route\_id



&#x20;   robot\_id



&#x20;   nodes

&#x20;   edges



&#x20;   estimated\_cost

&#x20;   estimated\_time



&#x20;   created\_at



&#x20;   planner

}



14\. Replanning Request

ReplanningRequest {

&#x20;   request\_id



&#x20;   robot\_id



&#x20;   reason



&#x20;   blocked\_resources



&#x20;   current\_route



&#x20;   priority



&#x20;   created\_at

}



15\. Deadlock

Deadlock {

&#x20;   deadlock\_id



&#x20;   robots



&#x20;   resources



&#x20;   wait\_edges



&#x20;   detected\_at

&#x20;   confirmed\_at

&#x20;   resolved\_at



&#x20;   status



&#x20;   recovery\_attempts

}



16\. Wait Edge

WaitEdge {

&#x20;   waiting\_robot

&#x20;   blocking\_robot



&#x20;   resource\_id



&#x20;   created\_at

}



17\. Human Blockage

HumanBlockage {

&#x20;   blockage\_id



&#x20;   resource\_id



&#x20;   detected\_at

&#x20;   cleared\_at



&#x20;   confidence



&#x20;   status

}



18\. Event

Event {

&#x20;   event\_id



&#x20;   type



&#x20;   timestamp



&#x20;   source



&#x20;   payload

}



19\. Event Type

ROBOT\_STATE\_UPDATED

ROBOT\_BLOCKED

ROBOT\_FAILED

ROBOT\_RECOVERED



HUMAN\_DETECTED

HUMAN\_CLEARED



TASK\_CREATED

TASK\_CANCELLED

TASK\_COMPLETED



RESERVATION\_GRANTED

RESERVATION\_RELEASED



REPLANNING\_REQUESTED

REPLANNING\_COMPLETED



DEADLOCK\_DETECTED

DEADLOCK\_RESOLVED



MAP\_CHANGED



20\. Historical Data



운영 분석을 위해 다음 데이터를 저장한다.



Robot trajectory

Reservation history

Task history

Route changes

Deadlock history

Replanning history

Waiting history

Traffic decisions



21\. Time Representation



모든 timestamp는 UTC 기반 Unix epoch 또는 ISO-8601 중 하나로 통일한다.



Runtime 내부에서는 monotonic clock을 사용할 수 있다.



22\. ID 규칙



ID는 사람이 읽을 수 있어야 한다.



예:



R001

T000123

C012

RES000123

DL00012





Production에서는 collision-free unique ID를 보장한다.



23\. Serialization



외부 API:



JSON





내부 high-performance communication:



Protocol Buffers





를 사용할 수 있다.



24\. Schema Evolution



Data schema 변경 시 backward compatibility를 고려한다.



필드 삭제보다:



deprecated





처리를 우선한다.

