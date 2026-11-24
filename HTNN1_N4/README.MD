# HTNN1_N4 - STM32F103C8, OLED SH1106 va menu

## Ban build hien tai

`src/main.c` chay bare-metal, clock HSI/2 x 4 = 16 MHz, APB1 = 16 MHz.
Khong khoi chay scheduler FreeRTOS. Cac module task/IPC/BSP van duoc giu
cho viec tich hop sau, nhung khong nam trong ban build mac dinh.

Build: `pio run`
Nap bang ST-Link: `pio run -t upload`

## Chan ket noi

| Chuc nang | Chan |
| --- | --- |
| OLED SCL | PB6 |
| OLED SDA | PB7 |
| UP | PB12 |
| DOWN | PB14 |
| SELECT | PB13 |

Nut nhan noi xuong GND, dau vao pull-up. Su kien nut phat khi tha sau
khi chong doi 25 ms. OLED dung dia chi 7-bit `0x3C`, I2C 100 kHz.
Can dien tro keo len SCL/SDA phu hop voi module.

## Luong chuong trinh

1. Cau hinh clock 16 MHz va SysTick polling cho delay.
2. Goi `i2c1_init()`, khoi tao OLED qua callback `Test_I2CWrite`.
3. Khoi tao ba nut va TIM2 dem 1 ms.
4. Khoi tao menu, nap nguong tu Flash neu co ban ghi hop le.
5. Vong lap doc TIM2, quet nut, xu ly menu va ve khi `dirty = 1`.

`g_i2c_test_stage`: 1 = clock OK, 2 = I2C init xong,
3 = OLED init xong, 4 = da gui frame menu.
`g_i2c_test_frames`, `g_button_test_events`, `g_button_test_last_event`
phuc vu quan sat trong debugger.

## Cac module

| File | Vai tro |
| --- | --- |
| `src/main.c` | Clock, callback phan cung, khoi tao va vong lap test |
| `include/Config_peripheral/i2c/i2c.c` | Driver I2C thanh ghi theo mau da chon |
| `lib/Device/SH1106/sh1106.c` | Framebuffer va truyen hinh OLED |
| `lib/Device/Button/button.c` | Chong doi ba nut |
| `lib/Application/Display/menu.c` | Menu, nguong, trang ngay gio, canh bao |
| `lib/Application/Storage/threshold_store.c` | Luu/nap bon nguong tren hai page Flash |
| `lib/Task/Display/app_ui.c` | Ket noi UI voi BSP cho ban FreeRTOS sau nay |
| `lib/Task/Display/task_display.c` | Task hien thi, nhan mau tu queue |
| `lib/Task/IPC/app_ipc.c` | Queue du lieu va yeu cau giua cac task |
| `lib/Task/BSP/board_port.c` | Adapter GPIO, I2C truc tiep, Flash va queue |

## Phan task de tich hop sau

Toan bo adapter phu thuoc task/queue nam trong `lib/Task/`:

```text
lib/Task/
  Display/  task_display.c/.h, app_ui.c/.h
  IPC/      app_ipc.c/.h
  BSP/      board_port.c/.h
```

Ban build menu loai thu muc `Task` va khong can include FreeRTOS.
Khi tich hop sau, them lai source cua Task, kernel/port/heap FreeRTOS
va cac duong dan include `lib/Task/Display`, `lib/Task/IPC`,
`lib/Task/BSP` cung header kernel. Thu vien FreeRTOS goc van nam tai
`include/FreeRTOS-Kernel`; menu va luu Flash duoc dung chung.

## I2C truc tiep

Driver giu cac ham `i2c1_init`, `i2c1_Start`, `i2c1_Stop`,
`i2c1_writeAddr`, `i2c1_writeData`, `i2c1_writeBytes`, `i2c1_readBytes`.
Dia chi truyen vao ham read/writeBytes la dia chi 7-bit.
Cau hinh CR2=16, CCR=80, TRISE=17 yeu cau APB1=16 MHz.

Khong co lop mutex. Neu tich hop FreeRTOS, chi mot task duoc truc tiep
su dung bus; cac task khac can gui yeu cau cho task so huu bus.
Cac ham driver tra `void`, polling khong timeout va khong bao loi ACK.
Callback tra 0 chi nghia la ham da quay ve, khong phai xac nhan suc khoe bus.
Ban build hien tai chi dung duong ghi de dieu khien OLED; duong doc can
kiem tra tren thiet bi that, dac biet giao dich doc 1/2 byte STM32F1.

## Luu nguong va RTC

SAVE nguong trong ban build mac dinh goi `ThresholdStore_Save`.
Sau reset, `ThresholdStore_Load` phuc hoi nguong; Flash trong/loi thi
menu dung gia tri mac dinh. Hai page 1 KB cuoi Flash 64 KB duoc danh
cho nguong tai `0x0800F800` va `0x0800FC00`; gioi han firmware 63488 byte.
Ban ghi co checksum, commit cuoi cung va luu luan phien hai page.

Chua co driver/task RTC va task cam bien trong workspace.
Ban build mac dinh khong gan callback ghi RTC: SAVE RTC se bao loi,
khong gia lap thanh cong. Ngay gio/cam bien chua co du lieu hien `--`.
Trong ban FreeRTOS chua tich hop, `Board_WriteRTC` chi gui yeu cau vao
queue; can task RTC xu ly va xac nhan viec ghi thiet bi.

## Kiem tra tren board

- Kiem tra OLED hien menu va UP/DOWN/SELECT chuyen trang dung.
- Doi nguong, SAVE, reset MCU va kiem tra gia tri duoc phuc hoi.
- Kiem tra BACK (CANCEL) khong luu thay doi.
- Chua xac nhan chay phan cung tu ket qua bien dich.
