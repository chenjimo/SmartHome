// ESPWebServerWrapper.h
#ifndef ESPWEBSERVERWRAPPER_H
#define ESPWEBSERVERWRAPPER_H

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>

class ESPWebServerWrapper {
public:
  ESPWebServerWrapper(const char *ssid, const char *password);
  void begin();
  void handleClient();

  String sendGetRequest(const char* url);
  String sendPostRequest(const char* serverAddress, const char* endpoint, const String& postData);
  bool oderOpen;//是否有指令传达
  bool NFCOpen;//是否有指令传达
  //需要同步的参数信息
  int oraginalAngleOline;
  int patternOline;
  bool findSunOnOline;
  int oderMessageOline;//指令码

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
