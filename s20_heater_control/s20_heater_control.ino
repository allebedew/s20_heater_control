
#define BUTTON      0     // 0 - pushed
#define RELAY       12    // 1 - on
#define GREEN_LED   13    // 0 - on
#define RX_PIN      1
#define TX_PIN      3

volatile unsigned long button_debounce = 0;
volatile bool button_pressed_flag = false;
unsigned long timer_started = 0;
unsigned long timer_duration = 10 * 1000;
bool error_state = true;

void setup() {
  
  Serial.begin(115200);
  
  pinMode(BUTTON, INPUT);
  pinMode(RELAY, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);

  attachInterrupt (digitalPinToInterrupt(BUTTON), button_int_handler, CHANGE);
  
}

void loop() {

  if (is_button_pressed()) {
    if (timer_started == 0) {
      start_timer();
    } else {
      stop_timer();
    }
  }

  check_timer();
  update_gpio();

  delay(10);
}

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

void update_gpio() {
  bool on = timer_started != 0;
  digitalWrite(RELAY, on);
  
  if (error_state) {
    digitalWrite(GREEN_LED, millis() % 500 > 250);
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
