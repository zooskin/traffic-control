Traffic Control Software

Robot \& Task Model Specification

1\. 목적



본 문서는 Traffic Controller에서 사용하는 Robot과 Task의 상태 및 lifecycle을 정의한다.



Traffic Controller의 모든 알고리즘은 Robot의 현재 상태를 기준으로 동작해야 한다.



2\. Robot 기본 모델

Robot {

&#x20;   robot\_id



&#x20;   pose

&#x20;   velocity

&#x20;   heading



&#x20;   footprint



&#x20;   status



&#x20;   current\_node

&#x20;   current\_edge



&#x20;   current\_task



&#x20;   route

&#x20;   reservation



&#x20;   battery



&#x20;   last\_update



&#x20;   created\_at

}



3\. Robot Pose

Pose {

&#x20;   x

&#x20;   y

&#x20;   theta

}



4\. Robot Footprint



Traffic Controller에서 corridor 및 intersection 점유 판단을 위해 robot 크기를 관리한다.



Footprint {

&#x20;   length

&#x20;   width

&#x20;   safety\_margin

}



5\. Robot Status

IDLE

ASSIGNED

PLANNING

MOVING

WAITING

BLOCKED

REPLANNING

RECOVERY

ARRIVED

FAILED

EMERGENCY\_STOP



6\. Robot State Machine



정상 lifecycle:



IDLE

&#x20; |

&#x20; v

ASSIGNED

&#x20; |

&#x20; v

PLANNING

&#x20; |

&#x20; v

MOVING

&#x20; |

&#x20; v

ARRIVED

&#x20; |

&#x20; v

IDLE



7\. Waiting



Robot이 traffic resource를 사용하지 못하면 WAITING 상태가 된다.



MOVING

&#x20;  |

&#x20;  v

WAITING

&#x20;  |

&#x20;  +----> MOVING





Waiting 상태에서는 다음 정보를 저장한다.



waiting\_since

waiting\_resource

waiting\_reason



8\. Waiting Reason

RESOURCE\_OCCUPIED

HIGHER\_PRIORITY\_ROBOT

HUMAN\_BLOCKAGE

RESERVATION\_DENIED

CONGESTION

DEADLOCK\_RECOVERY

SAFETY\_STOP



9\. Blocked



Robot이 예상 경로에서 일정 시간 이상 진행하지 못하면 BLOCKED 상태가 될 수 있다.



MOVING

&#x20;  |

&#x20;  v

BLOCKED





Blocked 판단은 configuration으로 관리한다.



예:



blocked\_timeout = 5 sec

minimum\_progress = 0.1 m



10\. Replanning

BLOCKED

&#x20;  |

&#x20;  v

REPLANNING

&#x20;  |

&#x20;  +----> MOVING

&#x20;  |

&#x20;  +----> WAITING

&#x20;  |

&#x20;  +----> RECOVERY



11\. Failed



Robot failure:



MOVING

&#x20;  |

&#x20;  v

FAILED





Failed robot이 점유하고 있던 resource는 상황에 따라 유지하거나 강제 해제할 수 있어야 한다.



안전한 resource release 여부는 별도의 정책으로 결정한다.



12\. Task

Task {

&#x20;   task\_id



&#x20;   pickup\_location

&#x20;   delivery\_location



&#x20;   priority



&#x20;   created\_at

&#x20;   deadline



&#x20;   assigned\_robot



&#x20;   status

}



13\. Task State

CREATED

ASSIGNED

PLANNED

EXECUTING

COMPLETED

FAILED

CANCELLED



14\. Task State Machine

CREATED

&#x20;  |

&#x20;  v

ASSIGNED

&#x20;  |

&#x20;  v

PLANNED

&#x20;  |

&#x20;  v

EXECUTING

&#x20;  |

&#x20;  v

COMPLETED





Failure:



EXECUTING

&#x20;  |

&#x20;  v

FAILED





Cancellation:



CREATED / ASSIGNED / PLANNED

&#x20;  |

&#x20;  v

CANCELLED



15\. Task Priority



Priority는 숫자로 표현한다.



priority = 0 \~ 100





높은 숫자가 높은 우선순위를 의미한다.



단순 priority만 사용하지 않고 waiting time 등을 Traffic Priority에서 별도로 고려한다.



16\. Task Assignment



Task와 Robot assignment는 별도 모듈에서 담당한다.



Task Manager

&#x20;     |

&#x20;     v

Assignment Engine

&#x20;     |

&#x20;     v

Robot





Traffic Controller는 assignment 결과를 받아 route planning을 수행한다.



17\. Robot Route



Robot은 현재 route를 가진다.



Robot

&#x20;|

&#x20;+-- current\_route

&#x20;|

&#x20;+-- current\_route\_index





예:



Route:

N01

N05

N07

N12

N20





현재 위치:



current\_route\_index = 2



18\. Robot Reservation



Robot은 여러 reservation을 가질 수 있다.



예:



R01

&#x20;|

&#x20;+-- C01

&#x20;+-- I03

&#x20;+-- C07





Reservation은 시간 순서대로 관리한다.



19\. Robot Event



Robot 상태 변경은 event를 생성한다.



RobotStateChanged





예:



robot\_id = R01



old\_state = MOVING

new\_state = WAITING



reason = RESOURCE\_OCCUPIED

resource\_id = C01



20\. State Update



Robot은 주기적으로 상태를 전송한다.



예:



RobotStateUpdate {

&#x20;   robot\_id

&#x20;   timestamp



&#x20;   x

&#x20;   y

&#x20;   theta



&#x20;   velocity



&#x20;   current\_node

&#x20;   current\_edge



&#x20;   battery



&#x20;   status

}



21\. State Timeout



일정 시간 동안 Robot 상태가 업데이트되지 않으면 timeout 상태를 감지한다.



last\_update

&#x20;       |

&#x20;       v

timeout?

&#x20;       |

&#x20;       +---- YES ---> COMMUNICATION\_LOST





COMMUNICATION\_LOST는 Robot 내부 상태와 별도로 system event로 관리할 수 있다.



22\. State Consistency



Traffic Controller가 가진 Robot 상태와 실제 Robot 상태가 다를 수 있다.



따라서 다음 정보를 관리한다.



controller\_timestamp

robot\_timestamp

last\_command\_id

last\_ack\_command\_id



23\. Command



Traffic Controller가 Robot에 보내는 command:



RobotCommand {

&#x20;   command\_id

&#x20;   robot\_id



&#x20;   action



&#x20;   route

&#x20;   target\_node



&#x20;   created\_at

}





Action:



MOVE

WAIT

STOP

RESUME

REROUTE

GO\_TO\_WAITING\_BAY





Emergency Stop은 일반 Traffic Command와 분리한다.



24\. Acceptance Criteria



다음 기능을 구현해야 한다.



Robot lifecycle

Task lifecycle

Robot state update

Robot state timeout

Task assignment interface

Route association

Reservation association

Robot command

State transition validation

Event generation

25\. Test Requirements

test\_robot\_state\_transition

test\_invalid\_robot\_transition

test\_task\_state\_transition

test\_waiting\_state

test\_blocked\_state

test\_replanning\_state

test\_robot\_timeout

test\_command\_generation

test\_route\_assignment

test\_reservation\_assignment

