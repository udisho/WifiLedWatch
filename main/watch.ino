#include "WifiCredSaver.h"
#include "UdiWifiClockManager.h"
#include "WifiCredObtainer.h"
#include "GenericDisplay.h"
#include "SevenSegDigit.h"

WifiCredSaver *credSaver;
char wifiNameBuff[MAX_PASSWORD_LENGHT];
char wifiPasswordBuff[MAX_PASSWORD_LENGHT];

#define UPDATE_INTERVAL_MSEC 4000
#define CHANGE_COLOR_INTERVAL_MSEC 1000000
#define SET_PASSWORD_TIMEOUT 1000000
#define LED_PIN 23

int lastMilisForClock;


void setup() {
  // init essential classes
  credSaver = new WifiCredSaver("W"); 
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  // try to connect to wifi 10 times with the password exist on the memory
  // retrive password if exist in memory
  //credSaver->saveWifiCred("GHHH", "gggg");
  credSaver->retriveWifi(wifiNameBuff, wifiPasswordBuff);

  // if (not connected) than open cred saver and reboot
  if (false == connectToWifi(wifiNameBuff, wifiPasswordBuff, 10))
  {
    // open credential obtainer than reboot
    String wifiName, password;
    WifiCredObtainer* obtainer = new WifiCredObtainer("The Amazing Watch");
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


  // start
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

void loop() {
  // 
  int currMillis = millis();
  if (hasThisMilisPassed(currMillis, UPDATE_INTERVAL_MSEC, &lastMilisForClock))
  {
    updateTime(currMillis);
  }
  if ((((currMillis + 500) / 1000) % 2) == 1)
  {
    digitalWrite(LED_PIN, HIGH);
  }
  else
  {
    digitalWrite(LED_PIN, LOW);
  }


}
