#ifndef _RUN_TIME_CONFIG_
#define _RUN_TIME_CONFIG_

#include "Common.h"
#include "SevenSegDigit.h"
#include "CaptivePortal.h"

//////////////////// GLOBAL VARIABLES /////////////////////
//LedColorEntry gLedColor;

class RunTimeConfigEngine : CaptivePortal
{
public:
    RunTimeConfigEngine(const char* configWifiName);
    void run(const char* configWifiName);
    static void setUpWebserver(AsyncWebServer &server, const IPAddress &localIP);
private:
};


#endif //_RUN_TIME_CONFIG_
