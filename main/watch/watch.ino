#include "WifiCredSaver.h"
#include "UdiWifiClockManager.h"
#include "WifiCredObtainer.h"
#include "GenericDisplay.h"
#include "SevenSegDigit.h"
#include "RunTimeConfigureEngine.h"
#include "Common.h"
#include "ConfigManager.h"

WifiCredSaver *credSaver;


TaskHandle_t Task1Handle = NULL;
TaskHandle_t Task2Handle = NULL;

#define LED_PIN 23

int lastMilisForClock;

void retriveAndUpdateConfig(void)
{
    gSelectedBrightness = gCfgMgr.getBrightness();
    gSelectedColor = getLedColorOptionByIndex(gCfgMgr.getColorIndex());
    gTimeFormat = gCfgMgr.getTimeFormat();
    LOG_INFO("retriving config from boot:\n[%-20s][%-4d]\n[%-20s][%-4s]\n[%-20s][%-4d]", "Brightnes", gSelectedBrightness, "Color", getLedColorOptionNameByColor(gSelectedColor), "Time Format", gTimeFormat);
}

void setup() 
{
    Serial.begin(115200);
    delay(1500);
    esp_log_level_set(LOG_TAG, ESP_LOG_DEBUG);
    // init essential classes
    Serial.println("Serial is working");

    credSaver = new WifiCredSaver("W"); 
    delay(1500);
    gCfgMgr.begin();
    retriveAndUpdateConfig();
    pinMode(LED_PIN, OUTPUT);

    credSaver->retriveWifi(wifiNameBuff, wifiPasswordBuff);

  // if (not connected) than open cred saver and reboot
  if (false == connectToWifi(wifiNameBuff, wifiPasswordBuff, 10, true))
  {
    // open credential obtainer than reboot
    String wifiName, password;
    initLedDisplay();
    gledDisplay->ShowCONN();
    WifiCredObtainer* obtainer = new WifiCredObtainer("The Amazing Watch");
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    
    obtainer->run(wifiName,password, SET_PASSWORD_TIMEOUT);
    credSaver->saveWifiCred(wifiName.c_str(), password.c_str());
    ESP.restart();
  }
  else
  {
    // continue starting essentials
    initClock();
    lastMilisForClock = millis();
  }


  xTaskCreatePinnedToCore(
    ClockLoop,           
    "Task 1",
    8192,
    NULL,
    1,
    &Task1Handle,
    1 // Core 1
);

xTaskCreatePinnedToCore(
    configureCaptivePortal,
    "Task 2",
    8192,
    NULL,
    1,
    &Task2Handle,
    0 // Core 0
);
}

RunTimeConfigEngine *gConfigRunTime = NULL;

void configureCaptivePortal(void *closue)
{
    gConfigRunTime = new RunTimeConfigEngine("Watch Config Site");
    
    while (1)
    {
        gConfigRunTime->run("Watch Config Site");
        //printf("hello from Loop2\n");
        vTaskDelay(10000);
    }
}
static bool hasThisMilisPassed(int milis, int interval, int* lastMilis)
{
  if ((milis - *lastMilis) >=  interval)
  {
    *lastMilis = milis;
    return true;
  }
  return false;
}

void ClockLoop(void* closure) {
    (void)closure;
    while (1) 
    {
        int currMillis = millis();
        updateTime(currMillis);

        if ((((currMillis + 500) / 1000) % 2) == 1)
        {
            digitalWrite(LED_PIN, HIGH);
        }
        else
        {
            digitalWrite(LED_PIN, LOW);
        }
    }
}

void loop() {
    vTaskSuspend(NULL);
}
