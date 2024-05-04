#include "CustomServo.h"
#include "StepperMotor28BYJ48.h"
#include "ESPWebServerWrapper.h"  //有关于WiFi相关的封装类（部分数信息需要直接前置在这里，包括一些声明）
#include <Arduino.h>
#include <ArduinoJson.h>  //用于对JSON数据的解析

//引脚的设置
#define WATER_pin 16     //水滴传感器
#define RGB_Rpin 13      //三色灯红灯引脚
#define RGB_Gpin 12      //三色灯绿灯引脚
#define RGB_Bpin 14      //三色灯蓝灯引脚
#define Stepping_Apin 5  //步进电机A引脚
#define Stepping_Bpin 4  //步进电机B引脚
#define Stepping_Cpin 0  //步进电机C引脚
#define Stepping_Dpin 2  //步进电机D引脚
#define Btn_Onepin 1     //按钮一，下方-TX
#define Btn_Twopin 3     //按钮二，上方-RX
#define PWM_pin D8       //舵机引脚
#define Sun_pin A0       //光感读取传感器


// ************************************WIFI连接及JIMO-IOT的API配置************************************
// WIFI 的配置
const char* ssid = "jimo";
const char* password = "1517962688";
// Client 的API 配置
const char* module_id = "1";  //1是智能晾衣架模块、2是智能浇水模块、3是智能门窗模块、4智能安防
String serverAddress = "http://iot.jimo.fun";
//心跳请求-POST
const char* heartbeatEndpoint = "/module/log";
String heartbeatPostData = "moduleId=" + String(module_id);
//数据上传-POST
const char* uploadinEndpoint = "/device/log";
//指令刷新-GET
String getOder = "/oder/log?moduleId=" + String(module_id);
//任务完成告诉Server-POST
const char* oderOK = "/oder/log";


//***************************************一些设备运行的配置参数****************************************
//关于网络连接异常的设置
int maxConnectionAttempts = 3;  // 设置最大连接失败次数
int connectionAttempts = 0;
bool connectState = false;  //连接状态标识,0为无连接，1为连接成功
int errorLimit = 5;         //设置最大在线异常数
int errorCount = 0;         //初始化计数
//关于自动寻光的一些参数设置
bool findSunOn = false;  //默认是关闭的
int value = 51;
//步进电机设置的参数
int steps = 0;
int stepCount = 0;      //限制正负旋转位置-500~+500，限制在90°内
bool isStepper = true;  //true操作对象为myStepper，false操作对象为SG90
//舵机设置参数
int oraginalAngle = 90;  //初始状态下的旋转记录数值
int targetlAngle = 90;   //初始状态下的旋转目标数值
//异步操作锁，默认关闭false
bool AsyncOn = false;
//关于模式设置的标识参数
int pattern = 0;  //对应R-G-B的1-2-4的值，需要加以一个参数findSunOn来区分智能晾晒或者是自动旋转！

//********************************关于定时任务的设置************************************************
//定时任务
unsigned long connectTime = 0;
unsigned long lastHeartbeatTime = 0;
unsigned long lastUploadingTime = 0;
unsigned long lastGetOderTime = 0;
unsigned long errorLimitTime = 0;
unsigned long SunMillis = 0;
unsigned long SpinMillis = 0;
unsigned long AsyncMillis = 0;
unsigned long BtnOMillis = 0;
unsigned long BtnTMillis = 0;
unsigned long SmartTMillis = 0;
unsigned long NFCTMillis = 0;

const int NFC_INTERVAL = 1000;          // 1秒 1000
const int Servo_INTERVAL = 50;          // 0.05秒 50
const int Stepper_INTERVAL = 5;         // 每5ms一次
const int SUN_INTERVAL = 2000;          // 每2s检查一次
const int SPIN_INTERVAL = 5000;         // 每5s暂停时间
const int SMART_INTERVAL = 10000;       // 每10S巡检时间
const int Reconnect_INTERVAL = 200000;  // 200秒 200000
const int HEARTBEAT_INTERVAL = 300000;  // 300秒 300000
const int UPLOADING_INTERVAL = 55000;   // 55秒 55000
const int GETODER_INTERVAL = 5000;      // 5秒 5000
const int ERROR_INTERVAL = 120000;      // 120秒 12000


//创建Server连接
ESPWebServerWrapper webServer(ssid, password);
//舵机的方法创建
CustomServo myServo(PWM_pin);
//创建封装的库方法对象
StepperMotor28BYJ48 myStepper(Stepping_Apin, Stepping_Bpin, Stepping_Cpin, Stepping_Dpin);


void setup() {
  Serial.begin(115200);
  // 设置按钮并拉高电阻检测
  pinMode(Btn_Onepin, INPUT_PULLUP);
  pinMode(Btn_Twopin, INPUT_PULLUP);
  pinMode(WATER_pin, INPUT);
  // 无需在此处初始化 CustomServo 对象，因为已在全局范围内完成
  myServo.pulse(targetlAngle);  //初始状态下的位置设置
  //输出引脚初始化
  pinMode(RGB_Rpin, OUTPUT);
  pinMode(RGB_Gpin, OUTPUT);
  pinMode(RGB_Bpin, OUTPUT);
  PatternLED(pattern);
  // Connect to Wi-Fi
  connectToWiFi();
  //根据联网状况先来一次心跳确认！
  if (connectState) {
    heartbeatFun();
  }
}

void loop() {
  //Web代理
  webServer.handleClient();
  // 每过200S检查一下网络状态，异常超标就不用再次尝试连接了直接进入离线模式。
  if (errorCount < errorLimit && millis() - connectTime >= Reconnect_INTERVAL) {
    connectionAttempts = 2;  //通过修改可变为每200秒有1次重连机会
    connectToWiFi();
    connectTime = millis();
  }
  //根据连接状态更新指示灯
  if (connectState) {     //在线模式需要刷新的数据
    PatternLED(pattern);  //跟新灯光模式
    //异常次数限制:失败异常每分钟次数累计超过几次直接进入离线模式（5次/2分钟）
    if (millis() - errorLimitTime >= ERROR_INTERVAL) {
      if (errorCount < errorLimit) {
        errorCount = 0;
      } else {
        connectState = false;  //表示服务器异常
        isOline(connectState);
        Serial.println("2分钟次数累计超过5次直接进入离线模式！");
      }
      errorLimitTime = millis();
    }
  } else {
    PatternLED(pattern);  //跟新灯光模式
  }

  //************线上模式*************
  if (connectState && errorCount < errorLimit) {  //连接正常且异常次数未达上限
                                                  // 定时执行心跳请求
    if (millis() - lastHeartbeatTime >= HEARTBEAT_INTERVAL) {
      heartbeatFun();
      Serial.println("300S");
      lastHeartbeatTime = millis();
    }

    // 定时执行传感器数据上传
    if (millis() - lastUploadingTime >= UPLOADING_INTERVAL) {
      uploadingFun();
      Serial.println("55S");
      lastUploadingTime = millis();
    }

    // 定时执行任务定时刷新
    if (millis() - lastGetOderTime >= GETODER_INTERVAL) {
      getOderFun();
      Serial.println("5S");
      lastGetOderTime = millis();
    }
  }
  //************离线模式***********
  else {
    // 定时获取传感器数据进行判断处理
    if (millis() - lastUploadingTime >= UPLOADING_INTERVAL) {
      offlineFun();  //离线模式
      Serial.println("offlineFun-55S");
      lastUploadingTime = millis();
    }
  }
  //************************按钮检测************************
  //下边的按钮，单点1秒内为切换方向，超过一秒后开始旋转点动操作！
  if (digitalRead(Btn_Onepin) == LOW) {
    if (BtnOMillis == 0) {
      BtnOMillis = millis();
    }
    //加一个时间检测，控制打开,
    if (millis() - BtnOMillis > 1000) {
      if (isStepper) {  //判断操作对象
        steps = 1;
      }
      AsyncOn = true;
    }
  } else {
    if (BtnOMillis) {
      if (millis() - BtnOMillis < 800) {  //按下时间短于800毫秒切换方向
        if (isStepper) {
          myStepper.foreward = !myStepper.foreward;  //切换方向
        } else {
          myServo.foreward = !myServo.foreward;  //切换方向
        }
      }
      //暂停运行
      AsyncOn = false;
      BtnOMillis = 0;  //恢复数值
      Serial.print("BtnOMillis***:");
    }
  }
  //上边的按钮，短按切换操作对象，长按2切换晾晒模式寻光、定时反面、手动旋转，长按5S切换智能模式和关闭智能模式。
  if (digitalRead(Btn_Twopin) == LOW) {
    if (BtnTMillis == 0) {
      BtnTMillis = millis();
    }
  } else {
    if (BtnTMillis) {
      if (millis() - BtnTMillis < 1000) {  //按下时间短于1秒切换方向
        isStepper = !isStepper;            //切换操作对象
      }
      //一次智能更改一个状态值！松手改变模式！
      if (millis() - BtnTMillis > 2000 && millis() - BtnTMillis < 5000) {
        //切换晾晒模式
        if (pattern == 1 || pattern == 3 || pattern == 5 || pattern == 7) {  //已经开启智能晾晒
          if (!findSunOn) {
            findSunOn = !findSunOn;
          } else {
            findSunOn = false;
            pattern--;
          }
        } else {
          pattern++;
        }
      }
      //切换智能模式
      if (millis() - BtnTMillis > 5000) {
        if (pattern == 4 || pattern == 5 || pattern == 6 || pattern == 7) {
          pattern -= 4;
        } else {
          pattern += 4;
        }
      }
      PatternLED(pattern);
      BtnTMillis = 0;  //恢复数值
      Serial.print("BtnTMillis***:");
    }
  }
  //*************************关于智能晾晒模式的自动实现***************************
  //定时反面的实现
  if ((pattern == 1 || pattern == 3 || pattern == 5 || pattern == 7) && !findSunOn) {
    if (millis() - SpinMillis >= SPIN_INTERVAL && millis() - SpinMillis < SPIN_INTERVAL + 3000) {
      AsyncOn = true;
      isStepper = true;
      steps = 1;
    } else if (millis() - SpinMillis >= SPIN_INTERVAL + 3000) {
      SpinMillis = millis();
      AsyncOn = false;
      isStepper = false;
    }
  }
  //2S刷新一次太阳参数
  if (findSunOn && millis() - SunMillis >= SUN_INTERVAL) {
    int value = analogRead(Sun_pin) * 100 / 1024;
    Serial.println("value :" + (String)value);
    if (value < 45) {  //正转
      AsyncOn = true;
      isStepper = true;
      steps = 1;
      myStepper.foreward = true;
    } else if (value > 55) {  //反转
      AsyncOn = true;
      isStepper = true;
      steps = 1;
      myStepper.foreward = false;
    } else {
      AsyncOn = false;
    }
  }
  //关于智能模式的任务执行，关于雨滴传感器参数是否做参考
  if ((pattern == 4 || pattern == 5 || pattern == 6 || pattern == 7) && millis() - SmartTMillis >= SMART_INTERVAL) {
    SmartTMillis = millis();
    if (digitalRead(WATER_pin) == LOW) {  //有雨滴避之
      AsyncOn = true;
      isStepper = false;
      myServo.foreward = false;
    } else {  //无雨滴伸开，这里应该通过户外传感器或者是天气API。暂时假设雨滴传感器在户外！
      AsyncOn = true;
      isStepper = false;
      myServo.foreward = true;
    }
  }
  //*****************************异步任务实现*************************************
  //关于的步进电机和舵机的一部控制方式，两个无法同时运转！
  if (AsyncOn) {
    //根据标识判断执行不同的操作
    if (isStepper && millis() - AsyncMillis >= Stepper_INTERVAL) {
      AsyncMillis = millis();
      //通过软件实现伺服限制在左右90
      if (myStepper.foreward && stepCount < 500) {
        stepCount++;
      } else if (!myStepper.foreward && stepCount > -500) {
        stepCount--;
      } else {
        myStepper.foreward = !myStepper.foreward;  //到达限位自动切换方向
        steps = 0;
      }
      //执行并异步操作返回刷新步数
      steps = myStepper.stepAsync(steps);
    } else if (!isStepper && millis() - AsyncMillis >= Servo_INTERVAL) {
      AsyncMillis = millis();
      if (myServo.foreward) {  //正转
        targetlAngle = 180;
      } else {  //反转
        targetlAngle = 90;
      }
      oraginalAngle = myServo.variationTrend(oraginalAngle, targetlAngle);
      if (targetlAngle == oraginalAngle) {  //达到目的自动关闭
        AsyncOn == false;
      }
    }
  }
  //********************************与局域网页面的信息同步**********************************
  //每秒同步一些基本参数信息
  if (webServer.NFCOpen && millis() - NFCTMillis >= NFC_INTERVAL) {
    NFCTMillis = millis();  //更新时间
    //刷新数据
    webServer.findSunOnOline = findSunOn;
    webServer.patternOline = pattern;
    webServer.oraginalAngleOline = oraginalAngle;
    //关闭刷新
    webServer.NFCOpen = false;
  }
  //刷新指令参数
  if (webServer.oderOpen) {
    //执行任务
    oderGOTO(String(webServer.oderMessageOline));
    //执行完成关闭
    webServer.oderOpen = false;
  }
}
// 网络初始化
void connectToWiFi() {
  Serial.println("initial to WiFi");
  //连接状态指示灯都亮
  PatternLED(7);
  //根据限制次数进行连接
  while (WiFi.status() != WL_CONNECTED && connectionAttempts < maxConnectionAttempts) {
    webServer.begin();  //有待修改######
    Serial.print(".");
    connectionAttempts++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    connectState = true;
    Serial.println("Connected to WiFi");
    Serial.print("Device IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    connectState = false;
    Serial.println("\nFailed to connect to WiFi. Starting offline mode.");
  }
  isOline(connectState);  //更新模式标识
}

//**************************************定时任务的处理******************************************
//value = (analogRead(A0)*100)/1024;//将测量到的值保存到变量value中
//Serial.println(value);//向串口发送测量到的值
// heartbeatFun心跳函数处理
void heartbeatFun() {
  String json = webServer.sendPostRequest(serverAddress.c_str(), heartbeatEndpoint, heartbeatPostData);
  if (json.isEmpty()) {
    ERRORFun("heartbeatEndpoint-json-Post");
  } else {
    Serial.print(json);
    Serial.println("***heartbeat-OK****");
  }
}
// uploadingFun数据上传方法
void uploadingFun() {
  offlineFun();  //同时自动执行检测任务
  // 光感参数信息、ID=5
  String uploadinpostData = "deviceId=5&value=" + String((analogRead(Sun_pin) * 100) / 1024) + "&bz=test";
  APIFun(uploadinpostData);
  // 雨滴信息、ID=14
  uploadinpostData = "deviceId=14&value=" + String(digitalRead(WATER_pin) * 100) + "&bz=test";
  APIFun(uploadinpostData);
  // 状态SG90、ID=15
  uploadinpostData = "deviceId=15&value=" + String(oraginalAngle) + "&bz=test";
  APIFun(uploadinpostData);
  //状态寻光模式、ID=17
  uploadinpostData = "deviceId=17&value=" + String(findSunOn * 100) + "&bz=test";
  APIFun(uploadinpostData);
  // 模式数据、ID=16
  uploadinpostData = "deviceId=16&value=" + String(pattern) + "&bz=test";
  APIFun(uploadinpostData);
}
//指令获取并执行回馈
void getOderFun() {
  String json = webServer.sendGetRequest((serverAddress + getOder).c_str());
  // Serial.println(json);
  if (json.isEmpty()) {
    ERRORFun("getOder-json-Get");
  } else {
    const char* json_data = json.c_str();
    // 获取 JSON 字符串的大小可能会导致栈溢出。
    size_t jsonSize = 2 * json.length() + 1;  // 增加内存分配大小
    DynamicJsonDocument doc(jsonSize);
    DeserializationError error = deserializeJson(doc, json_data);
    if (error) {
      Serial.print("Failed to parse JSON: ");
      Serial.println(error.c_str());
    }
    //200才代表有指令
    Serial.println(String(doc["status"]));
    if (String(doc["status"]) == "200") {  //判断是否为有指令的状态200
      // 从 JSON 对象中获取 "key" 字段的值
      const char* oderMessage = doc["data"]["oderMessage"];
      String id = String(doc["data"]["id"].as<int>());
      Serial.println(String(oderMessage));
      Serial.println(id);
      Serial.println("~~~getOder-OK-begin~~~");
      String status = oderGOTO(oderMessage);  //指令内容并执行
      oderOKFun(String(id), status);          //执行完命令回复服务器
      Serial.println("~~~getOder-OK-~~~");
    } else {
      Serial.println("~~~getOder-300~~~");
    }
  }
}
// 反馈执行状态
void oderOKFun(String id, String status) {
  String oderOKData = "id=" + id + "&status=" + status + "&bz=test";
  String json = webServer.sendPostRequest(serverAddress.c_str(), oderOK, oderOKData);
  if (json.isEmpty()) {
    ERRORFun("oderOKData-json-POST");
  } else {
    Serial.print(json);
    Serial.println("###oderOKData-OK####");
  }
}
// offlineFun离线模式处理处理方式
void offlineFun() {
}

// ************************************重复方法封装(防止参数传不进去)****************************
void APIFun(String uploadinpostData) {
  String json = webServer.sendPostRequest(serverAddress.c_str(), uploadinEndpoint, uploadinpostData);
  if (json.isEmpty()) {
    ERRORFun("uploadinEndpoint-json-POST");
  } else {
    Serial.print(json);
    Serial.println("###uploading-OK####");
  }
}
// 异常记录方案
void ERRORFun(String api) {
  errorCount++;
  Serial.printf("本周期第: %d 次异常！-%s", errorCount, api);
}
//对不同指令的执行！
String oderGOTO(String oder) {
  String status = "2";     //操作完回馈信息：1成功、2失败、0暂未向应、-1已撤销
  switch (oder.toInt()) {  //转化成int值进行处理
    case 11:               //收回衣架
      AsyncOn = true;
      isStepper = false;
      myServo.foreward = false;
      status = "1";
      break;
    case 12:  //伸开衣架
      AsyncOn = true;
      isStepper = false;
      myServo.foreward = true;
      status = "1";
      break;
    case 13:  //开启智能避雨
      if (pattern == 0 || pattern == 1 || pattern == 2 || pattern == 3) {
        pattern += 4;
      }
      status = "1";
      break;
    case 14:  //关闭智能避雨
      if (pattern == 4 || pattern == 5 || pattern == 6 || pattern == 7) {
        pattern -= 4;
      }
      status = "1";
      break;
    case 15:  //寻光旋转
      if (pattern == 0 || pattern == 2 || pattern == 4 || pattern == 6) {
        pattern++;
      }
      findSunOn = true;
      status = "1";
      break;
    case 16:  //反面旋转
      if (pattern == 0 || pattern == 2 || pattern == 4 || pattern == 6) {
        pattern++;
      }
      findSunOn = false;
      status = "1";
      break;
    case 17:  //关闭旋转
      if (pattern == 1 || pattern == 3 || pattern == 5 || pattern == 7) {
        findSunOn = false;
        pattern--;
      }
      status = "1";
      break;
    default:
      // 如果sensorValue的值不匹配任何case，执行此代码块
      status = "2";  //没有匹配到，表示执行失败！
      break;
  }
  PatternLED(pattern);
  return status;
}
//一些关于指示灯的处理，i的值R-G-B对应1-2-4，7为全亮，但这里对于G一般不做操控，
void PatternLED(int i) {
  digitalWrite(RGB_Rpin, HIGH);
  digitalWrite(RGB_Gpin, HIGH);
  digitalWrite(RGB_Bpin, HIGH);
  switch (i) {
    case 7:
      break;
    case 6:
      digitalWrite(RGB_Rpin, LOW);
      break;
    case 5:
      digitalWrite(RGB_Gpin, LOW);
      break;
    case 4:
      digitalWrite(RGB_Rpin, LOW);
      digitalWrite(RGB_Gpin, LOW);
      break;
    case 3:
      digitalWrite(RGB_Bpin, LOW);
      break;
    case 2:
      digitalWrite(RGB_Rpin, LOW);
      digitalWrite(RGB_Bpin, LOW);
      break;
    case 1:
      digitalWrite(RGB_Gpin, LOW);
      digitalWrite(RGB_Bpin, LOW);
      break;
    default:
      digitalWrite(RGB_Rpin, LOW);
      digitalWrite(RGB_Gpin, LOW);
      digitalWrite(RGB_Bpin, LOW);
      break;
  }
}
//关于在线和离线模式的切换，涉及到模式标识的改变。
void isOline(bool isOline) {
  //刷新模式标识
  if (pattern == 2 || pattern == 3 || pattern == 6 || pattern == 7) {
    if (!isOline) {
      pattern -= 2;
    }
  } else {
    if (isOline) {
      pattern += 2;
    }
  }
}
