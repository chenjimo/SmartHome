#include "DS18B20.h"
//DS18B20.cpp
//定义变量
unsigned char flag_temper = 0;

DS18B20::DS18B20(int pin) {
  DS18B20_Init();
  _DS18B20_DQ = pin;
}
//****************************************************
//DS18B20写1字节
//****************************************************
void DS18B20::DS18B20_Write_Byte(unsigned char dat) {
  unsigned char i;
  pinMode(_DS18B20_DQ, OUTPUT);
  delayMicroseconds(2);

  for (i = 0; i < 8; i++) {
    digitalWrite(_DS18B20_DQ, LOW);
    delayMicroseconds(2);  //大于1us

    if ((dat & 0x01) == 0)  //先写低位
    {
      digitalWrite(_DS18B20_DQ, LOW);
    } else {
      digitalWrite(_DS18B20_DQ, HIGH);
    }
    dat >>= 1;

    delayMicroseconds(120);  //延时60~120us

    digitalWrite(_DS18B20_DQ, HIGH);  //释放总线
    delayMicroseconds(2);             //大于1us
  }
}

//****************************************************
//DS18B20读1字节
//****************************************************
unsigned char DS18B20::DS18B20_Read_Byte() {
  unsigned char dat, i;
  for (i = 0; i < 8; i++) {
    pinMode(_DS18B20_DQ, OUTPUT);
    digitalWrite(_DS18B20_DQ, LOW);
    delayMicroseconds(2);             //大于1us
    digitalWrite(_DS18B20_DQ, HIGH);  //释放总线
    delayMicroseconds(2);             //大于1us

    dat >>= 1;

    pinMode(_DS18B20_DQ, INPUT_PULLUP);
    delayMicroseconds(2);

    if (digitalRead(_DS18B20_DQ)) {
      dat |= 0X80;
    } else {
      dat &= 0x7f;
    }
    delayMicroseconds(120);  //延时60~120us
  }
  return dat;
}

//****************************************************
//DS18B20初始化
//****************************************************
bool DS18B20::DS18B20_Init() {
  bool Flag_exist = 1;
  pinMode(_DS18B20_DQ, OUTPUT);

  digitalWrite(_DS18B20_DQ, HIGH);  //释放总线
  delayMicroseconds(2);             //延时>1us

  digitalWrite(_DS18B20_DQ, LOW);
  delayMicroseconds(750);  //延时480~960us

  digitalWrite(_DS18B20_DQ, HIGH);  //释放总线
  delayMicroseconds(60);            //延时15~60us

  pinMode(_DS18B20_DQ, INPUT_PULLUP);
  delayMicroseconds(2);
  Flag_exist = digitalRead(_DS18B20_DQ);
  delayMicroseconds(240);  //延时60~240us

  pinMode(_DS18B20_DQ, OUTPUT);
  delayMicroseconds(2);
  digitalWrite(_DS18B20_DQ, HIGH);  //释放总线
  return Flag_exist;
}

//**********************************************************
//读取温度函数，返回温度的绝对值，并标注flag_temper，flag_temper=1表示负，flag_temper=0表示正
//**********************************************************
float DS18B20::Get_temp(void)  //读取温度值
{
  float tt;
  unsigned char a, b;
  unsigned int temp;
  if (DS18B20_Init() == 0)  //初始化
  {
    DS18B20_Write_Byte(0xcc);  //忽略ROM指令
    DS18B20_Write_Byte(0x44);  //温度转换指令

    //	delay(750);				//PROTEUS仿真需要加

    if (DS18B20_Init() == 0)  //初始化
    {
      DS18B20_Write_Byte(0xcc);  //忽略ROM指令
      DS18B20_Write_Byte(0xbe);  //读暂存器指令
      a = DS18B20_Read_Byte();   //读取到的第一个字节为温度LSB
      b = DS18B20_Read_Byte();   //读取到的第一个字节为温度MSB

      temp = b;         //先把高八位有效数据赋于temp
      temp <<= 8;       //把以上8位数据从temp低八位移到高八位
      temp = temp | a;  //两字节合成一个整型变量

      if (temp > 0xfff) {
        flag_temper = 1;  //温度为负数
        temp = (~temp) + 1;
      } else {
        flag_temper = 0;  //温度为正或者0
      }

      tt = temp * 0.0625;    //得到真实十进制温度值
                             //因为DS18B20可以精确到0.0625度
                             //所以读回数据的最低位代表的是0.0625度
      temp = tt * 10 + 0.5;  //放大十倍
                             //这样做的目的将小数点后第一位也转换为可显示数字
                             //同时进行一个四舍五入操作。
      tt = (float)temp / 10;
    }
  }
  return tt;
}
