#ifndef StepperMotor28BYJ48_h
#define StepperMotor28BYJ48_h
//StepperMotor28BYJ48.h
#include <Arduino.h>

class StepperMotor28BYJ48 {
private:
  int pins[4];  //引脚信息
  int stepsPerRevolution;
  int currentStep;
  int sequence[4][4] = {
    { HIGH, LOW, LOW, LOW },
    { LOW, HIGH, LOW, LOW },
    { LOW, LOW, HIGH, LOW },
    { LOW, LOW, LOW, HIGH },
  };
  int stepDelay;  //每一步的延时时长


public:
  StepperMotor28BYJ48(int pin1, int pin2, int pin3, int pin4);
  void setSpeed(int speed);
  void step(int steps);
  void rotateDegrees(float degrees);
  void rotateRevolution(float revolutions);
  bool foreward;  //判断是否为正转，默认true正转标识
  int stepAsync(int steps);
  float rotateDegreesAsync(float degrees);
  float rotateRevolutionAsync(float revolutions);
};

#endif
