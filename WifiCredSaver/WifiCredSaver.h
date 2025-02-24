#ifndef WIFI_CRED_SAVER_H
#define WIFI_CRED_SAVER_H
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"
// #include "/home/udi/.arduino15/packages/esp32/hardware/esp32/2.0.17/tools/sdk/esp32/include/nvs_flash/include/nvs_flash.h"
// #include "/home/udi/.arduino15/packages/esp32/hardware/esp32/2.0.17/tools/sdk/esp32/include/nvs_flash/include/nvs.h"
//#include <iostream>
#define MAX_PASSWORD_LENGHT 60


class WifiCredSaver
{
public:
    WifiCredSaver(const char*);
    //~WifiCredSaver();

    bool saveWifiCred(const char* name, const char* password);
    bool retriveWifi(char nameBufffer[MAX_PASSWORD_LENGHT], char passwordBuffer[MAX_PASSWORD_LENGHT]);


private:
    const char *m_keyName;
};




#endif // WIFI_CRED_SAVER_H