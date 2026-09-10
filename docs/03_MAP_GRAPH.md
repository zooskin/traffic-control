Traffic Control Software

Map \& Traffic Graph Specification

1\. 목적



본 문서는 실제 물리 공간을 Traffic Control Software가 사용할 수 있는 Graph 구조로 변환하고 관리하기 위한 데이터 모델과 기능을 정의한다.



대상 환경은 다음과 같다.



좁고 긴 corridor

단일 차선

양방향 corridor

교차로

대기 공간

작업장

충전소

사람이 이동하는 공간



Traffic Controller는 물리적인 좌표만을 사용하지 않고 Traffic Graph와 Resource Graph를 함께 사용한다.



2\. 핵심 개념



전체 Map은 다음 구조로 표현한다.



Physical Map

&#x20;    |

&#x20;    v

Traffic Graph

&#x20;    |

&#x20;    +---- Node

&#x20;    |

&#x20;    +---- Edge

&#x20;    |

&#x20;    +---- Resource

&#x20;    |

&#x20;    +---- Conflict Zone

&#x20;    |

&#x20;    +---- Waiting Area



3\. Graph 정의



기본 Graph:



G = (V, E)



V = Node

E = Edge



하지만 Traffic Control을 위해 다음 정보를 추가한다.



Traffic Graph

&#x20;   |

&#x20;   +-- Node

&#x20;   +-- Edge

&#x20;   +-- Resource

&#x20;   +-- Conflict Zone

&#x20;   +-- Waiting Area



4\. Node



Node는 robot이 route에서 통과하거나 정지할 수 있는 logical point이다.



Node {

&#x20;   node\_id

&#x20;   x

&#x20;   y

&#x20;   z

&#x20;   type

&#x20;   orientation

&#x20;   capacity

&#x20;   resource\_id

}



5\. Node Type



다음 type을 지원한다.



NORMAL

INTERSECTION

STATION

PICKUP

DROPOFF

CHARGER

WAITING\_BAY

ENTRY

EXIT



6\. Edge



Edge는 두 Node 사이의 이동 구간이다.



Edge {

&#x20;   edge\_id

&#x20;   from\_node

&#x20;   to\_node



&#x20;   length

&#x20;   width



&#x20;   speed\_limit



&#x20;   direction

&#x20;   capacity



&#x20;   travel\_time



&#x20;   resource\_id



&#x20;   enabled

}



7\. Edge Direction

FORWARD

REVERSE

BIDIRECTIONAL





예를 들어 좁은 단일 corridor가 양방향으로 사용되는 경우:



A ---------------- B

&#x20;      Corridor





다음처럼 표현할 수 있다.



Edge C01

from = A

to = B

direction = BIDIRECTIONAL

capacity = 1



8\. Narrow Corridor



본 프로젝트에서 가장 중요한 resource 중 하나이다.



예:



A ---- C01 ---- B





C01이 단일 차선이라면:



capacity = 1





로 설정한다.



Traffic Controller는 단순히 A\*에서 C01을 통과하는 것뿐만 아니라 C01의 사용권을 관리해야 한다.



9\. Corridor Resource

CorridorResource {

&#x20;   resource\_id



&#x20;   type = CORRIDOR



&#x20;   capacity



&#x20;   allowed\_direction



&#x20;   entry\_node

&#x20;   exit\_node



&#x20;   min\_travel\_time

&#x20;   max\_travel\_time



&#x20;   current\_occupants



&#x20;   reservations

}



10\. Intersection



Intersection은 여러 Edge가 만나는 지점이다.



예:



&#x20;       B

&#x20;       |

&#x20;       |

A ------X------ C

&#x20;       |

&#x20;       |

&#x20;       D





X를 Intersection Resource로 관리한다.



IntersectionResource {

&#x20;   resource\_id



&#x20;   type = INTERSECTION



&#x20;   conflict\_groups



&#x20;   capacity



&#x20;   reservations

}



11\. Conflict Group



Intersection에서는 단순 capacity보다 어떤 movement가 서로 충돌하는지가 중요하다.



예:



A → C

B → D





두 movement가 동시에 실행될 수 없다면:



ConflictGroup:



A-C

B-D





로 정의한다.



12\. Movement



Intersection 내부 이동을 명시적으로 표현할 수 있어야 한다.



Movement {

&#x20;   movement\_id



&#x20;   from\_edge

&#x20;   to\_edge



&#x20;   intersection\_id



&#x20;   conflict\_group

}





예:



M01:

A → C



M02:

B → D



13\. Waiting Bay



좁은 corridor에서 deadlock을 해결하기 위해 waiting bay를 사용할 수 있다.



WaitingBay {

&#x20;   resource\_id



&#x20;   node\_id



&#x20;   capacity



&#x20;   compatible\_robot\_types



&#x20;   reservation

}



14\. Resource



모든 traffic-critical 공간은 Resource로 표현할 수 있어야 한다.



Resource {

&#x20;   resource\_id



&#x20;   type



&#x20;   capacity



&#x20;   current\_occupancy



&#x20;   reservations



&#x20;   enabled

}





Resource Type:



CORRIDOR

INTERSECTION

WAITING\_BAY

STATION

CHARGER



15\. Resource와 Edge의 관계



Edge는 Resource를 참조할 수 있다.



Edge

&#x20;|

&#x20;+-- resource\_id





예:



Edge E01

from = N01

to = N02

resource = C01





이렇게 하면 route가:



N01

&#x20;↓

E01

&#x20;↓

N02





인 경우 Traffic Controller는 E01을 이동하기 전에 C01 reservation을 확인할 수 있다.



16\. Map Version



Map은 반드시 version을 가진다.



Map {

&#x20;   map\_id

&#x20;   version

&#x20;   created\_at

&#x20;   updated\_at



&#x20;   nodes

&#x20;   edges

&#x20;   resources

}





예:



warehouse\_A

version = 12



17\. Map Validation



Map Load 시 다음 항목을 검사해야 한다.



Node

중복 node\_id

invalid coordinate

invalid type

Edge

존재하지 않는 node 참조

length <= 0

speed\_limit <= 0

invalid direction

Resource

중복 resource\_id

존재하지 않는 edge 참조

capacity <= 0

Graph

disconnected graph

unreachable node

orphan node

18\. Reachability



다음 API를 제공한다.



is\_reachable(

&#x20;   start\_node,

&#x20;   goal\_node

)





반환:



true

false



19\. Neighbor Query

get\_neighbors(node\_id)





반환:



\[

&#x20;   edge\_01,

&#x20;   edge\_02,

&#x20;   edge\_03

]



20\. Resource Query

get\_resource(resource\_id)



get\_resources\_for\_edge(edge\_id)



get\_resource\_occupancy(resource\_id)



21\. Route와 Graph



Route는 다음 구조를 가진다.



Route {

&#x20;   route\_id



&#x20;   robot\_id



&#x20;   start\_node

&#x20;   goal\_node



&#x20;   nodes

&#x20;   edges



&#x20;   total\_distance

&#x20;   estimated\_time



&#x20;   created\_at

}





예:



N01

&#x20;↓

E01

&#x20;↓

N02

&#x20;↓

E07

&#x20;↓

N08

&#x20;↓

E12

&#x20;↓

N20



22\. Traffic Graph와 Physical Map의 분리



Physical Map과 Traffic Graph를 분리한다.



Physical Map

&#x20;   |

&#x20;   v

Map Converter

&#x20;   |

&#x20;   v

Traffic Graph





Physical Map 변경이 Traffic Algorithm 코드에 직접 영향을 주지 않도록 한다.



23\. Configuration



다음 값은 configuration으로 관리한다.



default\_speed

corridor\_capacity

intersection\_capacity

robot\_length

robot\_width

safety\_margin

waiting\_bay\_capacity





Algorithm 코드에 hard coding하지 않는다.



24\. Acceptance Criteria



다음 기능이 구현되어야 한다.



Map Load

Map Save

Map Version

Node 조회

Edge 조회

Neighbor 조회

Resource 조회

Reachability

Map Validation

Corridor 정의

Intersection 정의

Conflict Group 정의

Waiting Bay 정의

25\. Test Requirements



최소 다음 test를 구현한다.



test\_map\_load

test\_map\_validation

test\_duplicate\_node

test\_invalid\_edge

test\_reachability

test\_neighbor\_query

test\_resource\_query

test\_corridor

test\_intersection

test\_conflict\_group

test\_waiting\_bay

