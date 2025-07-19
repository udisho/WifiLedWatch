#include <CaptivePortal.h>
#include "debug.h"
//namespace CaptivePortal
//{


void startSoftAccessPoint(const char *ssid, const char *password, const IPAddress &localIP, const IPAddress &gatewayIP) 
{
    #define MAX_CLIENTS 2
    #define WIFI_CHANNEL 6

    WiFi.mode(WIFI_MODE_AP);

    vTaskDelay(1000 / portTICK_PERIOD_MS);
    
    const IPAddress subnetMask(255, 255, 255, 0);
    WiFi.softAPConfig(localIP, gatewayIP, subnetMask);
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    if (!WiFi.softAP(ssid, password, WIFI_CHANNEL, 0, MAX_CLIENTS)) {
        LOG_ERROR("Failed to start SoftAP!");
    } else {
        LOG_INFO("SoftAP started.", "AP IP address[%s]",  WiFi.softAPIP().toString().c_str());
    }

    
    vTaskDelay(100 / portTICK_PERIOD_MS);
}



CaptivePortal::CaptivePortal(const char* configWifiName, setUpWebServerFunc setUpFunc) :
                             server(80), localIP(4, 3, 2, 1), gatewayIP(4, 3, 2, 1), subnetMask(255, 255, 255, 0)
{
    LOG_INFO("Setting AP (Access Point)…");
    vTaskDelay(200 / portTICK_PERIOD_MS);
    

    vTaskDelay(1000 / portTICK_PERIOD_MS);
    this->func = setUpFunc;
        
    LOG_INFO("\nCaptive Test, V0.5.0 compiled " __DATE__ " " __TIME__ " by CD_FER" "%s-%d\n\r", ESP.getChipModel(), ESP.getChipRevision());  //__DATE__ is provided by the platformio ide
   // LOG_INFO();
    
}
//}// CaptivePortal


void CaptivePortal::run(const char* configWifiName)
{
    vTaskDelay(2000 / portTICK_PERIOD_MS);  // לוודא שהמערכת מוכנה
    startSoftAccessPoint(configWifiName, NULL, this->localIP, this->gatewayIP);
    int wifiSetRet = esp_wifi_set_max_tx_power(40);
    if (ESP_OK != wifiSetRet)
    {
        LOG_ERROR("Couldnt set max power to wifi error [%d]", wifiSetRet);
    }

    this->dnsServer.setTTL(3600);
    this->dnsServer.start(53, "*", localIP);  // ← הזזה לפה

    func(this->server, this->localIP);
    server.begin();

    LOG_INFO("Startup Time: [%d]", millis());
}
