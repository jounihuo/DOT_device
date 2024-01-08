// This code is for Arduino control of spectral measurement device.
// 
#include <Servo.h>

// Pins for motor controls
const int enPin=8;      // Shield enabled pin
//const int stepXPin = 2; // X.STEP not in use
//const int dirXPin = 5;  // X.DIR  not in use
const int stepYPin = 3; // Y.STEP Front
const int dirYPin = 6;  // Y.DIR  Front
const int stepZPin = 4; // Z.STEP Back
const int dirZPin = 7;  // Z.DIR  Back

//Spectrometer controls
#define SPEC_TRG         A0
#define SPEC_ST          A1
#define SPEC_CLK         A2
#define SPEC_VIDEO       A3
#define SPEC_CHANNELS    288

// Stop limit switches
const int frontStopPin = 13;
const int backStopPin = 12;

// Light source on/off
const int lightPin = 9;

// Servo setup
Servo lightServo; 
Servo spectServo; 

const int lightUp = 180;  //50
const int lightDown = 180; //180 up position

const int spectUp = 120;  
const int spectDown = 20; 

// Initializing variables
float frontAngle = 1000;
float backAngle = 1000;

int buttonState = 0;
int buttonStateBack = 0;

const int stepsPerRev = 900;      // Stepper motor has 200 steps/rev. Gear ratio 4.5.
int pulseWidthMicros = 150;       // microseconds
const int millisBtwnSteps = 8000; // microseconds

const int nMeasPoints = int(360/60);
const int nAngleSteps = int(stepsPerRev/nMeasPoints);

int frontPos[nMeasPoints];
int backPos[int((nMeasPoints - 1)*nMeasPoints)];

int initStateF = 1;
int initStateB = 1;
int startState = 0;
int idleState = 1;
int measIdx = 0;
int value = 0;
int idx = 0;
int idm = 0;
float backAngleTemp = 0.0;

uint16_t data[SPEC_CHANNELS];

void setup() {
  Serial.begin(115200); // Baud Rate set to 115200
  
  pinMode(enPin, OUTPUT);
  digitalWrite(enPin, LOW);
  pinMode(stepYPin, OUTPUT);
  pinMode(stepZPin, OUTPUT);
  pinMode(dirYPin, OUTPUT);
  pinMode(dirZPin, OUTPUT);
  pinMode(frontStopPin, INPUT);
  pinMode(backStopPin, INPUT);
  pinMode(lightPin, OUTPUT);

  //Initialize measurement sequence locations.
  for (int i = 0; i < nMeasPoints; i++) {
      frontPos[i] = value;
      value = value + nAngleSteps;
  }
  for (int i = 0; i < nMeasPoints; i++){
    value = nAngleSteps;
    for (int j = 0; j < (nMeasPoints-1); j++){
        backPos[idx] = frontPos[i] + value;
        value = value + nAngleSteps;
        idx = idx + 1;
    }
  }

  //Set pins for spectormetr
  pinMode(SPEC_CLK, OUTPUT);
  pinMode(SPEC_ST, OUTPUT);
  digitalWrite(SPEC_CLK, HIGH); // Set SPEC_CLK High
  digitalWrite(SPEC_ST, LOW); // Set SPEC_ST Low

  //Servo setup
  lightServo.attach(10);
  spectServo.attach(11);  

  //Measurement open
  lightServo.write(lightDown);
  spectServo.write(spectUp);
  delay(1000);
  Serial.println(F("## CNC Shield Initialized"));
}

// Function to drive front wheel one step.
float frontStep(float angle) {
      digitalWrite(stepYPin, HIGH);
      delayMicroseconds(pulseWidthMicros);
      digitalWrite(stepYPin, LOW);
      delayMicroseconds(millisBtwnSteps);
      if (digitalRead(dirYPin) == LOW){
        angle -= 1;
      }
      if (digitalRead(dirYPin) == HIGH){
        angle += 1;
      }
      return angle;
}

// Function to drive back wheel one step.
float backStep(float angle) {
      digitalWrite(stepZPin, HIGH);
      delayMicroseconds(pulseWidthMicros);
      digitalWrite(stepZPin, LOW);
      delayMicroseconds(millisBtwnSteps);
      if (digitalRead(dirZPin) == LOW){
        angle += 1;
      }
      if (digitalRead(dirZPin) == HIGH){
        angle -= 1;
      }
      return angle;
}

/*
 * This functions reads spectrometer data from SPEC_VIDEO
 * Look at the Timing Chart in the Datasheet for more info
 */
void readSpectrometer(){

  int delayTime = 1; // delay time

  // Start clock cycle and set start pulse to signal start
  digitalWrite(SPEC_CLK, LOW);
  delayMicroseconds(delayTime);
  digitalWrite(SPEC_CLK, HIGH);
  delayMicroseconds(delayTime);
  digitalWrite(SPEC_CLK, LOW);
  digitalWrite(SPEC_ST, HIGH);
  delayMicroseconds(delayTime);

  //Sample for a period of time
  for(int i = 0; i < 15; i++){

      digitalWrite(SPEC_CLK, HIGH);
      delayMicroseconds(delayTime);
      digitalWrite(SPEC_CLK, LOW);
      delayMicroseconds(delayTime); 
 
  }

  //Set SPEC_ST to low
  digitalWrite(SPEC_ST, LOW);

  //Sample for a period of time
  for(int i = 0; i < 85; i++){

      digitalWrite(SPEC_CLK, HIGH);
      delayMicroseconds(delayTime);
      digitalWrite(SPEC_CLK, LOW);
      delayMicroseconds(delayTime); 
      
  }

  //One more clock pulse before the actual read
  digitalWrite(SPEC_CLK, HIGH);
  delayMicroseconds(delayTime);
  digitalWrite(SPEC_CLK, LOW);
  delayMicroseconds(delayTime);

  //Read from SPEC_VIDEO
  for(int i = 0; i < SPEC_CHANNELS; i++){

      data[i] = analogRead(SPEC_VIDEO);
      
      digitalWrite(SPEC_CLK, HIGH);
      delayMicroseconds(delayTime);
      digitalWrite(SPEC_CLK, LOW);
      delayMicroseconds(delayTime);
        
  }
  //Set SPEC_ST to high
  digitalWrite(SPEC_ST, HIGH);
  //Sample for a small amount of time
  for(int i = 0; i < 7; i++){
      digitalWrite(SPEC_CLK, HIGH);
      delayMicroseconds(delayTime);
      digitalWrite(SPEC_CLK, LOW);
      delayMicroseconds(delayTime);
  }
  digitalWrite(SPEC_CLK, HIGH);
  delayMicroseconds(delayTime);
}

/*
 * The function below prints out data to the terminal or 
 * processing plot
 */
void printData(){
  for (int i = 0; i < SPEC_CHANNELS; i++){
    Serial.print(data[i]);
    Serial.print(',');
  }
  Serial.print("\n");
}


void loop() {
  buttonState = digitalRead(frontStopPin);
  buttonStateBack = digitalRead(backStopPin);

  // Check if the limit switch is pressed during initialization.
  if (buttonState == HIGH and initStateF == 1) {
    digitalWrite(dirYPin, LOW);
    }

  // Check if the limit switch is pressed during initialization.
  if (buttonStateBack == HIGH and initStateB == 1) {
    digitalWrite(dirZPin, HIGH);
    }  

  // Setting the front wheel in initial position after limit switch.
  if (buttonState == LOW and initStateF == 1) {
    // Offset due to limit switch location.
    for (int i = 0; i < 14; i++) {
        frontStep(0.0);
    }
    Serial.println(F("## Front ring in position..."));
    digitalWrite(dirYPin, HIGH);
    initStateF = 0;
    if (initStateB == 0 and initStateF == 0){
      startState = 1;
    }
    frontAngle = 0.0;
  }

 // Setting the back wheel in initial position after limit switch.
 if (buttonStateBack == LOW and initStateB == 1) {
    // Offset due to limit switch location.
    for (int i = 0; i < 30; i++) {
        backStep(0.0);
    }
    Serial.println(F("## Back ring in position..."));
    digitalWrite(dirZPin, LOW);
    initStateB = 0;
    if (initStateB == 0 and initStateF == 0){
        startState = 1;
    }
    backAngle = stepsPerRev/2;
  }

  // Initial positioning back wheel
  if (initStateF == 1){
      frontStep(0.0);
  }
  // Initial positioning back wheel
  if (initStateB == 1){
      backStep(0.0);
  }

  // Measurement cycle
  if (startState == 1){
    for (int i = 0; i < nMeasPoints; i++){
      //Drive front wheel to position
      while (abs(frontAngle - frontPos[i])>0){
        frontAngle = frontStep(frontAngle);
      }
      for (int j = 0; j < (nMeasPoints-1); j++){
          //Drive back wheel to position
          while (abs(backAngle - backPos[idm])>0){
            backAngleTemp = backStep(backAngle);
            //Flip direction if needed
            if (abs(backAngle - backPos[idm]) < abs(backAngleTemp - backPos[idm])){
               digitalWrite(dirZPin, !digitalRead(dirZPin));
            }
            backAngle = backAngleTemp;
          }

          //Measurement open
          lightServo.write(lightUp);
          spectServo.write(spectDown);
          
          // Spectral measurement here!
          digitalWrite(lightPin, HIGH);

          delay(100);
          //Clear the spectrometer
          readSpectrometer();
          //This delay defines the measurement length
          delay(20);
          //The actual measurement
          readSpectrometer();
          
          printData(); 
          
          delay(600);
          digitalWrite(lightPin, LOW);  

          //Measurement close
          lightServo.write(lightDown);
          spectServo.write(spectUp);

          // Data output
          //Serial.print(frontPos[i]);
          //Serial.print(",");
          //Serial.println(backPos[idm]);

          idm += 1;
      }
    }
    idleState = 1;

    //Measurement open
    lightServo.write(lightDown);
    spectServo.write(spectUp);
    Serial.println(F("## DONE"));
  }

 startState = 0;

  if (idleState == 3){
    digitalWrite(lightPin, HIGH);
    delay(1000);
    readSpectrometer();
    readSpectrometer();
    printData(); 
    digitalWrite(lightPin, LOW);  
    delay(1000);
  }

}
