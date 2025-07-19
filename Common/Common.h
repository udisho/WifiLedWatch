#ifndef _COMMON_H_
#define _COMMON_H_
#include <crgb.h>
#include "GenericDisplay.h"


//#define UPDATE_INTERVAL_MSEC 4000
#define CHANGE_COLOR_INTERVAL_MSEC 1000000
#define SET_PASSWORD_TIMEOUT 1000000
#define TIME_UPDATE_INTERVAL 300000
#define DST_CHECK_INTERVAL   300000
#define MAX_PASSWORD_LENGHT 60
#define NUM_OF_TIME_CONNECTING_TO_WIFI_BEFORE_REBOOTING 25
// extern const CRGB colors[];
extern bool gHasConfigurationProvided;
extern int gSelectedBrightness;
extern CRGB gSelectedColor;
extern bool gUpdateTimeIs24Format;
extern int8_t gTimeFormat;
extern GenericDisplay* gGenericDisplay;

struct LedColorEntry {
    const char* name;
    CRGB color;
  };


const LedColorEntry ledColorOptions[] = {
    { "Red",       CRGB::Red },
    { "Green",     CRGB::Green },
    { "Blue",      CRGB::Blue },
    { "Yellow",    CRGB::Yellow },
    { "Cyan",      CRGB::Cyan },
    { "Magenta",   CRGB::Magenta },
    { "Orange",    CRGB::Orange },
    { "Purple",    CRGB::Purple },
    { "Aqua",      CRGB::Aqua },
    { "Lime",      CRGB::Lime },
    { "Indigo",    CRGB::Indigo },
    { "Teal",      CRGB::Teal },
    { "Turquoise", CRGB::Turquoise },
    { "Gold",      CRGB::Gold },
    { "Maroon",    CRGB::Maroon },
    { "Olive",     CRGB::Olive },
    { "Navy",      CRGB::Navy },
    { "SkyBlue",   CRGB::SkyBlue },
    { "Coral",     CRGB::Coral },
    { "Lavender",  CRGB::Lavender },
    { "Silver",    CRGB::Silver },
    { "Pink",      CRGB::Pink },
    { "White",     CRGB::White }
  };


constexpr size_t ledColorOptionsCount = sizeof(ledColorOptions) / sizeof(ledColorOptions[0]);

static CRGB getLedColorOptionByIndex(int index) {
    if (index < 0 || index >= (int)ledColorOptionsCount) {
        return CRGB::Black;  // default or error color
    }
    return ledColorOptions[index].color;
}

static const char* getLedColorOptionNameByColor(CRGB color) {
    for (size_t i = 0; i < ledColorOptionsCount; ++i) {
        if (ledColorOptions[i].color == color) {
            return ledColorOptions[i].name;
        }
    }
    return "Unknown";
}  
#endif // _COMMON_H_