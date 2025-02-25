#ifndef _WIF_CRED_OBTAINER_
#define _WIF_CRED_OBTAINER_
#include <WiFi.h>
class WifiCredObtainer {
public:
  WifiCredObtainer(const char* configWifiName);
  ~WifiCredObtainer();
  void run(String& wifiName, String& password, int timeout);

private:
  WiFiServer server;
};


#endif