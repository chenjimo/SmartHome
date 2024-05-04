#ifndef __DS18B20_H__
#define __DS18B20_H__
//DS18B20.h

#include <Arduino.h>

class DS18B20 {
public:
  DS18B20(int pin);
  void DS18B20_Write_Byte(unsigned char dat);
  unsigned char DS18B20_Read_Byte();
  bool DS18B20_Init();
  float Get_temp();
private:
  int _DS18B20_DQ;
  unsigned char flag_temper;
};

#endif
