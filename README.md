# KU_ESW
Konkuk University Embedded Software Class Sources.



## 1_KU-IPC

## 2_KU-PIR

## Term Project - I2I Elevator

### My Part

디스플레이 파이(Display Pi) 구현을 맡았다.

디스플레이 파이는 다음과 같이 세 가지로 이루어져 있다.

- PIR센서

- 터치센서
- 카메라

### Functions

| 센서     | 기능                                                         |
| -------- | ------------------------------------------------------------ |
| PIR센서  | 5초마다 엘리베이터 내부에 사람이 있는지 없는지 확인한다. 만약 사람이 감지되면 LED를 켜고, 5초동안 사람이 감지되지 않으면 LED를 끈다. |
| 터치센서 | 사람이 처음 들어오면 스마트미러가 거울모드로 설정되어 있다.(default값이 거울모드이다) 외부 파이(Exterior Pi)에도 카메라가 있어 외부배경을 보여주는데, 거울모드상태에서 터치 센서를 터치할 시, 외부배경모드로 변경된다. 반대로 외부배경모드에서 터치센서를 터치할 시, 거울모드로 변경횐다. |
| 카메라   | 거울모드 시 거울을 보고있는 것과 같은 화면을 디스플레이에 출력하는 카메라이다. 카메라를 통해 촬영한 화면을 단위시간마다 Display Pi의 파이 주소에 포트번호 8000으로 스트리밍 한다. 웹 브라우저 상에서는 overlay된 온습도와 먼지농도도 디스플레이에 계속 출력된다. |

### Codes

| 소스            | 함수             | 기능                                                         |
| --------------- | ---------------- | ------------------------------------------------------------ |
| touch_app.c     | main             | default인 거울모드 화면을 웹 브라우저로 보여주고, touch_dev를 연다. 그리고 while문으로 계속 ioctl함수를 호출하는데, 만약 터치센서를 누르게 되면, 커널 상에서 터치센서 값을 나타내는 전역변수 값이 바뀌면서 IOCTL_STATUS NUM cmd에서 바꿔야하는 모드 값을 리턴한다. 응용프로그램에서  ioctl함수 리턴 값이 1이면 거울모드를 보여주기 위해 closeBrowser.py를 실행한 후, openDisplay.py를 실행하고, 2이면 외부배경모드를 보여주기 위해 closeBrowser.py를 실행한 후, openExterior.py를 실행한다. (system함수 사용 시, 함수가 종료되는 시간이 오래걸려 뒤의 명령문을 실행하는 속도가 느려지기 때문에 fork문을 사용하였다) |
| touch_mknod.sh  |                  | touch_dev 디바이스 파일                                      |
| openDisplay.py  |                  | DisplayPi주소:8000 으로 웹 브라우저를 띄운다.                |
| openExterior.py |                  | ExteriorPi주소:8000으로 웹 브라우저를 띄운다.                |
| closeBrowser.py |                  | DisplayPi나 ExteriorPi 중 웹 브라우저를 하나만 띄우기 위해 그 전에 웹 브라우저를 종료한다.(pid를 알면, 해당 웹 브라우저의 pid를 kill하면 되는데, 매번 다른 pid로 생성되는데, 응용프로그램에서 그걸 받아오는 방법을 몰라 이렇게 웹 브라우저 전체를 닫는 무식한..방법으로 진행하게 되었다.) |
| display_mod.c   | display_mod_init | 모드가 현재 거울모드인지 외부배경모드인지를 나타내는 변수인 mode_status는 거울모드로 default값인 1로 초기화한다. 그리고 터치된 상태인지 아닌지를 나타내는 변수인 touched를 0으로 초기화한다. 그리고 캐릭터 디바이스를 등록한다. LED, PIR, TOUCH센서의 gpio번호를 등록하고, PIR, TOUCH센서 같은 경우 인터럽트 핸들러를 등록한다. PIR의 경우 인터럽트 핸들러를 활성화 해놓는다. 그리고 5초 이내로 사람이 감지되지 않으면 LED를 끄기위해 타이머를 추가한다. |
|                 | display_mod_exit | 캐릭터 디바이스 등록 해제하고, pir 타이머를 삭제하고, irq_pir을 비활성화하고, irq_pir과 irq_touch를 free_irq한다. 그리고 LED, PIR, TOUCH 모두 gpio_free한다. |
|                 | touch_open       | 터치센서 irq를 활성화하고, 터치센서 irq가 활성화된 상태(1)인지 비활성화된 상태(0)인지 나타내는 변수인 touch_status를 1로 설정한다. |
|                 | touch_close      | 터치센서 irq를 비활성화하고, 터치센서 irq가 활성화된 상태(1)인지 비활성화된 상태(0)인지 나타내는 변수인 touch_status를 0으로 설정한다. |
|                 | touch_isr        | 우선 irq_pir을 비활성화 시킨 후, 터치센서를 눌렀을 시(gpio_get_value(TOUCH)가 1일 시), 터치된 상태인지 아닌지를 나타내는 변수인 touched를 1로 설정하고, 현재 거울모드인지 외부배경모드인지를 나타내는 변수인 mode_status의 값에 따라 바꿔야할 모드 값으로 변경한다. 그리고 irq_pir을 다시 활성화 시킨다. |
|                 | touch_mod        | 응용프로그램에서 ioctl함수로 IOCTL_STATUS_NUM cmd를 인자로 넘겼을 시, 바뀌어야하는 모드 값인 mode_status를 리턴 값으로 한다. |
|                 | pir_isr          | 우선 irq_touch를 비활성화 시킨 후, 물체가 감지되었을 시, LED값을 1로 설정한다. 만약 터치센서 irq가 활성화된 상태인지 아닌지를 나타내는 변수인 touch_status가 0이면(비활성화 상태이면) 물체가 감지되었으므로 터치센서 irq를 활성화 시킨다. 그리고 타이머 충돌을 방지하기 위해 pir_timer를 삭제한 후, 다시 추가한다. |
|                 | pir_timer_func   | 5초마다 이 함수가 실행되는데, 만약 pir의 gpio value값이 1인 경우(FALLING), LED값을 0으로 설정한다. 만약 터치센서 irq가 활성화된 상태인지 아닌지를 나타내는 변수인 touch_status가 1이면(활성화 상태이면) 감지되는 물체가 없으므로 터치센서 irq를 비활성화 시킨다. 만약 PIR센서의 값이 0이면(RISING), LED값을 1로 설정한다. |

### 예외처리사항

사람이 감지되지 않을 경우, 터치 센서의 오작동을 대비하여 터치 센서에 해당하는 irq_num을 irq_disable한다.

### Issue Track

1. 파이 자체의 성능으로 인해 웹 브라우저를 띄우는 속도가 느리다.
2. 터치 센서가 전기적인 자극으로 인해 한 번 터치했는데 두 번, 세번 터치한 것처럼 처리 될 때가 있다.
3. PIR센서의 RISING FALLING이 상황에 맞지 않는 거 같다. => 민감도를 좀 더 둔감하게 해서 해결이 되었으면 좋겠다..