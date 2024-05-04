#include "StepperMotor28BYJ48.h"
//StepperMotor28BYJ48.cpp

StepperMotor28BYJ48::StepperMotor28BYJ48(int pin1, int pin2, int pin3, int pin4) {
  pins[0] = pin1;
  pins[1] = pin2;
  pins[2] = pin3;
  pins[3] = pin4;
  this->stepsPerRevolution = 2080;
  this->currentStep = 0;
  this->stepDelay = 5;
  this->foreward = true;

  for (int i = 0; i < 4; i++) {
    pinMode(pins[i], OUTPUT);
  }
}

void StepperMotor28BYJ48::setSpeed(int speed) {
  stepDelay = speed;
}

void StepperMotor28BYJ48::step(int steps) {
  for (int i = 0; i < steps; i++) {
    currentStep++;
    currentStep %= 4;
    if (foreward) {  //正转方法
      for (int j = 0; j < 4; j++) {
        digitalWrite(pins[j], sequence[currentStep][j]);
      }
    } else {  //反转方法
      for (int j = 0; j < 4; j++) {
        digitalWrite(pins[j], sequence[currentStep][3 - j]);
      }
    }

    delay(stepDelay);
  }
}

void StepperMotor28BYJ48::rotateDegrees(float degrees) {
  int steps = degrees / 360.0 * stepsPerRevolution;
  step(steps);
}

void StepperMotor28BYJ48::rotateRevolution(float revolutions) {
  int steps = revolutions * stepsPerRevolution;
  step(steps);
}
//*****************************进一步的异步实现方法******************************************
int StepperMotor28BYJ48::stepAsync(int steps) {
  if (steps) {
    currentStep++;
    currentStep %= 4;
    if (foreward) {  //正转方法
      for (int j = 0; j < 4; j++) {
        digitalWrite(pins[j], sequence[currentStep][j]);
      }
    } else {  //反转方法
      for (int j = 0; j < 4; j++) {
        digitalWrite(pins[j], sequence[currentStep][3 - j]);
      }
    }
    //以便于适应异步操作！
    steps--;
  }
  return steps;
}
float StepperMotor28BYJ48::rotateDegreesAsync(float degrees) {
  int steps = degrees / 360.0 * stepsPerRevolution;
  return stepAsync(steps) / stepsPerRevolution * 360.0;
}

float StepperMotor28BYJ48::rotateRevolutionAsync(float revolutions) {
  int steps = revolutions * stepsPerRevolution;
  return stepAsync(steps) / stepsPerRevolution;
}