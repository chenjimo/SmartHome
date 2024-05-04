#include "ESPWebServerWrapper.h"//有关于WiFi相关的封装类（部分数信息需要直接前置在这里，包括一些声明）
#include <Arduino.h>
#include <ArduinoJson.h>//用于对JSON数据的解析


// 引脚的配置
#define Sensor A0 //土壤湿度模拟信号输入地址
#define LED1 D5 //红二极管灯泡正极
#define LED2 D6 //绿二极管灯泡正极
#define buttonPin D3 //按钮正信号输入检测，拉低电平
#define water D0 //水位传感器的模拟信号（通过转换高低电平判断水位阈值）
#define relay D7 //继电器控制端，拉高水泵运行


// WIFI 的配置
const char* ssid = "jimo";
const char* password = "1517962688";
// Client 的API 配置
const char* module_id = "2";//1是智能晾衣架模块、2是智能浇水模块、3是智能门窗模块、4智能安防
String serverAddress = "http://iot.jimo.fun";
//心跳请求-POST
const char* heartbeatEndpoint = "/module/log";  
String heartbeatPostData = "moduleId=" + String(module_id);  
//数据上传-POST
const char* uploadinEndpoint = "/device/log";
//指令刷新-GET
String getOder = "/oder/log?moduleId="+ String(module_id);  
//任务完成告诉Server-POST
const char* oderOK = "/oder/log";


//一些设备运行的配置参数
int maxConnectionAttempts = 3;  // 设置最大连接失败次数
int connectionAttempts = 0;
int connectState = 0; //连接状态标识,0为无连接，1为连接成功
int errorLimit = 5;//设置最大在线异常数
int errorCount = 0;//初始化计数
// int RedLED = 0; //红灯启动不亮
// int GreenLED = 0; //绿灯启动不亮
//定时任务
unsigned long wateringStartTime = 0;
unsigned long connectTime = 0;
unsigned long lastHeartbeatTime = 0;
unsigned long lastUploadingTime = 0;
unsigned long lastGetOderTime = 0;
unsigned long errorLimitTime = 0;

const int WATERING_INTERVAL = 3000; // 3秒 3000
const int Reconnect_INTERVAL = 200000; // 200秒 200000
const int HEARTBEAT_INTERVAL = 300000; // 300秒 300000
const int UPLOADING_INTERVAL = 55000;  // 55秒 55000
const int GETODER_INTERVAL = 5000;    // 5秒 5000
const int ERROR_INTERVAL = 120000;    // 120秒 12000
//时间的触发和锁定
bool wateringInProgress = false;//锁住其中一个任务
int counter = 0;          // 按钮计数器
// // 创建对应的方法
// // 放法前置声明
// void heartbeatFun(); // 在这里声明heartbeatFun
// void uploadingFun(); // 在这里声明uploadingFun
// void getOderFun(); // 在这里声明getOderFun
// void oderOKFun(); // 在这里声明oderOKFun
// void offlineFun();// 在这里声明offlineFun
// void refresh();//刷新
// // 其他复用方法声明
// void APIFun();
// void oderGOTO();//任务执行
// void httpERROR();//异常处理
//创建Server连接
ESPWebServerWrapper webServer(ssid, password);


void setup() {
  Serial.begin(115200);
  //定义引脚
  pinMode(water,INPUT);//水位传感器
  pinMode(buttonPin,INPUT_PULLUP);//按钮,拉高电位
  pinMode(LED1,OUTPUT);//离线指示灯，先拉高初始状态运行
  digitalWrite(LED1,LOW);
  pinMode(LED2,OUTPUT);//在线指示灯，先拉高初始状态运行
  digitalWrite(LED2,LOW);
  pinMode(relay,OUTPUT);//继电器，初始状态拉低电压
  digitalWrite(relay,LOW);
  // Connect to Wi-Fi
  connectToWiFi();
  //根据联网状况先来一次心跳确认！
  if(connectState){
    heartbeatFun();
  }
}

void loop() {
  //Web代理
    webServer.handleClient();
  // 每过200S检查一下网络状态，异常超标就不用再次尝试连接了直接进入离线模式。
  if (errorCount < errorLimit && millis() - connectTime >= Reconnect_INTERVAL) {
      connectionAttempts = 2;//通过修改可变为每200秒有1次重连机会
      connectToWiFi();
      connectTime = millis();
  }
  //根据连接状态更新指示灯
  if(connectState){//在线模式需要刷新的数据
    digitalWrite(LED1,LOW);
    digitalWrite(LED2,HIGH);
    //异常次数限制:失败异常每分钟次数累计超过几次直接进入离线模式（5次/2分钟）
    if (millis() - errorLimitTime >= ERROR_INTERVAL) {
      if(errorCount < errorLimit){
        errorCount = 0;
      }else{
        connectState = 0;//表示服务器异常
        Serial.println("2分钟次数累计超过5次直接进入离线模式！");
      }
      errorLimitTime = millis();
    }
  }else{
    digitalWrite(LED1,HIGH);
    digitalWrite(LED2,LOW);
  }
  
  //************线上模式*************
  if(connectState && errorCount < errorLimit){//连接正常且异常次数未达上限
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
  else{
    // 定时获取传感器数据进行判断处理
    if (millis() - lastUploadingTime >= UPLOADING_INTERVAL) {
      offlineFun();//离线模式
      Serial.println("offlineFun-55S");
      lastUploadingTime = millis();
    }
  }
  //**********按钮检测控制************
  if(digitalRead(buttonPin) == LOW){
    //加一个滤波检测，控制浇水的状态改变
    counter++;
  }else{
    if (counter > 50){
      webServer.WateringBegin();
    }
    counter = 0;
  }
  //*********异步执行任务**********
      //开始浇水
  if (!wateringInProgress && webServer.watering) {
    startWatering();
  }

  // 停止浇水
  if (wateringInProgress && millis() - wateringStartTime >= WATERING_INTERVAL) {
    stopWatering();
  }


}
//网络初始化
void connectToWiFi() {
  Serial.println("initial to WiFi");
//连接状态指示灯都亮
  digitalWrite(LED1,HIGH);
  digitalWrite(LED2,HIGH);
  //根据限制次数进行连接
  while (WiFi.status() != WL_CONNECTED && connectionAttempts < maxConnectionAttempts) {
    webServer.begin();//有待修改######
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


//****************定时任务的处理********************
//value = (analogRead(A0)*100)/1024;//将测量到的值保存到变量value中
//Serial.println(value);//向串口发送测量到的值
// heartbeatFun心跳函数处理
void heartbeatFun(){
  String json = webServer.sendPostRequest(serverAddress.c_str(),heartbeatEndpoint,heartbeatPostData);
  if (json.isEmpty()) {
    ERRORFun("heartbeatEndpoint-json-Post");
  } else {
    Serial.print(json);
    Serial.println("***heartbeat-OK****"); 
  }
}
 // uploadingFun数据上传方法
void uploadingFun(){
  offlineFun();//同时自动执行检测任务
  // 水位数据上传，但是不记录数据库，仅用作报警提示水位不足##############################可能需要完善一下API
  String uploadinpostData = "deviceId=8&value="+String(digitalRead(water))+"&bz=test";
  APIFun(uploadinpostData);
  // 土壤湿度数据上传
  uploadinpostData = "deviceId=4&value="+String((analogRead(Sensor)*100)/1024)+"&bz=test";
  APIFun(uploadinpostData);
}
//指令获取并执行回馈
void getOderFun(){
 String json = webServer.sendGetRequest((serverAddress+getOder).c_str());
  // Serial.println(json); 
  if (json.isEmpty()) {
    ERRORFun("getOder-json-Get");
  } else {
      const char* json_data = json.c_str();
          // 获取 JSON 字符串的大小可能会导致栈溢出。
      size_t jsonSize = 2 * json.length() + 1; // 增加内存分配大小
      DynamicJsonDocument doc(jsonSize);
      DeserializationError error = deserializeJson(doc, json_data);
      if (error) {
        Serial.print("Failed to parse JSON: ");
        Serial.println(error.c_str());
      }
      //200才代表有指令
      Serial.println(String(doc["status"]));
      if(String(doc["status"]) == "200"){//判断是否为有指令的状态200
        // 从 JSON 对象中获取 "key" 字段的值
        const char* oderMessage = doc["data"]["oderMessage"];
        String id = String(doc["data"]["id"].as<int>());
        Serial.println(String(oderMessage));
        Serial.println(id);
        Serial.println("~~~getOder-OK-begin~~~");
        String status = oderGOTO(oderMessage);//指令内容并执行
        oderOKFun(String(id),status);//执行完命令回复服务器
        Serial.println("~~~getOder-OK-~~~");
      }else{
        Serial.println("~~~getOder-300~~~"); 
      }
  }
}
// 反馈执行状态
void oderOKFun(String id,String status){
    String oderOKData = "id="+id+"&status="+status+"&bz=test";
    String json = webServer.sendPostRequest(serverAddress.c_str(),oderOK,oderOKData);
    if (json.isEmpty()) {
      ERRORFun("oderOKData-json-POST");
  } else {
     Serial.print(json);
     Serial.println("###oderOKData-OK####"); 
  }
}
// offlineFun离线模式处理处理方式
void offlineFun(){
  String message = "抱歉，水量不足！请您及时补水后，再进行尝试(●'◡'●)";
  //通过实验数据建议保持在35~45即可。低于35不建议补水，超过53必须补水！
  if((analogRead(Sensor)*100)/1024 < 53){
    message = "暂时不需要浇水";
  }else{
    if(digitalRead(water)){
      webServer.WateringBegin();//启动继电器异步任务
      message = "您的植物已经喝到甘甜的水了，它很感激！";
    }
  }
  Serial.println(message); 
}

// 重复方法封装(防止参数传不进去)
void APIFun(String uploadinpostData){
    String json = webServer.sendPostRequest(serverAddress.c_str(),uploadinEndpoint,uploadinpostData);
  if (json.isEmpty()) {
    ERRORFun("uploadinEndpoint-json-POST");
  } else {
     Serial.print(json);
     Serial.println("###uploading-OK####"); 
  }
}
//异常记录方案
void ERRORFun(String api){
      errorCount++;
      Serial.printf("本周期第: %d 次异常！-%s", errorCount,api);

}
//对不同指令的执行！
//对于模块二增设不同的定义：3-不需要浇水、4-水不够。
String oderGOTO(String oder){
  String status = "2"; //操作完回馈信息：1成功、2失败、0暂未向应、-1已撤销
  switch(oder.toInt()) {//转化成int值进行处理
    case 21:
        //通过实验数据建议保持在35~45即可。低于35不建议补水，超过53必须补水！
        if((analogRead(Sensor)*100)/1024 < 35){
          status= 3;
        }else{
          if(digitalRead(water)){
            webServer.WateringBegin();//启动继电器异步任务
            status= 1;
          }else{
            status= 4;
          }
        }
      break;
    default:
    // 如果sensorValue的值不匹配任何case，执行此代码块
      status = "2";//没有匹配到，表示执行失败！
      break;
  }
   return status;
} 
//************异步实现开始和停止任务***********
void startWatering() {
  digitalWrite(relay, HIGH);  // Start watering
  wateringInProgress = true;
  wateringStartTime = millis();  // Record the start time
}

void stopWatering() {
  digitalWrite(relay, LOW);  // Stop watering
  wateringInProgress = false;
  webServer.WateringOK();//关闭任务
}