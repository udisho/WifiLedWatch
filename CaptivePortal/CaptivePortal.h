#ifndef _CAPTIVE_PORTA_
#define _CAPTIVE_PORTA_

#include <WiFi.h>
#include <AsyncTCP.h>  //https://github.com/me-no-dev/AsyncTCP using the latest dev version from @me-no-dev
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>	//https://github.com/me-no-dev/ESPAsyncWebServer using the latest dev version from @me-no-dev
#include <esp_wifi.h>			//Used for mpdu_rx_disable android workaround
#include <CaptivePortal.h>
#include "Common.h"

typedef void (*setUpWebServerFunc)(AsyncWebServer &server, const IPAddress &localIP);


class CaptivePortal
{
public:
    CaptivePortal(const char* configWifiName, setUpWebServerFunc func);
    //virtual void setUpWebserver(AsyncWebServer &server, const IPAddress &localIP);
    void run(const char* configWifiName);
    void processNextRequest(void){return dnsServer.processNextRequest();}
    AsyncWebServer server;
    bool hasConnectedClients() {
        return WiFi.softAPgetStationNum() > 0;
      }

private:
    DNSServer dnsServer;
    setUpWebServerFunc func;
    const IPAddress localIP;
    const IPAddress gatewayIP;
    const IPAddress subnetMask;
};





#endif