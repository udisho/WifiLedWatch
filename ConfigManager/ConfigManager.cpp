#include "ConfigManager.h"
ConfigManager gCfgMgr;

ConfigManager::ConfigManager() {}

void ConfigManager::begin() {
    preferences.begin("config", false); // namespace = "config", read/write = false
}

void ConfigManager::setColorIndex(int index) {
    preferences.putInt("color", index);
}

void ConfigManager::setBrightness(int value) {
    preferences.putInt("brightness", value);
}

void ConfigManager::setTimeFormat(int8_t timeFormat) {
    preferences.putInt("timeFormat", timeFormat);
}

int ConfigManager::getColorIndex() {
    return preferences.getInt("color", 0); // default 0
}

int ConfigManager::getBrightness() {
    return preferences.getInt("brightness", 128); // default 128
}

int8_t ConfigManager::getTimeFormat() {
    return preferences.getInt("timeFormat", true); // default true
}
