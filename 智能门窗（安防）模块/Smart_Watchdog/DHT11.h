#ifndef __DHT11_H__
#define __DHT11_H__
//DHT11.h
#include <Arduino.h>


class DHT11 {
public:
  DHT11(int pin);
  void DHT11_Init();
  unsigned char DHT11_Read_Byte();
  void DHT11_Read();

  unsigned char HUMI_Buffer_Int;
  unsigned char TEM_Buffer_Int;
private:
  int DHT11_DQ;
};

#endif
