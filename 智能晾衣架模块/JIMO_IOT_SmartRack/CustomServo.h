#ifndef CustomServo_h
#define CustomServo_h
//CustomServo.h

#include <Arduino.h>

class CustomServo {
public:
  CustomServo(int pin);                         // 构造函数
  void pulse(int angle);                        // 设置舵机角度
  int variationTrend(int oraginal, int angle);  //原始位置，和目标位置！
  bool foreward;                                //判断是否为正转，默认true正转标识

private:
  int _pin;  // 舵机引脚
};

#endif
