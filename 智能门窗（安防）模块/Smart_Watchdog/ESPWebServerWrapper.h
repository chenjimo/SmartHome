#include "Server.h"
// ESPWebServerWrapper.h
#ifndef ESPWEBSERVERWRAPPER_H
#define ESPWEBSERVERWRAPPER_H

#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>


class ESPWebServerWrapper {
public:
  ESPWebServerWrapper(const char* ssid, const char* password);
  void begin();
  void handleClient();
  void olineWINFun(int r);    //窗户的控制方法
  void olineWAYFun(int r);    //门的控制方法
  void olineWINorWAYstatusFun(bool status); //门窗判断方法

  String sendGetRequest(const char* url);
  String sendPostRequest(const char* serverAddress, const char* endpoint, const String& postData);
  int moduleStatus;  // 设置模式状态0自动、1智能、2严防。
  bool haveOder;     //给命令上一道开关锁

private:
  const char* ssid;
  const char* password;

  WebServer server;

  void handleRoot();
  void handleUpdateData();
  void buttonOne();
  void buttonTwo();
  void buttonThree();
  void buttonFour();
  void buttonFive();
};

#endif  // ESPWEBSERVERWRAPPER_H
