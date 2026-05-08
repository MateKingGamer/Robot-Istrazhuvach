#include <ESP32Servo.h>
#include <esp_now.h>
#include <WiFi.h>
#include <math.h>

Servo base, arm1,arm2,wrist,grabber;
const int BASE_PIN=14, ARM1_PIN=15, ARM2_PIN=18, WRIST_PIN=5, GRABBER_PIN=27;
int baseValue=0,arm1Value=45,arm2Value=45,wristValue=90,grabberValue=180;

Servo camSide, camAngle;
const int CAM_SIDE_PIN = 25, CAM_ANGLE_PIN = 26;  
int camAngleValue = 90;
int camSideValue=90;

bool autoTracking=false;
long long lastCamWrite=0;

void setup() {
  Serial.begin(115200);

  WiFi.mode(WIFI_STA);
   if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed!");
    return;
  }
  esp_now_register_recv_cb(onReceive);

  base.attach(BASE_PIN);
  arm1.attach(ARM1_PIN);
  arm2.attach(ARM2_PIN);
  wrist.attach(WRIST_PIN);
  grabber.attach(GRABBER_PIN);
  camSide.attach(CAM_SIDE_PIN);
  camAngle.attach(CAM_ANGLE_PIN);
  delay(150);

  defaultServo();
}

void loop() {
  if(autoTracking && millis()-lastCamWrite>400){
    camSideValue=90+(baseValue-90)/2;
    camSide.write(camSideValue);
    lastCamWrite=millis();
    Serial.println("vrti");
  }
}

void upCam(){
  camAngleValue-=6;
  if(camAngleValue<0) camAngleValue=0;
  camAngle.write(camAngleValue);
}
void downCam(){
  camAngleValue+=6;
  if(camAngleValue>180) camAngleValue=180;
  camAngle.write(camAngleValue);
}
void turnCamRight(){
  camSideValue-=6;
  if(camSideValue<0) camSideValue=0;
  camSide.write(camSideValue);
}
void turnCamLeft(){
  camSideValue+=6;
  if(camSideValue>180) camSideValue=180;
  camSide.write(camSideValue);
}

void onReceive(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  if (len > 0) { 
    int command = data[0];
    Serial.println(command);
    if(command==3){
      baseValue-=7;
      if(baseValue<0)baseValue=0;
      base.write(baseValue);
    }else if(command==4){
      baseValue+=7;
      if(baseValue>180) baseValue=180;
      base.write(baseValue);
    }else if(command==1){
      arm1Value-=7;
      if(arm1Value<0)arm1Value=0;
      arm1.write(arm1Value);
    }else if(command==2){
      arm1Value+=7;
      if(arm1Value>180) arm1Value=180;
      arm1.write(arm1Value);
    }else if(command==7){
      arm2Value-=7;
      if(arm2Value<0)arm2Value=0;
      arm2.write(arm2Value);
    }else if(command==6){
      arm2Value+=7;
      if(arm2Value>180) arm2Value=180;
      arm2.write(arm2Value);
    }else if(command==9){
      wristValue-=7;
      if(wristValue<0)wristValue=0;
      wrist.write(wristValue);
    }else if(command==8){
      wristValue+=7;
      if(wristValue>180) wristValue=180;
      wrist.write(wristValue);
    }else if(command==30){
      grabber.write(180);
    }else if(command==31){
      grabber.write(0);
    }else if(command==60) autoTracking=true;
    else if(command==61) autoTracking=false;
    else if(command==20) defaultServo();
    else if(!autoTracking){
      if(command==51) upCam();
      else if(command==52) downCam();
      else if(command==53) turnCamRight();
      else if(command==54) turnCamLeft();
    }
  }
}

void defaultServo(){
  baseValue=0; arm1Value=45; arm2Value=45; wristValue=90; grabberValue=180, camAngleValue = 90, camSideValue=90;
  base.write(baseValue);
  delay(500);
  arm1.write(arm1Value);
  delay(500);
  arm2.write(arm2Value);
  delay(500);
  wrist.write(wristValue);
  delay(500);
  grabber.write(grabberValue);
  delay(500);
  camSide.write(camSideValue);
  delay(500);
  camAngle.write(camAngleValue);
  delay(500);
  
}