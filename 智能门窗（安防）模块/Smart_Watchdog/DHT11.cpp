#include "DHT11.h"
//DHT11.cpp
//定义变量
unsigned char HUMI_Buffer_Int = 0;
unsigned char TEM_Buffer_Int = 0;

DHT11::DHT11(int pin) {
  DHT11_DQ = pin;
}

//****************************************************
//初始化DHT11
//****************************************************
void DHT11::DHT11_Init() {
  pinMode(DHT11_DQ, OUTPUT);

  digitalWrite(DHT11_DQ, LOW);  //拉低总线，发开始信号；
  delay(30);                    //延时要大于 18ms，以便 DHT11 能检测到开始信号；
  digitalWrite(DHT11_DQ, HIGH);
  delayMicroseconds(40);  //等待 DHT11 响应；
  pinMode(DHT11_DQ, INPUT_PULLUP);
  while (digitalRead(DHT11_DQ) == HIGH)
    ;
  delayMicroseconds(80);  //DHT11 发出响应，拉低总线 80us；
  if (digitalRead(DHT11_DQ) == LOW)
    ;
  delayMicroseconds(80);  //DHT11 拉高总线 80us 后开始发送数据；
}

//****************************************************
//读一个字节DHT11数据
//****************************************************
unsigned char DHT11::DHT11_Read_Byte() {
  unsigned char i, dat = 0;
  unsigned int j;

  pinMode(DHT11_DQ, INPUT_PULLUP);
  delayMicroseconds(2);
  for (i = 0; i < 8; i++) {
    while (digitalRead(DHT11_DQ) == LOW)
      ;                     //等待 50us；
    delayMicroseconds(40);  //判断高电平的持续时间，以判定数据是‘0’还是‘1’；
    if (digitalRead(DHT11_DQ) == HIGH)
      dat |= (1 << (7 - i));  //高位在前，低位在后；
    while (digitalRead(DHT11_DQ) == HIGH)
      ;  //数据‘1’，等待下一位的接收；
  }
  return dat;
}

//****************************************************
//读取温湿度值，存放在TEM_Buffer和HUMI_Buffer
//****************************************************
void DHT11::DHT11_Read() {
  DHT11_Init();

  HUMI_Buffer_Int = DHT11_Read_Byte();  //读取湿度的整数值
  DHT11_Read_Byte();                    //读取湿度的小数值
  TEM_Buffer_Int = DHT11_Read_Byte();   //读取温度的整数值
  DHT11_Read_Byte();                    //读取温度的小数值
  DHT11_Read_Byte();                    //读取校验和
  delayMicroseconds(50);                //DHT11拉低总线50us

  pinMode(DHT11_DQ, OUTPUT);
  digitalWrite(DHT11_DQ, HIGH);  //释放总线
}
