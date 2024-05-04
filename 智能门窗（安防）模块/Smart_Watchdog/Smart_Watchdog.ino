#include "ESPWebServerWrapper.h"  //有关于WiFi相关的封装类（部分数信息需要直接前置在这里，包括一些声明）
#include "DS18B20.h"              //温度传感器自行封装的方法库
#include "DHT11.h"                //DHT11的自行封装方法库
#include <Arduino.h>
#include <ArduinoJson.h>  //用于对JSON数据的解析


// 引脚的配置
//传感器等输入检测
#define MQ2 32          //烟雾检测
#define MQ135 33        //空气质量
#define PIN_DS18B20 23  // DS18B20温度引脚
#define PIN_DHT11 22    // DHT11温湿度引脚
#define SR602 2         //人体感应
#define SW420 25        //震动检测
//模式状态记录
#define LED1 15  //红二极管灯泡正极
#define LED2 14  //绿二极管灯泡正极
#define RLED 16  // 红二极管灯泡正极
#define GLED 17  // 绿二极管灯泡正极
#define YLED 5   // 黄二极管灯泡正极
//门窗的限位开关及控制电机配置
#define WINO 39  // 窗开
#define WINC 36  // 窗关
#define WAYO 34  // 门开
#define WAYC 35  // 门关
//1-2+正转开门开窗，1+2-反转为关门关窗
#define MWIN0 4   // A1
#define MWIN1 18  // A2
#define MWAY0 19  // B1
#define MWAY1 21  // B2
//************************************关于一些触摸按钮的设置************************************
// 定义触摸引脚
const int touchO = 12;
const int touchC = 13;
const int touchS = 27;
//计数器
//也可以用于松手判断锁，避免松手任务一直刷新执行，让其仅执行一次一面影响其他任务的操作！
int counterO = 0;
int counterC = 0;
unsigned long counterS = 0;
//操作对象的变更
bool isWINorWAY = true;  //true表示WIN，否则为WAY
// ************************************WIFI连接及JIMO-IOT的API配置************************************
const char* ssid = "jimo";
const char* password = "1517962688";
// Client 的API 配置****(通过软件上的实现3在线4则同时刷新)****
const char* module_id = "3";  //1是智能晾衣架模块、2是智能浇水模块、3是智能门窗模块、4智能安防模块
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
// 根据您的需求，调整此处的 JSON_BUFFER_SIZE
constexpr size_t JSON_BUFFER_SIZE = 1024;  // 例如设置为 1024


//**********************************一些设备运行的配置参数*************************************
int maxConnectionAttempts = 3;  // 设置最大连接失败次数
int connectionAttempts = 0;
bool changePattern = false;  //用于判断模式是否改变的标识
int connectState = 0;        //连接状态标识,0为无连接，1为连接成功，即为在线和离线模式的进入标识**************
int pattern = 0;             //0为自动模式、1为智能模式、2为严格模式，即为不同控制逻辑的标识************
int errorLimit = 5;          //设置最大在线异常数
int errorCount = 0;          //初始化计数
//一些传感器的报警和异常处理阈值参数信息
//pattern=0时自动模式的参数，同时也作为其他两个模式的参考数值
int MQ2_OPEN_WIN = 3350;    //MQ2开窗值
int MQ2_OPEN_WAY = 3900;    //MQ2开门值,或报警值
int MQ135_OPEN_WIN = 3350;  //MQ135开窗值
int MQ135_OPEN_WAY = 3900;  //MQ135开门值，或报警值
int SW420_ALTER = 1500;     //SW420报警值
//室内湿度的合适范围通常是40%到60%之间
int HUMI_OPEN_WIN = 60;   //户内湿度过高开窗门，需要根据模式以及户外的环境进行判断
int HUMI_CLOSE_WIN = 40;  //户内湿度过低关闭窗门，需要根据模式以及户外的环境进行判断
//舒适的室内温度应该在 18°C 到 24°C 之间。实际情况要和户外情况进行联合检测
int TEMP_OPEN_WIN = 24;   //开窗值
int TEMP_OPEN_WAY = 27;   //开门值，或报警值
int TEMP_CLOSE_WIN = 15;  //关窗值，或报警值
int TEMP_CLOSE_WAY = 18;  //关门值
// 电机速率设置，转速不可以过高会导致大量电流消耗！！！
//由于这里设置了高电平为基础，所以此处的给电压越大电机电压差越小，运转越小
//考虑客服启动扭矩（1V），逆差要大于255-200=55
const int MWINV = 160;  // 窗户电机速率
const int MWAYV = 170;  // 门电机速率

//***************************************定时任务********************************************
unsigned long connectTime = 0;
unsigned long lastHeartbeatTime = 0;
unsigned long lastUploadingTime = 0;
unsigned long lastGetOderTime = 0;
unsigned long errorLimitTime = 0;
unsigned long WAYKeyStartTime = 0;
unsigned long WINKeyStartTime = 0;
unsigned long rapidTime = 0;

unsigned long RAPID_INTERVAL = 3000; //3秒 3000
const int WAYKEY_INTERVAL = 3000;       // 3秒 3000
const int WINKEY_INTERVAL = 3000;       // 3秒 3000
const int Reconnect_INTERVAL = 200000;  // 200秒 200000
const int HEARTBEAT_INTERVAL = 300000;  // 300秒 300000
const int UPLOADING_INTERVAL = 77000;   // 77秒 77000
const int GETODER_INTERVAL = 10000;     // 10秒 10000
const int ERROR_INTERVAL = 120000;      // 120秒 120000
//开关门的异步操作锁
bool WINKey = false;            //窗的启动锁
bool WAYKey = false;            //门的启动锁
bool WINKeyInProgress = false;  //窗的任务锁
bool WAYKeyInProgress = false;  //门的任务锁
bool WAYisOpen = false;         //门判断开还是关
bool WINisOpen = false;         //窗判断开还是关
int WAYstatus = 2;              //在事件执行之前的门状态判断记录，便以恢复状态！0为关闭，1为开，2为暂停（一般不需要有其他处理解决）
int WINstatus = 2;              //在事件执行之前的窗户状态判断记录，便以恢复状态！0为关闭，1为开，2为暂停（一般不需要有其他处理解决）
bool WAYrecover = false;        //在执行事件后开启一次恢复时间锁恢复原来状态。
bool WINrecover = false;        //在执行事件后开启一次恢复时间锁恢复原来状态。
//**************************************一些命名和创建******************************************
//创建Server连接
ESPWebServerWrapper webServer(ssid, password);
// 创建DS18B20的对象
DS18B20 myDS18B20(PIN_DS18B20);
//创建DHT11的使用对象
DHT11 myDHT11(PIN_DHT11);

void setup() {
  Serial.begin(115200);
  //定义引脚
  //传感器的定义
  pinMode(SR602, INPUT);  //用于检测是否有人（0无人，1有人）
  //模式状态LED的定义，启动时应当全部亮起
  pinMode(LED1, OUTPUT);
  digitalWrite(LED1, HIGH);
  pinMode(LED2, OUTPUT);
  digitalWrite(LED2, HIGH);
  pinMode(RLED, OUTPUT);
  digitalWrite(RLED, HIGH);
  pinMode(GLED, OUTPUT);
  digitalWrite(GLED, HIGH);
  pinMode(YLED, OUTPUT);
  digitalWrite(YLED, HIGH);
  //门窗状态的检测,常闭低电压，拉低电压检测。
  pinMode(WINO, INPUT_PULLDOWN);
  pinMode(WINC, INPUT_PULLDOWN);
  pinMode(WAYO, INPUT_PULLDOWN);
  pinMode(WAYC, INPUT_PULLDOWN);
  //关于双直流电机的驱动,初始状态均设置为高电平，让电机不动。PWM信号控制驱动板L9110S
  analogWrite(MWIN0, 255);
  analogWrite(MWIN1, 255);
  analogWrite(MWAY0, 255);
  analogWrite(MWAY1, 255);
  // Connect to Wi-Fi
  connectToWiFi();
  //根据联网状况先来一次心跳确认！
  if (connectState) {
    heartbeatFun();
  }
}

void loop() {
  //Web代理，影响实时性可以考虑替换为异步处理
  webServer.handleClient();
  // 每过200S检查一下网络状态，异常超标就不用再次尝试连接了直接进入离线模式。
  if (errorCount < errorLimit && millis() - connectTime >= Reconnect_INTERVAL) {
    connectionAttempts = 2;  //通过修改可变为每200秒有1次重连机会
    connectToWiFi();
    connectTime = millis();
  }
  // //根据连接状态更新指示灯
  if (connectState) {  //在线模式需要刷新的数据
    digitalWrite(LED1, LOW);
    digitalWrite(LED2, HIGH);
    //异常次数限制:失败异常每分钟次数累计超过几次直接进入离线模式（3次/2分钟）
    if (millis() - errorLimitTime >= ERROR_INTERVAL) {
      if (errorCount < errorLimit) {
        errorCount = 0;
      } else {
        connectState = 0;  //表示服务器异常
        Serial.println("2分钟次数累计超过3次直接进入离线模式！");
      }
      errorLimitTime = millis();
    }
  } else {
    digitalWrite(LED1, HIGH);
    digitalWrite(LED2, LOW);
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
      Serial.println("77S");
      lastUploadingTime = millis();
    }

    // 定时执行任务定时刷新
    if (millis() - lastGetOderTime >= GETODER_INTERVAL) {
      getOderFun();
      Serial.println("10S");
      lastGetOderTime = millis();
    }
  }
  //************离线模式***********
  else {
    // 定时获取传感器数据进行判断处理
    if (millis() - lastUploadingTime >= UPLOADING_INTERVAL) {
      offlineFun();  //离线模式
      Serial.println("offlineFun-77S");
      lastUploadingTime = millis();
    }
  }
  //**********触摸检测控制-通过检测计数实现-手触摸时电容检测数值会会变小为15左右，非触摸状态下数值在65左右************
  if (touchRead(touchO) < 40 && !counterC && !counterS) {
    //加一个滤波检测，控制打开
    counterO++;
    if (counterO > 100) {
      MFun(1);
      Serial.print("counterO***:");
      Serial.println(isWINorWAY);
    }
  } else {
    if (counterO) {  //counterO不等于零则标记按过
      MFun(2);
      counterO = 0;  //恢复数值
    }
  }
  //这里假设两个触摸按钮的事件是冲突的所以要进行互锁
  if (touchRead(touchC) < 40 && !counterO && !counterS) {
    //加一个滤波检测，控制关闭
    counterC++;
    if (counterC > 100) {
      MFun(0);
      Serial.print("counterC***:");
      Serial.println(isWINorWAY);
    }
  } else {
    if (counterC) {
      MFun(2);
      counterC = 0;
    }
  }
  //这个是一个长按按钮，仅触发一次在松开的时候，同时在执行上面两个按钮是是不可以进行操作的！
  if (touchRead(touchS) < 40 && !counterO && !counterC) {
    // 如果按钮按下且计时器为0，记录按钮按下的时间戳
    if (counterS == 0) {
      counterS = millis();
    }
    // 如果按钮按下的时间超过2000毫秒，视为长按
    if (millis() - counterS > 2000) {
      pattern = (pattern + 1) % 3;  // 对3取模，保证pattern在0、1、2之间循环递增
      changePattern = true;         // 模式改变标识
      counterS = millis() - 1000;   // 防止预置干扰
    }
  } else {  // // 模式数据上传同步，但是不记录数据库##############################可能需要完善一下API#############################################################################################################################################################
    // 如果按钮松开且计时器不为0，且按下时间不超过1000毫秒，视为短按
    if (counterS != 0 && millis() - counterS < 1000) {
      isWINorWAY = !isWINorWAY;
    }
    //在此开始更新模式状态，同步云端模式ID=12。（仅在线模式启动此项且异常不超标的时候）
    if (changePattern && connectState && errorCount < errorLimit) {
      String uploadinpostData = "deviceId=12&value="+String(pattern)+"&bz=test";
      APIFun(uploadinpostData);
      Serial.println("#######远程模式同步措施#######");
      changePattern = false;  //改变完毕关闭开关锁
    }
    // 重置按钮按下的时间戳
    counterS = 0;
  }
  //******跟新模式显示*************
  PatternLED();
  //******快速自动反应(延迟控制在3秒)**************
  if(millis() - rapidTime > RAPID_INTERVAL){
    rapidFun();
    rapidTime = millis();
  }
  //*********异步执行任务**********
  //开始开关门
  if (!WAYKeyInProgress && WAYKey) {
    WAYFun(WAYisOpen);        //开门
    WAYKeyInProgress = true;  //时间锁打开
    WAYKeyStartTime = millis();
  }
  // 停止任务
  if (WAYKeyInProgress && millis() - WAYKeyStartTime >= WAYKEY_INTERVAL) {
    WAYFun(2);                 //停止
    WAYKeyInProgress = false;  //关闭任务锁
    WAYKey = false;            //执行完毕
    WAYrecover = true;         //开启状态恢复锁
  }
  //开始开关窗户
  if (!WINKeyInProgress && WINKey) {
    WINFun(WINisOpen);
    WINKeyInProgress = true;
    WINKeyStartTime = millis();
  }
  // 停止任务
  if (WINKeyInProgress && millis() - WINKeyStartTime >= WINKEY_INTERVAL) {
    WINFun(2);
    WINKeyInProgress = false;
    WINKey = false;
    WINrecover = true;
  }
  //********************传令方法类********************
  if(webServer.haveOder){
    //模式刷新指令给本地及云端IOT
    pattern = webServer.moduleStatus;
    //在此开始更新模式状态，同步云端模式ID=12。（仅在线模式启动此项且异常不超标的时候）
    if (connectState && errorCount < errorLimit) {
      String uploadinpostData = "deviceId=12&value="+String(pattern)+"&bz=test";
      APIFun(uploadinpostData);
      Serial.println("#######远程模式同步措施#######");
    }
    webServer.haveOder = false;
  }else {
    //刷新模式给web
    webServer.moduleStatus = pattern;
  }
}

//网络初始化
void connectToWiFi() {
  Serial.print("initial to WiFi");
  //连接状态指示灯都亮
  digitalWrite(LED1, HIGH);
  digitalWrite(LED2, HIGH);
  //根据限制次数进行连接
  while (WiFi.status() != WL_CONNECTED && connectionAttempts < maxConnectionAttempts) {
    webServer.begin();  //有待修改######
    Serial.print(".");
    connectionAttempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    connectState = 1;
    Serial.println("Connected to WiFi");
    Serial.print("Device IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    connectState = 0;
    Serial.println("\nFailed to connect to WiFi. Starting offline mode.");
  }
}

//************************************定时任务的处理****************************************
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
  // MQ2数据上传id=3
  String uploadinpostData = "deviceId=3&value=" + String(analogRead(MQ2)) + "&bz=test";
  APIFun(uploadinpostData);
  // MQ135数据上传id=6
  uploadinpostData = "deviceId=6&value=" + String(analogRead(MQ135)) + "&bz=test";
  APIFun(uploadinpostData);
  // DS18B20数据上传id=1
  uploadinpostData = "deviceId=1&value=" + String(myDS18B20.Get_temp()) + "&bz=test";
  APIFun(uploadinpostData);
  // DHT11_TEMP数据上传id=7
  uploadinpostData = "deviceId=7&value=" + String(myDHT11.TEM_Buffer_Int) + "&bz=test";
  APIFun(uploadinpostData);
  // DHT11_HUMI数据上传id=10
  uploadinpostData = "deviceId=10&value=" + String(myDHT11.HUMI_Buffer_Int) + "&bz=test";
  APIFun(uploadinpostData);
  // 门窗状态的更新id=11，三进制码，00窗关门关、01窗关门开、02窗关门间、10窗开门关、11窗开门开、12窗开门间、20窗间门关、21窗间门开、22窗间门间。
  WINorWAYstatusFun(true);
  WINorWAYstatusFun(false);
  uploadinpostData = "deviceId=11&value=" + String(WINstatus) + String(WAYstatus) + "&bz=test";
  APIFun(uploadinpostData);
}
//指令获取并执行回馈
void getOderFun() {
  String json = webServer.sendGetRequest((serverAddress + getOder).c_str());
  // Serial.println(json);
  if (json.isEmpty()) {
    ERRORFun("getOder-json-Get");
  } else {
    // 创建一个 JsonDocument 对象
    DynamicJsonDocument doc(JSON_BUFFER_SIZE);
    // 将您的 JSON 数据解析到 doc 中
    DeserializationError error = deserializeJson(doc, json);
    if (error) {
      Serial.print("Failed to parse JSON: ");
      Serial.println(error.c_str());
      return;
    }
    //200才代表有指令
    JsonObject root = doc.as<JsonObject>();
    String statusString = root["status"].as<String>();
    if (statusString == "200") {  //判断是否为有指令的状态200
      // 从 JSON 对象中获取 "key" 字段的值
      const char* oderMessage = root["data"]["oderMessage"];
      String id = root["data"]["id"].as<String>();
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
//*****************在线模式下根据指令的刷新和数据的上传进行更新模式（可能需要一些标识进行设置，同步本地和云端的模式）**************************
// ************************offlineFun离线模式处理处理方式**************
void offlineFun() {
  //获取各个传感器的参数进行对比，然后个根据模式状态进行分别处理
  //0为自动模式、1为智能模式、2为严格模式。pattern
  int MQ2_value = analogRead(MQ2);
  int MQ135_value = analogRead(MQ135);
  float DS18B20_value = myDS18B20.Get_temp();  //这个测的是户外温度
  myDHT11.DHT11_Read();
  int DHT11_TEMP = myDHT11.TEM_Buffer_Int;     //这个是室内温度
  int DHT11_HUMI = myDHT11.HUMI_Buffer_Int;
  switch (pattern) {
    case 0:  //自动模式，自动模式密钥执行大于触摸按钮操作！直接开门窗,有传感器来控制门的开关大小等，窗户不会自动恢复原始状态位置。
      //这里均是开窗户条件
      if ((MQ2_value > MQ2_OPEN_WIN) || (MQ135_value > MQ135_OPEN_WIN) || (DHT11_HUMI > HUMI_OPEN_WIN) || (DHT11_TEMP > TEMP_OPEN_WIN && ((DHT11_TEMP - DS18B20_value) > 2))) {
        WINorWAYstatusFun(true);  //提前存储一下窗位置信息
        WINKey = true;
        WINisOpen = true;
      }
      //关窗判断
      else if ((DHT11_HUMI < HUMI_CLOSE_WIN) || (DHT11_TEMP < TEMP_CLOSE_WIN && ((DS18B20_value - DHT11_TEMP) < 3))) {
        WINorWAYstatusFun(true);  //提前存储一下窗位置信息
        WINKey = true;
        WINisOpen = false;
      } else {
        if (WINrecover) {      //这里不做位置恢复操作
          WINrecover = false;  //关闭恢复锁
        }
      }
      //这里是开门的操作
      if ((MQ2_value > MQ2_OPEN_WAY) || (MQ135_value > MQ135_OPEN_WAY) || (DHT11_HUMI > (HUMI_OPEN_WIN + 15)) || (DHT11_TEMP > TEMP_OPEN_WAY && ((DHT11_TEMP - DS18B20_value) > 2))) {
        WINorWAYstatusFun(false);  //提前存储一下门位置信息
        WAYKey = true;
        WAYisOpen = true;
      }
      //关门判断
      else if ((DHT11_HUMI < (HUMI_OPEN_WIN - 15)) || (DHT11_TEMP < TEMP_CLOSE_WAY && ((DS18B20_value - DHT11_TEMP) < 4))) {
        WINorWAYstatusFun(false);  //提前存储一下门位置信息
        WAYKey = true;
        WAYisOpen = false;
      } else {
        if (WAYrecover) {      //仅在执行完一次自动任务后恢复
          WAYFun(WAYstatus);   //恢复状态值
          WAYrecover = false;  //关闭恢复锁
        }
      }
      break;
    case 1:  //智能模式,异常开窗，极限报警+开门！门窗均会恢复原始设定状态！
      //这里均是开窗户条件
      if ((MQ2_value > MQ2_OPEN_WIN) || (MQ135_value > MQ135_OPEN_WIN) || (DHT11_HUMI > HUMI_OPEN_WIN) || (DHT11_TEMP > TEMP_OPEN_WIN && ((DHT11_TEMP - DS18B20_value) > 2))) {
        WINorWAYstatusFun(true);  //提前存储一下窗位置信息
        WINKey = true;
        WINisOpen = true;
      }
      //关窗判断
      else if ((DHT11_HUMI < HUMI_CLOSE_WIN) || (DHT11_TEMP > TEMP_CLOSE_WIN && ((DS18B20_value - DHT11_TEMP) < 3))) {
        WINorWAYstatusFun(true);  //提前存储一下窗位置信息
        WINKey = true;
        WINisOpen = false;
      } else {
        if (WINrecover) {      //这里需要位置恢复操作
          WINFun(WINstatus);   //恢复原来位置
          WINrecover = false;  //关闭恢复锁
        }
      }
      //这里是开门的操作，如果在线模式正常的话也是报警值！
      if ((MQ2_value > MQ2_OPEN_WAY) || (MQ135_value > MQ135_OPEN_WAY) || (DHT11_HUMI > (HUMI_OPEN_WIN + 15)) || (DHT11_TEMP > TEMP_OPEN_WAY && ((DHT11_TEMP - DS18B20_value) > 2))) {
        WINorWAYstatusFun(false);  //提前存储一下门位置信息
        WAYKey = true;
        WAYisOpen = true;
        //报警措施
        AlterFun("311","Alter pattern-1 : MQ2_value:" + (String)MQ2_value + ",MQ135_value:" + (String)MQ135_value + ",DS18B20_value:" + (String)DS18B20_value + ",DHT11_TEMP:" + (String)DHT11_TEMP + ",DHT11_HUMI:" + (String)DHT11_HUMI);
      }
      //关门判断,无需报警
      else if ((DHT11_HUMI < (HUMI_OPEN_WIN - 15)) || (DHT11_TEMP < TEMP_CLOSE_WAY && ((DS18B20_value - DHT11_TEMP) < 4))) {
        WINorWAYstatusFun(false);  //提前存储一下门位置信息
        WAYKey = true;
        WAYisOpen = false;
      } else {
        if (WAYrecover) {      //仅在执行完一次自动任务后恢复
          WAYFun(WAYstatus);   //恢复状态值
          WAYrecover = false;  //关闭恢复锁
        }
      }
      break;
    case 2:  //严防模式，异常报警开窗,极限开门！
      //这里均是开窗户条件，数据异常需要报警提示
      if ((MQ2_value > MQ2_OPEN_WIN) || (MQ135_value > MQ135_OPEN_WIN) || (DHT11_HUMI > HUMI_OPEN_WIN) || (DHT11_TEMP > TEMP_OPEN_WIN && ((DHT11_TEMP - DS18B20_value) > 4))) {
        WINorWAYstatusFun(true);  //提前存储一下窗位置信息
        WINKey = true;
        WINisOpen = true;
        //报警措施，一*级报警
        AlterFun("321","Alter pattern-2*: MQ2_value:" + (String)MQ2_value + ",MQ135_value:" + (String)MQ135_value + ",DS18B20_value:" + (String)DS18B20_value + ",DHT11_TEMP:" + (String)DHT11_TEMP + ",DHT11_HUMI:" + (String)DHT11_HUMI);
      }
      //关窗判断,由于严防模式及时关窗有一定的必要性！
      else if ((MQ2_value < MQ2_OPEN_WIN) || (MQ135_value < MQ135_OPEN_WIN) || (DHT11_HUMI < HUMI_CLOSE_WIN) || (DHT11_TEMP < TEMP_OPEN_WIN)) {
        //能关则关不需要恢复初始状态！
        WINFun(0);
      } else {
        if (WINrecover) {      //这里需要位置恢复操作
          WINFun(WINstatus);   //恢复原来位置
          WINrecover = false;  //关闭恢复锁
        }
      }
      //这里是开门的操作,相对之前的限制更加的严格！！！如果在线模式正常的话也是报警值！
      if ((MQ2_value > MQ2_OPEN_WAY) || (MQ135_value > MQ135_OPEN_WAY) || (DHT11_HUMI > (HUMI_OPEN_WIN + 20)) || (DHT11_TEMP > (TEMP_OPEN_WAY + 20))) {
        WINorWAYstatusFun(false);  //提前存储一下门位置信息
        WAYKey = true;
        WAYisOpen = true;
        //报警措施，二**级报警！
        AlterFun("322","Alter pattern-2**: MQ2_value:" + (String)MQ2_value + ",MQ135_value:" + (String)MQ135_value + ",DS18B20_value:" + (String)DS18B20_value + ",DHT11_TEMP:" + (String)DHT11_TEMP + ",DHT11_HUMI:" + (String)DHT11_HUMI);
      }
      //关门判断,这里的关门策略是不同的关门更加宽松一些！
      else if ((MQ2_value < MQ2_OPEN_WAY) || (MQ135_value < MQ135_OPEN_WAY) || (DHT11_HUMI < (HUMI_OPEN_WIN + 5)) || (DHT11_TEMP < (TEMP_OPEN_WAY + 5))) {
        //能关闭就直接关闭，不需要恢复。
        WAYFun(0);
      } else {
        if (WAYrecover) {      //仅在执行完一次自动任务后恢复
          WAYFun(WAYstatus);   //恢复状态值
          WAYrecover = false;  //关闭恢复锁
        }
      }
      break;
  }
}
//对于一些急速反应的情况做一些特殊处理(注意：在循环中为了保证不抢走定时任务的恢复锁，建议将其放在定时循环任务之后，执行任务之前！)
void rapidFun() {
  //离线模式写的状态展示用按钮设置，
  bool someOne = digitalRead(SR602);    //有人true，无人false
  int SW420_value = analogRead(SW420);  //获取震动参数int SW420_ALTER = 200;//SW420报警值
  switch (pattern) {
    case 0:  //自动模式，自动模式密钥执行大于触摸按钮操作！
      if (someOne || SW420_value > SW420_ALTER) {
        //开门操作
        WINorWAYstatusFun(false);  //提前存储一下门位置信息
        WAYKey = true;
        WAYisOpen = true;
      } else {
        if (WAYrecover) {      //仅在执行完一次自动任务后恢复
          WAYFun(WAYstatus);   //恢复状态值
          WAYrecover = false;  //关闭恢复锁
        }
      }
      break;
    case 1:  //智能模式,有人开门异常报警！
      if (someOne) {
        //开门操作
        WINorWAYstatusFun(false);  //提前存储一下门位置信息
        WAYKey = true;
        WAYisOpen = true;
      } else {
        if (WAYrecover) {      //仅在执行完一次自动任务后恢复
          WAYFun(WAYstatus);   //恢复状态值
          WAYrecover = false;  //关闭恢复锁
        }
      }
      if (SW420_value > SW420_ALTER) {
        //报警措施
        AlterFun("312","Alter pattern-1 SW420:" + SW420_value);
      }
      break;
    case 2:  //严防模式，有人不开门（除非按钮），异常报警！
      if (SW420_value > SW420_ALTER) {
        //报警措施
        AlterFun("323","Alter pattern-2 SW420:" + SW420_value);
      }
      break;
  }
}
//******************************关于通用方法的书写********************************************
//关于设备的主动报警API的通过用方法
// 数据上传触发报警，记录数据库##############################可能需要完善一下API##################################################################################################
void AlterFun(String t,String s) {
  if (connectState && errorCount < errorLimit) {  //加一层在线判断
    //报警措施ID=13,
    String uploadinpostData = "deviceId=13&value="+t+"&bz="+s;
    APIFun(uploadinpostData);
    Serial.println(s);
  } else {
    //没有网络则不会报警
    Serial.println("offline" + s);
  }
}
// 重复方法封装(防止参数传不进去)
void APIFun(String uploadinpostData) {
  String json = webServer.sendPostRequest(serverAddress.c_str(), uploadinEndpoint, uploadinpostData);
  if (json.isEmpty()) {
    ERRORFun("uploadinEndpoint-json-POST");
  } else {
    Serial.print(json);
    Serial.println("###uploading-OK####");
  }
}
//异常记录方案
void ERRORFun(String api) {
  errorCount++;
  Serial.printf("本周期第: %d 次异常！-%s", errorCount, api.c_str());
}
//对不同指令的执行！
String oderGOTO(String oder) {
  String status = "2";  //操作完回馈信息：1成功、2失败、0暂未向应、-1已撤销
  //模块二增设3无需浇水（成功）、4水不足（失败提醒），模块三增设5无需改变（成功）。
  switch (oder.toInt()) {  //转化成int值进行处理,指令内容包含开关门窗、切换模式！
    case 31:               //开窗
      if (digitalRead(WINO)) {
        status = 5;
      } else {
        WINFun(1);
        status = 1;
      }
      break;
    case 32:  //关窗
      if (digitalRead(WINC)) {
        status = 5;
      } else {
        WINFun(0);
        status = 1;
      }
      break;
    case 33:  //开门
      if (digitalRead(WAYO)) {
        status = 5;
      } else {
        WAYFun(1);
        status = 1;
      }
      break;
    case 34:  //关门
      if (digitalRead(WAYC)) {
        status = 5;
      } else {
        WAYFun(0);
        status = 1;
      }
      break;
    case 35:  //自动模式
      if (pattern == 0) {
        status = 5;
      } else {
        pattern = 0;
        status = 1;
      }
      break;
    case 36:  //智能模式
      if (pattern == 1) {
        status = 5;
      } else {
        pattern = 1;
        status = 1;
      }
      break;
    case 37:  //严防模式
      if (pattern == 2) {
        status = 5;
      } else {
        pattern = 2;
        status = 1;
      }
      break;
    default:
      // 如果sensorValue的值不匹配任何case，执行此代码块
      status = "2";  //没有匹配到，表示执行失败！
      break;
  }
  return status;
}
//*******************************指示灯状态以及开关门的执行*********************************
//根据状态进行设置指示灯
void PatternLED() {
  digitalWrite(RLED, LOW);
  digitalWrite(GLED, LOW);
  digitalWrite(YLED, LOW);
  if (pattern == 0) {
    digitalWrite(YLED, HIGH);
  } else {
    if (pattern == 1) {
      digitalWrite(GLED, HIGH);
    } else {
      digitalWrite(RLED, HIGH);
    }
  }
}

//门窗的开关，定义r(0为关-反转、1为开-正转、2为停止)
void WINFun(int r) {
  if (r == 0 && !digitalRead(WINC)) {
    analogWrite(MWIN0, MWINV);  // 0-255范围，占空比为200/255
    return;
  }
  if (r == 1 && !digitalRead(WINO)) {
    analogWrite(MWIN1, MWINV);  // 0-255范围，占空比为200/255
    return;
  }
  analogWrite(MWIN0, 255);
  analogWrite(MWIN1, 255);
}
//由于收到低电平时，这会使得两个 H 桥都处于导通状态需要设置电机停止时均为高电平！！！
void WAYFun(int r) {
  if (r == 0 && !digitalRead(WAYC)) {
    analogWrite(MWAY0, MWAYV);  // 0-255范围，占空比为200/255
    return;
  }
  if (r == 1 && !digitalRead(WAYO)) {
    analogWrite(MWAY1, MWAYV);  // 0-255范围，占空比为200/255
    return;
  }
  analogWrite(MWAY0, 255);
  analogWrite(MWAY1, 255);
}

//封装一个重读的控制方法,此方法主要对触控按钮做的优化！
void MFun(int r) {
  if (isWINorWAY) {
    //自动模式密钥执行大于触摸按钮
    if (!WINKey) {
      WINFun(r);
    }
  } else {
    //自动模式密钥执行大于触摸按钮
    if (!WAYKey) {
      WAYFun(r);
    }
  }
}
//判断门窗状态的方法
void WINorWAYstatusFun(bool w) {
  if (w) {
    //w=true则是识别窗户状态
    if (digitalRead(WINO)) {
      WINstatus = 1;
    } else if (digitalRead(WINC)) {
      WINstatus = 0;
    } else {
      WINstatus = 2;
    }
  } else {
    if (digitalRead(WAYO)) {
      WAYstatus = 1;
    } else if (digitalRead(WAYC)) {
      WAYstatus = 0;
    } else {
      WAYstatus = 2;
    }
  }
}