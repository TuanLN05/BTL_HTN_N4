# Clone toàn bộ file trong nhánh develop về, sau đó code các task lần lượt trong thư mục "lib", và tổ chức file như trước

## Ví dụ:

### Phuoc

```text
lib/Sensor/MQ5/mq5.cpp
lib/Sensor/MQ5/mq5.h
```

### Quoc

```text
lib/Application/Display/displayOled.cpp
lib/Application/Display/displayOled.h
```

## Sử dụng hệ điều hành FreeRTOS để code cho các task

## Khi code task, sử dụng các ngoại vi đã tạo để code

## Riêng task code các cảm biến thì tạo thêm 1 drive "all_sensor" bao gồm các api đã tạo trong nhiệm vụ đã ghi trên github.

### Ví dụ:

- Tổ chức file:

  ```text
  lib/all_sensor.cpp
  lib/all_sensor.h
  ```

trong file `lib/all_sensor.h` bao gồm các hàm như: `float readMQ5();...`
