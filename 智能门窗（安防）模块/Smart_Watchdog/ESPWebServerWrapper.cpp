#include "WString.h"
// ESPWebServerWrapper.cpp
#include "ESPWebServerWrapper.h"
#include "DS18B20.h"  //温度传感器自行封装的方法库
#include "DHT11.h"    //DHT11的自行封装方法库

// 引脚的配置
//传感器等输入检测
#define MQ2 32          // MQ2传感器
#define MQ135 33        //
#define PIN_DS18B20 23  //
#define PIN_DHT11 22    //
#define SR602 2         //
#define SW420 25        // 黄二极管灯泡正极
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

const char* hostname = "Smart-Watchdog";
int olineWAYstatus = 2;  //在事件执行之前的门状态判断记录，便以恢复状态！0为关闭，1为开，2为暂停（一般不需要有其他处理解决）
int olineWINstatus = 2;  //在事件执行之前的窗户状态判断记录，便以恢复状态！0为关闭，1为开，2为暂停（一般不需要有其他处理解决）
//考虑客服启动扭矩（1V），逆差要大于255-200=55
const int olineMWINV = 160;  // 窗户电机速率
const int olineMWAYV = 170;  // 门电机速率
//设置一下按钮的双击暂停功能,初始状态false可以执行操作，当为true时做暂停处置！
bool btnWIN = false;
bool btnWAY = false;

// 创建DS18B20的对象
DS18B20 olinemyDS18B20(PIN_DS18B20);
//创建DHT11的使用对象
DHT11 olinemyDHT11(PIN_DHT11);

ESPWebServerWrapper::ESPWebServerWrapper(const char* ssid, const char* password)
  : ssid(ssid), password(password), server(80), moduleStatus(0), haveOder(false) {}

void ESPWebServerWrapper::begin() {
  WiFi.setHostname(hostname);
  WiFi.begin(ssid, password);
  int t = 20;
  while (WiFi.status() != WL_CONNECTED && t > 0) {  // 最多等待10秒则放弃连接
    delay(500);
    t--;
  }
  //注入映射
  server.on("/", HTTP_GET, [this]() {
    this->handleRoot();
  });
  //几个传感器的API数据：MQ2、MQ135、DS18B20、DHT11_TEMP、DHT11_HUMI、SR602、SW420、门窗开关状态、模式状态值
  server.on("/updateData", HTTP_GET, [this]() {
    this->handleUpdateData();
  });
  //按钮API：开窗户
  server.on("/btn1", HTTP_GET, [this]() {
    this->buttonOne();
  });
  //关窗户
  server.on("/btn2", HTTP_GET, [this]() {
    this->buttonTwo();
  });
  //开门
  server.on("/btn3", HTTP_GET, [this]() {
    this->buttonThree();
  });
  //关门
  server.on("/btn4", HTTP_GET, [this]() {
    this->buttonFour();
  });
  //模式切换
  server.on("/btn5", HTTP_GET, [this]() {
    this->buttonFive();
  });

  server.begin();
}
// **********Web页面与API有关**********
void ESPWebServerWrapper::handleClient() {
  server.handleClient();
}

void ESPWebServerWrapper::handleRoot() {
  // HTML内容
String html = "";
html += "<!DOCTYPE html>";
html += "<html lang=\"en\">";
html += "<head>";
html += "  <meta charset=\"UTF-8\">";
html += "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">";
html += "  <link rel=\"shortcut icon\" href=\"http://jimo.fun/img/my/jm.png\">";
html += "  <title>Smart_Watchdog-家庭控制台</title>";
html += "  <!-- 引入Chart.js库 -->";
html += "  <script src=\"https://cdn.staticfile.org/Chart.js/3.9.1/chart.js\"></script>";
html += "  <!--layui.js-->";
html += "  <script src=\"https://www.layuicdn.com/layui/layui.js\"></script>";
html += "  <!--layui.css-->";
html += "  <script src=\"https://www.layuicdn.com/layui/css/layui.css\"></script>";
html += "</head>";
html += "<style> button { background-color: #4CAF50; color: white; border: none; padding: 10px 20px; text-align: center; text-decoration: none; display: inline-block; font-size: 16px; margin: 4px 2px; cursor: pointer; border-radius: 4px; } body { font-family: Arial, sans-serif; text-align: center; margin-top: 50px; } .status { font-size: 20px; margin-top: 20px; font-weight: bold; color: rgb(255, 115, 0); } .modelV { font-size: 21px; font-weight: bold; color: #fc0404; } .status p:nth-child(2), .status p:nth-child(3) { display: inline-block; margin-left: 20px; } #windowStatus, #doorStatus { padding: 5px 10px; border-radius: 5px; background-color: #f0f0f0; color: #c903fa; } #PatternID { font-size: 16px; font-weight: bold; color: #e31dfd; background-color: #a0c0c5; border-radius: 5px; padding: 5px; } .option1 { color: rgb(247, 203, 10); } .option2 { color: rgb(14, 255, 54); } .option3 { color: red; } </style>";
html += "<body>";
html += "  <h1>实时家庭环境检测</h1>";
html += "  <a href=\"http://iot.jimo.fun/\" target=\"_blank\"><button>查看历史数据🤭，点我-去JIMO-IOT</button></a><a";
html += "    href=\"http://iot.jimo.fun/templates/controller/index.html\" target=\"_blank\"><button";
html += "      onclick=\"iotjimo()\">需要定时和远程操作以及其他服务，点我-去JIMO-IOT控制台O(∩_∩)O</button></a>";
html += "  <!-- 创建一个Canvas元素用于显示图表 -->";
html += "  <canvas id=\"myChart\" width=\"400\" height=\"200\"></canvas>";
html += "  <!-- 当前门和窗的状态显示 -->";
html += "  <div>";
html += "    <div class=\"status\">";
html += "      <p class=\"modelV\">当前设备模式: <span id=\"modeStatus\">正在获取...</span></p>";
html += "      <p>当前窗户的状态: <span id=\"windowStatus\">正在获取...</span></p>";
html += "      <p>当前门的状态: <span id=\"doorStatus\">正在获取...</span></p>";
html += "    </div>";
html += "  </div>";
html += "  <!-- 添加按钮 -->";
html += "  <button onclick=\"WINO()\">开窗</button>";
html += "  <button onclick=\"WINC()\">闭窗</button>";
html += "  <button onclick=\"WAYO()\">开门</button>";
html += "  <button onclick=\"WAYC()\">关门</button>";
html += "  <!-- <button onclick=\"startWatering()\">点我-洒洒水呀(●'◡'●)！</button> -->";
html += "  </br>";
html += "  <label for=\"PatternID\" class=\"modelV\">模式切换：</label>";
html += "  <select id=\"PatternID\" class=\"modelV\">";
html += "    <option value=\"0\" class=\"option1\">自动模式</option>";
html += "    <option value=\"1\" class=\"option2\">智能模式</option>";
html += "    <option value=\"2\" class=\"option3\">严防模式</option>";
html += "  </select>";
html += "  <button onclick=\"changePattern()\">点击切换(●'◡'●)！</button>";
html += "<script> let data1 = []; let data2 = []; let data3 = []; let data4 = []; let data5 = []; let data6 = []; let data7 = []; let intervalId; const ctx = document.getElementById('myChart').getContext('2d'); const myChart = new Chart(ctx, { type: 'line', data: { labels: [], datasets: [ { label: '烟雾指标', data: data1, borderColor: 'rgb(128, 0, 128)', tension: 0.1 }, { label: '空气质量', data: data2, borderColor: 'rgb(252, 160, 0)', tension: 0.1 }, { label: '室外温度', data: data3, borderColor: 'rgb(255, 0, 168)', tension: 0.1 }, { label: '室内温度', data: data4, borderColor: 'rgb(255, 0, 0)', tension: 0.1 }, { label: '室内湿度(%RH)', data: data5, borderColor: 'rgb(54, 162, 235)', tension: 0.1 }, { label: '人体感应', data: data6, borderColor: 'rgb(94, 255, 0)', tension: 0.1 }, { label: '震动警报', data: data7, borderColor: 'rgb(0, 0, 255)', tension: 0.1 } ] }, options: { scales: { x: { type: 'linear', position: 'bottom' }, y: { min: 0, max: 100 } } } }); let counter = 0; function updateData() { fetch('/updateData') .then(response => { if (!response.ok) { throw new Error('响应异常，请重试！！！'); } return response.text(); }) .then(dataFromServer => { var jsonObject = JSON.parse(dataFromServer); data1.push(parseFloat(jsonObject.MQ2)); data2.push(parseFloat(jsonObject.MQ135)); data3.push(parseFloat(jsonObject.DS18B20)); data4.push(parseFloat(jsonObject.DHT11_TEMP)); data5.push(parseFloat(jsonObject.DHT11_HUMI)); data6.push(parseFloat(jsonObject.SR602)); data7.push(parseFloat(jsonObject.SW420)); var w = document.getElementById('windowStatus'); var d = document.getElementById('doorStatus'); var s = jsonObject.DoorStatus; if (s == \"00\" || s == \"01\" || s == \"02\") { w.innerHTML = \"<span style='color: rgb(0, 68, 255);'>已关闭</span>\"; } else if (s == \"10\" || s == \"11\" || s == \"12\") { w.innerHTML = \"<span style='color: rgb(255, 154, 0);'>大开中</span>\"; } else if (s == \"20\" || s == \"21\" || s == \"22\") { w.innerHTML = \"<span style='color: rgb(0, 255, 171);'>半开中</span>\"; } if (s == \"10\" || s == \"20\" || s == \"00\") { d.innerHTML = \"<span style='color: rgb(0, 68, 255);'>已关闭</span>\"; } else if (s == \"11\" || s == \"21\" || s == \"01\") { d.innerHTML = \"<span style='color: rgb(255, 154, 0);'>大开中</span>\"; } else if (s == \"12\" || s == \"22\" || s == \"02\") { d.innerHTML = \"<span style='color: rgb(0, 255, 171);'>半开中</span>\"; } var m = document.getElementById('modeStatus'); switch (parseInt(jsonObject.pattern)) { case 0: m.innerHTML = \"<span style='color: rgb(255, 212, 0);'>自动模式</span>\"; break; case 1: m.innerHTML = \"<span style='color: rgb(0, 161, 255);'>智能模式</span>\"; break; case 2: m.innerHTML = \"<span style='color: rgb(255, 68, 0);'>严防模式</span>\"; break; default: m.innerHTML = \"<span style='color: rgb(255, 0, 229);'>更新失败！</span>\"; } myChart.data.labels.push(counter++); myChart.update(); if (myChart.data.labels.length > 30) { myChart.data.labels.shift(); data.shift(); } }) .catch(error => { console.error('Error:', error); layer.msg('网络响应异常，亲，请你检查设备是否在线！（绿灯亮为在线）'); }); } intervalId = setInterval(updateData, 3000); function WINO() { fetch('/btn1') .then(response => { if (!response.ok) { throw new Error('响应异常，请重试！！！'); } return response.text(); }) .then(message => { layer.msg('尊敬的主人: ' + message); }) .catch(error => { console.error('Error:', error); layer.msg('网络响应异常，亲，请你检查设备是否在线！（绿灯亮为在线）'); }); } function WINC() { fetch('/btn2') .then(response => { if (!response.ok) { throw new Error('响应异常，请重试！！！'); } return response.text(); }) .then(message => { layer.msg('亲爱的主人: ' + message); }) .catch(error => { console.error('Error:', error); layer.msg('网络响应异常，亲，请你检查设备是否在线！（绿灯亮为在线）', { time: 0, btn: ['知道了'] }); }); } function WAYO() { fetch('/btn3') .then(response => { if (!response.ok) { throw new Error('响应异常，请重试！！！'); } return response.text(); }) .then(message => { layer.msg('尊敬的主人: ' + message); }) .catch(error => { console.error('Error:', error); layer.msg('网络响应异常，亲，请你检查设备是否在线！（绿灯亮为在线）'); }); } function WAYC() { fetch('/btn4') .then(response => { if (!response.ok) { throw new Error('响应异常，请重试！！！'); } return response.text(); }) .then(message => { layer.msg('亲爱的主人: ' + message); }) .catch(error => { console.error('Error:', error); layer.msg('网络响应异常，亲，请你检查设备是否在线！（绿灯亮为在线）', { time: 0, btn: ['知道了'] }); }); } function changePattern() { fetch('/btn5?pattern=' + document.getElementById('PatternID').value) .then(response => { if (!response.ok) { throw new Error('响应异常，请重试！！！'); } return response.text(); }) .then(message => { layer.msg('敬爱的主人: ' + message); }) .catch(error => { console.error('Error:', error); layer.msg('网络响应异常，亲，请你检查设备是否在线！（绿灯亮为在线）', { time: 0, btn: ['知道了'] }); }); } </script>";
html += "</body>";
html += "</html>";
  server.send(200, "text/html", html);
}
// 更新传感器数据,并对数据做处理(0~100)！
void ESPWebServerWrapper::handleUpdateData() {
  StaticJsonDocument<256> doc;  // 定义一个 JSON 文档，容量为 256 字节
  //MQ2、MQ135、DS18B20、DHT11_TEMP、DHT11_HUMI、SR602、SW420、门窗开关状态、模式状态值
  doc["MQ2"] = (analogRead(MQ2) * 100) / 4096;
  doc["MQ135"] = (analogRead(MQ135) * 100) / 4096;
  doc["DS18B20"] = olinemyDS18B20.Get_temp();
  olinemyDHT11.DHT11_Read();	
  doc["DHT11_TEMP"] = olinemyDHT11.TEM_Buffer_Int;
  doc["DHT11_HUMI"] = olinemyDHT11.HUMI_Buffer_Int;
  doc["SR602"] = digitalRead(SR602) * 100;
  doc["SW420"] = (analogRead(SW420)*100)/4096;
  olineWINorWAYstatusFun(true);
  olineWINorWAYstatusFun(false);
  doc["DoorStatus"] = String(olineWINstatus) + String(olineWAYstatus);
  doc["pattern"] = moduleStatus;
  // 将 JSON 文档转换为字符串
  String apiString;
  serializeJson(doc, apiString);
  Serial.println(apiString);  // 打印生成的 API 字符串
  server.sendHeader("Content-Type", "text/html; charset=UTF-8");
  server.send(200, "text/plain", apiString);
}
//***********************按钮操作实现***********************************
// 开窗
void ESPWebServerWrapper::buttonOne() {
  String message = "你的窗户已是打开状态！(●'◡'●)";
  if (!digitalRead(WINO) && !btnWIN) {
    olineWINFun(1);
    message = "执行成功！窗户打开中...";
    btnWIN = true;
  }else if(btnWIN){
    olineWINFun(2);
    message = "暂停成功！";
    btnWIN = false;
  }
  server.sendHeader("Content-Type", "text/html; charset=UTF-8");
  server.send(200, "text/plain", message);
}
// 关窗
void ESPWebServerWrapper::buttonTwo() {
  String message = "你的窗户已是关闭状态！(●'◡'●)";
  if (!digitalRead(WINO) && !btnWIN) {
    olineWINFun(0);
    message = "执行成功！窗户关闭中...";
    btnWIN = true;
  }else if(btnWIN){
    olineWINFun(2);
    message = "暂停成功！";
    btnWIN = false;
  }
  server.sendHeader("Content-Type", "text/html; charset=UTF-8");
  server.send(200, "text/plain", message);
}
//开门
void ESPWebServerWrapper::buttonThree() {
  String message = "你的门已是打开状态！(●'◡'●)";
  if (!digitalRead(WINO) && !btnWAY) {
    olineWAYFun(1);
    message = "执行成功！门打开中...";
    btnWAY = true;
  }else if(btnWAY){
    olineWAYFun(2);
    message = "暂停成功！";
    btnWAY = false;
  }
  server.sendHeader("Content-Type", "text/html; charset=UTF-8");
  server.send(200, "text/plain", message);
}
//关门
void ESPWebServerWrapper::buttonFour() {
  String message = "你的门已是关闭状态！(●'◡'●)";
  if (!digitalRead(WINO)&& !btnWAY) {
    olineWAYFun(0);
    message = "执行成功！门关闭中...";
    btnWAY = true;
  }else if(btnWAY){
    olineWAYFun(2);
    message = "暂停成功！";
    btnWAY = false;
  }
  server.sendHeader("Content-Type", "text/html; charset=UTF-8");
  server.send(200, "text/plain", message);
}
//模式切换同步
void ESPWebServerWrapper::buttonFive() {
  String message = "Sorry-操作失败，请重试！";
  if (server.hasArg("pattern")) {
    //获取到参数，则更新数据和同步锁打开
    moduleStatus = server.arg("pattern").toInt();
    haveOder = true;
    message = "模式切换成功！";
  }
  server.sendHeader("Content-Type", "text/html; charset=UTF-8");
  server.send(200, "text/plain", message);
}
// *****************下边是Client的实现方法*****************连接判断、响应异常、超时。记录和触发errorCount++
String ESPWebServerWrapper::sendGetRequest(const char* url) {
  // Use HTTPS
  HTTPClient http;

  // Create a WiFiClient object to use with HTTPClient
  WiFiClient client;

  // Configure the HTTPClient
  http.begin(client, url);
  // 设置超时时间为 500 毫秒
  http.setTimeout(500);
  // Send GET request
  int httpCode = http.GET();
  // Check for a successful request
  if (httpCode == 200) {
    Serial.printf("HTTP response code: %d\n", httpCode);
    // Parse and return the response body
    return http.getString();
  } else {
    Serial.printf("HTTP request failed with error code: %d\n", httpCode);
    // Close the connection
    http.end();
    return String();
  }
}
String ESPWebServerWrapper::sendPostRequest(const char* serverAddress, const char* endpoint, const String& postData) {
  // Create WiFiClient object
  WiFiClient client;

  // Create HTTPClient object
  HTTPClient http;

  // // Specify the target server and port
  // String APIurl = serverAddress + endpoint;
  http.begin(client, String(serverAddress) + endpoint);
  // 设置超时时间为 500 毫秒
  http.setTimeout(500);
  // Set the content type header
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  // Send POST request and get the response
  int httpCode = http.POST(postData);

  // Check for a successful request
  if (httpCode == 200) {
    Serial.printf("HTTP status code: %d\n", httpCode);

    // Return the response payload
    return http.getString();
  } else {
    Serial.printf("HTTP request failed, error: %s\n", http.errorToString(httpCode).c_str());
    // End the request
    http.end();
    return String();
  }
}
//*************控制方法****************
//窗户的控制方法
void ESPWebServerWrapper::olineWINFun(int r) {
  if (r == 0 && !digitalRead(WINC)) {
    analogWrite(MWIN0, olineMWINV);  // 0-255范围，占空比为200/255
    return;
  }
  if (r == 1 && !digitalRead(WINO)) {
    analogWrite(MWIN1, olineMWINV);  // 0-255范围，占空比为200/255
    return;
  }
  analogWrite(MWIN0, 255);
  analogWrite(MWIN1, 255);
}
//门的控制方法
void ESPWebServerWrapper::olineWAYFun(int r) {
  if (r == 0 && !digitalRead(WAYC)) {
    analogWrite(MWAY0, olineMWAYV);  // 0-255范围，占空比为200/255
    return;
  }
  if (r == 1 && !digitalRead(WAYO)) {
    analogWrite(MWAY1, olineMWAYV);  // 0-255范围，占空比为200/255
    return;
  }
  analogWrite(MWAY0, 255);
  analogWrite(MWAY1, 255);
}
//**********一些通用方法****************
void ESPWebServerWrapper::olineWINorWAYstatusFun(bool w) {
  if (w) {
    //w=true则是识别窗户状态
    if (digitalRead(WINO)) {
      olineWINstatus = 1;
    } else if (digitalRead(WINC)) {
      olineWINstatus = 0;
    } else {
      olineWINstatus = 2;
    }
  } else {
    if (digitalRead(WAYO)) {
      olineWAYstatus = 1;
    } else if (digitalRead(WAYC)) {
      olineWAYstatus = 0;
    } else {
      olineWAYstatus = 2;
    }
  }
}