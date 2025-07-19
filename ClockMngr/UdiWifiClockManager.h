#include <WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include "GenericDisplay.h"
#include "SevenSegDigit.h"
#include "Common.h"
#include "debug.h"



// helper declartion
static void printIsDST(int day, int weekDay, int hour);
static bool hasLastSunday2AMOfOctoberYetPassed(int day, int weekDay, int hour);
static bool hasLastFriday2AmOfMarchPassed(int day, int weekDay, int hour);
static bool isDST();
static int getLedsFormatted(NTPClient* time, int8_t timeFormat);

////////// global varaibles /////////////////////////////////
const long standardTimeOffset = 7200;  // Change this for your standard time
const long dstOffset = 3600;           // DST offset in seconds
long currentOffset = standardTimeOffset;

WiFiUDP *udp = NULL;
int last4digit = 0;
int lastTimeUpdate = 0;
int lastDSTTimeUpdate = 0;
bool isDst = false;
NTPClient *timeClient;
char wifiNameBuff[MAX_PASSWORD_LENGHT];
char wifiPasswordBuff[MAX_PASSWORD_LENGHT];
LedDigiDispaly<4,4>* gledDisplay;
/////////////////////////////////////////////////////////////
void initLedDisplay(void)
{
    int i = 0;
    gledDisplay = new LedDigiDispaly<4,4>;
    gGenericDisplay = gledDisplay;
    if (!gledDisplay) {
        LOG_ERROR("Failed to allocate memory for LedDigiDispaly");
        return;
    }
    for (i = 0; i < 10; ++i)
    {
        int numToShow = 1111 * i;
        gledDisplay->ShowDigits(numToShow);
        LOG_INFO("printing digit: [%u]", numToShow);
        vTaskDelay(250 / portTICK_PERIOD_MS);
    }
}

void initClock(){
    
    initLedDisplay();
    udp = new WiFiUDP;
    timeClient = new NTPClient(*udp, "pool.ntp.org", 0, 60000);  // Offset set to 0, will manage it manually
    timeClient->begin();
    timeClient->update();
    while (timeClient->isTimeSet() != true)
    {
        if (true != timeClient->update())
        {
            LOG_INFO("update time was success time is [%s]", timeClient->getFormattedTime().c_str());
        }
        vTaskDelay(500 / portTICK_PERIOD_MS);

    }
    
    isDst = isDST();
    currentOffset = isDST() ? (standardTimeOffset + dstOffset) : standardTimeOffset;
    timeClient->setTimeOffset(currentOffset);
    LOG_INFO("Is DST is [%s]", isDst ? "true" : "false");
    LOG_INFO("Init done!");
}

int failedToConnectInARow = 0;

bool connectToWifi(const char *name, const char *password, int numOfTries, bool isFirstTime) 
{
    int i = 0;
    //if(isFirstTime == true)
    {
        WiFi.mode(WIFI_MODE_STA);
        if (WL_CONNECTED != WiFi.begin(name, password))
        {
            LOG_WARN("not conneced to WIFI yet waiting " );
        }

        while (WL_CONNECTED != WiFi.status() && numOfTries > i)
        {
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            ++i;
        }

        if (true != WiFi.mode(WIFI_AP_STA))
        {
            LOG_ERROR("couldnt change mode to AP && timeclient try again before REBOOTING");
            vTaskDelay(2000 / portTICK_PERIOD_MS);
            
            if (true != WiFi.mode(WIFI_AP_STA))
            {
                LOG_ERROR("couldnt change mode to AP && timeclient REBOOTING ");
                ESP.restart();
            }
        }
        else
        {
            LOG_INFO("Mode set to AP && timeclient");
        }
    }
    
    i = 0;
    
    while (WiFi.status() != WL_CONNECTED && numOfTries > i)
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        LOG_INFO("Connecting to WiFi num [%d] wifi name [%s]", i, name);
        ++i;
    }
    numOfTries = 0;

    if (WiFi.status() == WL_CONNECTED) {
        LOG_INFO("Connected to WiFi");
        return true;
    } else {
        LOG_ERROR("Could not connect to WiFi");
        return false;
    }
}
static void printUptime(void)
{
    uint64_t uptimeMicros = esp_timer_get_time();  
    uint64_t uptimeSeconds = uptimeMicros / 1000000;
    
    uint64_t days = uptimeSeconds / 86400;
    uint64_t hours = (uptimeSeconds % 86400) / 3600;
    uint64_t minutes = (uptimeSeconds % 3600) / 60;
    uint64_t seconds = uptimeSeconds % 60;
    
    LOG_INFO("Uptime: %llu days, %llu hours, %llu minutes, %llu seconds", days, hours, minutes, seconds);
    
    uint8_t temp = temperatureRead();  // Gives raw internal temp sensor
    LOG_INFO("Internal temp: %d C\n", temp);
}    

void updateTime(int currMilis) 
{
    // Update the NTP client
    if (currMilis / TIME_UPDATE_INTERVAL > lastTimeUpdate)
    {
        if (false == connectToWifi(wifiNameBuff, wifiPasswordBuff, 10, false))
        {        
            LOG_ERROR("Could not connect to wifi [%d] time in a row", failedToConnectInARow);
            failedToConnectInARow++;
            if(failedToConnectInARow >= NUM_OF_TIME_CONNECTING_TO_WIFI_BEFORE_REBOOTING)
            {
                LOG_ERROR("Could not connect to wifi [%d] REBOOTING!!", failedToConnectInARow);
                ESP.restart();
            }
        }

        if (false == timeClient->update())
        {
            LOG_WARN("update time was failed time is [%s]", timeClient->getFormattedTime().c_str());
        }
        else
        {
            LOG_INFO("update time was success time is [%s]", timeClient->getFormattedTime().c_str());
            failedToConnectInARow = 0;
        }

        lastTimeUpdate = currMilis / TIME_UPDATE_INTERVAL;
    }

    if (currMilis / DST_CHECK_INTERVAL > lastDSTTimeUpdate)
    {
        // Check if DST is in effect
        int currDst = isDST();
        if (currDst != isDst)
        {
            isDst = isDST();
            currentOffset = isDST() ? (standardTimeOffset + dstOffset) : standardTimeOffset;
            timeClient->setTimeOffset(currentOffset);
            LOG_INFO("Is DST changed to [%s]\n", isDst ? "true" : "false");
        }
        LOG_DEBUG("Is DST reamin the same [%s]", isDst ? "true" : "false");
        lastDSTTimeUpdate = currMilis / DST_CHECK_INTERVAL;
        printUptime();
    }

    int time4Digit = getLedsFormatted(timeClient, gTimeFormat);
    
    if (last4digit != time4Digit) 
    {
        last4digit = time4Digit;
        LOG_INFO("printing time [%u]", time4Digit);
    }
    
    vTaskDelay(5 / portTICK_PERIOD_MS);

    gledDisplay->ShowDigits(last4digit);
}


void changeClockColor()
{
//     gledDisplay->ChangeColor(color);
}

static bool hasLastFriday2AmOfMarchPassed(int day, int weekDay, int hour) {
    if (day < 25) {
        return false;
    }
    switch (weekDay) {
        case 0:   // Sunday
        return day > 26;
        case 1:   // Monday
        return day > 27;
        case 2:   // Tuesday
        return day > 28;
        case 3:   // Wednesday
        return day > 29;
        case 4:   // Thursday           
        return day > 30; 
        case 5:   // Friday If it's Friday, check if it's 2 AM or later
        return hour >= 2;
        case 6:   // Saturday
        return day > 25;
    }
    return false;  // Default case, shouldn't reach here
}

static bool isDST() {
    // Get current time
    long epochTime = timeClient->getEpochTime();
  
    // Convert epoch to a struct tm
    struct tm* timeinfo;
    timeinfo = localtime(&epochTime);
  
    // Check for DST (example: March last Sunday to October last Sunday)
    int month = timeinfo->tm_mon + 1;  // tm_mon is 0-11
    int day = timeinfo->tm_mday;
    int weekDay = timeClient->getDay();
    int hour = timeinfo->tm_hour;
  
    printIsDST(day, weekDay, hour);
  
    if ((month > 3 && month < 10) || (month == 3 && hasLastFriday2AmOfMarchPassed(day, weekDay, hour)) || (month == 10 && hasLastSunday2AMOfOctoberYetPassed(day, weekDay, hour))) {
      return true;  // DST is in effect
    }
    return false;  // Not in DST
}
  
//0  sunday 6 saturay
static bool hasLastSunday2AMOfOctoberYetPassed(int day, int weekDay, int hour) {
    if (day < 25) {
      return true;
    }
    switch (weekDay) {
      case 0:              // Sunday
        return hour < 2;   // If it's Sunday, check if it's 2 AM or later
      case 1:              // Monday
        return day == 25;  // Last Sunday would be the 25th or later
      case 2:              // Tuesday
        return day <= 26;  // Last Sunday would be the 25th or later
      case 3:              // Wednesday
        return day <= 27;  // Last Sunday would be the 25th or later
      case 4:              // Thursday
        return day <= 28;  // Last Sunday would be the 25th or later
      case 5:              // Friday
        return day <= 29;  // Last Sunday would be the 25th or later
      case 6:              // Saturday
        return day <= 30;  // Last Sunday would be the 25th or later
    }
    return false;  // Default case, shouldn't reach here
  }

  static void printIsDST(int day, int weekDay, int hour) {

    LOG_DEBUG("Is DST? day [%d] weekday [%d] hour [%d]", day, weekDay, hour);
  }

static int getLedsFormatted(NTPClient* time, int8_t timeFormat)
{
    int hours = time->getHours();
    int minutes = time->getMinutes();

    if (12 == timeFormat) // 12 time format
    {
        if (hours == 0) {
            hours = 12;  // Midnight -> 12 AM
        } else if (hours > 12) {
            hours -= 12; // e.g. 13:00 -> 1:00 PM
        }
    }

    return hours * 100 + minutes;
}