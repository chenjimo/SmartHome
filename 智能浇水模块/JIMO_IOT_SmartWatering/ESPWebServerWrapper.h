// ESPWebServerWrapper.h
#ifndef ESPWEBSERVERWRAPPER_H
#define ESPWEBSERVERWRAPPER_H

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>

class ESPWebServerWrapper {
public:
  ESPWebServerWrapper(const char *ssid, const char *password);
  void begin();
  void handleClient();
  void WateringOK();//完成任务后关闭
  void WateringBegin();//开始执行任务

  String sendGetRequest(const char* url);
  String sendPostRequest(const char* serverAddress, const char* endpoint, const String& postData);
  bool watering;//默认不浇水为0，启动后为1.

private:
  const char *ssid;
  const char *password;

  ESP8266WebServer server;

  void handleRoot();
  void handleUpdateData();
  void buttonOne();
  void buttonTwo();

};

#endif  // ESPWEBSERVERWRAPPER_H
