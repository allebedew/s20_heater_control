/*
 *  Heater control based on Sonnoff S20 smart socket
 *
 *  Alex Lebedev
 *  alex.lebedev@me.com
 *  
 */

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <EEPROM.h>
#include <DHT_U.h>

#include "creds.h"

#define BUTTON      0     // 0 - pushed
#define RELAY       12    // 1 - on
#define GREEN_LED   13    // 0 - on
#define RX_PIN      1
#define TX_PIN      3

#define HEARTBIT_INTERVAL       60 * 1000
#define SENSOR_READ_INTERVAL    10 * 1000

WiFiClient espClient;
PubSubClient mqtt(espClient);
DHT dht(RX_PIN, DHT21);

const char* mqtt_server       = "10.0.0.3";
const char* mqtt_device       = "towel_heater_fl1";
const char* mqtt_device_name  = "Towel Heater FL1";

volatile unsigned long button_debounce = 0;
volatile bool button_pressed_flag = false;
unsigned long timer_started = 0;
unsigned long timer_duration = 2 * 60 * 60 * 1000;
int status = 0; // 0 - all ok, -1 - no wifi, -2 - no mqtt
unsigned long last_mqtt_attempt = 0;
unsigned long last_heartbit = 0;
unsigned long last_read_sensor = 0;
float temperature = NAN, humidity = NAN;
bool sensor_error_sent = false;

const size_t topic_len = 100;
char topic[100];
const size_t msg_len = 25;
char msg[25];

struct {
  uint32_t timer_duration;
} settings;

void button_int_handler();
void mqtt_message_handler(char*, byte*, unsigned int);
void update_gpio();
bool is_button_pressed();
void blink_green_led(int);
void start_timer();
void stop_timer();
void connect_mqtt_if_needed();
void check_status();
void check_timer();
void update_mqtt(bool, bool, bool, bool);
void send_heartbit();
void format_topic(const char*, const char*, bool);
void mqtt_connected();
void mqtt_debug(const char*);
void read_sensors_and_send();

void setup() {  
  pinMode(BUTTON, INPUT);
  pinMode(RELAY, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  attachInterrupt(digitalPinToInterrupt(BUTTON), button_int_handler, CHANGE);
  update_gpio();

  EEPROM.begin(32);
  EEPROM.get(0, settings);

  // if (settings.timer_duration <= 0) {
    // settings.timer_duration = 120;
  // }

  dht.begin();

  WiFi.begin(ssid, password);  

  mqtt.setServer(mqtt_server, 1884);
  mqtt.setCallback(mqtt_message_handler);
}

void loop() {
  mqtt.loop();

  if (is_button_pressed()) {
    if (timer_started == 0) {
      start_timer();
    } else {
      stop_timer();
    }
    update_mqtt(false, true, false, false);    
  }

  connect_mqtt_if_needed();
  check_status();
  check_timer();
  update_gpio();
  read_sensors_and_send();
  send_heartbit();

  delay(50);
}

void check_status() {
//  WiFi.printDiag(Serial);
  if (!WiFi.status() == WL_CONNECTED) {
    status= -1;
  } else if (!mqtt.connected()) {
    status = -2;
  } else {
    status = 0;
  }
}

// ============= MQTT ==============

void connect_mqtt_if_needed() {
  if (mqtt.connected() || millis() - last_mqtt_attempt < 5000) {
    return;
  }
  if (mqtt.connect(mqtt_device, mqtt_login, mqtt_pass)) {
    mqtt_connected();
  }
  last_mqtt_attempt = millis();
}

void mqtt_connected() {
  update_mqtt(true, true, true, true);
  last_heartbit = millis();

  format_topic("Heat", "", true);
  mqtt.subscribe(topic);
  format_topic("Duration", "", true);
  mqtt.subscribe(topic);
}

void send_heartbit() {
  if (millis() - last_heartbit < 10 * 1000) {
    return;
  }
  update_mqtt(false, true, true, false);
  last_heartbit = millis();
}

void format_topic(const char* control, const char* meta, bool setter) {
  if (*control) {
    if (*meta) {
      snprintf(topic, topic_len, "/devices/%s/controls/%s/meta/%s", mqtt_device, control, meta);
    } else {
      if (setter) {
        snprintf(topic, topic_len, "/devices/%s/controls/%s/on", mqtt_device, control);
      } else {
        snprintf(topic, topic_len, "/devices/%s/controls/%s", mqtt_device, control);
      }
    }
  } else {
    if (*meta) {
      snprintf(topic, topic_len, "/devices/%s/meta/%s", mqtt_device, meta);
    } else {
      snprintf(topic, topic_len, "/devices/%s", mqtt_device);
    }
  }
}

void update_mqtt(bool meta, bool status, bool info, bool sensors) {
  if (status || info) {
    format_topic("Heat", "", false);
    mqtt.publish(topic, timer_started == 0 ? "0" : "1", true);

    format_topic("Duration", "", false);
    snprintf(msg, msg_len, "%d", (int)(timer_duration / 60000));
    mqtt.publish(topic, msg, true);

    format_topic("Time",  "", false);
    int min_left;
    if (timer_started == 0) {
      min_left = timer_duration / 60000;
    } else {
      min_left = (timer_duration - (millis() - timer_started)) / 60000;
    }
    snprintf(msg, msg_len, "%d min", min_left);
    mqtt.publish(topic, msg, true);
  }
  if (info) {
    format_topic("IP", "", false);
    mqtt.publish(topic, WiFi.localIP().toString().c_str(), true);

    format_topic("RSSI", "", false);
    snprintf(msg, msg_len, "%d dB", (int)WiFi.RSSI());
    mqtt.publish(topic, msg, true);
  }
  if (meta) {
    format_topic("", "name", false);
    mqtt.publish(topic, mqtt_device_name, true);

    format_topic("Heat", "type", false);
    mqtt.publish(topic, "switch", true);
    format_topic("Heat", "order", false);
    mqtt.publish(topic, "1", true);

    format_topic("Duration", "type", false);
    mqtt.publish(topic, "range", true);
    format_topic("Duration", "max", false);
    mqtt.publish(topic, "300", true);
    format_topic("Duration", "order", false);
    mqtt.publish(topic, "2", true);

    format_topic("Time", "type", false);
    mqtt.publish(topic, "text", true);
    format_topic("Time", "readonly", false);
    mqtt.publish(topic, "1", true);
    format_topic("Time", "order", false);
    mqtt.publish(topic, "3", true);

    format_topic("IP", "type", false);
    mqtt.publish(topic, "text", true);
    format_topic("IP", "readonly", false);
    mqtt.publish(topic, "1", true);
    format_topic("IP", "order", false);
    mqtt.publish(topic, "4", true);

    format_topic("RSSI", "type", false);
    mqtt.publish(topic, "text", true);
    format_topic("RSSI", "readonly", false);
    mqtt.publish(topic, "1", true);
    format_topic("RSSI", "order", false);
    mqtt.publish(topic, "5", true);

    format_topic("Temperature", "type", false);
    mqtt.publish(topic, "temperature", true);
    format_topic("Temperature", "readonly", false);
    mqtt.publish(topic, "1", true);
    format_topic("Temperature", "order", false);
    mqtt.publish(topic, "6", true);

    format_topic("Humidity", "type", false);
    mqtt.publish(topic, "rel_humidity", true);
    format_topic("Humidity", "readonly", false);
    mqtt.publish(topic, "1", true);
    format_topic("Humidity", "order", false);
    mqtt.publish(topic, "7", true);
  }
  if (sensors) {
    if (isnan(humidity) || isnan(temperature)) {
      if (sensor_error_sent == false) {
        sensor_error_sent = true;

        format_topic("Humidity", "error", false);
        mqtt.publish(topic, "r", true);
        format_topic("Temperature", "error", false);
        mqtt.publish(topic, "r", true);
      }
    } else {
      if (sensor_error_sent == true) {
        sensor_error_sent = false;
        
        format_topic("Humidity", "error", false);
        mqtt.publish(topic, "", true);
        format_topic("Temperature", "error", false);
        mqtt.publish(topic, "", true);
      }
      format_topic("Temperature", "", false);
      snprintf(msg, msg_len, "%f", temperature);
      mqtt.publish(topic, msg);

      format_topic("Humidity", "", false);
      snprintf(msg, msg_len, "%f", temperature);
      mqtt.publish(topic, msg);
    }
  }
}

void mqtt_message_handler(char* rcv_topic, byte* payload, unsigned int length) {
  strncpy(msg, (char*)payload, length);
  msg[length] = 0;

  format_topic("Heat", "", true);
  if (strcmp(topic, rcv_topic) == 0) {
    if (strcmp(msg, "1") == 0) {
      if (timer_started == 0) {
        start_timer();
      }
    } else if (strcmp(msg, "0") == 0) {
      if (timer_started != 0) {
        stop_timer();
      }
    }
    update_mqtt(false, true, false, false);
    return;
  }

  format_topic("Duration", "", true);
  if (strcmp(topic, rcv_topic) == 0) {

    settings.timer_duration = 123;
    EEPROM.put(0, settings);
    EEPROM.commit();

    return;
  }
}

void mqtt_debug(const char* text) {
  format_topic("Debug", "", false);
  mqtt.publish(topic, text, false);
}

// ================= Timer ======================

void start_timer() {
  blink_green_led(5);
  timer_started = millis();
  Serial.println("Timer started");
}

void stop_timer() {
  timer_started = 0;
  Serial.println("Timer stopped");
}

void check_timer() {
  if (timer_started != 0 && millis() - timer_started > timer_duration) {
    stop_timer();
  }
}

// =========== GPIO =================

void update_gpio() {
  bool on = timer_started != 0;
  digitalWrite(RELAY, on);
  
  if (status == -1) {
    digitalWrite(GREEN_LED, millis() % 500 > 250);
  } else if (status == -2) {
    digitalWrite(GREEN_LED, millis() % 1000 > 500);
  } else {
    digitalWrite(GREEN_LED, on);
  }
}

void blink_green_led(int count) {
  for (int i=0; i<count; ++i) {
    digitalWrite(GREEN_LED, 1);
    delay(50);
    digitalWrite(GREEN_LED, 0);
    delay(50);
  }
}

// ============== Button =================

void button_int_handler() {
  if (digitalRead(BUTTON) == 0) { 
    if (millis() - button_debounce < 200) return;
    button_pressed_flag = true;
    button_debounce = millis();
  }
}

bool is_button_pressed() {
  bool flag = button_pressed_flag;
  button_pressed_flag = false;
  return flag;
}

// ============== Sensors =====================

void read_sensors_and_send() {
  if (millis() - last_read_sensor < SENSOR_READ_INTERVAL) return;
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();
  last_read_sensor = millis();
  update_mqtt(false, false, false, true);
}

// =============== OTA =======================

void setup_ota() {


}
