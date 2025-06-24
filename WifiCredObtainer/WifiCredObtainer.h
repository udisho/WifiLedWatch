#ifndef _WIF_CRED_OBTAINER_
#define _WIF_CRED_OBTAINER_
#include <WiFi.h>
#include <AsyncTCP.h>  //https://github.com/me-no-dev/AsyncTCP using the latest dev version from @me-no-dev
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>	//https://github.com/me-no-dev/ESPAsyncWebServer using the latest dev version from @me-no-dev
#include <esp_wifi.h>			//Used for mpdu_rx_disable android workaround

class WifiCredObtainer {
public:
    WifiCredObtainer(const char* configWifiName);
    ~WifiCredObtainer();
    void run(String& wifiName, String& password, int timeout);

private:
  AsyncWebServer server;
  DNSServer dnsServer;
};


#endif