# 임베디드소프트웨어1 과제 #1

## 응용 코드 - ku_ipc_lib.c

테스트코드에서 #include "ku_ipc_lib.c"를 통해 다음과 같이 ku_msgget, ku_msgclose, ku_msgsnd, ku_msgrcv함수들을 사용할 수 있습니다.

각 함수들의 기능은 Functionality에 설명되어 있습니다.

| Function Name | ku_msgget                                                    |
| ------------- | ------------------------------------------------------------ |
| Parameters    | int key, int msgflg                                          |
| Functionality | 인자로 넘겨받은 key를 ioctl함수를 통해 선언되어있는 IOCTL_MSGQCHECK_NUM cmd인자를 넘겨준다. 해당 ioctl함수에서 -1이 반환된 경우, 커널에 있는 메세지 큐 리스트에 해당 key가 없으므로 IOCTL_MSGQCREAT_NUM cmd인자를 넘겨줘서 해당 key의 메세지큐를 새로 생성한다. 만약 커널에 있는 메세지 큐 리스트에 해당 key가 있다면, ioctl함수 반환값으로 받은 msqid를 반환한다. |
| Return Value  | int(메세지큐를 새로 생성하거나 해당 key의 메세지 큐가 이미 존재하는 경우 msqid 리턴, msgflg에 KU_IPC_EXCL가 포함되어 있는데, 해당 key의 메세지 큐가 없는 경우 -1 리턴) |

| Function Name | ku_msgclose                                                  |
| ------------- | ------------------------------------------------------------ |
| Parameters    | int msqid                                                    |
| Functionality | 우선 인자로 넘어온 msqid에 해당하는 메세지 큐가 있는지 확인하기 위해 ioctl함수에 cmd인자로 IOCTL_MSGQCHECK_NUM를 넘겨준다. 만약 있으면 ioctl함수에 cmd인자로 IOCTL_MSGQCLOSE_NUM을 넘겨준다. 없는 경우 close할 메세지큐가 없으므로 -1을 리턴한다. |
| Return Value  | int(정상적으로 close했을 시 0, close할 메세지큐가 없는 경우 또는 close를 이미 했는데 또 시도할 경우 -1을 리턴한다.) |

| Function Name | ku_msgsnd                                                    |
| ------------- | ------------------------------------------------------------ |
| Parameters    | int msqid, void *msgp, int msgsz, int msgflg                 |
| Functionality | 우선 인자로 넘어온 msqid에 해당하는 메세지 큐가 있는지 확인하기 위해 ioctl함수에 cmd인자로 IOCTL_MSGQCHECK_NUM를 넘겨준다. 해당 msqid를 갖는 메세지큐가 있고, 해당 메세지큐에 메세지를 넣을 수 있으면 write()함수를 통해 커널 모듈 코드에서의 ku_ipc_mod_write함수를 호출한다. 인자로 넘겨받은 msgflg에 KU_IPC_NOWAIT가 있을 시, write할 수 있을 때까지 기다리지 않고 -1을 리턴한다. msgflg & KU_IPC_NOWAIT == 0일 시, while문을 통해 write가 될 때까지 반복문을 돈다. |
| Return Value  | int(msgsnd를 성공한 경우 0, 실패한 경우 -1 리턴)             |

| Function Name | ku_msgrcv                                                    |
| ------------- | ------------------------------------------------------------ |
| Parameters    | int msqid, void *msgp, int msgsz, long msgtyp, int msgflg    |
| Functionality | 우선 인자로 넘어온 msqid에 해당하는 메세지 큐가 있는지 확인하기 위해 ioctl함수에 cmd인자로 IOCTL_MSGQCHECK_NUM를 넘겨준다. 해당 msqid를 갖는 메세지큐가 있고, 해당 메세지큐에 메세지가 있으면 read()함수를 통해 커널 모듈 코드에서의 ku_ipc_mod_read함수를 호출한다. 인자로 넘겨받은 msgflg에 KU_IPC_NOWAIT가 있을 시, read할 수 있을 때까지 기다리지 않고 -1을 리턴한다. msgflg & KU_IPC_NOWAIT == 0일 시, while문을 통해 read가 될 때까지 반복문을 돈다. 또한 인자로 넘겨받은 msgflg에 KU_MSG_NOERROR가 있을 시, 커널에 있는 메세지큐의 메세지 크기가 읽으려는 메세지 크기보다 클 경우, 해당 메세지를 읽으려는 메세지 크기만큼만 읽는다. msgflg & KU_MSG_NOERROR == 0일 시에는 읽지 않고 -1을 리턴한다. |
| Return Value  | int(msgrcv를 성공한 경우 receive한 메세지의 크기, 실패한 경우 -1 리턴) |



## 커널 모듈 헤더파일 - ku_ipc.h

```c
#define KUIPC_MAXMSG 10
#define KUIPC_MAXVOL 3000

#define KU_IPC_CREAT 00001000
#define KU_IPC_EXCL 00002000
#define KU_IPC_NOWAIT 00004000

#define KU_MSG_NOERROR 010000
```

이 외의 선언해야할 것들은 c파일에 넣었습니다.

## 커널 모듈 코드 - ku_ipc.c



| Function Name | ku_ipc_mod                                                   |
| ------------- | ------------------------------------------------------------ |
| Parameters    | struct file * file, unsigned int cmd, unsigned long arg      |
| Functionality | 응용 코드에서 define되어 있는 ioctl함수를 호출 할 시 호출되는 함수이다. 선언한 cmd로는 IOCTL_MSGQCHECK_NUM, IOCTL_MSGQCREAT_NUM, IOCTL_MSGQCLOSE_NUM가 있다. cmd가 IOCTL_MSGQCHECK_NUM인 경우, arg인자로 넘어오는 key값에 해당하는 메세지 큐가 있는지 ku_mqueue를 순회하면서 찾는다. 있는 경우, 해당 메세지 큐의 msqid를 반환하고, 없는 경우 -1을 반환한다. 응용 코드에서 IOCTL_MSGQCHECK_NUM에서 -1이 반환된 경우에는 cmd로 IOCTL_MSGQCREAT_NUM를 호출하여 메세지 큐를 생성하고 그 생성된 메세지 큐의 msqid를 반환한다. |
| Return Value  | static long(응용 코드에서 ioctl함수에 반환되는 값으로, IOCTL_MSGQCHECK_NUM, IOCTL_MSGQCREAT_NUM의 경우 새로 생성되는 msqid나, 이미 존재하는 msqid를 반환) |

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

| Function Name | ku_ipc_mod_read                                              |
| ------------- | ------------------------------------------------------------ |
| Parameters    | struct file *file, char *buf, size_t buflen, loff_t *f_pos   |
| Functionality | 응용 코드로부터 받은 buf(struct userbuf형)로부터 msqid를 받아와 전체 메세지 큐 리스트인 ku_mqueue에서 해당 msqid를 key로 갖는 메세지 큐를 찾는다. 그리고 메세지 큐에 읽어올 데이터가 있는 경우, 해당 메세지 큐를 순회하면서 from_userbuf(struct userbuf형)로부터 얻어온 msgbuf의 메세지 타입이 같은 처음 메세지를 read한다. 만약 msgflg & KU_MSG_NOERROR != 0인 경우, 읽으려는 메세지의 크기보다 메세지 큐에 있는 메세지의 크기가 클 경우, 읽으려는 메세지의 크기만큼만 읽고, 나머지는 버린다. |
| Return Value  | static int(메세지 큐에 읽어올 데이터가 없고, KU_IPC_NOWAIT이 0이 아닌 경우 -1 리턴, 정상적으로 read했을 시 0 리턴) |

| Function Name | ku_ipc_mod_write                                             |
| ------------- | ------------------------------------------------------------ |
| Parameters    | struct file *file, const char *buf, size_t buflen, loff_t *f_pos |
| Functionality | 응용 코드로부터 받은 buf(struct userbuf형)로부터 msqid를 받아와 전체 메세지 큐 리스트인 ku_mqueue에서 해당 msqid를 key로 갖는 메세지 큐를 찾는다. 그리고 메세지 큐에 넣을 수 있는 경우, copy_from_user로 from_userbuf(struct userbuf형)로부터 얻어온 text를 kernel buf(struct message형)인 tmp_msg->text로 복사한다. struct message형 tmp_msg에 필요한 정보들(type, text, textLen)을 넣어준 후, 해당 메세지 큐의 메시지 리스트에 list_add_tail한다. 메시지를 추가하였으므로 해당 메세지 큐의 msgCnt를 1 증가시키고, vol을 받은 msgbuf크기만큼 늘려준다. \*copy_from_user 또는 커널의 링크드리스트에 추가할 시에는 spin_lock_t인 ku_lock으로 락을 걸고, 다 끝났을 때 락을 해제한다\* |
| Return Value  | static int(메세지 큐에 buf 인자를 넣을 경우, KUIPC_MAXVOL를 넘거나, 메세지 수가 KUIPC_MAXMSG를 넘어가는 경우 -1 리턴, 정상적으로 write했을 시 0 리턴 ) |

| Function Name | ku_ipc_mod_init                                              |
| ------------- | ------------------------------------------------------------ |
| Parameters    | X                                                            |
| Functionality | 캐릭터디바이스를 등록한다. 추가로 spin_lock_t인 ku_lock을 초기화하며, 메세지 큐 리스트인 ku_mqueue의 list_head인 qlist를 초기화한다. |
| Return Value  | static int(정상종료 시 0 리턴)                               |

| Function Name | ku_ipc_mod_exit                                              |
| ------------- | ------------------------------------------------------------ |
| Parameters    | X                                                            |
| Functionality | 캐릭터디바이스를 등록 해제한다.  list_for_each_safe로 리스트를 순회하면서 메세지 구조체의 mlist와 메세지 큐 구조체의 qlist를 연결 해제하고, 할당 해제한다. |
| Return Value  | X                                                            |



by 컴퓨터공학과 201511269 송상연