// ESP32HTTPUpdateServer.h
#ifndef ESP8266
#ifndef __HTTP_UPDATE_SERVER_H
#define __HTTP_UPDATE_SERVER_H

#include <Update.h>
#include <WebServer.h>

namespace esp32httpupdateserver_ns {
class ESP32HTTPUpdateServer {
public:
  ESP32HTTPUpdateServer(bool serial_debug = false);

  void setup(WebServer* server) {
    setup(server, String(), String());
  }

  void setup(WebServer* server, const String& path) {
    setup(server, path, String(), String());
  }

  void setup(WebServer* server, const String& username, const String& password) {
    setup(server, "/update", username, password);
  }

  void setup(WebServer* server, const String& path, const String& username, const String& password);

  void updateCredentials(const String& username, const String& password) {
    _username = username;
    _password = password;
  }

protected:
  void _setUpdaterError();

private:
  bool _serial_output;
  WebServer* _server;
  String _username;
  String _password;
  bool _authenticated;
  String _updaterError;
};
};  // namespace esp32httpupdateserver_ns
#endif
#endif