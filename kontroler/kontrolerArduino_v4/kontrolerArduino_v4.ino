 #include <SoftwareSerial.h>

const int MIDDLE_BUTTON_PIN=6;
const int RIGHT_BUTTON_PIN=7;
const int LEFT_BUTTON_PIN=5;
const int SW1_BUTTON_PIN=4;
const int SW2_BUTTON_PIN=8;
const int MODE_BUTTON_PIN=10;
const int X_PIN=A2, Y_PIN=A3;

int xSW1, ySW1;
byte movementSend=3;
bool movementChanged=false;

const int CAM_X_PIN=A1, CAM_Y_PIN=A0;
int xSW2,ySW2;
byte camSend=8;
bool camChanged=false;

int middleButton=0, rightButton=0, leftButton=0, sw1Button=0, sw2Button=0,modeButton=0;
long long lastsw1Button=0, lastsw2Button=0, lastrightButton=0, lastleftButton=0, lastMiddleButton=0, lastModeSwitch=0;
int previousModeButton=0;
bool registerLR;
SoftwareSerial esp(2, 3); // TX, RX

void setup() { 
  pinMode(MIDDLE_BUTTON_PIN, INPUT);
  pinMode(RIGHT_BUTTON_PIN, INPUT);
  pinMode(LEFT_BUTTON_PIN, INPUT);
  pinMode(SW1_BUTTON_PIN, INPUT_PULLUP);
  pinMode(SW2_BUTTON_PIN, INPUT_PULLUP);
  pinMode(MODE_BUTTON_PIN, INPUT_PULLUP);

  esp.begin(9600);
  Serial.begin(9600);
}

void loop() {
  xSW1=analogRead(X_PIN);
  Serial.println();
  Serial.println(xSW1);
  ySW1=analogRead(Y_PIN);
  Serial.println(ySW1);
  Serial.println();
  xSW2=analogRead(CAM_X_PIN);
  ySW2=analogRead(CAM_Y_PIN);

  middleButton=digitalRead(MIDDLE_BUTTON_PIN);//reading values
  rightButton=digitalRead(RIGHT_BUTTON_PIN);
  leftButton=digitalRead(LEFT_BUTTON_PIN);
  sw1Button=digitalRead(SW1_BUTTON_PIN);
  Serial.println(sw1Button);
  sw2Button=digitalRead(SW2_BUTTON_PIN);

  previousModeButton=modeButton;
  modeButton=digitalRead(MODE_BUTTON_PIN);
  //Serial.println(modeButton);

  if(modeButton!=previousModeButton){
    previousModeButton=modeButton;
    esp.println(100);
    Serial.println("menja");
  }
  
  registerLR=true;
  if(leftButton==1 && rightButton==1 && millis()-lastModeSwitch>1000){
    registerLR=false;
    esp.println(10);
    lastModeSwitch=millis();
  }
  if(middleButton==1 && millis()-lastMiddleButton>300){
    esp.println(11);
    lastMiddleButton=millis();
  }
  if(rightButton==1 && millis()-lastrightButton>300 && millis()-lastModeSwitch>1000 && registerLR){
    esp.println(12);
    lastrightButton=millis();
  }//turn on/off car light
  if(leftButton==1 && millis()-lastleftButton>300 && millis()-lastModeSwitch>1000 &&  registerLR){
    esp.println(13);
    lastleftButton=millis();
  }//change speed
  if(sw1Button==0 && millis()-lastsw1Button>1000 && millis()-lastModeSwitch>1000){
    esp.println(14);//org 14
    //Serial.println("dugme");
    lastsw1Button=millis();
  }//for rain Sensor Car
  if(sw2Button==0 && millis()-lastsw2Button>1000 && millis()-lastModeSwitch>1000){
    esp.println(15);//org 15
    //Serial.println("dugme2");
    lastsw2Button=millis();
  }//for ground Sensor Car

  movementChanged=false;
  if(ySW1>400 && ySW1<700 && xSW1>400 && xSW1<700 && movementSend!=5){
    movementSend=5;
    movementChanged=true;
  }else if(ySW1>700){
    movementSend=2;
    movementChanged=true;
  }else if(ySW1<300){
    movementSend=1;
    movementChanged=true;
  }else if(xSW1>700 && ySW1>400 && ySW1<700){
    movementSend=4;
    movementChanged=true;
  }else if(xSW1<300 && ySW1>400 && ySW1<700){
    movementSend=3;
    movementChanged=true;
  }
  if(movementChanged){
    esp.println(movementSend);
  }//sistem za da uglavnom ne prakja celo vreme nego samo ako nova vrednost e razlichna od proshlu

  camChanged=false;
  if(ySW2>400 && ySW2<700 && xSW2>400 && xSW2<700 && camSend!=50){
    camSend=50;
    camChanged=true;
  }else if(ySW2>700){
    camSend=52;
    camChanged=true;
  }else if(ySW2<300){
    camSend=51;
    camChanged=true;
  }else if(xSW2>700 && ySW2>400 && ySW2<700){
    camSend=54;
    camChanged=true;
  }else if(xSW2<300 && ySW2>400 && ySW2<700){
    camSend=53;
    camChanged=true;
  }
  if(camChanged){
    esp.println(camSend);
  }
  delay(100);
}