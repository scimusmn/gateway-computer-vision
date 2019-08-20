#include <Adafruit_MotorShield.h>

#include <gfxfont.h>
#include <Adafruit_SPITFT.h>
#include <Adafruit_SPITFT_Macros.h>
#include <Adafruit_GFX.h>

#include <Wire.h>

// steps left/right of center to turn
const int steps_turn = 50;

// steps to get home
const int steps_home_left = 8;
const int steps_home_right = 8;

typedef enum { LEFT, CENTER, RIGHT } orientation;

orientation dir = CENTER;

Adafruit_MotorShield afms;
Adafruit_StepperMotor *motor;

const int green_led   = 2;
const int red_led     = 3;
const int hall_switch = 8;

bool go = true;

void setup() {
  // set up motor shield & stepper
  afms = Adafruit_MotorShield();
  afms.begin();

  motor = afms.getStepper(200,1);
  motor->setSpeed(10);

  // set up serial
  Serial.begin(9600);

  // set up pins
  pinMode(green_led,OUTPUT);
  pinMode(red_led,  OUTPUT);
  pinMode(hall_switch, INPUT);
  digitalWrite(hall_switch, HIGH);  //activate pull-up
  

  // home
  go_home();

  // go is true
  digitalWrite(green_led,HIGH);
}

void loop() {
  while ( Serial.available() > 0 ) {
    switch( Serial.read() ) {
      case 'f':
        //BACKWARD
        face_center();
        flush_serial();
        break;
      case 'l':
        //left
        face_left();
        flush_serial();
        break;
      case 'r':
        //right
        face_right();
        flush_serial();
        break;
      case 'g':
        go = true;
        digitalWrite(green_led,1);
        digitalWrite(red_led,  0);
        break;
      case 's':
        go = false;
        digitalWrite(red_led,  1);
        digitalWrite(green_led,0);
        break;
      default:
        //ignore other characters
        break;
    }
  }
  delay(10);
}

//~~~~~~~~~~~~~~~~

void go_home() {
  if ( !digitalRead(hall_switch) ) {
    // the magnet is already in range of the switch
    motor->step(steps_turn, FORWARD, DOUBLE);
  }
  delay(10);
  while ( digitalRead(hall_switch) ) {
    motor->step(1,BACKWARD,DOUBLE);
    delay(10);
  }
  delay(10);
  motor->step(steps_home_left,BACKWARD,DOUBLE);
  delay(50);
  motor->release();
}

void face_center() {
  switch ( dir ) {
    case LEFT:
      while ( digitalRead(hall_switch) ) {
        motor->step(1,BACKWARD,DOUBLE);
      }
      motor->step(steps_home_left,BACKWARD,DOUBLE);
      break;
    case RIGHT:
      while ( digitalRead(hall_switch) ) {
        motor->step(1,FORWARD,DOUBLE);
      }
      motor->step(steps_home_right,FORWARD,DOUBLE);
      break;
    case CENTER:
      //already facing BACKWARD, ignore
      break;
    default:
      //invalid direction
      break;
  }
  dir = CENTER;
  delay(50);
  motor->release();
}

void face_left() {
  switch ( dir ) {
    case LEFT:
      //already facing left, ignore
      break;
    case RIGHT:
      motor->step(2*steps_turn, FORWARD, DOUBLE);
      break;
    case CENTER:
      motor->step(steps_turn, FORWARD, DOUBLE);
      break;
    default:
      //invalid direction
      break;
  }
  dir = LEFT;
  delay(50);
  motor->release();
}

void face_right() {
  switch ( dir ) {
    case LEFT:
      motor->step(2*steps_turn, BACKWARD, DOUBLE);
      break;
    case RIGHT:
      //already facing right, ignore
      break;
    case CENTER:
      motor->step(steps_turn, BACKWARD, DOUBLE);
      break;
    default:
      //invalid direction
      break;
  }
  dir = RIGHT;
  delay(50);
  motor->release();
}

void flush_serial() {
  while (Serial.available() > 0) {
    Serial.read();
  }
}
