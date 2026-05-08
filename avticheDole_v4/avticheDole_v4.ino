#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

// --- CUSTOM MAC ADDRESS ---
uint8_t customMac[] = {0xDC, 0x06, 0x75, 0xF7, 0xC1, 0x54};

typedef struct controlMessage {
  int command;
  int speed;
} controlMessage;

controlMessage incoming;

const int ENA_FL = 4;  // PWM Front-Left
const int IN1_FL = 3;  // Dir Front-Left 1
const int IN2_FL = 2;  // Dir Front-Left 2

const int ENB_FR = 5;  // PWM Front-Right
const int IN3_FR = 0;  // Dir Front-Right 1
const int IN4_FR = 1;  // Dir Front-Right 2

const int ENA_RL = 6;  // PWM Rear-Left
const int IN1_RL = 7;  // Dir Rear-Left 1
const int IN2_RL = 8;  // Dir Rear-Left 2

const int ENB_RR = 20;  // PWM Rear-Right
const int IN3_RR = 9; // Dir Rear-Right 1
const int IN4_RR = 10; // Dir Rear-Right 2 (RX Pin)

int speed=210;

// --- DATA STRUCTURE ---
typedef struct struct_message {
    int command; 
    /* Commands:
       F: Forward, B: Backward, L: Strafe Left, R: Strafe Right
       U: Rotate Left, V: Rotate Right
       Q: Diag Forward-Left, E: Diag Forward-Right
       Z: Diag Backward-Left, C: Diag Backward-Right
       S: Stop
    */
    int speed; // 0 to 255
} struct_message;

struct_message myData;

// --- MOTOR CONTROL HELPER ---
// dir: 1 = Forward, -1 = Backward, 0 = Stop
void setMotor(int enPin, int in1Pin, int in2Pin, int dir, int speed) {
  analogWrite(enPin, (dir == 0) ? 0 : speed); // If stopped, speed is 0
  if (dir == 1) {
    digitalWrite(in1Pin, HIGH);
    digitalWrite(in2Pin, LOW);
  } else if (dir == -1) {
    digitalWrite(in1Pin, LOW);
    digitalWrite(in2Pin, HIGH);
  } else {
    digitalWrite(in1Pin, LOW);
    digitalWrite(in2Pin, LOW);
  }
}

// --- FULL MECANUM KINEMATICS ---

void moveForward(int s) {
  setMotor(ENA_FL, IN1_FL, IN2_FL, 1, s); setMotor(ENB_FR, IN3_FR, IN4_FR, 1, s);
  setMotor(ENA_RL, IN1_RL, IN2_RL, 1, s); setMotor(ENB_RR, IN3_RR, IN4_RR, 1, s);
}

void moveBackward(int s) {
  setMotor(ENA_FL, IN1_FL, IN2_FL, -1, s); setMotor(ENB_FR, IN3_FR, IN4_FR, -1, s);
  setMotor(ENA_RL, IN1_RL, IN2_RL, -1, s); setMotor(ENB_RR, IN3_RR, IN4_RR, -1, s);
}

void strafeLeft(int s) {
  setMotor(ENA_FL, IN1_FL, IN2_FL, -1, s); setMotor(ENB_FR, IN3_FR, IN4_FR, 1, s);
  setMotor(ENA_RL, IN1_RL, IN2_RL, 1, s);  setMotor(ENB_RR, IN3_RR, IN4_RR, -1, s);
}

void strafeRight(int s) {
  setMotor(ENA_FL, IN1_FL, IN2_FL, 1, s);  setMotor(ENB_FR, IN3_FR, IN4_FR, -1, s);
  setMotor(ENA_RL, IN1_RL, IN2_RL, -1, s); setMotor(ENB_RR, IN3_RR, IN4_RR, 1, s);
}

void rotateLeft(int s) {
  setMotor(ENA_FL, IN1_FL, IN2_FL, -1, s); setMotor(ENB_FR, IN3_FR, IN4_FR, 1, s);
  setMotor(ENA_RL, IN1_RL, IN2_RL, -1, s); setMotor(ENB_RR, IN3_RR, IN4_RR, 1, s);
}

void rotateRight(int s) {
  setMotor(ENA_FL, IN1_FL, IN2_FL, 1, s);  setMotor(ENB_FR, IN3_FR, IN4_FR, -1, s);
  setMotor(ENA_RL, IN1_RL, IN2_RL, 1, s);  setMotor(ENB_RR, IN3_RR, IN4_RR, -1, s);
}

void diagForwardRight(int s) {
  // FL and RR drive forward, FR and RL are stopped
  setMotor(ENA_FL, IN1_FL, IN2_FL, 1, s);  setMotor(ENB_FR, IN3_FR, IN4_FR, 0, 0);
  setMotor(ENA_RL, IN1_RL, IN2_RL, 0, 0);  setMotor(ENB_RR, IN3_RR, IN4_RR, 1, s);
}

void diagForwardLeft(int s) {
  // FR and RL drive forward, FL and RR are stopped
  setMotor(ENA_FL, IN1_FL, IN2_FL, 0, 0);  setMotor(ENB_FR, IN3_FR, IN4_FR, 1, s);
  setMotor(ENA_RL, IN1_RL, IN2_RL, 1, s);  setMotor(ENB_RR, IN3_RR, IN4_RR, 0, 0);
}

void diagBackwardRight(int s) {
  // FR and RL drive backward, FL and RR are stopped
  setMotor(ENA_FL, IN1_FL, IN2_FL, 0, 0);  setMotor(ENB_FR, IN3_FR, IN4_FR, -1, s);
  setMotor(ENA_RL, IN1_RL, IN2_RL, -1, s); setMotor(ENB_RR, IN3_RR, IN4_RR, 0, 0);
}

void diagBackwardLeft(int s) {
  // FL and RR drive backward, FR and RL are stopped
  setMotor(ENA_FL, IN1_FL, IN2_FL, -1, s); setMotor(ENB_FR, IN3_FR, IN4_FR, 0, 0);
  setMotor(ENA_RL, IN1_RL, IN2_RL, 0, 0);  setMotor(ENB_RR, IN3_RR, IN4_RR, -1, s);
}

void stopRobot() {
  setMotor(ENA_FL, IN1_FL, IN2_FL, 0, 0);  setMotor(ENB_FR, IN3_FR, IN4_FR, 0, 0);
  setMotor(ENA_RL, IN1_RL, IN2_RL, 0, 0);  setMotor(ENB_RR, IN3_RR, IN4_RR, 0, 0);
}

// --- ESP-NOW CALLBACK ---
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  memcpy(&myData, incomingData, sizeof(myData));
  
  switch(myData.command) {
    case 2: moveForward(myData.speed); break;
    case 1: moveBackward(myData.speed); break;
    case 100: strafeLeft(myData.speed); break;
    case 101: strafeRight(myData.speed); break;
    case 3: rotateLeft(myData.speed); break;
    case 4: rotateRight(myData.speed); break;
    case 51: diagForwardLeft(myData.speed); break;
    case 52: diagForwardRight(myData.speed); break;
    case 53: diagBackwardLeft(myData.speed); break;
    case 54: diagBackwardRight(myData.speed); break;
    default: stopRobot(); break;
  }
}

void setup() {
  // Initialize Motor Pins
  pinMode(ENA_FL, OUTPUT); pinMode(IN1_FL, OUTPUT); pinMode(IN2_FL, OUTPUT);
  pinMode(ENB_FR, OUTPUT); pinMode(IN3_FR, OUTPUT); pinMode(IN4_FR, OUTPUT);
  pinMode(ENA_RL, OUTPUT); pinMode(IN1_RL, OUTPUT); pinMode(IN2_RL, OUTPUT);
  pinMode(ENB_RR, OUTPUT); pinMode(IN3_RR, OUTPUT); pinMode(IN4_RR, OUTPUT);
  
  stopRobot();

  // 1. Set Wi-Fi to Station Mode
  WiFi.mode(WIFI_STA);
  
  // 2. Apply the custom MAC address before initializing ESP-NOW
  esp_wifi_set_mac(WIFI_IF_STA, customMac);

  // 3. Initialize ESP-NOW
  if (esp_now_init() != ESP_OK) return;
  esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));
}

void loop() {
  // Awaiting ESP-NOW commands
}