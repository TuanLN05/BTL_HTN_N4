# Nạp firmware STM32 qua web
Nạp firmware tại đường link này: http://stm32-fota.local/

thay đổi:

*WIFI_SSID = "Tuanzzz";
*WIFI_PASSWORD = "22222222";

cùng mạng với máy tính

#Sơ đồ nối dây
|esp32|STM32|Active|
|16-rx|PA9-tx||
|17-tx|PA10-rx||
|gpio26|nrst|pull down|
|gpio25|boot0|pull up|
|gnd|gnd||