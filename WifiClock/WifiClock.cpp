
#include <WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <WifiClock.h>


WiFiUDP udp;

// Set your timezone offset in seconds (e.g., for UTC+1, use 3600)
const long standardTimeOffset = 7200;  // Change this for your standard time
const long dstOffset = 3600;           // DST offset in seconds
long currentOffset = standardTimeOffset;

// Create an NTP client
NTPClient timeClient(udp, "pool.ntp.org", 0, 60000);  // Offset set to 0, will manage it manually
int last4digit = 0;
// for knowing if the wifi is connected on the machine
bool isConnectedToWifi = false;

char wifiNameBuf[60];
char wifiPasswordBuf[60];

void setGlobalVars(void) {
  Serial.begin(115200);
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

static bool hasLastThursday2AmOfMarchPassed(int day, int weekDay, int hour) {
  if (day < 25) {
    return false;
  }
  switch (weekDay) {
    case 0:  // Sunday
      return day > 27;
    case 1:  // Monday
      return day > 28;
    case 2:  // Tuesday
      return day > 29;
    case 3:  // Wednesday
      return day > 30;
    case 4:              // Thursday
      return hour >= 2;  // If it's Thursday, check if it's 2 AM or later
    case 5:              // Friday
      return day > 25;
    case 6:  // Saturday
      return day > 26;
  }
  return false;  // Default case, shouldn't reach here
}
static bool isDST() {
  // Get current time
  long epochTime = timeClient.getEpochTime();

  // Convert epoch to a struct tm
  struct tm* timeinfo;
  timeinfo = localtime(&epochTime);

  // Check for DST (example: March last Sunday to October last Sunday)
  int month = timeinfo->tm_mon + 1;  // tm_mon is 0-11
  int day = timeinfo->tm_mday;
  int weekDay = timeClient.getDay();
  int hour = timeinfo->tm_hour;

  printIsDST(day, weekDay, hour);

  if ((month > 3 && month < 10) || (month == 3 && hasLastThursday2AmOfMarchPassed(day, weekDay, hour)) || (month == 10 && hasLastSunday2AMOfOctoberYetPassed(day, weekDay, hour))) {
    return true;  // DST is in effect
  }
  return false;  // Not in DST
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

static void RunClock(LedDigiDispaly<4,4>* display) {
  while (1)
  {
        // Update the NTP client
    //Serial.println("before update");
    timeClient.update();
    //Serial.println("after update");

    // Check if DST is in effect
    currentOffset = isDST() ? (standardTimeOffset + dstOffset) : standardTimeOffset;

    // Get the time adjusted for DST
    timeClient.setTimeOffset(currentOffset);

    int time4Digit = timeClient.getHours() * 100 + timeClient.getMinutes();
    
    
    if (last4digit != time4Digit) {
        last4digit = time4Digit;
        display->ShowDigits(time4Digit);
        printf("printing time [%u] n", time4Digit);
    }
    // Print the current time
    //Serial.print("Current local time: ");
    //Serial.println(timeClient.getFormattedTime());

    // Wait 10 seconds before the next update
    delay(1000);
  }
}

void Run(const char *name, const char *password, int numOfTries, LedDigiDispaly<4,4>* display) {

  // set global variables and other stuff
  setGlobalVars();

  isConnectedToWifi = connectToWifi(name, password, numOfTries);
  //Serial.println("After Connect");
  delay(100);
  if (isConnectedToWifi) {
    // Start the NTP client
    timeClient.begin();
    //Serial.println("After imeClient.begin");
  
  RunClock(display);
  }
}





