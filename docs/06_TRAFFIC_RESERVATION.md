Traffic Control Software

Traffic Reservation Engine Specification

1\. 목적



Reservation Engine은 Robot이 Traffic Resource를 사용할 수 있는 권리를 관리한다.



핵심 질문:



"어떤 Robot이 어떤 공간을 언제 사용할 수 있는가?"



2\. Resource



Reservation 대상은 다음과 같다.



CORRIDOR

INTERSECTION

WAITING\_BAY

STATION

CHARGER



3\. Reservation 기본 구조

Reservation {

&#x20;   reservation\_id



&#x20;   robot\_id

&#x20;   resource\_id



&#x20;   start\_time

&#x20;   end\_time



&#x20;   priority



&#x20;   status



&#x20;   created\_at



&#x20;   version

}



4\. Reservation Status

REQUESTED

GRANTED

ACTIVE

EXTENDED

RELEASED

CANCELLED

EXPIRED



5\. Reservation Request

ReservationRequest {

&#x20;   robot\_id

&#x20;   resource\_id



&#x20;   requested\_start\_time

&#x20;   estimated\_duration



&#x20;   priority



&#x20;   route\_id

}



6\. 기본 Flow

Robot

&#x20; |

&#x20; v

Request Resource

&#x20; |

&#x20; v

Reservation Manager

&#x20; |

&#x20; +---- Conflict 없음 ----> GRANT

&#x20; |

&#x20; +---- Conflict 있음 ----> WAIT



7\. Conflict Detection



두 reservation A, B가 동일 resource를 사용한다고 한다.



다음 조건이면 time conflict이다.



A.start < B.end

AND

B.start < A.end





그리고:



overlapping reservations > resource.capacity





이면 conflict이다.



8\. Capacity



일반 resource:



capacity = N





Narrow corridor:



capacity = 1





Intersection:



conflict group 기준으로 판단한다.



9\. Corridor Reservation



좁은 단일 corridor:



C01

capacity = 1





Robot R01이:



10:00:00 \~ 10:00:10





예약했다면 R02는 해당 시간에 C01에 진입할 수 없다.



10\. Corridor Direction



Bidirectional corridor에서는 방향 충돌을 고려한다.



예:



A ---------------- B



R01: A → B

R02: B → A





동일 corridor에서 동시에 진입할 수 없다면:



R01 → GRANT

R02 → WAIT





로 처리한다.



11\. Intersection Reservation



Intersection에서는 movement conflict를 검사한다.



예:



M01 = A → C

M02 = B → D





Conflict:



M01 ↔ M02





이면 동시에 reservation할 수 없다.



12\. Reservation Window



Robot의 resource 사용은 time window로 표현한다.



\[entry\_time, exit\_time]





예:



C01

10.0 sec \~ 18.0 sec



13\. Safety Buffer



실제 Robot의 위치 오차와 통신 지연을 고려해 buffer를 둘 수 있다.



effective\_start =

start\_time - entry\_buffer



effective\_end =

end\_time + exit\_buffer





예:



entry\_buffer = 0.5 sec

exit\_buffer = 1.0 sec





값은 configuration으로 관리한다.



14\. Reservation Grant



Reservation Manager가 grant할 때 다음을 반환한다.



ReservationResult {

&#x20;   status



&#x20;   reservation\_id



&#x20;   resource\_id



&#x20;   start\_time

&#x20;   end\_time



&#x20;   reason

}





Status:



GRANTED

WAIT

REJECTED



15\. WAIT



Resource가 사용 중이면 WAIT를 반환한다.



status = WAIT





예상 가능한 경우:



next\_available\_time





을 반환한다.



16\. Priority



Conflict 발생 시 priority를 비교한다.



기본 priority:



PriorityScore =

&#x20;   w\_task \* task\_priority

&#x20; + w\_wait \* waiting\_time

&#x20; + w\_deadlock \* deadlock\_risk



17\. Starvation Prevention



높은 priority robot만 계속 resource를 획득하면 낮은 priority robot이 영원히 기다릴 수 있다.



따라서 waiting time에 따라 priority를 증가시킬 수 있다.



effective\_priority =

base\_priority

\+

aging\_factor \* waiting\_time



18\. Reservation Extension



실제 환경에서는 Robot이 예상보다 오래 corridor를 점유할 수 있다.



예:



Human detected

↓

Robot stopped

↓

Travel time 증가





기존 reservation:



10 \~ 20 sec





예상:



10 \~ 30 sec





이 경우 reservation extension을 요청한다.



19\. Extension Flow

Robot

&#x20;↓

Delay Detected

&#x20;↓

Estimate New Exit Time

&#x20;↓

Reservation Extension

&#x20;↓

Conflict Check



20\. Extension Conflict



Extension으로 인해 다음 Robot의 reservation과 충돌할 경우:



R01 extension

&#x20;      |

&#x20;      v

R02 conflict

&#x20;      |

&#x20;      v

R02 WAIT / REPLAN





영향받는 robot을 자동으로 찾을 수 있어야 한다.



21\. Reservation Release



Robot이 resource를 실제로 빠져나오면 reservation을 release한다.



ACTIVE

&#x20; |

&#x20; v

RELEASED





실제 Robot 위치가 resource exit을 통과했는지를 기준으로 release할 수 있어야 한다.



22\. Reservation Expiration



Robot이 reservation을 사용하지 않고 일정 시간이 지나면:



GRANTED

&#x20;  |

&#x20;  v

EXPIRED





처리할 수 있다.



단, 실제 safety 상태를 확인하지 않고 자동 release하면 안 된다.



23\. Reservation Ownership



각 reservation에는 owner가 존재한다.



resource\_id = C01



owner = R01





현재 active reservation을 조회할 수 있어야 한다.



get\_active\_reservation(resource\_id)



24\. Reservation Conflict Query

find\_conflicts(

&#x20;   resource\_id,

&#x20;   start\_time,

&#x20;   end\_time

)





반환:



\[

&#x20;   reservation\_01,

&#x20;   reservation\_02

]



25\. Affected Robot



Reservation 변경으로 영향을 받는 Robot을 조회할 수 있어야 한다.



get\_affected\_robots(

&#x20;   resource\_id,

&#x20;   reservation\_id

)



26\. Reservation Chain



다음 상황을 추적할 수 있어야 한다.



R01 waits for C01

R02 owns C01

R02 waits for C02

R03 owns C02





이는 Deadlock Manager가 사용하는 dependency 정보가 된다.



27\. Reservation vs Route



Route:



R01

A → B → C → D





Reservation:



B

10\~15 sec



C

15\~20 sec



D

20\~25 sec





Route와 Reservation은 별도의 데이터 구조로 관리한다.



28\. Rolling Reservation



전체 route를 장시간 reservation하지 않는다.



초기 구현에서는 일정 horizon까지만 reservation한다.



예:



Reservation Horizon = 10 sec





Robot이 이동하면서 다음 resource reservation을 계속 확보한다.



Current

&#x20;  |

&#x20;  +-- C01

&#x20;  +-- C02

&#x20;  +-- C03



29\. Reservation Horizon



Configuration:



reservation\_horizon = 10 sec





Traffic 특성에 따라 조정한다.



너무 짧으면:



reservation churn 증가

planning frequency 증가



너무 길면:



traffic flexibility 감소

불필요한 resource lock 증가

30\. Commit Zone



Robot이 곧 진입할 resource는 cancellation이 어려운 상태로 관리할 수 있다.



예:



Approach Zone

Commit Zone

Resource





Commit Zone 진입 후에는 reservation을 함부로 변경하지 않는다.



31\. Reservation State Consistency



다음 상태를 일치시켜야 한다.



Robot State

Reservation State

Resource Occupancy





예:



Robot이 C01에서 빠졌는데:



occupancy = 1





로 남아 있으면 traffic이 불필요하게 막힌다.



32\. Recovery



Controller 재시작 시 Reservation을 복구할 수 있어야 한다.



복구 정보:



reservation\_id

robot\_id

resource\_id

start\_time

end\_time

status

version





실제 Robot 위치와 비교하여 reservation을 재검증한다.



33\. Concurrency



Reservation Manager는 동시에 여러 Robot이 reservation을 요청하는 상황을 처리해야 한다.



예:



R01 → Request C01

R02 → Request C01

R03 → Request C01





동시에 요청되어도 resource capacity를 초과해서 grant하면 안 된다.



34\. Atomicity



Reservation 변경은 atomic하게 처리되어야 한다.



Check Conflict

&#x20;     +

Create Reservation





이 두 작업 사이에 다른 요청이 개입해서는 안 된다.



35\. Idempotency



같은 reservation request가 반복되어도 duplicate reservation이 생성되면 안 된다.



request\_id





를 사용한다.



36\. Decision Logging



모든 reservation decision을 기록한다.



예:



robot\_id = R01

resource = C01



decision = WAIT



reason = RESOURCE\_OCCUPIED



owner = R02



expected\_available = 15.5 sec



37\. Reservation API



필수 API:



request\_reservation()

extend\_reservation()

release\_reservation()

cancel\_reservation()



get\_reservation()

get\_active\_reservations()



find\_conflicts()

get\_affected\_robots()



38\. Acceptance Criteria



다음 상황에서 resource conflict가 발생하지 않아야 한다.



Test 1



두 Robot이 동일 corridor를 동시에 진입하려는 경우.



Test 2



서로 반대 방향에서 동일 corridor에 진입하는 경우.



Test 3



Intersection conflict.



Test 4



Robot이 corridor 안에서 정지하는 경우.



Test 5



Reservation extension.



Test 6



동시 reservation request.



Test 7



Controller restart 후 reservation recovery.



39\. Test Requirements

test\_reservation\_grant

test\_reservation\_conflict

test\_corridor\_capacity

test\_bidirectional\_conflict

test\_intersection\_conflict

test\_priority

test\_priority\_aging

test\_reservation\_extension

test\_extension\_conflict

test\_release

test\_expiration

test\_concurrent\_request

test\_idempotency

test\_recovery

test\_affected\_robot



40\. 중요 설계 원칙



Reservation Engine은 단순히 "예약 DB"가 아니다.



다음 역할을 담당하는 Traffic Control의 핵심 component다.



Resource Conflict Detection

&#x20;       +

Priority Decision

&#x20;       +

Time Window Management

&#x20;       +

Traffic Coordination

&#x20;       +

Deadlock Dependency Information





따라서 다른 모듈에서 Reservation 상태를 직접 변경하지 않는다.



모든 변경은 Reservation Manager를 통해 수행한다.

