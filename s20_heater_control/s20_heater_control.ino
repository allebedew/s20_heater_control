
/*
 *  Heater control based on Sonnoff S20 smart socket
 *
 *  Alex Lebedev
 *  alex.lebedev@me.com
 *
 */

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "creds.h"

#define BUTTON      0     // 0 - pushed
#define RELAY       12    // 1 - on
#define GREEN_LED   13    // 0 - on
#define RX_PIN      1
#define TX_PIN      3

WiFiClient espClient;
PubSubClient mqtt(espClient);

const char* mqtt_server = "10.0.0.4";
const char* mqtt_name   = "s20_heater_1";

volatile unsigned long button_debounce = 0;
volatile bool button_pressed_flag = false;
unsigned long timer_started = 0;
unsigned long timer_duration = 10 * 1000;
int status = 0; // 0 - all ok, -1 - no wifi, -2 - no mqtt
unsigned long last_mqtt_attempt = 0;

void setup() {
  Serial.begin(115200);
  delay(10);
  
  pinMode(BUTTON, INPUT);
  pinMode(RELAY, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  attachInterrupt (digitalPinToInterrupt(BUTTON), button_int_handler, CHANGE);
  update_gpio();

  WiFi.begin(ssid, password);  
  mqtt.setServer(mqtt_server, 1884);
  mqtt.setCallback(callback);
}

void loop() {
  mqtt.loop();

  if (is_button_pressed()) {
    if (timer_started == 0) {
      start_timer();
    } else {
      stop_timer();
    }
  }

  connect_mqtt_if_needed();
  check_status();
  check_timer();
  update_gpio();

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

void connect_mqtt_if_needed() {
  if (mqtt.connected() || millis() - last_mqtt_attempt < 5000) {
    return;
  }
  Serial.println("Connecting MQTT...");
  if (mqtt.connect(mqtt_name, mqtt_login, mqtt_pass)) {
    Serial.println("Connected");
    mqtt.publish("/test/heater", "online");
  } else {
    Serial.print("Failed to connect, rc=");
    Serial.println(mqtt.state());
  }
  last_mqtt_attempt = millis();
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  /*
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // Switch on the LED if an 1 was received as first character
  if ((char)payload[0] == '1') {
    digitalWrite(BUILTIN_LED, LOW);   // Turn the LED on (Note that LOW is the voltage level
    // but actually the LED is on; this is because
    // it is active low on the ESP-01)
  } else {
    digitalWrite(BUILTIN_LED, HIGH);  // Turn the LED off by making the voltage HIGH
  }
  */
}

void start_timer() {
  blink_green_led(5);
  timer_started = millis();
  mqtt.publish("/test/heater/on", "1");
  Serial.println("Timer started");
}

void stop_timer() {
  timer_started = 0;
  mqtt.publish("/test/heater/on", "0");
  Serial.println("Timer stopped");
}

void check_timer() {
  if (timer_started != 0 && millis() - timer_started > timer_duration) {
    stop_timer();
  }
}

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

void button_int_handler() {
  if (digitalRead(BUTTON) == 0) { 
    if (millis() - button_debounce < 200) {
      return;
    }
    button_pressed_flag = true;
    button_debounce = millis();
  }
}

bool is_button_pressed() {
  bool flag = button_pressed_flag;
  button_pressed_flag = false;
  return flag;
}

void blink_green_led(int count) {
  for (int i=0; i<count; ++i) {
    digitalWrite(GREEN_LED, 1);
    delay(50);
    digitalWrite(GREEN_LED, 0);
    delay(50);
  }
}
