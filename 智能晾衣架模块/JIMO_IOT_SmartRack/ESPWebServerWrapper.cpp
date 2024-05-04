// ESPWebServerWrapper.cpp
#include "ESPWebServerWrapper.h"

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
const char* hostname = "Smart-Rack";

ESPWebServerWrapper::ESPWebServerWrapper(const char* ssid, const char* password)
  : ssid(ssid), password(password), server(80), oderOpen(0), NFCOpen(0), oraginalAngleOline(90), patternOline(0), findSunOnOline(0), oderMessageOline(0) {}

void ESPWebServerWrapper::begin() {
  WiFi.hostname(hostname);
  WiFi.begin(ssid, password);
  int t = 20;
  while (WiFi.status() != WL_CONNECTED && t > 0) {  //最多等待10秒则放弃连接
    delay(500);
    t--;
  }
  //注入映射、数据更新、指令下达
  server.on("/", HTTP_GET, [this]() {
    this->handleRoot();
  });
  server.on("/updateData", HTTP_GET, [this]() {
    this->handleUpdateData();
  });
  server.on("/btn1", HTTP_GET, [this]() {
    this->buttonOne();
  });
  server.on("/btn2", HTTP_GET, [this]() {
    this->buttonTwo();
  });

  server.begin();
}

//********Web页面与API有关******
void ESPWebServerWrapper::handleClient() {
  server.handleClient();
}

void ESPWebServerWrapper::handleRoot() {
  //HTML内容**********************************
String html = "";
html += "<!DOCTYPE html>";
html += "<html lang=\"en\">";
html += "<head>";
html += "  <meta charset=\"UTF-8\">";
html += "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">";
html += "  <link rel=\"shortcut icon\" href=\"http://jimo.fun/img/my/jm.png\">";
html += "  <title>Smart_Rack-家庭控制台</title>";
html += "  <!-- 引入Chart.js库 -->";
html += "  <script src=\"https://cdn.staticfile.org/Chart.js/3.9.1/chart.js\"></script>";
html += "  <!--layui.js-->";
html += "  <script src=\"https://www.layuicdn.com/layui/layui.js\"></script>";
html += "  <!--layui.css-->";
html += "  <script src=\"https://www.layuicdn.com/layui/css/layui.css\"></script>";
html += "</head>";
html += "<style> button { background-color: #4CAF50; color: white; border: none; padding: 10px 20px; text-align: center; text-decoration: none; display: inline-block; font-size: 16px; margin: 4px 2px; cursor: pointer; border-radius: 4px; } body { font-family: Arial, sans-serif; text-align: center; margin-top: 50px; } .status { font-size: 20px; margin-top: 20px; font-weight: bold; color: rgb(255, 115, 0); } .modelV { font-size: 21px; font-weight: bold; color: #fc0404; } .status p:nth-child(2), .status p:nth-child(3) { display: inline-block; margin-left: 20px; } #WaterStatus, #SpinStatus { padding: 5px 10px; border-radius: 5px; background-color: #f0f0f0; color: #c903fa; } #oderMessage { font-size: 16px; font-weight: bold; color: #e31dfd; background-color: #a0c0c5; border-radius: 5px; padding: 5px; } .option1 { color: rgb(247, 203, 10); } .option2 { color: rgb(14, 255, 54); } .option3 { color: red; } </style>";
html += "<body>";
html += "  <h1>实时智能晾衣架状态监视</h1>";
html += "  <a href=\"http://iot.jimo.fun/\" target=\"_blank\"><button>查看历史数据🤭，点我-去JIMO-IOT</button></a><a";
html += "    href=\"http://iot.jimo.fun/templates/controller/index.html\" target=\"_blank\"><button";
html += "      onclick=\"iotjimo()\">需要定时和远程操作以及其他服务，点我-去JIMO-IOT控制台O(∩_∩)O</button></a>";
html += "  <!-- 创建一个Canvas元素用于显示图表 -->";
html += "  <canvas id=\"myChart\" width=\"400\" height=\"200\"></canvas>";
html += "  <!-- 当前门和窗的状态显示 -->";
html += "  <div>";
html += "    <div class=\"status\">";
html += "      <p class=\"modelV\">是否开启智能避雨模式: <span id=\"modeStatus\">正在获取...</span></p>";
html += "      <p>当前是否有雨: <span id=\"WaterStatus\">正在获取...</span></p>";
html += "      <p>当前智能晾晒模式: <span id=\"SpinStatus\">正在获取...</span></p>";
html += "    </div>";
html += "  </div>";
html += "  <!-- <button onclick=\"startWatering()\">点我-洒洒水呀(●'◡'●)！</button> -->";
html += "  </br>";
html += "  <label for=\"oderMessage\" class=\"modelV\">下达控制指令：</label>";
html += "  <select id=\"oderMessage\" class=\"modelV\">";
html += "    <option value=\"11\" class=\"option3\">收回衣架</option>";
html += "    <option value=\"12\" class=\"option1\">伸开衣架</option>";
html += "    <option value=\"13\" class=\"option2\">开启智能避雨</option>";
html += "    <option value=\"14\" class=\"option3\">关闭智能避雨</option>";
html += "    <option value=\"15\" class=\"option5\">寻光旋转</option>";
html += "    <option value=\"16\" class=\"option1\">反面旋转</option>";
html += "    <option value=\"17\" class=\"option3\">关闭旋转</option>";
html += "  </select>";
html += "  <button onclick=\"changePattern()\">点击下达(●'◡'●)！</button>";
html += "<script> let data1 = []; let data2 = []; let intervalId; const ctx = document.getElementById('myChart').getContext('2d'); const myChart = new Chart(ctx, { type: 'line', data: { labels: [], datasets: [ { label: '光感参数', data: data1, borderColor: 'rgb(252, 160, 0)', tension: 0.1 }, { label: '衣架角度', data: data2, borderColor: 'rgb(0, 255, 187)', tension: 0.1 } ] }, options: { scales: { x: { type: 'linear', position: 'bottom' }, y: { min: 0, max: 100 } } } }); let counter = 0; function updateData() { fetch('/updateData') .then(response => { if (!response.ok) { throw new Error('响应异常，请重试！！！'); } return response.text(); }) .then(dataFromServer => { var jsonObject = JSON.parse(dataFromServer); data1.push(parseInt(jsonObject.sunV)); data2.push(parseInt(jsonObject.SG90)); var w = document.getElementById('WaterStatus'); var s = document.getElementById('SpinStatus'); var spin = jsonObject.spinV; var water = jsonObject.waterV; if (spin == \"0\") { s.innerHTML = \"<span style='color: rgb(0, 68, 255);'>暂未开启智能晾晒</span>\"; } else if (spin == \"1\") { s.innerHTML = \"<span style='color: rgb(255, 154, 0);'>智能寻光晾晒中</span>\"; } else if (spin == \"2\") { s.innerHTML = \"<span style='color: rgb(0, 255, 171);'>智能反面晾晒中</span>\"; } if (water == \"1\") { w.innerHTML = \"<span style='color: rgb(0, 68, 255);'>暂未下雨</span>\"; } else if (water == \"0\") { w.innerHTML = \"<span style='color: rgb(255, 154, 0);'>下雨中</span>\"; } var m = document.getElementById('modeStatus'); switch (parseInt(jsonObject.smartWater)) { case 0: m.innerHTML = \"<span style='color: rgb(255, 212, 0);'>未开启</span>\"; break; case 1: m.innerHTML = \"<span style='color: rgb(0, 161, 255);'>已开启</span>\"; break; default: m.innerHTML = \"<span style='color: rgb(255, 0, 229);'>更新失败！</span>\"; } myChart.data.labels.push(counter++); myChart.update(); if (myChart.data.labels.length > 30) { myChart.data.labels.shift(); data.shift(); } }) .catch(error => { console.error('Error:', error); layer.msg('网络响应异常，亲，请你检查设备是否在线！（绿灯亮为在线）'); }); } intervalId = setInterval(updateData, 3000); function changePattern() { fetch('/btn1?oderMessage=' + document.getElementById('oderMessage').value) .then(response => { if (!response.ok) { throw new Error('响应异常，请重试！！！'); } return response.text(); }) .then(message => { layer.msg('敬爱的主人: ' + message); }) .catch(error => { console.error('Error:', error); layer.msg('网络响应异常，亲，请你检查设备是否在线！（绿灯亮为在线）', { time: 0, btn: ['知道了'] }); }); } </script>";
html += "</body>";
html += "</html>";
  server.send(200, "text/html", html);
}
//更新：雨滴信息、光感信息、SG90位置、旋转模式、智能避雨状态
void ESPWebServerWrapper::handleUpdateData() {
  //开启数据同步
  NFCOpen = true;
  StaticJsonDocument<256> doc;  // 定义一个 JSON 文档，容量为 256 字节
  doc["waterV"] = digitalRead(WATER_pin);
  doc["sunV"] = analogRead(Sun_pin) * 100 / 1024;
  doc["SG90"] = oraginalAngleOline - 90;
  int status = 0;
  if (patternOline == 1 || patternOline == 3 || patternOline == 5 || patternOline == 7) {
    if (findSunOnOline) {//智能寻光晾晒中
      status = 1;
    } else {//智能反面晾晒中
      status = 2;
    }
  } else {
    status = 0;
  }
  doc["spinV"] = status;
  if (patternOline == 4 || patternOline == 5 || patternOline == 6 || patternOline == 7) {
    status = 1;
  } else {
    status = 0;
  }
  String message = "";
  doc["smartWater"] = status;
  serializeJson(doc, message);
  server.sendHeader("Content-Type", "text/html; charset=UTF-8");
  server.send(200, "text/plain", message);
}
//执行异步任务同步线上模式的下达指令码相同
void ESPWebServerWrapper::buttonOne() {
  String message = "抱歉，执行失败，请再进行尝试(●'◡'●)";
  if (server.hasArg("oderMessage")) {
    //获取到参数，则更新数据和同步锁打开
    oderMessageOline = server.arg("oderMessage").toInt();
    oderOpen = true;
    message = "指令已下达成功！";
  }
  server.sendHeader("Content-Type", "text/html; charset=UTF-8");
  server.send(200, "text/plain", message);
}
//测试按钮
void ESPWebServerWrapper::buttonTwo() {
  String message = "这是一个测试按钮！！！";
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