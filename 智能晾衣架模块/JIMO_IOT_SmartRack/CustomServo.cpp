#include "CustomServo.h"
//CustomServo.cpp

CustomServo::CustomServo(int pin) {
  _pin = pin;
  pinMode(_pin, OUTPUT);
}
//基础运动控制
void CustomServo::pulse(int angle) {
  int pulsewidth = int((angle * 11) + 500);  // 计算高电平时间
  digitalWrite(_pin, HIGH);                  // 设置高电平
  delayMicroseconds(pulsewidth);             // 延时 pulsewidth （us）
  digitalWrite(_pin, LOW);                   // 设置低电平
  delay(20 - pulsewidth / 1000);             // 延时 20 - pulsewidth / 1000 （ms）
}
//缓冲方式的运转,渐变之目标位置！（限制在90~180）且返回操作后的角度
int CustomServo::variationTrend(int oraginal, int angle) {
  //当目标角度大于原角度则增加,一次加一点
  if (angle > oraginal && oraginal < 180) {
    oraginal++;
    int pulsewidth = int((oraginal * 11) + 500);  // 计算高电平时间
    digitalWrite(_pin, HIGH);                     // 设置高电平
    delayMicroseconds(pulsewidth);                // 延时 pulsewidth （us）
    digitalWrite(_pin, LOW);                      // 设置低电平
    delay(20 - pulsewidth / 1000);                // 延时 20 - pulsewidth / 1000 （ms）
  }
  //当目标角度小于原角度则减少
  if (angle < oraginal && oraginal > 90) {
    oraginal--;
    int pulsewidth = int((oraginal * 11) + 500);  // 计算高电平时间
    digitalWrite(_pin, HIGH);                     // 设置高电平
    delayMicroseconds(pulsewidth);                // 延时 pulsewidth （us）
    digitalWrite(_pin, LOW);                      // 设置低电平
    delay(20 - pulsewidth / 1000);                // 延时 20 - pulsewidth / 1000 （ms）
  }
  // delay(50);//降低速度,测试过自动循环的结果在2.2ms
  return oraginal;
}
