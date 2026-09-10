Traffic Control Software

Dynamic Replanning Specification

1\. 목적



Dynamic Replanning Engine은 실행 중인 Robot의 route가 더 이상 적합하지 않을 때 새로운 route를 계산하고 Traffic Controller가 이를 안전하게 적용하도록 한다.



본 시스템에서는 사람이 robot의 이동 경로를 막거나, 다른 robot이 정지하거나, corridor가 장시간 점유되는 상황이 빈번하기 때문에 Dynamic Replanning이 핵심 기능이다.



2\. 기본 원칙



전체 fleet을 매번 재계획하지 않는다.



Event

&#x20; |

&#x20; v

Affected Resource

&#x20; |

&#x20; v

Affected Robots

&#x20; |

&#x20; v

Replanning





즉, 가능한 경우 local replanning을 우선한다.



3\. Replanning Trigger



다음 event에서 replanning을 검토한다.



HUMAN\_BLOCKAGE

ROBOT\_BLOCKAGE

ROBOT\_FAILURE

COMMUNICATION\_LOST

RESOURCE\_BLOCKED

RESERVATION\_CONFLICT

ROUTE\_INVALID

CONGESTION

DEADLOCK

MAP\_CHANGED



4\. Human Blockage



사람이 corridor에 들어온 경우:



Robot

&#x20; |

&#x20; v

Human Detected

&#x20; |

&#x20; v

Robot STOP / WAIT

&#x20; |

&#x20; +---- short blockage ----> WAIT

&#x20; |

&#x20; +---- long blockage ----> REPLAN





초기 정책은 다음과 같이 설정한다.



blockage < threshold

&#x20;   -> WAIT



blockage >= threshold

&#x20;   -> REPLAN



5\. Blockage Threshold



Configuration:



human\_blockage\_replan\_threshold





예:



human\_blockage\_replan\_threshold = 10 sec





실제 값은 simulation benchmark를 통해 결정한다.



6\. Replanning Scope



Replanning scope는 다음 단계로 확장한다.



Level 1



Blocked robot만 replanning.



Level 2



Blocked resource를 사용하는 robot.



Level 3



Blocked resource 이후의 traffic chain에 포함된 robot.



Level 4



Deadlock에 관련된 모든 robot.



7\. Affected Robot Detection



예:



C01 blocked



R01 → C01

R02 → C01

R03 → C02 → C01





Affected:



R01

R02

R03





Traffic dependency graph를 이용해 affected robot을 찾는다.



8\. Replanning Request

ReplanningRequest {

&#x20;   robot\_id



&#x20;   current\_position

&#x20;   current\_node



&#x20;   goal\_node



&#x20;   current\_route



&#x20;   blocked\_resources

&#x20;   blocked\_edges



&#x20;   current\_reservations



&#x20;   reason



&#x20;   priority

}



9\. Replanning Response

ReplanningResult {

&#x20;   robot\_id



&#x20;   status



&#x20;   old\_route

&#x20;   new\_route



&#x20;   cost



&#x20;   reason



&#x20;   planning\_latency

}





Status:



SUCCESS

NO\_ROUTE

WAIT\_REQUIRED

RECOVERY\_REQUIRED

FAILED



10\. Route Validity



현재 route가 다음 조건을 만족하는지 확인한다.



Resource available

AND

Edge enabled

AND

Node reachable

AND

Reservation feasible



11\. Replanning Policy



기본 정책:



Current Route

&#x20;     |

&#x20;     v

Is Valid?

&#x20;     |

&#x20; +---+---+

&#x20;YES     NO

&#x20; |       |

Continue  Replan



12\. Replanning Cost



새 route가 기존 route보다 크게 좋아지는 경우에만 route를 변경한다.



new\_cost < old\_cost \* (1 - improvement\_threshold)





예:



improvement\_threshold = 0.10



13\. Route Stability



짧은 시간 안에 route가 계속 변경되지 않도록 한다.



minimum\_route\_hold\_time





예:



minimum\_route\_hold\_time = 5 sec





단, safety-critical 상황이나 deadlock recovery에서는 예외적으로 route 변경을 허용한다.



14\. Replanning Priority



Replanning priority:



DEADLOCK

ROBOT\_FAILURE

SAFETY\_BLOCKAGE

RESOURCE\_BLOCKAGE

LONG\_HUMAN\_BLOCKAGE

CONGESTION

NORMAL\_OPTIMIZATION





높은 priority event를 먼저 처리한다.



15\. Replanning Queue



동시에 여러 replanning event가 발생할 수 있다.



Replanning Queue

&#x20;   |

&#x20;   +-- R01 DEADLOCK

&#x20;   +-- R05 ROBOT\_FAILURE

&#x20;   +-- R12 HUMAN\_BLOCKAGE

&#x20;   +-- R20 CONGESTION





Priority Queue를 사용한다.



16\. Debouncing



Human이 순간적으로 감지/해제되는 경우:



blocked

unblocked

blocked

unblocked





replanning이 반복될 수 있다.



따라서 blockage event에 debounce를 적용한다.



blockage\_confirm\_time



17\. Replanning Cancellation



이미 처리할 필요가 없어진 request는 취소한다.



예:



Human leaves corridor

&#x20;       |

&#x20;       v

Blockage resolved

&#x20;       |

&#x20;       v

Cancel replanning



18\. Reservation과 Replanning



새 route를 계산했다고 즉시 기존 reservation을 삭제하지 않는다.



순서는:



Calculate New Route

&#x20;      |

&#x20;      v

Validate New Route

&#x20;      |

&#x20;      v

Acquire New Reservation

&#x20;      |

&#x20;      v

Commit

&#x20;      |

&#x20;      v

Release Old Reservation



19\. Two-Phase Route Change



Route 변경은 다음 단계로 수행한다.



Phase 1



Prepare



new route

new reservation



Phase 2



Commit



activate new route

release old route





이를 통해 route 변경 중 traffic inconsistency를 방지한다.



20\. Failed Replanning



새 route가 없으면 즉시 실패 처리하지 않는다.



다음 순서로 처리한다.



NO\_ROUTE

&#x20;  |

&#x20;  v

WAIT

&#x20;  |

&#x20;  v

Retry

&#x20;  |

&#x20;  +---- success

&#x20;  |

&#x20;  +---- failure

&#x20;         |

&#x20;         v

&#x20;      Recovery



21\. Retry



Configuration:



replanning\_retry\_interval

max\_replanning\_retry





예:



retry\_interval = 2 sec

max\_retry = 5



22\. Cascading Replanning



하나의 resource blockage가 여러 robot에게 영향을 줄 수 있다.



C01 blocked

&#x20;|

&#x20;+-- R01

&#x20;+-- R02

&#x20;+-- R03

&#x20;      |

&#x20;      v

R03 replanning

&#x20;      |

&#x20;      v

C02 congestion

&#x20;      |

&#x20;      v

R04 replanning





이런 cascade를 제한하기 위해:



max\_replanning\_batch





를 설정할 수 있다.



23\. Congestion Replanning



Congestion은 단순 blockage와 다르게 처리한다.



예:



Resource occupancy

waiting robots

expected waiting time





를 기반으로 congestion score를 계산한다.



congestion\_score =

&#x20;   w1 \* occupancy

&#x20; + w2 \* queue\_length

&#x20; + w3 \* waiting\_time



24\. Replanning Hysteresis



Congestion이 조금 변했다고 route를 계속 변경하지 않는다.



다음 조건을 만족할 때만 route 변경:



new\_cost + switching\_penalty < old\_cost



25\. Replanning Event Log



모든 replanning을 기록한다.



timestamp

robot\_id



reason



old\_route

new\_route



old\_cost

new\_cost



blocked\_resource



planning\_latency



result



26\. API

request\_replanning()

cancel\_replanning()

get\_replanning\_status()



detect\_affected\_robots()

validate\_route()

commit\_route\_change()



27\. Acceptance Criteria



다음 상황을 simulation으로 검증한다.



Human enters corridor

Robot stops

Blockage persists

Replanning occurs

Alternative route found

New reservation acquired

Robot resumes





그리고:



No alternative route

→ WAIT

→ retry

→ recovery





도 검증한다.



28\. Test Requirements

test\_human\_blockage

test\_robot\_blockage

test\_robot\_failure

test\_resource\_blockage

test\_affected\_robot\_detection

test\_replanning

test\_no\_route

test\_retry

test\_route\_hysteresis

test\_route\_change\_commit

test\_old\_reservation\_release

test\_replanning\_priority

test\_replanning\_debounce

