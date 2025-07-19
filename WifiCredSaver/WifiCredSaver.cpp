#include "WifiCredSaver.h"
#include "WString.h"
#include "debug.h"

WifiCredSaver::WifiCredSaver(const char* key) : m_keyName(key) 
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
}

bool WifiCredSaver::saveWifiCred(const char* name, const char* password)
{
    esp_err_t err;
    nvs_handle_t myPasswordHandle;
    nvs_handle_t myNameHandle;
    String wifiKeyName(this->m_keyName); 
    wifiKeyName += "name";
    String wifiKeyPassword(this->m_keyName); 
    wifiKeyPassword += "password";
    
    err = nvs_open(wifiKeyName.c_str(),
                    NVS_READWRITE,
                    &myNameHandle);

    if (err != ESP_OK) 
    {
        LOG_ERROR("(%s) opening NVS handle! [%s] 0",
            esp_err_to_name(err),
            wifiKeyName.c_str());
        
        return false;
    }
    err = nvs_open(wifiKeyPassword.c_str(),
                   NVS_READWRITE,
                   &myPasswordHandle);

    if (err != ESP_OK) 
    {
        LOG_ERROR("(%s) opening NVS handle! [%s] 0",
               esp_err_to_name(err),
               wifiKeyPassword.c_str());
        return false;
    }
    else 
    {
        err = nvs_set_str(myNameHandle,
                   wifiKeyName.c_str(),
                   name);
        if (ESP_OK != err)
        {
            LOG_ERROR("Error could not set the [%s] [%s]",  wifiKeyName.c_str(), esp_err_to_name(err));
            return false;
        }

        err = nvs_set_str(myPasswordHandle, 
                          wifiKeyPassword.c_str(), 
                          password);
        if (ESP_OK != err)
        {
            LOG_ERROR("Could not set the wifi password [%s]", esp_err_to_name(err));
            return false;
        }

        err = nvs_commit(myNameHandle);
        err |= nvs_commit(myPasswordHandle);

        LOG_INFO("%s Setting wifi password, key name [%s]", ((err != ESP_OK) ? "Failed!" : "Done"), this->m_keyName);

        nvs_close(myNameHandle);
        nvs_close(myPasswordHandle);
    } 

    return true;  
}

bool WifiCredSaver::retriveWifi(char nameBufffer[MAX_PASSWORD_LENGHT], char passwordBuffer[MAX_PASSWORD_LENGHT])
{
    esp_err_t err;
    nvs_handle_t myPasswordHandle;
    nvs_handle_t myNameHandle;
    String passwordKeyName(this->m_keyName); 
    passwordKeyName += "password";
    String WifiNameKeyName(this->m_keyName); 
    WifiNameKeyName += "name";
   
    err = nvs_open(WifiNameKeyName.c_str(),
                    NVS_READWRITE,
                    &myNameHandle);

    if (err != ESP_OK) 
    {
        LOG_ERROR("(%s) opening NVS handle! [%s] 0 \n", esp_err_to_name(err), WifiNameKeyName.c_str());
        return false;
    }
    err = nvs_open(passwordKeyName.c_str(),
                   NVS_READWRITE,
                   &myPasswordHandle);

    if (err != ESP_OK) 
    {
        LOG_ERROR("(%s) opening NVS handle! [%s] 0", esp_err_to_name(err), passwordKeyName.c_str());
        return false;
    }
else 
    {
        size_t nameLenght = MAX_PASSWORD_LENGHT;
        size_t passwordLenght = MAX_PASSWORD_LENGHT;
        

        err = nvs_get_str(myNameHandle,
                   WifiNameKeyName.c_str(),
                   nameBufffer,
                   &nameLenght);
        if (ESP_OK != err)
        {
            LOG_ERROR("Could not Get the [%s] [%s]",  WifiNameKeyName.c_str(), esp_err_to_name(err));
            return false;
        }
        
        err = nvs_get_str(myPasswordHandle, 
                          passwordKeyName.c_str(), 
                          passwordBuffer,
                          &passwordLenght);
        if (ESP_OK != err)
        {
            LOG_INFO("Error could not set the wifi password");
            return false;
        }

        LOG_INFO("%s Getting password for key name [%s]", ((err != ESP_OK) ? "Failed!" : "Done"), this->m_keyName);
    
        nvs_close(myNameHandle);
        nvs_close(myPasswordHandle);
        
        return true;
    } 
    
    return true;
}
