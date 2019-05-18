# 임베디드소프트웨어1 과제 #2

## 응용 코드 - ku_pir_lib.c

테스트코드에서 #include "ku_pir_lib.c"를 통해 다음과 같이 ku_pir_open, ku_pir_close, ku_pir_read, ku_pir_flush, ku_pir_insertData함수들을 사용할 수 있습니다.

각 함수들의 기능은 Functionality에 설명되어 있습니다.

ioctl()함수의 cmd명령에 대한 설명은 ku_pir.c 부분에 있습니다.

| Function Name | ku_pir_open                                                  |
| ------------- | ------------------------------------------------------------ |
| Parameters    | X                                                            |
| Functionality | ioctl함수에 cmd인자로 IOCTL_PIRQ_CREATE_NUM을 넘겨준다.      |
| Return Value  | int(프로세스마다 생성되는 큐를 식별할 수 있는 fd값을 반환한다.) |

| Function Name | ku_pir_close                                                 |
| ------------- | ------------------------------------------------------------ |
| Parameters    | int fd                                                       |
| Functionality | ioctl함수에 cmd인자로 IOCTL_PIRQ_DELETE_NUM을, arg인자로 매개변수인 fd를 넘겨준다. |
| Return Value  | int(해당 fd값을 갖는 큐가 있고, 정상적으로 close되면 0, 해당 fd값을 갖는 큐가 없으면 -1을 리턴한다.) |

| Function Name | ku_pir_read                                                  |
| ------------- | ------------------------------------------------------------ |
| Parameters    | int fd, struct ku_pir_data *data                             |
| Functionality | ioctl함수에 cmd인자로 IOCTL_PIR_READ_NUM을, arg인자로 데이터를 읽을 큐를 찾게끔 매개변수인  fd를 포함하는 kernel로부터 데이터를 받을 구조체인 fd_pir_data를 넘겨준다. ioctl함수가 종료되고 나서는 kernel에서 해당 fd값을 갖는 큐로부터 읽어온 데이터가 들어가 있는 fd_pir_data구조체로부터 timestamp와 rf_flag값을 매개변수인 ku_pir_data구조체의 timestamp와 rf_flag에 넣는다. |
| Return Value  | X                                                            |

| Function Name | ku_pir_flush                                                 |
| ------------- | ------------------------------------------------------------ |
| Parameters    | int fd                                                       |
| Functionality | ioctl함수에 cmd인자로 IOCTL_PIRQ_RESET_NUM을, arg인자로 매개변수인 fd를 넘겨준다. |
| Return Value  | X                                                            |

| Function Name | ku_pir_insertData                                            |
| ------------- | ------------------------------------------------------------ |
| Parameters    | long unsigned int ts, char rf_flag                           |
| Functionality | ioctl함수에 cmd인자로 IOCTL_PIRQ_RESET_NUM을, arg인자로 매개변수인 ts와 rf_flag를 ku_pir_data구조체에 저장한 후, ku_pir_data구조체를 넘겨준다. |
| Return Value  | int(성공 시 0, 실패 시 -1을 반환한다.)                       |



## 커널 모듈 헤더파일 - ku_pir.h

한 큐당 최대 pir데이터 개수를 나타내는 KUPIR_MAX_MSG와 GPIO PIR SENSOR번호를 나타내는 KUPIR_SENSOR와 장치 이름을 나타내는 DEV_NAME이 선언되어있습니다.

KUPIR_MAX_MSG는 10으로 선언해놓았으며, KUPIR_SENSOR는 정해진대로 17로 두었고, DEV_NAME도 정해진대로 "ku_pir_dev"로 선언되어있습니다.

이 외에는 ioctl()에 전달되는 cmd와 관련된 매크로 함수와 ku_pir_data구조체와 ku_pir_data구조체에 int fd를 추가하여 ku_pir_read시 사용하는 fd_pir_data구조체가 선언되어있습니다.

## 커널 모듈 코드 - ku_pir.c

```c
struct pir_data
{
	struct list_head plist;
	long unsigned int timestamp;
	char rf_flag;
};

struct data_queue
{
	struct list_head qlist;
	int fd;
	int dataCnt;
	struct pir_data *pir;
};

struct data_queue ku_queue;
int count;
spinlock_t count_lock;
static int irq_num;
wait_queue_head_t ku_wq;
```

다음과 같이 list_head plist, timestamp, rf_flag로 이루어진 pir_data 구조체와, pir_data들과 프로세스마다 구별할 수 있는 식별값 fd, 그리고 해당 큐의 pir_data개수인 dataCnt로 이루어진 data_queue구조체가 선언되어있습니다.

글로벌 변수의 경우 전체 데이터큐인 ku_queue, 새로운 프로세스가 접근할 때마다 프로세스마다 구별할 수 있는 식별값인 fd를 count값을 1씩 올려주면서 넣어줄 count와 그 count의 동시접근을 제어하기 위한 count_lock, interrupt를 처리하고자 하는 irq_num, 만약 읽을 데이터가 없을 경우, 읽을 데이터가 생길 때까지 sleep하기 위해 넣어두는 wait queue인 ku_wq가 있습니다.

| Function Name | search_queue                                                 |
| ------------- | ------------------------------------------------------------ |
| Parameters    | int fd                                                       |
| Functionality | 전체 큐 리스트인 ku_queue를 훑으면서 매개변수의 fd값을 가진 큐를 찾는다. |
| Return Value  | struct data_queue *(해당 fd값을 가진 큐가 있을 시 해당 큐의 base pointer를, 없을 시 NULL을 반환한다.) |

| Function Name | flush_data_queue                                             |
| ------------- | ------------------------------------------------------------ |
| Parameters    | struct data_queue *flush_q                                   |
| Functionality | flush할 큐인 매개변수 flush_q의 pir_data들을 훑으면서 pir_data들을 모두 free한다. 그리고 pir_data들을 모두 지웠으므로 flush_q의 dataCnt를 0으로 설정한다. |
| Return Value  | X                                                            |

| Function Name | print_queue_status                                           |
| ------------- | ------------------------------------------------------------ |
| Parameters    | X                                                            |
| Functionality | 전체 큐 리스트인 ku_queue를 훑으면서 모든 큐들과 그 큐에 들어있는 pir_data들의 정보를 커널 메세지로 보여준다. |
| Return Value  | X                                                            |

| Function Name              | ku_pir_mod                                                   |
| -------------------------- | ------------------------------------------------------------ |
| Parameters                 | struct file * file, unsigned int cmd, unsigned long arg      |
| Functionality              | 응용 코드에서 define되어 있는 ioctl함수를 호출 할 시 호출되는 함수이다. 매크로함수로 선언한 cmd 명령으로는 IOCTL_PIRQ_CREATE_NUM, IOCTL_PIRQ_RESET_NUM, IOCTL_PIRQ_DELETE_NUM, IOCTL_PIR_READ_NUM, IOCTL_PIR_WRITE_NUM가 있다. |
| IOCTL_PIRQ_CREATE_NUM      | 새로운 data_queue를 생성한다. data_queue구조체의 fd값 같은 경우, 새로운 data_queue를 생성할 때마다 글로벌 변수인 count를 count_lock을 걸고, 1씩 증가시키면서 fd값에 넣어준다. 새로운 data_queue의 dataCnt를 0으로 초기화하고, pir_data구조체의 plist를 초기화하고, 전체 큐 리스트인 ku_queue의 qlist에 생성한 data_queue의 qlist를 list_add_rcu()로 추가한다. |
| IOCTL_PIRQ_RESET_NUM       | 인자로 넘어온 fd값에 해당하는 큐가 있을 경우, flush할 큐인 flush_q를 인자로 하여, flush_data_queue(flush_q)함수를 호출한다. |
| IOCTL_PIRQ_PIRQ_DELETE_NUM | 인자로 넘어온 fd값에 해당하는 큐가 있을 경우, 삭제할 큐인 del_q를 인자로 하여, 우선 flush_data_queue(del_q)로 del_q 안의 pir_data들을 삭제한 후, del_q를 전체 큐 리스트인 ku_queue로부터 list_del_rcu()하고, 할당해제한다. |
| IOCTL_PIR_READ_NUM         | ku_pir_lib.c로부터 받아온 fd_pir_data를 copy_from_user()로 커널의 변수인 fd_user_data로 받아온다. 그리고 받아온 fd_user_data의 fd에 해당하는 큐를 search_queue()함수로 찾는다. 데이터를 읽어올 큐인 q_to_read의 dataCnt가 0인 경우, dataCnt가 0보다 커질 경우 깨어나게끔 wait_event()함수를 호출함으로써 wait queue인 ku_wq에 넣어놓는다. q_to_read에 읽을 데이터가 있는 경우에는 해당 큐의 head data의 timestamp, rf_flag를 fd_user_data->data의 timestamp, rf_flag로 copy한다. 그리고 fd_user_data를 copy_to_user()로 ioctl함수의 arg인자로 데이터를 넘겨준다. 읽은 데이터는 해당 큐에서 list_del_rcu()하고, 할당해제한다. |
| IOCTL_PIR_WRITE_NUM        | ku_pir_lib.c로부터 받아온 ku_pir_data를 copy_from_user()로 커널의 변수인 user_data로 받아온다. 그리고 list_for_each_entry_rcu()로 전체 큐 리스트인 ku_queue를 훑으면서 각각의 큐에 메모리를 할당한 pir_data구조체인 kern_data를 user_data로부터 값을 copy하여 list_add_tail_rcu()로 추가한다. 만약 큐의 dataCnt가 KUPIR_MAX_MSG인 경우, list_for_each_safe()로 해당 큐에서 가장 오래된 데이터인 head_data를 할당해제하고나서 tail에 추가한다. 그리고 read 시 데이터가 없어서 sleep한 프로세스, 즉, wait queue에 있는 프로세스를 깨운다. |
| Return Value               | static long(응용 코드에서 ioctl함수에 반환되는 값으로, IOCTL_PIRQ_CREATE_NUM의 경우 fd값을, IOCTL_PIRQ_PIRQ_DELETE_NUM의 경우 data_queue 삭제 성공 시 0, 실패 시 -1을, IOCTL_PIR_WRITE_NUM의 경우 pir_data를 넣기 성공 시 0, 실패 시 -1을 반환한다.) |

| Function Name | ku_ipc_mod_open                                     |
| ------------- | --------------------------------------------------- |
| Parameters    | struct inode *inode, struct file *file              |
| Functionality | 응용 코드에서 장치를 open했을 시 호출되는 함수이다. |
| Return Value  | static int(정상종료 시 0 리턴)                      |

| Function Name | ku_ipc_mod_release                                   |
| ------------- | ---------------------------------------------------- |
| Parameters    | struct inode *inode, struct file *file               |
| Functionality | 응용 코드에서 장치를 close했을 시 호출되는 함수이다. |
| Return Value  | static int(정상종료 시 0 리턴)                       |

| Function Name | ku_ipc_isr                                                   |
| ------------- | ------------------------------------------------------------ |
| Parameters    | int irq, void* dev_id                                        |
| Functionality | KUPIR_SENSOR, 과제에서는 GPIO 17번으로 정해진 irq에 interrupt가 발생할 시 실행되는 인터럽트 핸들러 함수이다. 함수 내부는 ku_pir_mod의 IOCTL_PIR_WRITE_NUM에서 실행되는 내용과 비슷하다고 볼 수 있다. pir_data구조체인 tmp_data를 선언한 후, tmp_data->timestamp는 jiffies로 설정해두고, gpio_get_value(KUPIR_SENSOR)가 0이면 tmp_data->rf_flag를 '0'(RISING), 1이면 tmp_data->rf_flag를 '1'(FALLING)으로 설정한다. 그리고 그 tmp_data를 list_for_each_entry_rcu()로 전체 큐 리스트인 ku_queue를 훑으며 각각의 큐에 list_add_tail_rcu()로 추가한다. 만약 큐의 dataCnt가 KUPIR_MAX_MSG인 경우, list_for_each_safe()로 해당 큐에서 가장 오래된 데이터인 head_data를 할당해제하고나서 tail에 추가한다. 그리고 read 시 데이터가 없어서 sleep한 프로세스, 즉, wait queue에 있는 프로세스를 깨운다. |
| Return Value  | static irqreturn_t                                           |

| Function Name | ku_ipc_mod_init                                              |
| ------------- | ------------------------------------------------------------ |
| Parameters    | X                                                            |
| Functionality | 캐릭터디바이스를 등록한다. 전체 큐 리스트인 ku_queue의 list_head인 qlist를 초기화한다. 글로벌 변수인 count값을. 0으로 초기화하고, count값을 증가시킬 때 동시 접근을 막기 위한 spin_lock_t인 count_lock을 초기화한다. sleep한 프로세스를 넣기 위한 wait queue인 ku_wq도 초기화한다. 또한, 인터럽트 핸들러 함수를 등록한다. |
| Return Value  | static int(정상종료 시 0 리턴)                               |

| Function Name | ku_ipc_mod_exit                                              |
| ------------- | ------------------------------------------------------------ |
| Parameters    | X                                                            |
| Functionality | 캐릭터디바이스를 등록 해제한다. request_irq()함수로 등록된 인터럽트 핸들러 함수를 제거한다. |
| Return Value  | X                                                            |

by 컴퓨터공학과 201511269 송상연