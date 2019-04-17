
# S20 Heater Control

### 2do

- OTA
- BMP280 sensor

### creds.h should contain

``` 
const char* ssid        = "";
const char* password    = "";
const char* mqtt_login  = "";
const char* mqtt_pass   = "";
```

### MQTT Interface

```
/devices/s20_heater_1/meta/name Towel Heater

/devices/s20_heater_1/controls/Heat
/devices/s20_heater_1/controls/Heat/meta/type switch
/devices/s20_heater_1/controls/Heat/meta/order 1

/devices/s20_heater_1/controls/Duration
/devices/s20_heater_1/controls/Duration/meta/type range
/devices/s20_heater_1/controls/Duration/meta/max 300
/devices/s20_heater_1/controls/Duration/meta/order 2

/devices/s20_heater_1/controls/Time
/devices/s20_heater_1/controls/Time/meta/type text
/devices/s20_heater_1/controls/Time/meta/readonly 1
/devices/s20_heater_1/controls/Time/meta/order 3

/devices/s20_heater_1/controls/IP
/devices/s20_heater_1/controls/IP/meta/type text
/devices/s20_heater_1/controls/IP/meta/readonly 1
/devices/s20_heater_1/controls/IP/meta/order 4

/devices/s20_heater_1/controls/RSSI
/devices/s20_heater_1/controls/RSSI/meta/type value
/devices/s20_heater_1/controls/RSSI/meta/readonly 1
/devices/s20_heater_1/controls/RSSI/meta/order 5

/devices/s20_heater_1/controls/Temperature
/devices/s20_heater_1/controls/Temperature/meta/type temperature
/devices/s20_heater_1/controls/Temperature/meta/readonly 1
/devices/s20_heater_1/controls/Temperature/meta/order 6

/devices/s20_heater_1/controls/Humidity
/devices/s20_heater_1/controls/Humidity/meta/type rel_humidity
/devices/s20_heater_1/controls/Humidity/meta/readonly 1
/devices/s20_heater_1/controls/Humidity/meta/order 7
```
