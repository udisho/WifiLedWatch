#pragma once
#include <Preferences.h>

class ConfigManager {
public:
    ConfigManager();
    void begin();

    // Setters
    void setColorIndex(int index);
    void setBrightness(int value);
    void setTimeFormat(int8_t timeFormat);

    // Getters
    int getColorIndex();
    int getBrightness();
    int8_t getTimeFormat();


private:
    Preferences preferences;
};

extern ConfigManager gCfgMgr;

