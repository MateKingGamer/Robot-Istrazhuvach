#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <esp_now.h>
#include <WiFi.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

Adafruit_MPU6050 mpu;

uint8_t doleMAC[] = {0xDC, 0x06, 0x75, 0xF7, 0xC1, 0x54};
uint8_t goreMAC[] = {0x00, 0x4B, 0x12, 0xEB, 0xEB, 0xCC}; // gore
uint8_t gore2MAC[] = {0x00, 0x70, 0x07, 0x8A, 0x08, 0xA4}; // gore ruka

int8_t commandDole = 0, commandGore = 0;
int lastCommandDole=0, lastCommandGore=0;

int buttonPin1=3, buttonPin2=1;
String mode="drive";
long long startTime=0, startTime2=0, startTime3=0, startTime4=0;

int speedInt=150; 
bool changedBack=true;

Adafruit_SSD1306 display(128, 32, &Wire, -1);

struct controlMessage {
  int command;
  int speed;
};

controlMessage cmd;

void setup() {
  Serial.begin(115200);
  Wire.begin(8, 9); // SDA,SCL

  if (!mpu.begin()) {
    Serial.println("MPU6050 not found. Check wiring.");
    while (1);
  }

  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW Init Failed");
    return;
  }

  // Register Dole Peer
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, doleMAC, 6);
  peerInfo.channel = 1;
  peerInfo.encrypt = false;
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add Dole peer");
    return;
  }

  // Register Gore Peer
  esp_now_peer_info_t peer2 = {};
  memcpy(peer2.peer_addr, goreMAC, 6);
  peer2.channel = 1;
  peer2.encrypt = false;
  esp_now_add_peer(&peer2);

  // Register Gore 2 Peer
  esp_now_peer_info_t peer3 = {};
  memcpy(peer3.peer_addr, gore2MAC, 6);
  peer3.channel = 1;
  peer3.encrypt = false;
  esp_now_add_peer(&peer3);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.begin(115200);
    Serial.println(F("SSD1306 allocation failed"));
    for (;;); // Don’t continue, loop forever
  }

  display.clearDisplay();
  display.setTextSize(2);             // Big text
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(32, 0);
  display.setRotation(2);
  display.println(F("Mode:"));
  display.setCursor(30, 16);
  display.println(F("Drive"));
  display.display();  

  pinMode(buttonPin1, INPUT);
  pinMode(buttonPin2, INPUT);
  Serial.println("Hand ESP Ready");
}

void loop() {
  
  if(millis()-startTime4>3000 && changedBack ==false){
    display.clearDisplay();
    display.setCursor(32, 0);
    display.println(F("Mode:"));
    if(mode=="drive"){
      display.setCursor(30, 16);
      display.println(F("Drive"));
    }else if(mode=="cam"){
      display.setCursor(25, 16);
      display.println(F("Camera"));
    }else if(mode=="drive 2"){
      display.setCursor(20, 16);
      display.println(F("Drive 2"));
    }
    display.display();  
    changedBack=true;
  }
  
  checkButtons();
  checkingAngle();
  
  // Send Dole commands
  if(commandDole!=lastCommandDole){
    lastCommandDole=commandDole;
    cmd.command=commandDole;
    cmd.speed=speedInt;
    Serial.println(commandDole);
    esp_now_send(doleMAC, (uint8_t*)&cmd, sizeof(cmd));
  }
  
  // Send Gore (and Gore 2) commands
  if(commandGore!=lastCommandGore && commandGore!=0){
    lastCommandGore=commandGore;
    
    // Always send to goreMAC
    esp_now_send(goreMAC, (uint8_t*)&commandGore, sizeof(commandGore));
    
    // Also send to gore2MAC if we are specifically in camera mode
    if(mode == "cam") {
      esp_now_send(gore2MAC, (uint8_t*)&commandGore, sizeof(commandGore));
    }
  }
}

void checkButtons(){
  int button1=digitalRead(buttonPin1);
  int button2=digitalRead(buttonPin2);
  
  if(button1==LOW && millis()-startTime>400 && mode!="drive 2"){
    display.clearDisplay();
    display.setCursor(32, 0);
    display.println(F("Mode:"));
    startTime=millis();
    if(mode=="drive"){
      display.setCursor(25, 16);
      display.println(F("Camera"));
      mode="cam";
    }else if(mode=="cam"){
      display.setCursor(30, 16);
      display.println(F("Drive"));
      mode="drive";
    }
    display.display();
  }
  if(button2==LOW && millis()-startTime2>400 && mode!="cam"){
    display.clearDisplay();
    display.setCursor(32, 0);
    display.println(F("Mode:"));
    startTime2=millis();
    if(mode=="drive"){
      display.setCursor(20, 16);
      display.println(F("Drive 2"));
      mode="drive 2";
    }else if(mode=="drive 2"){
      display.setCursor(30, 16);
      display.println(F("Drive"));
      mode="drive";
    }
    display.display(); 
  }
}

void checkingAngle(){
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  if(mode=="drive"){
    if (a.acceleration.y > 5.0) commandDole = 4;  // Backward
    else if (a.acceleration.y < -5.0) commandDole = 3;  // Forward
    else if (a.acceleration.x < -5.0) commandDole = 1;  // Right
    else if (a.acceleration.x > 5.0)  commandDole = 2;  // Left
    else commandDole = 5;  // Neutral
    
  }else if(mode=="cam"){
    if (a.acceleration.y > 5.0) commandGore = 53;  // cam Backward
    else if (a.acceleration.y < -5.0) commandGore = 54;  // cam Forward
    else if (a.acceleration.x < -5.0) commandGore = 51;  // cam Right
    else if (a.acceleration.x > 5.0)  commandGore = 52;  // cam Left
    else commandGore = 50;  // Neutral cam
    
  }else if(mode=="drive 2"){
    // Drive 2 specific mapped commands
    if (a.acceleration.y > 5.0) commandDole = 101;       // Down (Backward)
    else if (a.acceleration.y < -5.0) commandDole = 100; // Up (Forward)
    else if (a.acceleration.x < -5.0) commandDole = 1; // Right
    else if (a.acceleration.x > 5.0)  commandDole = 2; // Left
    else commandDole = 5;                              // Neutral
  }
}