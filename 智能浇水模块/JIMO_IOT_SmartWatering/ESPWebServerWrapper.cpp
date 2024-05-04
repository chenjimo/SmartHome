// ESPWebServerWrapper.cpp
#include "ESPWebServerWrapper.h"
#define LED1 D5 //红二极管灯泡正极
#define LED2 D6 //绿二极管灯泡正极
#define Sensor A0 //土壤湿度传感器
#define water D0 //水位传感器
const char *hostname = "Smart-Watering";

ESPWebServerWrapper::ESPWebServerWrapper(const char *ssid, const char *password) : ssid(ssid), password(password), server(80),watering(0) {}

void ESPWebServerWrapper::begin() {
  WiFi.hostname(hostname);
  WiFi.begin(ssid, password);
  int t = 20;
  while (WiFi.status() != WL_CONNECTED && t > 0) {//最多等待10秒则放弃连接
    delay(500);
    t--;
  }
  //注入映射
  server.on("/", HTTP_GET, [this]() { this->handleRoot();});
  server.on("/updateData", HTTP_GET, [this]() { this->handleUpdateData();});
  server.on("/btn1", HTTP_GET, [this]() { this->buttonOne(); });
  server.on("/btn2", HTTP_GET, [this]() { this->buttonTwo(); });

  server.begin();

}
//开启任务
void ESPWebServerWrapper::WateringBegin(){
  watering=1;
}
//完成任务，恢复状态
void ESPWebServerWrapper::WateringOK(){
  watering=0;
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
html += "  <title>Smart_Watering-家庭控制台</title>";
html += "    <!-- 引入Chart.js库 --> <script src=\"https://cdn.staticfile.org/Chart.js/3.9.1/chart.js\"></script>";
html += "    <!--layui.js-->    <script src=\"https://www.layuicdn.com/layui/layui.js\"></script>";
html += "    <!--layui.css-->    <script src=\"https://www.layuicdn.com/layui/css/layui.css\"></script>";
html += "</head>";
html += "<style>";
html += "  button {";
html += "    background-color: #4CAF50;";
html += "    color: white;";
html += "    border: none;";
html += "    padding: 10px 20px;";
html += "    text-align: center;";
html += "    text-decoration: none;";
html += "    display: inline-block;";
html += "    font-size: 16px;";
html += "    margin: 4px 2px;";
html += "    cursor: pointer;";
html += "    border-radius: 4px;";
html += "  }";
html += "</style>";
html += "";
html += "<body>";
html += "  <h1>实时土壤湿度数据</h1>";
html += "<a href=\"http://iot.jimo.fun/\" target=\"_blank\"><button >查看历史数据🤭，点我-去JIMO-IOT</button></a>";
html += "  <!-- 创建一个Canvas元素用于显示图表 -->";
html += "  <canvas id=\"myChart\" width=\"400\" height=\"200\"></canvas>";
html += "";
html += "  <!-- 添加按钮 -->";
html += "  <button onclick=\"waterHigh()\">点我-查看水仓水位</button>";
html += "  <button onclick=\"startWatering()\">点我-洒洒水呀(●'◡'●)！</button>";
html += "</br>";
html += "<a href=\"http://iot.jimo.fun/templates/controller/index.html\" target=\"_blank\"><button onclick=\"iotjimo()\" >需要定时和远程操作以及其他服务，点我-去JIMO-IOT控制台O(∩_∩)O</button></a>";
html += "";
html += "";
html += "  <script>";
html += "let intervalId,data=[];const ctx=document.getElementById(\"myChart\").getContext(\"2d\"),myChart=new Chart(ctx,{type:\"line\",data:{labels:[],datasets:[{label:\"土壤湿度(%RH)\",data:data,borderColor:\"rgb(75, 192, 192)\",tension:.1}]},options:{scales:{x:{type:\"linear\",position:\"bottom\"},y:{min:0,max:100}}}});let counter=0;function updateData(){fetch(\"/updateData\").then(t=>{if(!t.ok)throw new Error(\"响应异常，请重试！！！\");return t.text()}).then(t=>{data.push(parseInt(t)),myChart.data.labels.push(counter++),myChart.update(),myChart.data.labels.length>30&&(myChart.data.labels.shift(),data.shift())}).catch(t=>{console.error(\"Error:\",t),layer.msg(\"网络响应异常，亲，请你检查设备是否在线！（绿灯亮为在线）\")})}function startWatering(){fetch(\"/btn1\").then(t=>{if(!t.ok)throw new Error(\"响应异常，请重试！！！\");return t.text()}).then(t=>{layer.msg(\"尊敬的主人: \"+t)}).catch(t=>{console.error(\"Error:\",t),layer.msg(\"网络响应异常，亲，请你检查设备是否在线！（绿灯亮为在线）\")})}function waterHigh(){fetch(\"/btn2\").then(t=>{if(!t.ok)throw new Error(\"响应异常，请重试！！！\");return t.text()}).then(t=>{layer.msg(\"亲爱的主人: \"+t)}).catch(t=>{console.error(\"Error:\",t),layer.msg(\"网络响应异常，亲，请你检查设备是否在线！（绿灯亮为在线）\")})}intervalId=setInterval(updateData,1e3);";
html += "  </script>";
html += "</body>";
html += "</html>";
html += "";
  server.send(200, "text/html", html);
}
//更新湿度数据
void ESPWebServerWrapper::handleUpdateData() {
 // Read the analog Sensor value
  int sensorValue = (analogRead(Sensor)*100)/1024;
  // Convert the sensor value to String
  String data = String(sensorValue);
  server.sendHeader("Content-Type", "text/html; charset=UTF-8");
  server.send(200, "text/plain", data);
}
//执行异步浇水任务，注意需要判断水位和湿度阈值
void ESPWebServerWrapper::buttonOne() {
  String message = "抱歉，水量不足！请您及时补水后，再进行尝试(●'◡'●)";
  //通过实验数据建议保持在35~45即可。低于35不建议补水，超过53必须补水！
  if((analogRead(Sensor)*100)/1024 < 35){
    message = "您的植物已经有很多水份了，请你不要过度溺爱它会无法呼吸的！";
  }else{
    if(digitalRead(water)){
      watering = 1;//启动继电器异步任务
      message = "您的植物已经喝到甘甜的水了，它很感激！";
    }
  }
  server.sendHeader("Content-Type", "text/html; charset=UTF-8");
  server.send(200, "text/plain", message);
}
//查询水位
void ESPWebServerWrapper::buttonTwo() {
  String message = "你的水量不足！！！请您及时补水";
  if(digitalRead(water)){
    message = "你的水量充足！";
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