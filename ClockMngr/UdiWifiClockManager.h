#include <WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include "GenericDisplay.h"
#include "SevenSegDigit.h"

#define TIME_UPDATE_INTERVAL 120000
#define DST_CHECK_INTERVAL   240000

// helper declartion
static void printIsDST(int day, int weekDay, int hour);
static bool hasLastSunday2AMOfOctoberYetPassed(int day, int weekDay, int hour);
static bool hasLastFriday2AmOfMarchPassed(int day, int weekDay, int hour);
static bool isDST();

////////// global varaibles /////////////////////////////////
const long standardTimeOffset = 7200;  // Change this for your standard time
const long dstOffset = 3600;           // DST offset in seconds
long currentOffset = standardTimeOffset;
LedDigiDispaly<4,4>* gledDisplay;
WiFiUDP *udp = NULL;
int last4digit = 0;
int lastTimeUpdate = 0;
bool isDst = false;
NTPClient *timeClient;
/////////////////////////////////////////////////////////////
void initLedDisplay(void)
{
    int i = 0;
    gledDisplay = new LedDigiDispaly<4,4>;
    for (i = 0; i < 10; ++i)
    {
        gledDisplay->ShowDigits(1111 * i);
        delay(250);
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
            printf("update time was failed time is [%s]\n", timeClient->getFormattedTime().c_str());
        }
        delay(500);
    }
    
    isDst = isDST();
    currentOffset = isDST() ? (standardTimeOffset + dstOffset) : standardTimeOffset;
    timeClient->setTimeOffset(currentOffset);
    printf ("Is DST is [%h]\n", isDst);
    printf("Init done!\n");
}

void updateTime(int currMilis) 
{
    // Update the NTP client
    if (currMilis % TIME_UPDATE_INTERVAL != lastTimeUpdate)
    {
        if (true != timeClient->update())
        {
            printf("update time was failed time is [%s]\n", timeClient->getFormattedTime().c_str());
        }
        else
        {
            printf("update time was success time is [%s]\n", timeClient->getFormattedTime().c_str());
        }

        lastTimeUpdate = currMilis % TIME_UPDATE_INTERVAL;
    }
    
    if (currMilis % DST_CHECK_INTERVAL != lastTimeUpdate)
    {
        // Check if DST is in effect
        int currDst = isDST();
        if (currDst != isDst)
        {
            isDst = isDST();
            currentOffset = isDST() ? (standardTimeOffset + dstOffset) : standardTimeOffset;
            timeClient->setTimeOffset(currentOffset);
            printf ("Is DST Changed to [%b]\n", isDst);
        }
    }

    int time4Digit = timeClient->getHours() * 100 + timeClient->getMinutes();
    
    if (last4digit != time4Digit) 
    {
        last4digit = time4Digit;
        printf("printing time [%u] n", time4Digit);
    }
    
    delay(5);

    gledDisplay->ShowDigits(last4digit);
}

void changeClockColor()
{
    gledDisplay->ChangeColor();
}

bool connectToWifi(const char *name, const char *password, int numOfTries) {
    WiFi.begin(name, password);
    int i = 0;
    while (WiFi.status() != WL_CONNECTED && numOfTries > i) {
      delay(1000);
      Serial.print("Connecting to WiFi num [");
      Serial.print(numOfTries);
      Serial.print("] ");
      Serial.println(name);
      ++i;
    }
    numOfTries = 0;
  
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("Connected to WiFi");
      return true;
    } else {
      Serial.println("Could not connect to WiFi");
      return false;
    }
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
  
    //printIsDST(day, weekDay, hour);
  
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
    Serial.print("Is DST? day [");
    Serial.print(day);
    Serial.print("] weekDay [");
    Serial.print(weekDay);
    Serial.print("] hour [");
    Serial.print(hour);
    Serial.println("]");
  }