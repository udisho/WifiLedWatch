#ifndef _WIF_CRED_OBTAINER_
#define _WIF_CRED_OBTAINER_
#include <CaptivePortal.h>

class WifiCredObtainer : CaptivePortal
{
public:
    WifiCredObtainer(const char* configWifiName);
    ~WifiCredObtainer();
    void run(String& wifiName, String& password, int timeout);
    static void setUpWebserver(AsyncWebServer &server, const IPAddress &localIP);
    
private:


};


#endif