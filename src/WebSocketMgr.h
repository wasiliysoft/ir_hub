#ifndef WEB_SOCKET_MGR_H
#define WEB_SOCKET_MGR_H
#include <WebSocketsServer.h>

#ifndef IRHUB_CONFIG_H
#include "config.h"
#endif

class WebSocketMgr {
private:
  WebSocketsServer webSocket = WebSocketsServer(81);

public:
  void begin() { webSocket.begin(); }
  void update() { webSocket.loop(); }

  void notifyReceivedDataSetChanged(const IRData irData) {
    String lastIRCode = irData.hexcode;
    String lastIRProtocol = irData.protocol;
    String lastIRRaw = irData.raw;
    String jsonData = "{\"code\":\"" + lastIRCode + "\",\"protocol\":\"" +
                      lastIRProtocol + "\",\"raw\":\"" + lastIRRaw + "\"}";
    webSocket.broadcastTXT(jsonData);
  }
};

#endif