#include <esp_now.h>
#include <WiFi.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd(0x27, 16, 2);

String mode="Sensor Car";

String sensorMode="Manual";
String handMode="Drive + Cam";//Drive + Cam i Hand Move

String currentLightMode="light_off";
String currentSpeed="Medium";
String gasState="gas_low", hydroState="no_hydro";
String clawStatus="open";
String autoTracking="OFF";

long long startTime;
int counter=0;

float temperature=0, humidity=0;

int pressure=0, altitude=0, heading=0;
bool foundGps=false, showGps=false;
float lat=0, lng=0, speed=0;
int speedInt=150;

typedef struct controlMessage {
  int command;
  int speed;
} controlMessage;
controlMessage cmd;

struct telemetry{
  float lat, lng;
  int speed, pressure, altitude, heading;
  int humidity, temperature;
};
struct message{
  int id;
  int command;
  telemetry tel;
};message incomingData;

uint8_t doleMAC[] = {0xDC, 0x06, 0x75, 0xF7, 0xC1, 0x54};//dole
uint8_t goreMAC[] = {0x00, 0x4B, 0x12, 0xEB, 0xEB, 0xCC}; // gore
uint8_t gore2MAC[] = {0x00, 0x70, 0x07, 0x8A, 0x08, 0xA4}; // gore
int command=0;

String sensorCtrlMode="Camera";

void onReceive(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  if (len==sizeof(message)) { 
    memcpy(&incomingData, data, sizeof(message));
    if (incomingData.id==1) dataCheck(incomingData.command);
    else{
      lat=incomingData.tel.lat;
      lng=incomingData.tel.lng;
      speed=incomingData.tel.speed;
      pressure=incomingData.tel.pressure;
      altitude=incomingData.tel.altitude;
      heading=incomingData.tel.heading;
      humidity=incomingData.tel.humidity;
      temperature=incomingData.tel.temperature;
      if(lat==0) showGps=false;
      else showGps=true;
    }
  }else if (len == sizeof(int)) {
    int receivedCommand;
    memcpy(&receivedCommand, data, sizeof(int));
    dataCheck(receivedCommand);
  }
}

void setup() {
  delay(500);
  Serial.begin(115200); // Debugging to PC
  Serial2.begin(9600, SERIAL_8N1, 4, 17);  // Communication with Arduino
  Wire.begin(23, 21);
  //delay(500);
  lcd.init();                      
  lcd.backlight();

  delay(500);
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  //esp_now_register_send_cb(esp_now_send_cb_t(onDataSent));
  esp_now_register_recv_cb(esp_now_recv_cb_t(onReceive));

  esp_now_peer_info_t peer1 = {};
  memcpy(peer1.peer_addr, doleMAC, 6);
  peer1.channel = 1;  
  peer1.encrypt = false;
  esp_now_add_peer(&peer1);

  esp_now_peer_info_t peer2 = {};
  memcpy(peer2.peer_addr, goreMAC, 6);
  peer2.channel = 1;
  peer2.encrypt = false;
  esp_now_add_peer(&peer2);

  esp_now_peer_info_t peer3 = {};
  memcpy(peer3.peer_addr, gore2MAC, 6);
  peer3.channel = 1;
  peer3.encrypt = false;
  esp_now_add_peer(&peer3);
  delay(500);
  lcd_write_hand("chose_mode");
}

void loop() {
  if(mode=="Sensor Car"){
    if(millis()-startTime>3000){
      if(counter==0){
        lcd_write_sensor(gasState);
        counter=1;
      }else if(counter==1){
        lcd_write_sensor(hydroState);
        counter=2;
      }else if(counter==2){
        lcd_write_sensor("temperature");
        counter=3;
      }else if(counter==3){
        lcd_write_sensor("humidity");
        counter=4;
      }else if(counter==4){
        if(showGps) lcd_write_sensor("gps");
        else lcd_write_sensor("no_gps");
        counter=5;
      }else if(counter==5){
        lcd_write_sensor("prs/alt");
        counter=6;
      }else if(counter==6){
        lcd_write_sensor("head/speed");
        counter=0;
      }
    startTime=millis();
    }//lcd writer for info
  }else{
    if(millis()-startTime>3000){
      lcd_write_hand("handMode");
      startTime=millis();
    }
    
  }


  if (Serial2.available()) {
    String data = Serial2.readStringUntil('\n');
    int incoming=data.toInt();
    Serial.println(incoming);
    if(incoming==100){
      if(sensorCtrlMode=="Drive"){
        sensorCtrlMode="Camera";
      }else{
        sensorCtrlMode="Drive";
      }
      lcd_write_sensor("drive switch");
    }else if(incoming==10){
      if(mode=="Sensor Car"){
        mode="Hand Car";
        handMode=="Drive + Cam";
      }else{
        mode="Sensor Car";
        handMode=="no";
      }
      lcd_write_hand("mode switch");
    }else if(incoming==11){
      if(mode=="Sensor Car"){
        if(sensorMode=="Automatic"){
          sensorMode="Manual";
          command=2;
        }else{
          sensorMode="Automatic"; 
          command=1;
        }
        esp_now_send(goreMAC, (uint8_t*)&command, sizeof(command));
        lcd_write_sensor("sensor_mode");
      }else{//mode==ruka
          if(handMode=="Drive + Cam") handMode="Hand Move";
          else handMode="Drive + Cam";
          lcd_write_hand("handMode");
      }
    }else if(incoming==12){
      if(mode=="Sensor Car"){
        if(currentLightMode=="light_off"){
          currentLightMode="light_on";
          command=8;
        }else if(currentLightMode=="light_on"){
          currentLightMode="light_off";
          command=9;
        }
        lcd_write_sensor(currentLightMode);
        esp_now_send(goreMAC, (uint8_t*)&command, sizeof(command));
      }else{
        command=20;//default all servos to starting positions
        lcd_write_hand("default_servos");
        esp_now_send(gore2MAC, (uint8_t*)&command, sizeof(command));
      }
    }else if(incoming==13){
      if(currentSpeed=="Low"){
      currentSpeed="Medium";
      speedInt=150;
      }else if(currentSpeed=="Medium"){
        currentSpeed="High";
        speedInt=255;
      }else if(currentSpeed=="High"){
        currentSpeed="Low";
        speedInt=80;
      }
      lcd_write_sensor(currentSpeed);
    }else if(incoming==14){
      if(mode=="Sensor Car"){
        command=10;
        //esp_now_send(goreMAC, (uint8_t*)&command, sizeof(command));
      }else{
        /*if(clawStatus=="closed"){
          clawStatus="open";
          command=30;
        }else{
          clawStatus="closed";
          command=31;
        }
        lcd_write_hand(clawStatus);
        esp_now_send(gore2MAC, (uint8_t*)&command, sizeof(command));//open claw*/
      }
    }else if(incoming==15){
      if(mode=="Sensor Car"){
        command=11;
        esp_now_send(goreMAC, (uint8_t*)&command, sizeof(command));
      }else{
        //autotracking
        /*if(autoTracking=="OFF"){
          autoTracking="ON";
          command=60;
        }else{
          autoTracking="OFF";
          command=61;
        }
        lcd_write_hand("autoTracking");
        esp_now_send(gore2MAC, (uint8_t*)&command, sizeof(command));*/
        if(clawStatus=="closed"){
          clawStatus="open";
          command=30;
        }else{
          clawStatus="closed";
          command=31;
        }
        lcd_write_hand(clawStatus);
        esp_now_send(gore2MAC, (uint8_t*)&command, sizeof(command));//open claw
      }
    }else if(incoming==1 || incoming==2 || incoming==3 || incoming==4 || incoming==5){
      if(mode=="Sensor Car" || handMode=="Drive + Cam"){
        if(sensorCtrlMode=="Drive"){
          if(incoming==3)incoming=100;
          else if(incoming==4)incoming=101;
        }
        cmd.command=incoming;
        cmd.speed=speedInt;
        esp_now_send(doleMAC, (uint8_t*)&cmd, sizeof(cmd));//drive i za sensor i ruka del kd e
      }else{
        //int movementSend=incoming-1;//ruka mrdanje dole i arm1, desan joystick
        esp_now_send(gore2MAC, (uint8_t*)&incoming, sizeof(incoming));
      }
    }else if(incoming==50 || incoming==51 || incoming==52 || incoming==53 || incoming==54){
      if((mode=="Sensor Car" || handMode=="Drive + Cam") && sensorCtrlMode=="Camera") esp_now_send(goreMAC, (uint8_t*)&incoming, sizeof(incoming));
      else if(sensorCtrlMode=="Drive" && handMode!="Hand Move"){
        cmd.command=incoming;
        cmd.speed=speedInt;
        esp_now_send(doleMAC, (uint8_t*)&cmd, sizeof(cmd));
        Serial.println("prakja");
      }else if(handMode=="Drive + Cam" && sensorCtrlMode=="Camera") esp_now_send(gore2MAC, (uint8_t*)&incoming, sizeof(incoming));
      else if(handMode=="Hand Move"){
        int movementSend=incoming-45;
        esp_now_send(gore2MAC, (uint8_t*)&movementSend, sizeof(movementSend));
      }
    }
  }
}
void lcd_write_sensor(String s){
  lcd.clear();
  if(s=="sensor_mode"){
    lcd.setCursor(2,0);
    lcd.print("Current Mode: ");
    if(sensorMode=="Manual") lcd.setCursor(5,1);
    else lcd.setCursor(3,1);
    lcd.print(sensorMode);
    startTime=millis();
  }else if(s=="gas_high"){
    lcd.setCursor(4,0);
    lcd.print("WARNING!");
    lcd.setCursor(1,1);
    lcd.print("HIGH GAS LEVEL");
  }else if(s=="gas_low"){
    lcd.setCursor(4,0);
    lcd.print("SECURE!");
    lcd.setCursor(0,1);
    lcd.print("NO GAS DETECTED");
  }else if(s=="Low"){
    lcd.setCursor(1,0);
    lcd.print("Current Speed: ");
    lcd.setCursor(6,1);
    lcd.print(currentSpeed);
    startTime=millis();
  }else if(s=="Medium"){
    lcd.setCursor(1,0);
    lcd.print("Current Speed: ");
    lcd.setCursor(5,1);
    lcd.print(currentSpeed);
    startTime=millis();
  }else if(s=="High"){
    lcd.setCursor(1,0);
    lcd.print("Current Speed: ");
    lcd.setCursor(6,1);
    lcd.print(currentSpeed);
    startTime=millis();
  }else if(s=="error_1"){
    lcd.setCursor(2,0);
    lcd.print("DARK OUTSIDE");
    lcd.setCursor(0,1);
    lcd.print("LIGHT ALREADY ON");
    startTime=millis();
  }else if(s=="error_2"){
    lcd.setCursor(2,0);
    lcd.print("DARK OUTSIDE");
    lcd.setCursor(0,1);
    lcd.print("CANT TURN OFF");
    startTime=millis();
  }else if(s=="temperature"){
    lcd.setCursor(2,0);
    lcd.print("Temperature: ");
    lcd.setCursor(4,1);
    lcd.print(temperature);
    lcd.setCursor(9,1);
    lcd.print((char)223);
    lcd.setCursor(10,1);
    lcd.print("C");
  }else if(s=="humidity"){
    lcd.setCursor(2,0);
    lcd.print("Air Humidity: ");
    lcd.setCursor(5,1);
    lcd.print(humidity);
    lcd.setCursor(10,1);
    lcd.print("%");
  }else if(s=="light_on"){
    lcd.setCursor(2,0);
    lcd.print("Light State: ");
    lcd.setCursor(7,1);
    lcd.print("ON");
    startTime=millis();
  }else if(s=="light_off"){ 
    lcd.setCursor(2,0);
    lcd.print("Light State: ");
    lcd.setCursor(6,1);
    lcd.print("OFF");
    startTime=millis();
  }else if(s=="raining"){
    lcd.setCursor(1,0);
    lcd.print("Rain Status:");
    lcd.setCursor(4,1);
    lcd.print("RAINING");
    startTime=millis();
  }else if(s=="not_raining"){ 
    lcd.setCursor(2,0);
    lcd.print("Rain Status:");
    lcd.setCursor(2,1);
    lcd.print("NOT RAINING");
    startTime=millis();
  }else if(s=="dry_ground"){ 
    lcd.setCursor(0,0);
    lcd.print("Moisture Status:");
    lcd.setCursor(6,1);
    lcd.print("DRY");
    startTime=millis();
  }else if(s=="wet_ground"){ 
    lcd.setCursor(0,0);
    lcd.print("Moisture Status:");
    lcd.setCursor(6,1);
    lcd.print("WET");
    startTime=millis();
  }else if(s=="gps"){
    lcd.setCursor(0,0);
    lcd.print("La:");
    lcd.setCursor(4,0);
    lcd.print(lat);
    lcd.setCursor(0,1);
    lcd.print("Lg:");
    lcd.setCursor(4,1);
    lcd.print(lng);
  }else if(s=="no_gps"){
    lcd.setCursor(3,0);
    lcd.print("NO GPS FIX");
    lcd.setCursor(1,1);
    lcd.print("NEED CLEAR SKY");
  }else if(s=="prs/alt"){
    lcd.setCursor(0,0);
    lcd.print("Pressure:");
    lcd.setCursor(11,0);
    lcd.print(pressure);
    lcd.setCursor(0,1);
    lcd.print("Altitude:");
    lcd.setCursor(11,1);
    lcd.print(altitude);
  }else if(s=="head/speed"){
    lcd.setCursor(0,0);
    lcd.print("Heading:"); 
    lcd.setCursor(10,0);
    lcd.print(heading);
    lcd.setCursor(0,1);
    lcd.print("Speed:");  
    lcd.setCursor(8,1);
    lcd.print(speed);
  }else if(s=="no_hydro"){
    lcd.setCursor(4,0);
    lcd.print("SECURE!");
    lcd.setCursor(1,1);
    lcd.print("NO H2 DETECTED");
  }else if(s=="yes_hydro"){
    lcd.setCursor(4,0);
    lcd.print("WARNING!");
    lcd.setCursor(2,1);
    lcd.print("HIGH H2 LEVEL"); 
  }else if(s=="drive switch"){
    lcd.setCursor(1,0);
    lcd.print("Current Mode:");
    lcd.setCursor(4,1);
    lcd.print(sensorCtrlMode);
  }
}

void dataCheck(int command){
  Serial.println(command);
  if(command==0){
    gasState="gas_low";
  }else if(command==1){
    gasState="gas_high";
  }else if(command==2){// kd e vekj ukljucheno svetlo na avtiche od Sensor Car
    lcd_write_sensor("error_1");
  }else if(command==3){//kd ne mozhe da se iskljuchi zatoj sto e tmno
    lcd_write_sensor("error_2");
    currentLightMode="light_on";
  }else if(command==4){
    lcd_write_sensor("raining");
  }else if(command==5){//raincheck
    lcd_write_sensor("not_raining");
  }else if(command==6){
    lcd_write_sensor("dry_ground");
  }else if(command==7){//groundcheck
    lcd_write_sensor("wet_ground");
  }else if(command==8){
    hydroState="no_hydro";//nema
  }else if(command==9){
    hydroState="yes_hydro";//ima
  }
}

void lcd_write_hand(String s){
  lcd.clear();
  if(s=="handMode"){
    lcd.setCursor(1,0);
    lcd.print("Current Mode: ");
    if(handMode=="Drive + Cam") lcd.setCursor(2,1);
    else lcd.setCursor(3,1);
    lcd.print(handMode);
  }else if(s=="mode switch"){
    lcd.setCursor(0,0);
    lcd.print("Controller Mode: ");
    if(mode=="Sensor Car") lcd.setCursor(3,1);
    else lcd.setCursor(4,1);
    lcd.print(mode);
    startTime=millis();
  }else if(s=="default_servos"){
    lcd.setCursor(1,0);
    lcd.print("ALL SERVOS TO");
    lcd.setCursor(0,1);
    lcd.print("DEFAULT POSITION");
    startTime=millis();
  }else if(s=="open" || s=="closed"){
    lcd.setCursor(2,0);
    lcd.print("Claw Status:");
    if(s=="open"){
      lcd.setCursor(6,1);
      lcd.print("OPEN");
    }else{
      lcd.setCursor(5,1);
      lcd.print("CLOSED");  
    }
    startTime=millis();
  }else if(s=="autoTracking"){
    lcd.setCursor(1,0);
    lcd.print("Auto Tracker:");
    lcd.setCursor(6,1);
    lcd.print(autoTracking);
    startTime=millis();
  }
}