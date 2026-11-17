## 1. Các hàm cần gọi trong main()

Gọi theo thứ tự sau, một lần khi khởi động:

| Hàm | Tác dụng |
| --- | --- |
| `I2C_Init()` | Cấu hình I2C1: PB6=SCL, PB7=SDA và TIM2 phục vụ timeout. |
| `Board_InitButtons()` | Cấu hình nút kéo lên nội: UP=PB12, DOWN=PB14, SELECT=PB13; nhấn nối xuống GND. |
| `I2C_RTOS_Init()` | Tạo mutex bảo vệ bus I2C dùng chung. Trả `0` khi thành công. |
| `App_IPC_Init()` | Tạo ba queue trao đổi dữ liệu hiển thị, yêu cầu chỉnh giờ và yêu cầu đổi ngưỡng. Trả `0` khi thành công. |
| `xTaskCreate(Task_Display, ...)` | Tạo task giao diện. Trả `pdPASS` khi thành công. |
| `vTaskStartScheduler()` | Bắt đầu chạy các task bằng FreeRTOS. |
Stack 768

## 2. Gửi dữ liệu từ task cảm biến lên OLED

Include `app_ipc.h`, rồi gọi:

```c
BaseType_t Display_PostSensor(MenuSensor id, int32_t value, uint8_t valid);
```

| ID | Dữ liệu | Đơn vị truyền | Ví dụ |
| --- | --- | --- | --- |
| `MENU_AHT_TEMP` | Nhiệt độ AHT | Độ C × 10 | `253` = 25.3 °C |
| `MENU_AHT_HUM` | Độ ẩm AHT | % × 10 | `650` = 65.0 % |
| `MENU_BMP_TEMP` | Nhiệt độ BMP | Độ C × 10 | `280` = 28.0 °C |
| `MENU_BMP_PRESS` | Áp suất BMP | hPa × 10 | `10132` = 1013.2 hPa |
| `MENU_MQ5_ADC` | Mẫu ADC MQ5 | Số đếm ADC 12 bit | `1800`, trong khoảng 0..4095 |

`valid=1`: mẫu hợp lệ. `valid=0`: mẫu lỗi/chưa có; màn hình hiện `--` và không dùng mẫu đó để tính cảnh báo.

```c
// Gọi trong task sau khi đọc cảm biến thật:
BaseType_t sent = Display_PostSensor(MENU_AHT_TEMP, 253, 1);
if (sent != pdTRUE) {
    // Queue đầy/chưa khởi tạo: giữ mẫu và thử gửi ở chu kỳ tiếp theo.
}

// Ví dụ khi đọc cảm biến thất bại:
Display_PostSensor(MENU_AHT_TEMP, 0, 0);
```

Hàm sao chép dữ liệu vào queue và trả ngay, không chờ khi queue đầy. Task cảm biến không gọi các hàm vẽ OLED hoặc sửa trực tiếp `Menu`.

## 3. Gửi giờ từ task RTC lên OLED

```c
MenuDateTime now = {
    .year = 2026, .month = 11, .day = 14,
    .hour = 10, .minute = 30, .second = 0
};
// now là ví dụ; khi chạy thật, điền giờ vừa đọc từ chip RTC.
BaseType_t sent = Display_PostRTC(&now);
```

`Display_PostRTC()` sao chép ngày giờ vào queue; trả `pdTRUE` khi gửi được. Có thể gửi mỗi giây. Menu chấp nhận năm 2000..2099 và kiểm tra ngày giờ hợp lệ.

### Nhận yêu cầu chỉnh giờ từ menu

Khi chọn SAVE ngày giờ, menu gọi `Board_WriteRTC()` → `Display_RequestRTC()` → `g_rtc_request_queue`.

Trong task RTC, nhận như sau:

```c
MenuDateTime requested;
if (xQueueReceive(g_rtc_request_queue, &requested, 0) == pdTRUE) {
    // TODO: gọi driver của chip RTC để ghi requested.
    // Sau đó đọc lại giờ thật và gửi Display_PostRTC().
}
```

Timeout `0` nghĩa là queue rỗng thì tiếp tục xử lý việc khác. Nếu task chỉ chuyên chờ yêu cầu, có thể dùng timeout phù hợp và xử lý việc đọc giờ định kỳ riêng.

## 4. Nhận ngưỡng trong task điều khiển

Include `app_ipc.h`. Nhận từ `g_limit_request_queue`:

```c
ThresholdRequest req;
while (xQueueReceive(g_limit_request_queue, &req, 0) == pdTRUE) {
    // req.limit: loại ngưỡng; req.value: giá trị mới.
    // Cập nhật biến ngưỡng riêng của task điều khiển tại đây.
}
```

| Loại ngưỡng | Đơn vị | Khoảng cho phép | Mặc định khi chưa có Flash hợp lệ |
| --- | --- | --- | --- |
| `MENU_LIMIT_TEMP` | °C × 10 | -400..850 | 350 = 35.0 °C |
| `MENU_LIMIT_HUM` | % × 10 | 0..1000 | 800 = 80.0 % |
| `MENU_LIMIT_PRESS` | hPa × 10 | 3000..12000 | 10500 = 1050.0 hPa |
| `MENU_LIMIT_GAS` | Số đếm ADC | 0..4095 | 2000 |

## 5. Lưu ngưỡng vào Flash

Luồng tự động, không cần gọi thêm trong `main()`:

```text
Chỉnh ngưỡng → SAVE → Board_OnThresholdChanged()
                     → ThresholdStore_Save()
                     → thông báo qua queue và cập nhật ngưỡng menu

Khởi động → Task_Display → App_UI_Init()
                         → Menu_Init() đặt mặc định
                         → ThresholdStore_Load() nạp ngưỡng đã lưu
```

| Hàm | Tác dụng |
| --- | --- |
| `ThresholdStore_Save(values)` | Lưu mảng đủ `MENU_LIMIT_COUNT` ngưỡng. Trả `0` thành công, `-1` lỗi. Không ghi lại nếu giá trị không đổi. |
| `ThresholdStore_Load(values)` | Đọc bản hợp lệ mới nhất vào mảng. Trả `0` thành công, `-1` nếu không có bản hợp lệ hoặc tham số lỗi. |
| `Board_OnThresholdChanged(id, value, user)` | Callback lưu toàn bộ ngưỡng vào Flash trước khi gửi thông báo cho task điều khiển. |

Chỉ task display sử dụng module lưu hiện tại; module không có mutex cho nhiều task ghi đồng thời. `Menu_SetThreshold()` chỉ đổi RAM, không tự lưu Flash. `Display_RequestLimit()` chỉ gửi queue, cũng không tự lưu Flash.

Hai page 1 KB tai `0x0800F800` va `0x0800FC00` dung de luu nguong. Project dung linker mac dinh PlatformIO; `board_upload.maximum_size = 63488` kiem tra firmware khong vuot 62 KB de tranh chong len vung luu nguong.

Hai bản ghi luân phiên có checksum và dấu hoàn tất ghi cuối cùng. Nếu mất điện lúc ghi, lần khởi động sau sẽ chọn bản còn hợp lệ. Nếu cả hai bản không hợp lệ, dùng mặc định. Ghi Flash tạm chặn ngắt, nên có thể làm trễ task trong lúc SAVE.

Chọn SAVE thành công thì ngưỡng tồn tại sau khi rút nguồn. Nếu lỗi, menu hiện `FLASH SAVE FAILED!` và giữ ngưỡng RAM cũ. Xóa toàn bộ Flash khi nạp firmware sẽ xóa cả cấu hình.

## 6. Dùng chung I2C trong các task

Include `i2c_rtos.h` và sử dụng các hàm có mutex:

| Hàm | Tác dụng |
| --- | --- |
| `I2C_RTOS_Write(addr, data, len)` | Gửi một dãy byte. |
| `I2C_RTOS_ReadBytes(addr, data, len)` | Đọc một dãy byte trực tiếp. |
| `I2C_RTOS_ReadRegBytes(addr, reg, data, len)` | Gửi địa chỉ thanh ghi rồi đọc bằng repeated START; khóa mutex cho toàn bộ giao dịch. |

`addr` là địa chỉ 7 bit, không dịch trái trước khi truyền. Mã trả về: `0` thành công; `1` NACK; `2` lỗi bus; `3` timeout; `4` tham số sai; `5` không lấy được mutex trong 100 ms; `6` mutex chưa khởi tạo.

Các task dùng chung I2C1 cần đi qua lớp này. Không dùng `I2C_DelayMs()` để chờ chu kỳ task; dùng `vTaskDelay()` hoặc `vTaskDelayUntil()`.

## 7. Các hàm bên trong task display

Các hàm sau đã được task display gọi, không cần gọi thêm ở task khác:

| Hàm | Tác dụng |
| --- | --- |
| `App_UI_Init()` | Khởi tạo OLED, bộ lọc nút, menu và nạp ngưỡng Flash; chỉ gọi khi scheduler đã chạy. |
| `App_UI_GetMenu()` | Lấy con trỏ menu do task display quản lý. |
| `Menu_SetSensor()` / `Menu_SetRTC()` | Đưa dữ liệu nhận từ queue vào menu. |
| `App_UI_Poll(now_ms)` | Quét nút, xử lý thao tác, vẽ nếu nội dung đổi. |
| `Menu_Render()` | Vẽ vào framebuffer rồi gửi OLED; giữ cờ cần vẽ nếu I2C lỗi. |

Task display chờ nguồn 100 ms, thử lại sau 500 ms nếu khởi tạo OLED lỗi, rồi quét theo chu kỳ mục tiêu 20 ms. Bộ lọc nút dùng 25 ms và phát sự kiện khi **thả nút**.

## 8. Giải thích code giao diện và ứng dụng

Phần này giải thích luồng ứng dụng, FreeRTOS, nút bấm, menu, lưu Flash và vẽ OLED. Không đi sâu vào driver I2C, RTC, ADC hoặc GPIO. Các mục hướng dẫn tích hợp ở trên được giữ lại để tham khảo.

### 8.1. Sơ đồ các file và luồng chạy

| File | Vai trò |
| --- | --- |
| `src/main.c` | Khởi tạo tài nguyên, tạo task và chạy scheduler. |
| `src/app/task_display.c` | Quản lý vòng lặp giao diện và nhận dữ liệu từ queue. |
| `src/app/app_ui.c` | Ghép OLED, bộ xử lý nút, menu và ngưỡng lưu trong Flash. |
| `src/button/button.c` | Chống dội và phát sự kiện nút. |
| `src/app/menu.c` | Quản lý trang, lựa chọn, bản nháp, cảnh báo và nội dung hiển thị. |
| `src/bsp/board_port.c` | Cung cấp callback nối giao diện với các chức năng của board. |
| `src/app/threshold_store.c` | Đọc, kiểm tra và lưu ngưỡng vào Flash. |
| `src/app/app_ipc.c` | Tạo queue và đóng gói thông điệp giữa các task. |
| `src/sh1106/sh1106.c` | Quản lý framebuffer, vẽ điểm ảnh và chữ lên OLED. |

```text
main()
  -> Khởi tạo tài nguyên và tạo các task
  -> vTaskStartScheduler()
       -> Task_Display()
            -> App_UI_Init()
            -> Lặp:
                 Nhận thông điệp -> cập nhật dữ liệu menu
                 App_UI_Poll()
                   -> Button_Poll()
                   -> Menu_HandleButton()
                   -> Menu_Render()
                 Chờ đến chu kỳ tiếp theo
```

### 8.2. main.c: khởi tạo và chạy FreeRTOS

`main()` chuẩn bị phần cứng và tài nguyên trước khi gọi `vTaskStartScheduler()`. `App_IPC_Init()` phải tạo queue trước khi các task bắt đầu sử dụng chúng.

Lệnh tạo task giao diện hiện tại:

```c
configASSERT(xTaskCreate(Task_Display, "display", 768, NULL,
                         tskIDLE_PRIORITY + 2, NULL) == pdPASS);
```

- `Task_Display`: hàm mà task sẽ thực thi.
- `"display"`: tên task để quan sát khi debug.
- `768`: độ sâu stack theo đơn vị `StackType_t`, không phải byte. Trên cổng ARM_CM3 hiện tại, 768 phần tử tương đương 3072 byte.
- `NULL` đầu tiên: không truyền tham số cho task.
- `tskIDLE_PRIORITY + 2`: mức ưu tiên của task.
- `NULL` cuối: không lưu handle của task vào biến riêng.
- `pdPASS`: kết quả tạo task thành công.

`configASSERT()` kiểm tra điều kiện bắt buộc. Với cấu hình hiện tại, nếu sai thì tắt ngắt và dừng trong vòng lặp vô hạn. Khi debug, cách này giúp phát hiện việc khởi tạo thất bại ngay tại nơi kiểm tra.

Lưu ý: code hiện tại đặt một số lời gọi khởi tạo bên trong `configASSERT`. Nếu sau này định nghĩa assert thành macro rỗng, cần tách lời gọi ra ngoài để chúng vẫn được thực thi:

```c
BaseType_t result = xTaskCreate(Task_Display, "display", 768, NULL,
                               tskIDLE_PRIORITY + 2, NULL);
configASSERT(result == pdPASS);
// Khi tắt assert, vẫn cần xử lý result nếu tạo task thất bại.
```

Sau `vTaskStartScheduler()`, FreeRTOS điều phối các task. Vòng `for (;;) {}` cuối `main()` giữ CPU tại chỗ nếu scheduler trả về.

### 8.3. task_display.c: task sở hữu giao diện

`Task_Display()` thực hiện hai giai đoạn.

**Khởi tạo:** chờ 100 ms, gọi `App_UI_Init()`. Nếu thất bại thì chờ 500 ms và thử lại. `vTaskDelay()` cho task tạm ngủ để các task khác có thể chạy.

**Vòng lặp hoạt động:** đọc các thông điệp đang chờ bằng `xQueueReceive(..., 0)`, cập nhật menu, xử lý nút và vẽ màn hình. Timeout bằng `0` nghĩa là không đứng chờ nếu queue rỗng.

```c
g_oled_render_result = App_UI_Poll(
    (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS));
vTaskDelayUntil(&last, pdMS_TO_TICKS(20));
```

`xTaskGetTickCount()` lấy số tick kể từ khi scheduler chạy; nhân `portTICK_PERIOD_MS` để đổi sang mili giây. `vTaskDelayUntil()` dùng mốc chu kỳ trước để giữ nhịp mục tiêu 20 ms. Nếu xử lý mất quá lâu thì chu kỳ thực tế có thể trễ.

Các task khác gửi dữ liệu qua queue; chỉ task display trực tiếp sửa menu và vẽ OLED. Cách tổ chức này tránh nhiều task cùng thay đổi giao diện.

Các biến quan sát khi debug:

| Biến | Ý nghĩa |
| --- | --- |
| `g_oled_stage` | 1: chờ ban đầu; 2: đang khởi tạo; 3: khởi tạo lỗi và chờ thử lại; 4: đã khởi tạo thành công. |
| `g_oled_init_attempts` | Tổng số lần đã thử khởi tạo giao diện. |
| `g_oled_render_result` | 0: không cần vẽ; 1: đã vẽ; -1: lỗi. |

`volatile` giúp các lần đọc/ghi biến này được thực hiện theo yêu cầu của chương trình, thuận tiện quan sát khi debug; nó không thay thế cơ chế bảo vệ dữ liệu giữa các task.

### 8.4. app_ui.c: nối các thành phần giao diện

```c
static Sh1106 oled;
static ButtonManager buttons;
static Menu menu;
```

Ba biến giữ trạng thái OLED, nút và menu trong suốt thời gian chạy. `static` ở phạm vi file giới hạn việc truy cập trực tiếp tên biến trong file này.

`App_UI_Init()` làm lần lượt:

1. Tạo `MenuHooks` và gán các callback xử lý thao tác SAVE.
2. Khởi tạo OLED; lỗi thì trả `-1` để task thử lại.
3. Khởi tạo bộ xử lý nút với thời gian chống dội 25 ms.
4. Khởi tạo menu với giá trị mặc định.
5. Đọc ngưỡng hợp lệ từ Flash và đưa vào menu nếu có.
6. Gửi các ngưỡng đang dùng sang queue cho task điều khiển.
7. Trả `0` nếu hoàn tất khởi tạo.

```c
MenuHooks hooks = {0};
hooks.threshold_saved = Board_OnThresholdChanged;
```

`hooks` chứa con trỏ hàm. Gán tên hàm ở đây chưa gọi hàm; menu sẽ gọi nó khi người dùng chọn SAVE. Cách này cho phép menu dùng chức năng lưu mà không phải tự xử lý Flash.

`App_UI_Poll(now_ms)` là một lượt xử lý:

```c
ButtonEvent event = Button_Poll(&buttons, now_ms);
if (event != BUTTON_NONE)
    Menu_HandleButton(&menu, event);
return Menu_Render(&menu);
```

Hàm không có vòng lặp riêng vì task display đã gọi nó định kỳ.

`App_UI_GetMenu()` trả `&menu`: địa chỉ menu đang dùng, không tạo bản sao. Task display dùng con trỏ này để cập nhật dữ liệu nhận từ queue.

### 8.5. button.c: chống dội và tạo sự kiện

`Button_Init()` xóa trạng thái ban đầu, lưu callback đọc nút và thời gian chống dội. Nếu truyền thời gian bằng 0, code dùng mặc định 25 ms.

Mỗi nút có hai trạng thái:

- `raw`: trạng thái đọc tức thời.
- `stable`: trạng thái đã được công nhận sau khi ổn định đủ lâu.

Trong `Button_Poll()`:

1. Đọc trạng thái từng nút qua callback.
2. Nếu khác `raw` trước đó, cập nhật `raw` và ghi lại `changed_at_ms`.
3. Nếu `raw` khác `stable` và đã giữ đủ thời gian chống dội, cập nhật `stable`.
4. Chỉ phát sự kiện khi trạng thái ổn định chuyển sang thả nút.

Ví dụ, khi nhấn rồi thả SELECT, chương trình nhận `BUTTON_SELECT` sau khi trạng thái thả đã ổn định đủ lâu. Giữ nút không tự tạo sự kiện lặp.

Hàm dùng phép trừ thời gian kiểu `uint32_t` thay vì chờ bằng delay. Vì vậy task vẫn có thể nhận dữ liệu và xử lý menu trong lúc chống dội. Nếu nhiều nút cùng phát sự kiện trong một lượt quét, hàm chỉ trả sự kiện đầu tiên theo thứ tự quét.

### 8.6. menu.c: trạng thái trang và thao tác người dùng

`Menu` gom toàn bộ trạng thái giao diện:

| Thành phần | Tác dụng |
| --- | --- |
| `page` | Trang đang mở. |
| `selected_main`, `selected_limit`, `selected_action` | Vị trí đang chọn trong từng danh sách. |
| `sensor[]`, `valid_mask` | Giá trị hiển thị và bit đánh dấu mẫu hợp lệ. |
| `threshold[]` | Các ngưỡng đang áp dụng. |
| `editing_limit`, `edit_limit_value` | Loại ngưỡng và giá trị bản nháp đang chỉnh. |
| `alarm_mask` | Mỗi bit biểu diễn một loại cảnh báo đang hoạt động. |
| `popup_visible` | Có đang hiển thị popup cảnh báo hay không. |
| `dirty` | Nội dung đã thay đổi và cần vẽ lại. |
| `hooks` | Các callback được gọi khi người dùng lưu thay đổi. |

`Menu_Init()` xóa trạng thái, lưu đối tượng màn hình và callback, chọn trang HOME, đặt ngưỡng mặc định và bật `dirty` để vẽ lần đầu.

`Menu_HandleButton()` hoạt động như máy trạng thái: cùng một sự kiện nút nhưng tác dụng phụ thuộc `page`.

```text
HOME --SELECT--> MAIN
MAIN --chọn SET THRESHOLDS--> LIMIT_LIST
LIMIT_LIST --chọn ngưỡng--> LIMIT_CHANGE
LIMIT_CHANGE --SELECT--> LIMIT_CONFIRM
LIMIT_CONFIRM --SAVE hoặc CANCEL--> LIMIT_LIST
MAIN --chọn ALERT STATUS--> ALERTS --SELECT--> MAIN
```

Ở trang danh sách, UP/DOWN di chuyển lựa chọn theo vòng: đi qua đầu sẽ về cuối và ngược lại. Ở trang chỉnh ngưỡng, UP/DOWN tăng hoặc giảm bản nháp theo bước riêng của loại ngưỡng; vượt giới hạn thì quay vòng.

| Ngưỡng | Mặc định | Bước chỉnh | Khoảng lưu trong code |
| --- | --- | --- | --- |
| Nhiệt độ | 35.0 °C | 0.5 °C | -40.0 đến 85.0 °C |
| Độ ẩm | 80.0 % | 1.0 % | 0.0 đến 100.0 % |
| Áp suất | 1050.0 hPa | 1.0 hPa | 300.0 đến 1200.0 hPa |
| Gas | 2000 | 50 | 0 đến 4095 |

Các ngưỡng mặc định trong code là giá trị mẫu để thử giao diện, không phải giá trị đã được xác nhận cho ứng dụng thực tế.

**Bản nháp và SAVE:** thao tác tăng giảm chỉ sửa `edit_limit_value`. Khi chọn SAVE, menu gọi callback lưu Flash trước; chỉ sau khi callback thành công mới cập nhật `threshold[]`. CANCEL bỏ bản nháp. Nếu lưu lỗi, giữ ngưỡng cũ và hiện `FLASH SAVE FAILED!` để người dùng thử lại hoặc hủy.

**Dữ liệu và cảnh báo:** `Menu_SetSensor()` cập nhật giá trị và bit hợp lệ. Nếu mẫu không đổi cả giá trị lẫn trạng thái hợp lệ, hàm bỏ qua. Mẫu không hợp lệ hiển thị `--` và không tham gia so sánh ngưỡng.

`recompute_alarms()` đánh dấu cảnh báo khi mẫu hợp lệ lớn hơn hoặc bằng ngưỡng. Popup bật khi có cảnh báo mới; ưu tiên gas, nhiệt độ, độ ẩm rồi áp suất trong nhóm cảnh báo mới. SELECT đóng popup nhưng không xóa cảnh báo đang hoạt động. Khi hết mọi cảnh báo, popup được đóng.

**Vẽ theo cờ dirty:** `Menu_Render()` trả ngay nếu `dirty == 0`. Nếu cần vẽ, hàm xóa framebuffer, chọn hàm `draw_*` theo trang hoặc popup rồi gửi khung hình. Chỉ khi gửi thành công mới xóa `dirty`; lỗi thì giữ cờ để lượt sau thử lại.

Các giá trị có một chữ số thập phân được lưu bằng số nguyên nhân 10: `253` biểu diễn `25.3`. `fmt_x10()` đổi chúng thành chuỗi để tránh dùng số thực trong phần định dạng này.

### 8.7. board_port.c: callback của ứng dụng

Trong phạm vi giao diện, hai chức năng đáng chú ý là:

- `Board_DelayMs()`: đổi mili giây sang tick và gọi `vTaskDelay()`. Hàm dùng khi scheduler đã chạy.
- `Board_OnThresholdChanged()`: gom các ngưỡng hiện tại, thay ngưỡng được chỉnh bằng giá trị mới, lưu toàn bộ vào Flash rồi gửi thông báo thay đổi sang queue.

Luồng lưu ngưỡng:

```text
Người dùng chọn SAVE
  -> Menu_HandleButton()
  -> hooks.threshold_saved(...)
  -> Board_OnThresholdChanged()
  -> ThresholdStore_Save(values)
  -> Display_RequestLimit(id, value)
  -> Menu_SetThreshold() cập nhật ngưỡng trong RAM
```

Code hiện tại bỏ qua kết quả của `Display_RequestLimit()`. Vì vậy lưu Flash thành công không bảo đảm task điều khiển đã nhận thông báo, đặc biệt nếu queue đầy.

### 8.8. threshold_store.c: giữ ngưỡng sau khi mất nguồn

Module dành hai page Flash 1 KB tại `0x0800F800` và `0x0800FC00`. Linker dành riêng vùng này; firmware sử dụng 62 KB đầu.

Mỗi `Record` chứa:

| Trường | Tác dụng |
| --- | --- |
| `magic` | Dấu nhận biết đúng định dạng bản ghi. |
| `sequence` | Số thứ tự để chọn bản mới hơn. |
| `values[]` | Toàn bộ ngưỡng. |
| `checksum` | Giá trị kiểm tra dữ liệu, tính trên phần trước trường checksum. |
| `commit` | Dấu hoàn tất, được ghi sau cùng. |
| `reserved` | Phần dự phòng. |

`valid()` chỉ chấp nhận bản có magic, commit, checksum đúng và ngưỡng trong khoảng cho phép. `latest()` kiểm tra hai page và chọn bản hợp lệ mới hơn. Nếu chỉ một bản hợp lệ thì chọn bản đó; cả hai lỗi thì trả `NULL`.

`ThresholdStore_Load()` sao chép ngưỡng từ bản mới nhất vào mảng bên gọi. Không có bản hợp lệ thì trả `-1`, để ứng dụng tiếp tục dùng mặc định.

`ThresholdStore_Save()` thực hiện:

1. Kiểm tra con trỏ và khoảng giá trị.
2. Nếu giống bản đã lưu thì trả thành công ngay, tránh ghi Flash không cần thiết.
3. Chuẩn bị bản ghi mới với sequence tăng lên và checksum mới.
4. Chọn page còn lại để giữ nguyên bản hợp lệ cũ trong lúc ghi.
5. Lưu trạng thái ngắt, tắt ngắt và mở khóa Flash.
6. Xóa page đích, ghi từng nửa từ 16 bit và kiểm tra dữ liệu đọc lại.
7. Ghi commit sau cùng rồi kiểm tra toàn bản ghi.
8. Khóa Flash, khôi phục trạng thái ngắt và trả kết quả.

Nếu mất điện giữa lúc ghi, bản chưa hoàn tất thường bị loại bởi commit/checksum và bản cũ vẫn có thể được dùng. Nếu không còn bản hợp lệ, ứng dụng dùng mặc định. Ghi Flash chặn ngắt trong quá trình lưu nên có thể làm trễ tác vụ; module hiện tại được dùng từ task display và không có mutex cho nhiều bên ghi đồng thời.

### 8.9. app_ipc.c: trao đổi dữ liệu bằng queue

IPC là trao đổi dữ liệu giữa các phần chạy độc lập. Ở đây các task gửi cấu trúc dữ liệu vào queue FreeRTOS thay vì sửa trực tiếp menu của task display.

`App_IPC_Init()` tạo queue hiển thị chứa 16 `DisplayMessage`, queue yêu cầu đổi ngưỡng chứa 8 `ThresholdRequest` và một queue 2 phần tử cho chức năng ngày giờ. Hàm trả `0` khi tất cả được tạo thành công, ngược lại trả `1`.

`DisplayMessage` chứa loại thông điệp và dữ liệu tương ứng. Task display đọc trường `kind` để biết cách cập nhật menu. `ThresholdRequest` chứa loại ngưỡng `limit` và giá trị `value`.

```c
BaseType_t sent = Display_PostSensor(MENU_AHT_TEMP, 253, 1);
if (sent != pdTRUE) {
    // Chưa gửi được; bên gọi quyết định giữ mẫu và thử ở chu kỳ sau.
}
```

Các hàm gửi hiện tại dùng timeout `0`: không chờ khi queue đầy. Queue sao chép nội dung cấu trúc khi gửi thành công nên bên gửi có thể dùng lại biến cục bộ sau đó. Bỏ qua kết quả trả về có thể làm mất một thông điệp khi queue đầy.

### 8.10. sh1106.c: framebuffer, điểm ảnh và chữ

OLED có kích thước 128 × 64 điểm ảnh, mỗi điểm chỉ cần một bit sáng/tắt. Framebuffer trong RAM cần:

```text
128 × 64 / 8 = 1024 byte
```

Mỗi byte chứa 8 điểm theo chiều dọc. `SH1106_DrawPixel()` tìm byte và bit tương ứng:

```c
index = (y / 8) * SH1106_WIDTH + x;
mask = 1 << (y & 7);
```

- `y / 8`: page chứa điểm ảnh.
- `x`: cột trong page.
- `y & 7`: vị trí bit trong byte.
- Bật điểm bằng phép OR với mask; tắt điểm bằng AND với mask đảo.

`SH1106_Clear()` chỉ xóa framebuffer trong RAM. Màn hình thật thay đổi khi gọi `SH1106_Display()`.

`SH1106_DrawText()` lấy mẫu font 5 × 7 của từng ký tự rồi gọi hàm vẽ điểm ảnh. Mỗi ký tự chiếm bước ngang 6 pixel: 5 cột chữ và 1 cột cách. Hàm dừng khi hết chuỗi hoặc không đủ chỗ theo chiều ngang, không tự xuống dòng. Trong menu, hàm `line()` đặt mỗi dòng cách nhau 8 pixel.

`SH1106_Display()` gửi framebuffer theo 8 page, mỗi page 128 byte, chia thành các nhóm 32 byte. Vị trí hiển thị bắt đầu ở cột 2 để khớp module SH1106 đang dùng. Nếu một bước gửi thất bại, hàm trả `-1`; gửi hết thì trả `0`.

### 8.11. Một số cú pháp C thường gặp

| Cú pháp | Ý nghĩa trong code |
| --- | --- |
| `&menu` | Lấy địa chỉ biến menu. |
| `Menu *m` | Con trỏ trỏ đến một đối tượng Menu. |
| `m->page` | Truy cập trường page qua con trỏ m. |
| `memset(..., 0, ...)` | Đặt các byte của vùng nhớ về 0 để khởi tạo trạng thái. |
| `memcpy()` | Sao chép một số byte giữa hai vùng nhớ. |
| `(void)function()` | Gọi hàm và chủ động bỏ qua kết quả trả về. |
| `(void)user` | Đánh dấu tham số hiện chưa dùng, tránh cảnh báo compiler. |
| `const` | Không cho sửa đối tượng qua cách truy cập được khai báo const. |
| `enum` | Đặt tên cho các giá trị số như loại ngưỡng, trang hoặc sự kiện. |
| `mask & (1 << i)` | Kiểm tra bit thứ i có được bật hay không. |
| `static` trước hàm | Giới hạn tên hàm trong file nguồn hiện tại. |

Ví dụ một thao tác hoàn chỉnh: người dùng thả UP, bộ chống dội phát `BUTTON_UP`, menu chỉnh bản nháp, bật `dirty`, rồi lượt render vẽ giá trị mới. Khi chọn SAVE, callback lưu Flash trước; thành công mới áp dụng ngưỡng trong menu và trở về danh sách.
