#include <ESP32Servo.h>
#include <DHT.h>
#include <esp_now.h>
#include <WiFi.h>
#include <Wire.h>
#include <esp_wifi.h>

#include <TinyGPSPlus.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>
#include <qmc5883p.h>

#include "FS.h"
#include "SD.h"
#include "SPI.h"

SPIClass customSPI = SPIClass(HSPI);
File file;

TinyGPSPlus gps;
HardwareSerial GPS_Serial(1);

Adafruit_BMP280 bmp;
QMC5883P mag;


float fmap(float x, float in_min, float in_max, float out_min, float out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

float remapHeading(float h) {
  float result;

  if (h >= 146 && h < 225) {
    result = fmap(h, 146, 225, 0, 90);
  } else if (h >= 225 && h < 316) {
    result = fmap(h, 225, 316, 90, 180);
  } else if (h >= 316 || h < 50) {
    float x = (h >= 316) ? h - 316 : h + (360 - 316);
    result = fmap(x, 0, 94, 180, 270);
  } else {
    result = fmap(h, 50, 146, 270, 360);
  }

  if (result >= 360) result -= 360;
  return result;
}
bool camChanged=false;
String carMode="Manual";

DHT dht(33, 22);//dht pin,dht type

const int LIGHT_STATUS_PIN = 13;
const int LIGHT_PIN = 16;
String lightStatus = "off";

const int GAS_PIN = 35;
const int HYDROGEN_PIN=12;

Servo camSide, camAngle;
int camAngleValue = 90,camSideValue=90;
const int CAM_SIDE_PIN = 26;  
const int CAM_ANGLE_PIN = 25;

const int trigPin = 17;
const int echoPinF=2, echoPinR=5, echoPinL = 39; 
float distanceF, distanceL, distanceR;

bool changed = false;

const int checkGroundPin = 23;
int checkGroundValue;

const int checkRainPin = 32;
int rainValue;
const int rainServoPin = 27;
Servo rainServo;


const int moistureServoPin = 14;
int moistureValue = 0;
const int MOISTURE_SENSOR_PIN = 36;
Servo moistureServo;

long long startTimeTel=0;

uint8_t dolemMAC[] = {0xDC, 0x06, 0x75, 0xF7, 0xC1, 0x54};
uint8_t controllerMAC[]={0x14, 0x33, 0x5C, 0x52, 0xA4, 0x28};

typedef struct controlMessage {
  int command;
  int speed;
} controlMessage;
controlMessage cmd;

struct telemetry{
  float lat, lng;
  int speed, pressure, altitude, heading;
  int humidity, temperature;
};telemetry tel;

struct message{
  int id;
  int command;
  telemetry tel;
};message msg;


void onReceive(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  if (len > 0) {
    int command = data[0];
    checkData(command);
  }
}

/*void onDataSent(const wifi_tx_info_t*mac_addr, esp_now_send_status_t status) {
  Serial.print("\r\nLast Packet Send Status: ");

  if (status == ESP_NOW_SEND_SUCCESS) {
    Serial.println("Delivery Success");
  } else {
    Serial.println("Delivery Fail (No ACK)");
  }
}*/

void setup() { 
  Serial.begin(115200);

  Wire.begin();
  WiFi.mode(WIFI_STA);
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
   if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed!");
    return;
  }
  esp_now_register_recv_cb(onReceive);
  //esp_now_register_send_cb(esp_now_send_cb_t(onDataSent));

  esp_now_peer_info_t peer1 = {};
  memcpy(peer1.peer_addr, dolemMAC, 6);
  peer1.channel = 1;  
  peer1.encrypt = false;
  esp_now_add_peer(&peer1);

  esp_now_peer_info_t peer2 = {};
  memcpy(peer2.peer_addr, controllerMAC, 6);
  peer2.channel = 1;
  peer2.encrypt = false;
  esp_now_add_peer(&peer2);

  delay(500);
  GPS_Serial.begin(9600, SERIAL_8N1, 18, -1);

  delay(500);
  bmp.begin(0x76);

  delay(500);
  if (!mag.begin()) {
    Serial.println("Initialization failed!");
    while (true);
  }
  mag.setHardIronOffsets(0.198f, -0.023f);
  delay(500);

  pinMode(LIGHT_STATUS_PIN, INPUT);  
  pinMode(LIGHT_PIN,OUTPUT);
  digitalWrite(LIGHT_PIN, HIGH); 

  camSide.attach(CAM_SIDE_PIN);
  camAngle.attach(CAM_ANGLE_PIN);
  camAngle.write(camAngleValue);
  camSide.write(camSideValue);

  pinMode(echoPinF, INPUT);  
  pinMode(echoPinL, INPUT);  
  pinMode(echoPinR, INPUT); 
  pinMode(checkGroundPin, INPUT); 

  pinMode(GAS_PIN, INPUT);
  pinMode(HYDROGEN_PIN, INPUT); 

  rainServo.attach(rainServoPin);
  moistureServo.attach(moistureServoPin);
  rainServo.write(0);

  dht.begin();
  move("motorStop");

  customSPI.begin(15, 34, 19, 4);//pinovi za sd
  if (!SD.begin(4, customSPI)) {
    Serial.println("Card Mount Failed");
    return;
  }
  
  file = SD.open("/info.txt", FILE_APPEND);
  if(file){
    file.println("");
    file.println("");
    file.println("CAR START");
    Serial.println("Started SD");
    file.close();
  }
}

void loop() {
  if(camChanged==true) camSide.write(camSideValue);
  lightCheck("sensor");//proverue dali ima svetlo ako enma pali ga
  dhtCheck();
  checkTelemetry();
  if(carMode=="Automatic"){
    obstacleChecker("fb");
    if(checkGroundValue==1){
      move("backward");
      delay(1000);
      obstacleChecker("lr");
      if(distanceL <= distanceR){
        move("turnRight");
      }else{
        move("turnLeft");
      }
    }else if(distanceF<=20){
      obstacleChecker("lr");
      if(distanceL<=20 && distanceR<=20){
        while(distanceL<=20 && distanceR<=20){
          obstacleChecker("lr");
          move("backward");
          delay(100);
        }
      }else if(distanceL <= distanceR){
        move("turnRight");
      }else{
        move("turnLeft");
      }
    }else{
      move("forward");
    }
  }else if(carMode=="Manual" && changed){
    move("motorStop");
  }
}

void obstacleChecker(String s){
  if(s=="fb"){
    digitalWrite(trigPin, HIGH);//na ist nachin se chita od svi tri ultrasonic senzori i se pretvara u cm
    delayMicroseconds(20);
    digitalWrite(trigPin, LOW);
    distanceF = 0.017 * pulseIn(echoPinF, HIGH);
    delay(50);
    checkGroundValue=digitalRead(checkGroundPin);//bottom check with ir sensor
  }else if(s=="lr"){
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(20);
    digitalWrite(trigPin, LOW);
    distanceL = 0.017 * pulseIn(echoPinL, HIGH);
    delay(50);
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(20);
    digitalWrite(trigPin, LOW);
    distanceR = 0.017 * pulseIn(echoPinR, HIGH);
    delay(50);
  }
    
  /*Serial.print("front ");
  Serial.println(distanceF);
  Serial.print("left ");
  Serial.println(distanceL);
  Serial.print("right ");
  Serial.println(distanceR);
  Serial.print("bottom ");
  Serial.println(checkGroundValue);
  Serial.println();
  Serial.println();*/
}
void move(String s){
  if(s=="forward") cmd.command = 1;
  else if (s=="backward") cmd.command = 2;
  else if (s=="turnRight") cmd.command = 3;
  else if (s=="turnLeft") cmd.command = 4;
  else if (s=="motorStop") cmd.command = 5;
  cmd.speed = 0;//smeni u avtichedole kod ako e speed=0 da osstane proshli zatoj sto ne se prakja na ovj modul
  esp_now_send(dolemMAC, (uint8_t*)&cmd, sizeof(cmd));
}

void lightCheck(String s){
  file = SD.open("/info.txt", FILE_APPEND);
  int light=digitalRead(LIGHT_STATUS_PIN);
  if(s=="sensor"){   
    if(light==1 && lightStatus=="off"){
        delay(30);
        digitalWrite(LIGHT_PIN, LOW);//svetlo se pali na low a gasi se na high
        lightStatus="on";
        file.println("Light Status: On");
    }else if(light==0 && lightStatus=="on"){
        delay(30);
        digitalWrite(LIGHT_PIN, HIGH);
        lightStatus="off";
        file.println("Light Status: Off");
    }
  }else if(s=="on"){//to turn on 13
    if(lightStatus=="on"){
      msg.command=2;
      msg.id=1;
      esp_now_send(controllerMAC, (uint8_t*)&msg, sizeof(msg));
    }else{
      digitalWrite(LIGHT_PIN, LOW);
      file.println("Light Status: On");
    }
  }else if(s=="off"){//to turn off 14
    if(lightStatus=="on"){
      msg.command=3;
      msg.id=1;
      esp_now_send(controllerMAC, (uint8_t*)&msg, sizeof(msg));
    }else{
      digitalWrite(LIGHT_PIN, HIGH);
      file.println("Light Status: Off");
    }
  }
  file.close();
}

void upCam(){
  camAngleValue-=6;
  if(camAngleValue<0)camAngleValue=0;
  camAngle.write(camAngleValue);
}
void downCam(){
  camAngleValue+=6;
  if(camAngleValue>180)camAngleValue=180;
  camAngle.write(camAngleValue);
}
void turnCamRight(){
  camSideValue-=6;
  if(camSideValue<0)camSideValue=0;
  camChanged=true;
}
void turnCamLeft(){
  camSideValue+=6;
  if(camSideValue>180)camSideValue=180;
  camChanged=true;
}

void gasCheck() {
  static int lastGasState = -1; 
  static unsigned long lastChangeTime = 0;
  const unsigned long stableTime = 30; 
  static int confirmedGasState = -1; 
  int gas = digitalRead(GAS_PIN); 
  if (gas != lastGasState) { 
    lastChangeTime = millis(); 
    lastGasState = gas; 
  }if ((millis() - lastChangeTime) > stableTime && gas != confirmedGasState) {
    msg.command = gas ? 0 : 1; 
    msg.id=1;
    esp_now_send(controllerMAC, (uint8_t*)&msg, sizeof(msg));
    confirmedGasState = gas; 
    file = SD.open("/info.txt", FILE_APPEND);
    if(file){
      if(msg.command==0)file.println("NO GAS DETECTED");
      else file.println("GAS DETECTED");
      file.close();
    }
  }//ovoj e za mq135

  static int lastHydState = -1;
  static unsigned long lastHydChangeTime = 0;
  const unsigned long stableTimeHyd = 30;
  static int confirmedHydState = -1;
  int hyd = digitalRead(HYDROGEN_PIN);
  
  if (hyd != lastHydState) {
    lastHydChangeTime = millis();
    lastHydState = hyd;
  }if ((millis() - lastHydChangeTime) > stableTimeHyd && hyd != confirmedHydState) {
    msg.command = hyd ? 8 : 9; // 8 = nema gas, 9 = ima gas
    msg.id=1;
    esp_now_send(controllerMAC, (uint8_t*)&msg, sizeof(msg));
    confirmedHydState = hyd;
    file = SD.open("/info.txt", FILE_APPEND);
    if(file){
      if(msg.command==8)file.println("NO HYDROGEN GAS DETECTED");
      else file.println("HYDROGEN GAS DETECTED");
      file.close();
    }
  }//ovoj e za hydrogen senzor

}//uglavnom provertue dali stvarno se smenila vrednost i prakja ako se ako ne nisto

void dhtCheck(){
  static long nowTime=0;
  if((millis()-nowTime)>10000){
    tel.humidity=dht.readHumidity();
    tel.temperature=dht.readTemperature();
    
    file = SD.open("/info.txt", FILE_APPEND);
    if(file){
      file.printf("Temperature is: %d", tel.temperature);
      file.printf(", Humidity is: %d\n", tel.humidity);
      file.close();
    }
    nowTime=millis();
  }
}//proverue temperaturu i vlazhnost

void rainCheck(){
  rainServo.write(90);
  delay(2000);
  rainValue=analogRead(checkRainPin);
  //Serial.println("rain");
  //Serial.println(rainValue);
  delay(2000);
  rainServo.write(0);
  file = SD.open("/info.txt", FILE_APPEND);
  if(rainValue<2000){
    msg.command=4;
    if(file) file.printf("Rain Sensor: Not Raining");
  }else{
    msg.command=5;
    if(file) file.printf("Rain Sensor: Raining");
  }
  msg.id=1;
  esp_now_send(controllerMAC, (uint8_t*)&msg, sizeof(msg));
  file.close();
}

void groundCheck(){
  moistureServo.write(130);
  delay(1200);
  moistureServo.write(90);
  delay(1000);
  moistureValue=analogRead(MOISTURE_SENSOR_PIN);
  //Serial.println("ground");
  //Serial.println(moistureValue);
  delay(2000);
  moistureServo.write(50);
  delay(1200);
  moistureServo.write(90);
  file = SD.open("/info.txt", FILE_APPEND);
  if(moistureValue<2000){
    msg.command=7;
    if(file) file.printf("Ground Sensor: Wet");
  }else{
    msg.command=6;
    if(file) file.printf("Ground Sensor: Dry");
  }
  msg.id=1;
  esp_now_send(controllerMAC, (uint8_t*)&msg, sizeof(msg));
  file.close();
}

void checkTelemetry(){
  if(millis()-startTimeTel>=10000){
    file = SD.open("info.txt", FILE_APPEND);
    while (GPS_Serial.available()) {
      gps.encode(GPS_Serial.read());
    }
    if (gps.location.isValid()) {
      tel.lat=gps.location.lat();
      tel.lng=gps.location.lng();
      tel.speed=int(gps.speed.kmph());
      if(file) file.println("GPS LOCATED");
    }else{
      tel.lat=0;
      tel.lng=0;
      tel.speed=0;
      if(file) file.println("NO GPS LOCATED");
    }

    float pressure = bmp.readPressure() / 100.0F;  // hPa
    float temperature = bmp.readTemperature();    // °C
    float altitude = (8.31432 * (temperature + 273.15)) / 0.28404373326 * log(1021 / pressure);
    tel.pressure=(int)pressure;
    tel.altitude=(int)altitude;
    if(file){  
      file.print("Pressure is: ");
      file.print(pressure);
      file.print("Altitude is: ");
      file.print(altitude);
    }

    float xyz[3];
    if (mag.readXYZ(xyz)) {
      xyz[0] *= 0.373f / 0.397f;
      xyz[1] *= 0.373f / 0.350f;
      float heading = remapHeading(mag.getHeadingDeg(5.4));
      tel.heading=(int)heading;
      if(file){  
        file.print("Heading is: ");
        file.print(heading);
      }
    }
    msg.id=2;
    msg.command=0;
    msg.tel=tel;
    esp_now_send(controllerMAC, (uint8_t*)&msg, sizeof(msg));
    file.close();
    startTimeTel=millis();
  }
}

void checkData(int data){
  camChanged=false;
  file = SD.open("/info.txt", FILE_APPEND);
  if(file){
    file.print("Received data");
    file.println(data);
    file.close();
  }
  Serial.println(data);
  if(data==1){
      carMode="Automatic";
      changed=true;
    }else if(data==2){
      carMode="Manual";
      changed=true;
    }else if(data==51) upCam();
    else if(data==52) downCam();
    else if(data==53) turnCamRight();
    else if(data==54) turnCamLeft(); 
    else if(data==8) lightCheck("on");
    else if(data==9) lightCheck("off");
    else if(data==10) rainCheck();
    else if(data==11) groundCheck();
}