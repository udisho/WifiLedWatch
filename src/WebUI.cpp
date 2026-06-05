#include "WebUI.h"
#include "LedDisplay.h"
#include "TimeManager.h"
#include "ConfigStore.h"
#include "WifiManager.h"
#include "Heartbeat.h"
#include <Preferences.h>
#include <WiFi.h>
#include <Arduino.h>
#include <Update.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

static int extractInt(const String& json, const char* key);
static String extractString(const String& json, const char* key);
static bool extractBool(const String& json, const char* key);

// Firmware magic marker — must exist in every valid NeoTick binary
#define OTA_MAGIC     "NEOTICK_FW_V3"
#define OTA_MAGIC_LEN 13
static const char NEOTICK_MAGIC[] __attribute__((used)) = OTA_MAGIC;
static const char OTA_PASSWORD[] = "neotick2024";

// OTA upload state
static bool otaMagicFound = false;
static bool otaPasswordOK = false;
static uint8_t otaTail[OTA_MAGIC_LEN];
static size_t otaTailLen = 0;

// === PWA icon (192x192 PNG: user-provided icon, palette-compressed) ===
static const uint8_t ICON_PNG[] PROGMEM = {
  0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
  0x00, 0x00, 0x00, 0xc0, 0x00, 0x00, 0x00, 0xc0, 0x08, 0x03, 0x00, 0x00, 0x00, 0x65, 0x02, 0x9c,
  0x35, 0x00, 0x00, 0x01, 0xfe, 0x50, 0x4c, 0x54, 0x45, 0x11, 0x14, 0x26, 0x09, 0x0a, 0x1d, 0x0b,
  0x0d, 0x21, 0xf9, 0xb6, 0x32, 0x26, 0x26, 0x36, 0x0f, 0x24, 0x37, 0x2f, 0x32, 0x48, 0xfe, 0xc3,
  0x34, 0x43, 0xdb, 0xea, 0x12, 0x29, 0x45, 0x2a, 0x2d, 0x43, 0xee, 0xac, 0x2e, 0x11, 0x11, 0x1d,
  0x3c, 0xda, 0xe9, 0x13, 0x35, 0x4a, 0x45, 0xe7, 0xf5, 0x1a, 0x55, 0x68, 0xcd, 0x98, 0x2f, 0x18,
  0x46, 0x56, 0xb5, 0x87, 0x2d, 0x2b, 0x39, 0x68, 0x73, 0x57, 0x28, 0x4c, 0x39, 0x24, 0x8c, 0x68,
  0x29, 0x4c, 0x78, 0xd1, 0x24, 0x58, 0x70, 0x2b, 0x76, 0x86, 0x2c, 0x46, 0x73, 0x45, 0xc9, 0xd5,
  0xd8, 0xa4, 0x30, 0x2f, 0x87, 0x95, 0xa7, 0x79, 0x2b, 0x65, 0x4b, 0x26, 0x23, 0x1c, 0x25, 0x3c,
  0xe5, 0xf2, 0x59, 0x44, 0x25, 0x99, 0x73, 0x2a, 0x35, 0x99, 0xa7, 0x28, 0x45, 0x59, 0x38, 0xa8,
  0xb4, 0x48, 0xf4, 0xfc, 0x4b, 0x66, 0xb7, 0x28, 0x65, 0x75, 0x37, 0xb8, 0xc6, 0x19, 0x47, 0x67,
  0x1d, 0x65, 0x77, 0x39, 0x4a, 0x85, 0x32, 0x24, 0x1d, 0x53, 0x86, 0xe5, 0x43, 0xb8, 0xf3, 0x43,
  0xc8, 0xf7, 0x46, 0xd4, 0xdd, 0x22, 0x6b, 0x81, 0x25, 0x1b, 0x1c, 0x44, 0x96, 0xe7, 0x39, 0xc7,
  0xd3, 0x7d, 0x61, 0x2a, 0x43, 0xa5, 0xee, 0x41, 0xba, 0xc8, 0x47, 0x86, 0xdb, 0xbc, 0x91, 0x2f,
  0xfe, 0xd4, 0x38, 0xc5, 0x8d, 0x2d, 0xf3, 0xb7, 0x40, 0x42, 0x53, 0x95, 0x3d, 0x32, 0x27, 0x38,
  0xb2, 0xbe, 0x45, 0x5a, 0xa3, 0x4c, 0x6b, 0xc2, 0x19, 0x3d, 0x61, 0xff, 0xc3, 0x41, 0x48, 0x35,
  0x1e, 0x1d, 0x73, 0x84, 0x20, 0x51, 0x5f, 0xff, 0xd5, 0x43, 0x1e, 0x6a, 0x82, 0x50, 0x7d, 0xe1,
  0x78, 0x56, 0x1c, 0x2f, 0x91, 0x9e, 0x3c, 0x5c, 0xa7, 0x82, 0x5a, 0x23, 0x40, 0x2e, 0x23, 0x3d,
  0xd3, 0xdd, 0x14, 0x1d, 0x41, 0x3a, 0x51, 0x93, 0x40, 0x4e, 0x88, 0x42, 0xb1, 0xbe, 0x0f, 0x30,
  0x3e, 0x40, 0xab, 0xb8, 0x3e, 0x64, 0xb4, 0xbe, 0x8c, 0x1c, 0x44, 0x2f, 0x1f, 0x95, 0x6a, 0x1f,
  0x25, 0x71, 0x7d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xee, 0x5b, 0x8a, 0xe1, 0x00, 0x00, 0x00, 0x80, 0x74,
  0x52, 0x4e, 0x53, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x09, 0x79, 0x94, 0x07, 0x00, 0x00, 0x11, 0xd2, 0x49, 0x44, 0x41, 0x54, 0x78,
  0xda, 0xe5, 0x9d, 0x07, 0x77, 0xdb, 0xb6, 0x16, 0x80, 0x81, 0x4b, 0xd1, 0xd4, 0xa0, 0x24, 0x6b,
  0x79, 0xc9, 0xf2, 0x56, 0x1d, 0xd9, 0x8d, 0xed, 0x36, 0x71, 0xdd, 0xa6, 0xd9, 0xc9, 0x4b, 0xfb,
  0xba, 0x77, 0xdf, 0xfc, 0xff, 0x7f, 0xe3, 0x01, 0x24, 0xc6, 0x05, 0x08, 0x50, 0xa4, 0x24, 0xd7,
  0xe2, 0x79, 0xc8, 0x69, 0xdc, 0xd8, 0x32, 0x79, 0x3f, 0xdc, 0x89, 0x41, 0x90, 0xd0, 0x8a, 0x37,
  0xb2, 0x0e, 0x42, 0x80, 0xfc, 0x02, 0x60, 0x7d, 0x13, 0x64, 0x5b, 0x6f, 0x00, 0x3f, 0x19, 0xcc,
  0x25, 0x20, 0x85, 0x2f, 0x25, 0xaf, 0xa8, 0xae, 0x8c, 0xfe, 0x25, 0x3f, 0xe1, 0xbe, 0x99, 0xfe,
  0x0e, 0x18, 0x8d, 0xb0, 0x3f, 0x40, 0xf2, 0x9a, 0xb8, 0x0b, 0xc9, 0x81, 0x98, 0x0b, 0x40, 0x48,
  0x58, 0x5b, 0xaa, 0x35, 0x8d, 0x7f, 0x64, 0xda, 0x46, 0xa6, 0x0d, 0x45, 0x1b, 0x8f, 0xc7, 0xc3,
  0xe1, 0x46, 0x93, 0x70, 0x11, 0x05, 0x43, 0x59, 0x00, 0x20, 0xb5, 0xda, 0xc6, 0x86, 0xb8, 0x0f,
  0xba, 0xa3, 0xbe, 0x6f, 0xd3, 0xd3, 0x90, 0xbc, 0xd9, 0xcf, 0x6d, 0xb8, 0xdb, 0x70, 0x88, 0x44,
  0x1f, 0x8f, 0xaf, 0x92, 0xf6, 0xfb, 0xef, 0x57, 0xe3, 0x61, 0x93, 0x28, 0xed, 0x95, 0x01, 0x20,
  0xb5, 0x44, 0xd8, 0xda, 0x3d, 0x36, 0x1f, 0xbf, 0x46, 0xe2, 0x20, 0xaf, 0x5f, 0xff, 0x3e, 0xe6,
  0x0c, 0x4e, 0x2d, 0x10, 0xbf, 0xf8, 0x1b, 0xa9, 0xf8, 0xe4, 0x61, 0x5b, 0xd8, 0x1c, 0x8e, 0x19,
  0xc2, 0xeb, 0xdf, 0x86, 0x44, 0x18, 0x52, 0x21, 0x80, 0x44, 0xfc, 0xe6, 0x43, 0x0b, 0x2f, 0x5b,
  0x63, 0x78, 0xf5, 0xfa, 0xdd, 0xec, 0xdd, 0x90, 0x38, 0x5c, 0xc1, 0x09, 0x00, 0x5c, 0xfc, 0xda,
  0xba, 0x88, 0x9f, 0xb4, 0x8d, 0xab, 0xd9, 0x6c, 0xf6, 0xae, 0x01, 0x19, 0x25, 0xb8, 0x00, 0xc8,
  0x3a, 0xf5, 0xbe, 0x6a, 0xc3, 0x77, 0x0c, 0xe1, 0x8a, 0xd8, 0x9e, 0xe0, 0x00, 0x48, 0xba, 0x9f,
  0xac, 0x5f, 0x6b, 0x8c, 0x67, 0xb3, 0xf7, 0xef, 0x7e, 0xb5, 0x08, 0x48, 0x65, 0xe4, 0x27, 0x21,
  0x69, 0xbe, 0x9e, 0xbd, 0x9f, 0x35, 0x4c, 0x02, 0x1b, 0x80, 0x34, 0xd7, 0x54, 0xfc, 0xb4, 0x8d,
  0x19, 0xc1, 0xd0, 0x20, 0x20, 0x59, 0xf3, 0x5f, 0x63, 0xf9, 0x99, 0x27, 0xcc, 0xde, 0xef, 0x0c,
  0xb1, 0x27, 0x9b, 0x00, 0xd0, 0x5c, 0x73, 0xf9, 0x09, 0x69, 0xce, 0x76, 0x0c, 0x02, 0x62, 0xdb,
  0xff, 0x9a, 0xcb, 0xcf, 0x7c, 0x99, 0x11, 0x34, 0x34, 0x01, 0xa9, 0x9a, 0xfc, 0x8c, 0x60, 0x67,
  0x67, 0xe7, 0x57, 0x45, 0x40, 0x4c, 0x07, 0xa8, 0x80, 0xfc, 0xcc, 0x0f, 0x76, 0xbe, 0x9c, 0xa5,
  0xb5, 0x36, 0x18, 0x00, 0xdc, 0x81, 0xab, 0x20, 0x7f, 0x48, 0xae, 0x6e, 0x6e, 0x7e, 0x83, 0x94,
  0x00, 0x10, 0x00, 0x73, 0xe0, 0xb0, 0x0a, 0x00, 0xa4, 0x41, 0x66, 0x37, 0x37, 0x43, 0x48, 0x95,
  0x40, 0x2a, 0x67, 0x40, 0x0c, 0x20, 0x6c, 0xec, 0xdc, 0xec, 0x10, 0x0b, 0xa0, 0x2a, 0x06, 0xc4,
  0x6d, 0xa8, 0xc1, 0x8d, 0x68, 0x9c, 0xaa, 0x80, 0xa0, 0x08, 0x14, 0x92, 0xca, 0x10, 0x34, 0x66,
  0x52, 0x05, 0xa4, 0x72, 0x06, 0x94, 0xfa, 0xf1, 0x70, 0xe7, 0x66, 0x4c, 0x39, 0x01, 0xd1, 0x0a,
  0x20, 0xa4, 0x4a, 0x04, 0x3f, 0xde, 0xbc, 0x27, 0x18, 0xa0, 0x59, 0x25, 0x05, 0xa4, 0xc9, 0xe0,
  0xbb, 0x24, 0x10, 0x11, 0x65, 0x41, 0xa4, 0x62, 0xed, 0x9f, 0x37, 0x33, 0xa2, 0x01, 0x6a, 0x55,
  0x03, 0x08, 0xc9, 0xf8, 0x66, 0xa7, 0xa1, 0x00, 0xaa, 0xe5, 0xc2, 0x09, 0x00, 0xcf, 0x05, 0x57,
  0x18, 0xa0, 0x62, 0x06, 0x14, 0x36, 0xc2, 0x1f, 0x53, 0x1b, 0xaa, 0xa6, 0x05, 0xf1, 0x64, 0x36,
  0xde, 0x49, 0x6c, 0x48, 0x00, 0xd4, 0xaa, 0xa7, 0x81, 0x61, 0x32, 0xb2, 0x21, 0x62, 0x20, 0x1c,
  0x12, 0x52, 0x39, 0x27, 0x98, 0xed, 0x8c, 0x25, 0x40, 0xf5, 0x82, 0x28, 0x23, 0xf8, 0xf5, 0xc7,
  0xef, 0x7e, 0x23, 0x55, 0x06, 0x20, 0x57, 0xdf, 0xbd, 0x53, 0x00, 0xb5, 0x4a, 0x02, 0xec, 0xcc,
  0x04, 0x40, 0xad, 0x9a, 0x00, 0xc3, 0x9d, 0x59, 0xa3, 0xc2, 0x00, 0x0d, 0x06, 0xf0, 0xbe, 0x51,
  0x71, 0x0d, 0xec, 0x54, 0x5a, 0x03, 0xe1, 0xff, 0x2f, 0x00, 0xac, 0xe2, 0xfe, 0xf0, 0x60, 0x26,
  0x04, 0xb5, 0x2d, 0x20, 0xc7, 0xc7, 0xcb, 0x09, 0x00, 0xb5, 0x6b, 0x78, 0x20, 0x00, 0xa8, 0x4d,
  0x82, 0x1a, 0xa9, 0x3d, 0x1f, 0xc1, 0x52, 0xf2, 0xb7, 0x26, 0xc7, 0x0f, 0x03, 0xc0, 0x6e, 0x1d,
  0x05, 0xbd, 0x63, 0x72, 0xfa, 0x62, 0x1b, 0x96, 0x90, 0x7f, 0x12, 0x45, 0xbd, 0xa5, 0x8b, 0xa1,
  0x45, 0x00, 0xe0, 0x4d, 0x2b, 0x08, 0x82, 0xe8, 0x90, 0xc0, 0xe8, 0x9b, 0x01, 0x33, 0xa3, 0x38,
  0x5e, 0x4c, 0xfe, 0xa0, 0x15, 0x44, 0x83, 0x07, 0x00, 0x80, 0x5a, 0x10, 0x05, 0xbc, 0x6d, 0x13,
  0xd8, 0x7e, 0x31, 0xd9, 0x02, 0x5a, 0x9e, 0x20, 0xa6, 0xc7, 0xbd, 0xe4, 0x1a, 0x4b, 0x12, 0x2c,
  0x14, 0x85, 0xe0, 0x28, 0x95, 0x3f, 0x08, 0x8e, 0x08, 0x0c, 0xa2, 0xe0, 0x70, 0x11, 0x5f, 0x66,
  0xf2, 0xb7, 0x92, 0x6b, 0xb4, 0x96, 0x8a, 0x04, 0x4b, 0x01, 0xb4, 0x98, 0x1b, 0xc0, 0x36, 0xfb,
  0xd2, 0xda, 0x2e, 0x6f, 0x40, 0x83, 0x28, 0x95, 0x3f, 0xe8, 0x2d, 0x3f, 0xa2, 0x29, 0x0d, 0x30,
  0x52, 0x00, 0x04, 0x0e, 0x83, 0x16, 0xb3, 0xe4, 0xc3, 0xd2, 0xbd, 0xf8, 0x5c, 0x6a, 0x71, 0x69,
  0x80, 0x2f, 0xcb, 0x03, 0x7c, 0xae, 0x6f, 0xce, 0x4d, 0x88, 0xb7, 0xd2, 0x49, 0xbc, 0x27, 0xaf,
  0xf1, 0x12, 0xd6, 0x01, 0x20, 0x2a, 0x1d, 0x4e, 0x15, 0xc0, 0x60, 0x79, 0x80, 0x4e, 0x59, 0x80,
  0x6d, 0x64, 0xbf, 0xc2, 0x14, 0x26, 0xa5, 0x35, 0x20, 0xe4, 0x0f, 0x0e, 0x97, 0x04, 0x18, 0xdf,
  0x7c, 0xb9, 0x30, 0x00, 0x4f, 0x43, 0xb2, 0x27, 0x8f, 0xe8, 0x82, 0x00, 0x67, 0xcb, 0x01, 0x34,
  0x16, 0x01, 0x38, 0x13, 0xf7, 0xe6, 0x31, 0x5c, 0x02, 0x30, 0x53, 0x2e, 0x95, 0x0c, 0x26, 0xf2,
  0x1a, 0x4b, 0xe4, 0x72, 0x3e, 0x29, 0xd1, 0x18, 0x7f, 0x56, 0x1e, 0xe0, 0x10, 0x03, 0xc8, 0x9e,
  0xdc, 0x2a, 0x27, 0x48, 0x6b, 0x15, 0x00, 0x64, 0x41, 0x00, 0x11, 0x79, 0x92, 0xe8, 0x39, 0xe1,
  0x61, 0x94, 0x87, 0xd4, 0x72, 0xce, 0x58, 0x93, 0xdc, 0xd1, 0xe7, 0x0f, 0x00, 0x20, 0xcd, 0x86,
  0xf5, 0x1e, 0x3c, 0x67, 0x00, 0x09, 0x42, 0x39, 0x15, 0x68, 0x80, 0xd1, 0xb2, 0x4e, 0x5c, 0x1e,
  0x80, 0x60, 0x80, 0xd3, 0x40, 0xc8, 0x1f, 0x95, 0xf2, 0xc6, 0xad, 0x15, 0x02, 0xdc, 0x94, 0x06,
  0x98, 0x60, 0xf5, 0x4f, 0x02, 0x69, 0xce, 0x65, 0xc6, 0x43, 0x0a, 0x20, 0x38, 0x5d, 0xda, 0x84,
  0x3e, 0x2b, 0x0d, 0x20, 0x24, 0x7e, 0xc5, 0x7b, 0x4f, 0x67, 0xb5, 0x32, 0x7d, 0xc9, 0x14, 0xb7,
  0xa0, 0xf3, 0x3b, 0x01, 0xc2, 0x72, 0x00, 0xd2, 0x7e, 0x13, 0x00, 0x72, 0x2c, 0x15, 0x50, 0xa6,
  0xaa, 0x81, 0xa3, 0xd5, 0x01, 0x7c, 0x51, 0x5a, 0x03, 0xa6, 0x03, 0xea, 0xbc, 0x1c, 0x9c, 0xd2,
  0xa2, 0xa9, 0x40, 0xd6, 0x83, 0xdc, 0xf0, 0xfe, 0x72, 0x00, 0x66, 0xbf, 0xaf, 0x50, 0xef, 0x81,
  0xe2, 0x09, 0x06, 0x7c, 0xef, 0x7c, 0x49, 0x80, 0x56, 0x6d, 0xd9, 0x44, 0xb6, 0x2c, 0x00, 0x4b,
  0xcc, 0xca, 0x0b, 0xb6, 0xe6, 0xee, 0x44, 0xd7, 0xd5, 0xc8, 0x2a, 0x00, 0x84, 0x06, 0xfe, 0x56,
  0xce, 0x07, 0x90, 0x03, 0xfe, 0xcc, 0x6d, 0x46, 0xab, 0x20, 0x2a, 0x9c, 0xcc, 0x34, 0xc0, 0x84,
  0x90, 0x65, 0x01, 0xbe, 0xd0, 0x00, 0x50, 0x0e, 0x80, 0xf5, 0x5e, 0x8c, 0x33, 0x73, 0xa2, 0x82,
  0x92, 0x00, 0x73, 0x3d, 0x3f, 0x4f, 0xa5, 0xe1, 0x62, 0x00, 0x6a, 0x48, 0x2c, 0x86, 0xb3, 0xb0,
  0xa5, 0x00, 0x8a, 0xd6, 0xc6, 0x70, 0xa8, 0x00, 0x72, 0x7e, 0xa3, 0x3f, 0xbd, 0xbb, 0xbb, 0x9b,
  0xee, 0xe6, 0x03, 0x7c, 0xfc, 0x49, 0x69, 0x80, 0x91, 0x05, 0x40, 0xe0, 0x79, 0xd9, 0x98, 0xa2,
  0x95, 0xe6, 0xb7, 0xba, 0x70, 0xda, 0x3d, 0xe1, 0xad, 0x7b, 0xd9, 0xc9, 0x03, 0xd8, 0x2d, 0x0f,
  0xf0, 0xb9, 0xb6, 0x5f, 0xb0, 0x74, 0x12, 0x6d, 0xb3, 0xed, 0x6b, 0x85, 0x00, 0x82, 0x79, 0x00,
  0xe1, 0xb3, 0x47, 0xf5, 0x7a, 0xbd, 0xdd, 0x6e, 0xd7, 0xbb, 0xed, 0x3e, 0x09, 0xf3, 0x00, 0x3e,
  0x2e, 0x07, 0x40, 0x1d, 0x0e, 0x38, 0x51, 0x16, 0x11, 0x43, 0x5c, 0x66, 0x3c, 0xe3, 0x37, 0xba,
  0xf3, 0x54, 0x7e, 0xf6, 0x57, 0xbd, 0x7b, 0x17, 0x92, 0x15, 0x02, 0xe8, 0xb0, 0x39, 0xc9, 0x28,
  0xa5, 0xc5, 0x46, 0x66, 0x85, 0x66, 0xb9, 0xd4, 0x30, 0xc2, 0x37, 0x1c, 0xe8, 0x77, 0x13, 0xd9,
  0xd3, 0xd6, 0xdd, 0x73, 0xab, 0x20, 0x05, 0xf8, 0x64, 0x51, 0x00, 0x34, 0xb1, 0x79, 0xac, 0x22,
  0x6b, 0x8f, 0x96, 0xaa, 0x07, 0x7d, 0x00, 0xf4, 0xe0, 0x84, 0x59, 0x4f, 0x57, 0x02, 0x5c, 0xe6,
  0x68, 0xe0, 0xd3, 0xd2, 0x00, 0xd2, 0x7e, 0x23, 0x1d, 0x41, 0x74, 0x58, 0x8c, 0x8a, 0x54, 0x97,
  0xa0, 0x0b, 0x28, 0x5f, 0x05, 0x78, 0xce, 0x01, 0x84, 0xfc, 0xed, 0x93, 0xbb, 0xce, 0x2a, 0x01,
  0x9e, 0x5b, 0x79, 0x8b, 0xcd, 0x8d, 0xb2, 0xe9, 0xea, 0x20, 0xa3, 0x96, 0xdc, 0xd9, 0xd5, 0x79,
  0x00, 0x97, 0x27, 0x52, 0xfe, 0x36, 0x6f, 0x2b, 0x05, 0xe8, 0x99, 0x00, 0x7c, 0xeb, 0x6f, 0x4c,
  0xcf, 0xe4, 0xb0, 0xa0, 0x88, 0x0a, 0x10, 0x80, 0xef, 0xd3, 0x97, 0xd2, 0x7c, 0xda, 0x2b, 0x07,
  0xb0, 0x23, 0x48, 0x0a, 0x50, 0x4b, 0xc7, 0xc6, 0xc5, 0xea, 0x09, 0x78, 0x93, 0xad, 0xa6, 0xad,
  0x8c, 0x3b, 0xed, 0xd6, 0xbb, 0x0a, 0xe0, 0x64, 0x95, 0x3e, 0x00, 0x7a, 0x40, 0x76, 0x26, 0x4d,
  0x88, 0xb7, 0x81, 0x1a, 0x99, 0xbd, 0x81, 0xf9, 0xf5, 0x60, 0x34, 0x67, 0x38, 0x00, 0x17, 0x02,
  0x20, 0xe9, 0xff, 0x93, 0x03, 0xba, 0x42, 0x80, 0x63, 0x3c, 0x23, 0xc2, 0x9f, 0x52, 0xe4, 0xdb,
  0xaf, 0x63, 0xaa, 0x47, 0xb9, 0xfc, 0xdb, 0xf3, 0xca, 0x29, 0x9d, 0xba, 0x7d, 0x1f, 0xbd, 0x94,
  0xf2, 0x9f, 0xb0, 0x3f, 0x1d, 0xf7, 0xc2, 0xe0, 0x62, 0x00, 0xc8, 0x01, 0x99, 0xdc, 0x54, 0x5d,
  0xb1, 0xa7, 0x2b, 0x0c, 0xf6, 0xfd, 0xa2, 0xe5, 0x94, 0x0f, 0x00, 0x3a, 0x2c, 0x03, 0x33, 0xe9,
  0x99, 0x2f, 0x77, 0x1f, 0x7d, 0x4d, 0xee, 0x07, 0xe0, 0x94, 0x03, 0x80, 0x5d, 0x22, 0xb5, 0x12,
  0xb0, 0xc2, 0xe5, 0x94, 0xef, 0x93, 0xd0, 0xff, 0xe5, 0x51, 0x92, 0x0b, 0x1e, 0xb1, 0x34, 0x66,
  0x3d, 0xdc, 0x6a, 0x00, 0xd4, 0x17, 0x07, 0xd8, 0x4a, 0x35, 0x20, 0x93, 0x99, 0x4a, 0x4e, 0xac,
  0x9e, 0xc8, 0x1f, 0xd8, 0x64, 0xcb, 0x29, 0xd7, 0x87, 0xc2, 0x83, 0x3b, 0x56, 0xcb, 0xb5, 0xcf,
  0x7d, 0x95, 0x50, 0x0a, 0xb0, 0x59, 0xff, 0xb4, 0x24, 0xc0, 0x1b, 0xe4, 0x80, 0x18, 0x00, 0x25,
  0xb3, 0x79, 0xf3, 0x13, 0x78, 0x38, 0x00, 0x79, 0xf1, 0xa2, 0xd3, 0xef, 0x87, 0x73, 0x86, 0x94,
  0x59, 0x00, 0xc8, 0x5f, 0x3f, 0x47, 0x11, 0x84, 0x8d, 0x67, 0xa8, 0xb1, 0xf6, 0xea, 0x98, 0x9f,
  0x70, 0x5e, 0x4d, 0x97, 0x53, 0x3d, 0x65, 0xd6, 0xce, 0x9b, 0x52, 0x4a, 0xf3, 0x56, 0xf7, 0x53,
  0x13, 0xca, 0x68, 0x20, 0xbf, 0xfb, 0xe8, 0xa9, 0x9a, 0x85, 0xb0, 0x34, 0x06, 0x3a, 0x99, 0x1d,
  0xc9, 0x47, 0x2b, 0x12, 0xf9, 0xc1, 0x31, 0x3d, 0xdc, 0xca, 0x8c, 0x67, 0x60, 0xb1, 0x41, 0xbd,
  0x0d, 0x00, 0x86, 0x97, 0x64, 0xc5, 0x8f, 0x31, 0x00, 0xf6, 0x2c, 0xf6, 0x93, 0x6b, 0x05, 0x30,
  0x50, 0x0f, 0x90, 0x3b, 0x2f, 0xa6, 0x00, 0x64, 0x32, 0x4f, 0x51, 0x7d, 0x1f, 0xf7, 0xaa, 0x53,
  0x02, 0x34, 0x04, 0x40, 0x28, 0x54, 0x99, 0x73, 0x15, 0x26, 0xe6, 0x48, 0x02, 0xb4, 0x6a, 0x2a,
  0x2c, 0x88, 0xa7, 0xa1, 0xd4, 0x30, 0xa5, 0xb5, 0x25, 0x9f, 0xad, 0x77, 0x48, 0xc1, 0x4a, 0x27,
  0x0f, 0x40, 0xae, 0xfd, 0x8a, 0xc7, 0xef, 0x01, 0x7d, 0x84, 0x03, 0x3c, 0xae, 0x7f, 0xba, 0xd9,
  0xd0, 0x1a, 0x90, 0x08, 0xc4, 0xb7, 0x3e, 0xcd, 0x01, 0x54, 0x08, 0x54, 0xf2, 0x27, 0xb9, 0x38,
  0xa6, 0x5f, 0xa1, 0xe1, 0x3e, 0x55, 0x32, 0x81, 0xfd, 0x84, 0x7f, 0xac, 0xd2, 0xb6, 0x48, 0xe6,
  0x29, 0x68, 0x9e, 0x02, 0xf0, 0x4f, 0xcc, 0x30, 0xfa, 0x2d, 0x03, 0xb0, 0x9d, 0xd8, 0x3f, 0x37,
  0xc0, 0x01, 0xec, 0x09, 0x85, 0xe4, 0xcc, 0x01, 0xb1, 0x7f, 0x5f, 0x8e, 0xcc, 0x78, 0x86, 0x00,
  0x75, 0x5b, 0xd3, 0xce, 0x13, 0x55, 0xc9, 0x6b, 0x6c, 0x27, 0x29, 0x43, 0x48, 0x67, 0x89, 0x08,
  0xce, 0xd8, 0x0f, 0x56, 0x1e, 0x78, 0xda, 0xad, 0x23, 0x0d, 0xf8, 0xc2, 0x81, 0xfc, 0x1d, 0xfe,
  0xb0, 0xc7, 0xe8, 0x05, 0x8a, 0xa2, 0xa9, 0xf5, 0xa4, 0x7f, 0xb1, 0xe1, 0xf0, 0x16, 0xdb, 0xbf,
  0xc1, 0xc3, 0x28, 0x97, 0x5f, 0x3d, 0x22, 0xa5, 0xef, 0x0f, 0x29, 0x2e, 0x85, 0xaf, 0xc4, 0x66,
  0x85, 0xe8, 0x48, 0xe4, 0x3c, 0x0b, 0xa0, 0xe0, 0x36, 0xa3, 0x30, 0xec, 0x24, 0x00, 0x4a, 0x03,
  0xb4, 0xc8, 0x66, 0x24, 0x18, 0x7c, 0x83, 0xf2, 0xb0, 0xb6, 0xcc, 0x14, 0x70, 0x14, 0x44, 0xd1,
  0xa1, 0x38, 0x3d, 0x42, 0x85, 0x47, 0xab, 0x03, 0x29, 0x8f, 0xc5, 0x51, 0x32, 0x3f, 0x2f, 0x0e,
  0x9a, 0x28, 0xb3, 0x43, 0x0a, 0xa9, 0x20, 0x0b, 0x00, 0x76, 0x53, 0xa6, 0x89, 0xec, 0x33, 0x25,
  0xe0, 0x7d, 0x97, 0x3a, 0xaa, 0x61, 0xe6, 0x70, 0x7c, 0xc6, 0x92, 0x53, 0x8c, 0x8d, 0x19, 0x40,
  0x62, 0x68, 0x0b, 0x48, 0xaa, 0x21, 0xae, 0xa9, 0x98, 0xa2, 0x23, 0x32, 0xcc, 0xdb, 0x28, 0xcf,
  0x36, 0xa3, 0x94, 0x05, 0xf0, 0x6d, 0xb7, 0x6d, 0xf8, 0x80, 0xfc, 0x1d, 0xa1, 0x54, 0x11, 0xc7,
  0xc1, 0xc8, 0x10, 0x30, 0x78, 0xc1, 0x22, 0x7d, 0xac, 0x4f, 0xe9, 0xc0, 0xe6, 0xc9, 0x9c, 0x19,
  0x05, 0x7f, 0x99, 0x1a, 0x95, 0x00, 0xea, 0x1a, 0xa3, 0x48, 0x67, 0x0b, 0xf5, 0x21, 0x82, 0x3b,
  0xcc, 0x88, 0x3c, 0x46, 0x47, 0x9a, 0x1a, 0x30, 0x00, 0x40, 0x99, 0xae, 0xf1, 0x61, 0xed, 0x1c,
  0x89, 0x0c, 0x74, 0x30, 0x62, 0x06, 0x8f, 0x43, 0xb7, 0xfc, 0xb5, 0xf4, 0x41, 0x04, 0xbb, 0x1f,
  0x41, 0x44, 0x48, 0x6c, 0x2b, 0xf4, 0xfb, 0x23, 0x4a, 0xf0, 0x05, 0x24, 0x24, 0x26, 0xf0, 0x58,
  0x82, 0x1f, 0xc0, 0xd9, 0xb0, 0x0d, 0xeb, 0x47, 0x90, 0x71, 0xe4, 0x43, 0x1a, 0xa6, 0x3c, 0xcc,
  0x83, 0x72, 0x4a, 0x2c, 0x86, 0x11, 0x50, 0x69, 0x9c, 0x3d, 0x66, 0x25, 0xaf, 0x11, 0xe3, 0x7f,
  0x4a, 0x01, 0xe8, 0xee, 0xd5, 0xa7, 0xb3, 0xc4, 0x71, 0xa6, 0x83, 0x55, 0x42, 0x53, 0x86, 0x62,
  0xdf, 0x1a, 0xe5, 0x29, 0x7e, 0x91, 0x92, 0x04, 0xb2, 0x0b, 0xb2, 0x1a, 0x38, 0xc9, 0x73, 0x62,
  0x00, 0x6d, 0xce, 0x52, 0xbe, 0x24, 0x69, 0x81, 0x76, 0x4d, 0x30, 0x8c, 0xdc, 0xc8, 0xa9, 0xc8,
  0x26, 0x88, 0xa6, 0x14, 0xc1, 0x54, 0x1e, 0x75, 0x23, 0x73, 0xa7, 0x27, 0x70, 0xd8, 0xb6, 0x60,
  0x64, 0x62, 0x06, 0xf0, 0xa8, 0x6b, 0x39, 0xb1, 0xbb, 0xff, 0x89, 0xa5, 0x81, 0x64, 0x24, 0x63,
  0x7c, 0xde, 0xf8, 0x7f, 0xed, 0x2f, 0xa0, 0x0d, 0x09, 0x7b, 0x11, 0x55, 0x41, 0xcc, 0x96, 0xd4,
  0xd0, 0xac, 0xcf, 0xa8, 0x4d, 0x80, 0x86, 0x99, 0x89, 0x2d, 0x71, 0x74, 0x46, 0xc0, 0x07, 0xfc,
  0xc4, 0xe0, 0xf0, 0x77, 0xbb, 0xd1, 0xec, 0xc5, 0xd4, 0x35, 0x3c, 0x16, 0x34, 0x57, 0xf6, 0x4c,
  0x18, 0x35, 0x00, 0x3c, 0x8e, 0x93, 0xf9, 0xf6, 0x88, 0x00, 0x99, 0x67, 0xb0, 0x28, 0x20, 0x99,
  0x57, 0x4c, 0x8e, 0xfc, 0x61, 0x19, 0xbd, 0x36, 0xff, 0x1a, 0x3e, 0x5f, 0x2e, 0x08, 0xe0, 0x86,
  0x60, 0xdf, 0x19, 0xbc, 0x38, 0x84, 0x39, 0x8a, 0x4e, 0xca, 0xa3, 0xdb, 0x5b, 0x6a, 0x1b, 0x74,
  0x7a, 0x06, 0x14, 0xcf, 0x03, 0x2f, 0x7a, 0xee, 0x5e, 0x28, 0x42, 0x85, 0x00, 0xbe, 0xc6, 0x3e,
  0x90, 0x1b, 0x14, 0x64, 0xd2, 0xa2, 0x7c, 0xfe, 0x21, 0x2d, 0x23, 0xe7, 0xf5, 0xff, 0x80, 0xed,
  0xc9, 0x74, 0x08, 0xc4, 0x55, 0xc0, 0x4a, 0xc2, 0x48, 0x10, 0x50, 0x28, 0xaf, 0x08, 0x05, 0xd0,
  0x29, 0x0e, 0x20, 0xfb, 0xe7, 0x36, 0xd9, 0x30, 0xc9, 0x08, 0xe6, 0x1a, 0x10, 0x17, 0xf2, 0xac,
  0x96, 0x15, 0x31, 0xfd, 0x11, 0x9f, 0x47, 0xbd, 0x4d, 0x69, 0x96, 0x01, 0xd8, 0x2b, 0xa7, 0x01,
  0x26, 0xbf, 0xd8, 0x1c, 0xc1, 0xe7, 0xaf, 0xf2, 0xe4, 0x8f, 0x69, 0xb2, 0x1a, 0x1f, 0x05, 0x3f,
  0xa9, 0xd2, 0x4e, 0x37, 0x31, 0x26, 0x8e, 0x7a, 0x71, 0x51, 0xf9, 0xad, 0x88, 0xe5, 0x05, 0xa0,
  0xd8, 0x5a, 0x1c, 0x04, 0x84, 0x55, 0xc2, 0x2d, 0xb4, 0x2a, 0x4c, 0xfd, 0xfd, 0x7f, 0xad, 0x96,
  0x2e, 0x93, 0xdc, 0x2c, 0xb3, 0x38, 0xe0, 0x79, 0xad, 0xe0, 0x7a, 0x21, 0x05, 0xcc, 0xd1, 0x80,
  0xe3, 0x9a, 0x2a, 0xe4, 0xc5, 0x68, 0x9f, 0x09, 0x17, 0x8c, 0x7a, 0xe5, 0x87, 0x7f, 0xa9, 0x6d,
  0x7d, 0x54, 0x25, 0xe6, 0xf4, 0xd0, 0x31, 0x34, 0xaf, 0x15, 0x2f, 0x0d, 0xf0, 0xd4, 0x61, 0x42,
  0xd4, 0xe9, 0x76, 0xc9, 0xd7, 0x18, 0xcd, 0xaa, 0xbc, 0x11, 0x03, 0x02, 0xa7, 0xff, 0x52, 0xb5,
  0x31, 0x2d, 0x99, 0xed, 0x35, 0xce, 0xd2, 0x41, 0xeb, 0xf4, 0x7e, 0x00, 0x9a, 0x97, 0x17, 0x7c,
  0x99, 0xd8, 0x6b, 0x40, 0xea, 0xf0, 0xbb, 0xd8, 0x98, 0x97, 0x75, 0xdc, 0x5c, 0x9f, 0x9b, 0xa7,
  0x67, 0x7b, 0x0f, 0x81, 0xe0, 0x03, 0xa5, 0xf0, 0xb4, 0x50, 0x6e, 0x3d, 0x64, 0x8c, 0x88, 0xf5,
  0xd0, 0x8f, 0xaa, 0x9f, 0x71, 0x80, 0x3d, 0x0f, 0x80, 0xe7, 0xb2, 0x31, 0xb2, 0xdf, 0x63, 0xa5,
  0x17, 0x30, 0x7f, 0x4f, 0x18, 0x8a, 0x9e, 0x70, 0xaf, 0xe1, 0xe3, 0xa4, 0x00, 0x2f, 0x73, 0x53,
  0x2f, 0x41, 0x66, 0x4c, 0x2f, 0x45, 0xa7, 0xa6, 0x06, 0xf6, 0x50, 0x22, 0x0b, 0xa9, 0x4a, 0x41,
  0xe9, 0x17, 0x57, 0x14, 0xd5, 0xf6, 0x4b, 0x5c, 0x27, 0x21, 0x2a, 0x72, 0xb4, 0x2b, 0x8b, 0x05,
  0x2c, 0x7c, 0x62, 0x21, 0x02, 0x78, 0x49, 0xa9, 0xaf, 0xa2, 0x70, 0xcd, 0x4b, 0x68, 0x63, 0x26,
  0x4e, 0x0d, 0x84, 0xf8, 0xa8, 0x49, 0x70, 0x5e, 0x99, 0xcf, 0xcb, 0xb6, 0xe4, 0xbc, 0x2c, 0x95,
  0xa4, 0xe9, 0x5f, 0x54, 0xb2, 0x0b, 0x11, 0xa8, 0x5a, 0x4b, 0x6d, 0x1d, 0xe3, 0x63, 0x16, 0xd1,
  0x32, 0xe1, 0x80, 0x82, 0xeb, 0xf8, 0x45, 0x51, 0x7d, 0x51, 0x26, 0x61, 0xa8, 0x67, 0x2f, 0x71,
  0xa7, 0x7a, 0x33, 0x31, 0x35, 0x8f, 0x3f, 0x73, 0x00, 0xe8, 0x7d, 0xc7, 0xfa, 0x08, 0x48, 0xa1,
  0x5f, 0xf3, 0x37, 0x08, 0xd5, 0x0b, 0xf8, 0x23, 0x03, 0x80, 0xbe, 0x54, 0xde, 0x91, 0xd5, 0xb3,
  0x9e, 0xb0, 0x27, 0x17, 0xe7, 0x77, 0xbf, 0xdc, 0x9d, 0xff, 0x37, 0xc4, 0xf3, 0x01, 0x06, 0x81,
  0xed, 0xc4, 0xa1, 0xec, 0x41, 0x2a, 0x3b, 0x33, 0x73, 0x7d, 0xe4, 0x80, 0x2f, 0xd1, 0x29, 0x96,
  0xe2, 0xd2, 0x42, 0x73, 0xca, 0x61, 0xf4, 0x84, 0xbb, 0x11, 0x4b, 0xa8, 0xda, 0xf6, 0x78, 0x46,
  0xcd, 0x14, 0x45, 0x91, 0x81, 0x77, 0x9e, 0x75, 0xd9, 0x66, 0x89, 0x6e, 0xb7, 0xbb, 0xbf, 0x69,
  0x9c, 0x7a, 0x69, 0x01, 0x5c, 0x64, 0x4c, 0x08, 0x07, 0x1d, 0x0a, 0x99, 0xa2, 0x5d, 0xd9, 0xef,
  0x20, 0xab, 0x28, 0x0b, 0x56, 0x4f, 0xe2, 0x25, 0xdb, 0x03, 0xb5, 0x94, 0x6a, 0x99, 0xf0, 0xcc,
  0x2a, 0x57, 0xd1, 0x1c, 0x0b, 0x5b, 0xa1, 0xa9, 0xa7, 0x4b, 0xc5, 0xdd, 0xae, 0x24, 0xc0, 0x2e,
  0x26, 0x01, 0x1e, 0x1b, 0xd3, 0x2a, 0xba, 0xff, 0xd5, 0x57, 0x4a, 0xb1, 0x4f, 0xe3, 0x6d, 0xbb,
  0xc6, 0x69, 0xad, 0x38, 0xcc, 0x29, 0x27, 0x8a, 0xf5, 0x84, 0xbb, 0x91, 0x23, 0x7a, 0x7a, 0x6f,
  0x08, 0x1a, 0x16, 0x68, 0x1b, 0x4b, 0xd6, 0xc8, 0x1e, 0xe9, 0x9d, 0x06, 0xed, 0x50, 0x4d, 0x01,
  0x1a, 0x73, 0xae, 0xc9, 0xdc, 0xa8, 0x35, 0x2f, 0x24, 0x18, 0x40, 0xba, 0x43, 0xfa, 0x05, 0x95,
  0x12, 0x03, 0xad, 0x7e, 0x62, 0x9e, 0xa3, 0xaa, 0x9d, 0x5f, 0xfc, 0x9b, 0x4f, 0x43, 0xb6, 0x54,
  0xda, 0xd6, 0x1f, 0x52, 0x5c, 0xd1, 0x88, 0x12, 0xd5, 0x41, 0x86, 0x83, 0xc2, 0x66, 0x57, 0x6f,
  0x95, 0x68, 0x77, 0xbf, 0xa6, 0xd6, 0xa0, 0xdb, 0xa7, 0x01, 0xa0, 0x86, 0x13, 0xc8, 0x41, 0x30,
  0x88, 0x3b, 0xf0, 0x7f, 0xeb, 0x6d, 0xbb, 0x69, 0xef, 0x99, 0x27, 0xe6, 0x5a, 0x40, 0xd7, 0x68,
  0x43, 0x20, 0x75, 0x00, 0x1c, 0x01, 0x01, 0x7d, 0xd2, 0x2e, 0xb2, 0x0e, 0x3a, 0x3d, 0x69, 0xeb,
  0x95, 0x6e, 0xb9, 0x4e, 0x8c, 0x32, 0x8e, 0xd7, 0x84, 0x94, 0x38, 0xa6, 0x5d, 0x6b, 0x33, 0xd2,
  0xdb, 0x76, 0x9d, 0x47, 0x34, 0x82, 0x19, 0xc8, 0x94, 0xc7, 0x04, 0xff, 0x46, 0xc7, 0x90, 0x05,
  0x68, 0x63, 0x85, 0x36, 0x51, 0xc0, 0x01, 0xe6, 0x52, 0xed, 0x95, 0xe0, 0x0b, 0xc5, 0x77, 0x21,
  0xc5, 0x00, 0x38, 0x8c, 0x7a, 0x00, 0x4c, 0xbf, 0xd2, 0x46, 0x8a, 0x96, 0xb9, 0x47, 0x86, 0x06,
  0xb0, 0x0e, 0x14, 0x40, 0xac, 0x97, 0xe4, 0xb1, 0xcb, 0xe0, 0x65, 0x42, 0x8b, 0x5c, 0xc9, 0x66,
  0x02, 0xb4, 0x1b, 0x94, 0x38, 0x66, 0x39, 0xfd, 0x00, 0x28, 0xfa, 0x50, 0x5c, 0x83, 0x60, 0x80,
  0x23, 0x0b, 0xc0, 0x85, 0x42, 0xf4, 0x44, 0x7a, 0x50, 0xcb, 0x07, 0xa0, 0x86, 0x0f, 0xc7, 0x6f,
  0xd5, 0x66, 0x8f, 0x04, 0x21, 0x24, 0xe6, 0xd4, 0x4e, 0x2e, 0x80, 0x19, 0x0f, 0xa9, 0x59, 0xe0,
  0x68, 0xf5, 0x1f, 0x65, 0x0e, 0x44, 0xce, 0x9e, 0x85, 0x8b, 0x17, 0xf0, 0xcf, 0x54, 0x14, 0xd2,
  0x15, 0x79, 0xcd, 0x75, 0x46, 0x73, 0xba, 0x5f, 0xc8, 0xd8, 0xf0, 0x74, 0x49, 0x88, 0x5b, 0x03,
  0x8d, 0x30, 0x03, 0x60, 0xcc, 0xf8, 0x23, 0xe3, 0x14, 0x03, 0x32, 0x1f, 0x00, 0xd5, 0x9e, 0x62,
  0xb6, 0xe7, 0x5a, 0x5a, 0x49, 0x30, 0x07, 0x20, 0x19, 0x3c, 0xf4, 0x51, 0x14, 0x62, 0x3b, 0xb6,
  0xc0, 0x28, 0xe9, 0xb4, 0x13, 0x87, 0x61, 0x36, 0x0a, 0xc9, 0x9a, 0x0c, 0xa7, 0x64, 0x5d, 0xc6,
  0x20, 0x07, 0xa4, 0x4e, 0x02, 0xbb, 0x9d, 0xa2, 0x7a, 0x42, 0x7c, 0x02, 0x55, 0xe4, 0xe0, 0x53,
  0x01, 0x99, 0x8a, 0x3c, 0xd0, 0xd6, 0x7b, 0xe6, 0xf0, 0xd1, 0xd5, 0x3e, 0x00, 0xbb, 0xb6, 0xa2,
  0xaa, 0x54, 0x4b, 0x94, 0x80, 0x06, 0x64, 0xd1, 0x96, 0xe7, 0x90, 0x6e, 0xdb, 0x17, 0x54, 0x49,
  0x37, 0x11, 0xd5, 0x34, 0x45, 0x1b, 0x4f, 0xa9, 0x17, 0x20, 0xe4, 0x99, 0x2c, 0x71, 0xe0, 0xee,
  0x93, 0xbe, 0x29, 0x3f, 0xcc, 0x07, 0x90, 0x15, 0x0e, 0x35, 0x2b, 0x4c, 0x6e, 0xd3, 0x91, 0xde,
  0x77, 0x3c, 0x0f, 0x20, 0xf5, 0x02, 0xbd, 0x51, 0xfd, 0x54, 0x00, 0x8c, 0xf4, 0x46, 0x03, 0x9f,
  0x09, 0xa5, 0xfb, 0x46, 0x93, 0x62, 0xe8, 0xe4, 0x6d, 0x47, 0x2f, 0xc6, 0x99, 0x90, 0x24, 0xe3,
  0x03, 0x60, 0x9e, 0xe3, 0xae, 0x35, 0xe0, 0x19, 0x90, 0x59, 0x4e, 0xec, 0x32, 0x21, 0xd0, 0xf5,
  0xc4, 0x4b, 0xf1, 0xc1, 0x91, 0x39, 0x9e, 0xa1, 0x99, 0x9a, 0x44, 0x6e, 0x58, 0x39, 0x38, 0xbf,
  0x3c, 0x3f, 0xe8, 0xe3, 0x75, 0x7b, 0x13, 0x52, 0x38, 0x71, 0x5f, 0x17, 0x73, 0x9e, 0xb3, 0xdb,
  0x95, 0x88, 0xb8, 0x3f, 0x6f, 0x8b, 0x1d, 0xd7, 0x8e, 0x46, 0xc0, 0x6c, 0x59, 0x30, 0xd1, 0xe2,
  0xb6, 0x1f, 0xc0, 0x28, 0x87, 0xd4, 0xb9, 0xe9, 0xd8, 0x82, 0x8c, 0x8a, 0x6f, 0x3e, 0x80, 0xd5,
  0xb1, 0xb1, 0x1e, 0x90, 0x4d, 0xe2, 0xa2, 0x27, 0xd7, 0x1b, 0xbb, 0xdb, 0xd5, 0xac, 0x16, 0xaa,
  0xc8, 0x7d, 0x00, 0xd6, 0x88, 0xcc, 0x18, 0x68, 0x69, 0x1f, 0xa8, 0x2b, 0x80, 0x26, 0xd1, 0x75,
  0x84, 0x7d, 0xfe, 0xbf, 0x34, 0x07, 0xb4, 0x51, 0x66, 0xbe, 0x0f, 0x8b, 0xf0, 0x4b, 0xcf, 0xb0,
  0x0a, 0xd8, 0x0f, 0xd4, 0x9e, 0x8a, 0x41, 0x36, 0x75, 0x80, 0x13, 0xc1, 0xf6, 0x2d, 0x6d, 0x43,
  0xe1, 0x26, 0x02, 0x08, 0x33, 0x27, 0xed, 0xe7, 0xd8, 0x43, 0xcf, 0x7b, 0xba, 0x7f, 0xe6, 0x9f,
  0x3a, 0xf3, 0x1e, 0x26, 0x4b, 0x83, 0x87, 0xf6, 0x88, 0xd2, 0x32, 0x58, 0x73, 0x58, 0x8c, 0x67,
  0x8b, 0xb3, 0x4e, 0xc0, 0x00, 0xf6, 0x33, 0x00, 0x4e, 0x69, 0x44, 0x04, 0xd1, 0xdb, 0x76, 0xbd,
  0xaf, 0x70, 0x80, 0x4c, 0x55, 0xa4, 0xf7, 0x50, 0xdc, 0xe2, 0x75, 0xfa, 0x74, 0x44, 0xe9, 0xea,
  0x04, 0x7b, 0xaa, 0x5a, 0x0d, 0xf6, 0x68, 0x26, 0x11, 0x78, 0x01, 0x3c, 0x2f, 0x50, 0x50, 0x37,
  0x1f, 0xb8, 0xe5, 0x07, 0x57, 0x5d, 0x7d, 0xaa, 0x76, 0xa3, 0xf1, 0x89, 0x52, 0x40, 0x23, 0x4a,
  0xcf, 0x5b, 0x1a, 0x8c, 0xb1, 0x9d, 0xcb, 0x22, 0x9c, 0x00, 0x1b, 0xe1, 0x7c, 0x8f, 0xa4, 0xce,
  0x01, 0x59, 0xbe, 0xe1, 0xe9, 0x0a, 0x90, 0x3d, 0xc4, 0xce, 0x76, 0x5b, 0x90, 0xde, 0x87, 0x57,
  0x72, 0x48, 0xe1, 0x03, 0xc8, 0x4c, 0xec, 0x16, 0x02, 0xa8, 0x15, 0x08, 0x2b, 0xbd, 0xc0, 0x07,
  0x40, 0xbd, 0x00, 0x3a, 0x75, 0x25, 0x1b, 0x02, 0xc9, 0x44, 0x02, 0x8c, 0xf2, 0x5f, 0x29, 0x02,
  0x14, 0x15, 0x62, 0xd4, 0xeb, 0xc4, 0xc5, 0x35, 0x60, 0x0d, 0x67, 0x4b, 0xbc, 0x04, 0x04, 0x0d,
  0x8e, 0xd9, 0x3c, 0x8f, 0xdc, 0xd3, 0x92, 0x07, 0x60, 0xd4, 0x02, 0xae, 0x8a, 0xc9, 0x0d, 0x00,
  0xc5, 0x01, 0x7e, 0x2a, 0x01, 0x40, 0xd0, 0xe0, 0x98, 0x0d, 0x83, 0x5b, 0xbe, 0x7a, 0x30, 0x6f,
  0x68, 0x91, 0x31, 0xcb, 0xc5, 0x34, 0x10, 0x4f, 0x82, 0x9c, 0xde, 0x03, 0xef, 0xab, 0x53, 0xae,
  0x75, 0x32, 0xd3, 0x00, 0xc1, 0x29, 0x5d, 0xb4, 0xa9, 0x31, 0x65, 0x59, 0x1f, 0xb8, 0x6d, 0xc9,
  0x24, 0x74, 0x54, 0xee, 0x8e, 0x32, 0xf6, 0x7f, 0x38, 0xcc, 0x19, 0x90, 0x15, 0xd7, 0x80, 0x69,
  0x42, 0x64, 0x0e, 0x00, 0x18, 0x00, 0xc2, 0x1b, 0x4f, 0x0b, 0x15, 0xa3, 0xaa, 0xc9, 0x32, 0xfc,
  0xc3, 0x19, 0x02, 0xf8, 0xb9, 0xf8, 0x0b, 0x82, 0xdc, 0x35, 0x37, 0x29, 0x06, 0x80, 0xda, 0xf5,
  0x9c, 0xde, 0xf3, 0xbf, 0xac, 0x47, 0x04, 0xff, 0x0f, 0xdb, 0x08, 0xa0, 0xb6, 0xc4, 0xdb, 0x81,
  0xee, 0x09, 0xc0, 0x7f, 0x47, 0x31, 0x8a, 0x89, 0x56, 0x0a, 0x10, 0x96, 0x06, 0x50, 0xfb, 0x12,
  0x93, 0xe5, 0xb9, 0x32, 0x77, 0x8c, 0x5b, 0xe2, 0x4c, 0x1f, 0x04, 0x70, 0x0c, 0xb0, 0xf0, 0xeb,
  0x99, 0x84, 0x06, 0x48, 0x49, 0x80, 0xad, 0x02, 0x00, 0x2e, 0xa9, 0xd8, 0xf7, 0x44, 0x24, 0x1d,
  0xf1, 0x87, 0x3d, 0x52, 0x65, 0xb0, 0x27, 0x01, 0x17, 0x25, 0xa0, 0x8b, 0x02, 0xa8, 0x27, 0xb1,
  0x5b, 0xd7, 0x65, 0xfc, 0x8e, 0x0b, 0x7a, 0xcd, 0xd6, 0x8c, 0xd9, 0x63, 0x2a, 0xdf, 0xb3, 0x4e,
  0x10, 0x00, 0x93, 0x07, 0x05, 0xb8, 0x2d, 0xfd, 0xae, 0xb1, 0xeb, 0x6d, 0x2e, 0xf8, 0xf7, 0xf4,
  0x34, 0x50, 0x27, 0x9b, 0x2c, 0x0e, 0x40, 0x5d, 0x00, 0x73, 0x13, 0x19, 0x1b, 0x51, 0xa6, 0x37,
  0x6f, 0xb9, 0x4a, 0xf9, 0x02, 0x59, 0xe4, 0x3f, 0x3c, 0x01, 0x5e, 0xb7, 0xd4, 0x73, 0x94, 0x2b,
  0xf6, 0x81, 0x02, 0x99, 0x58, 0x6c, 0x9c, 0x8e, 0x9e, 0x93, 0xc5, 0xee, 0x7d, 0xfd, 0x92, 0xd5,
  0x50, 0x62, 0x66, 0x23, 0x5b, 0x91, 0x2f, 0xa4, 0x81, 0x27, 0x65, 0x00, 0x20, 0x1e, 0x44, 0x4b,
  0x2a, 0xff, 0xe8, 0x98, 0xca, 0xe2, 0xf4, 0x9a, 0xd2, 0xa5, 0xe3, 0x68, 0x49, 0x00, 0x3e, 0x55,
  0xc8, 0xf7, 0xc9, 0x1c, 0x2f, 0x67, 0xbc, 0xf4, 0xec, 0x9b, 0x57, 0xd1, 0x12, 0x59, 0xc0, 0x70,
  0xe2, 0x27, 0xa5, 0x9c, 0x98, 0xf9, 0x41, 0xef, 0x05, 0x93, 0x9f, 0x2c, 0x1e, 0x3e, 0x92, 0xdf,
  0x1d, 0xbc, 0x68, 0x2d, 0x25, 0xff, 0xa2, 0x26, 0x94, 0x98, 0xf1, 0xe0, 0x76, 0xb9, 0xb7, 0x10,
  0xf2, 0x8a, 0xe9, 0xb6, 0xf7, 0x15, 0xa5, 0xcb, 0x5b, 0xd0, 0x22, 0x1a, 0xa0, 0x2b, 0x7c, 0x85,
  0xe3, 0x8a, 0x4c, 0xa8, 0xf3, 0xd7, 0x03, 0xd0, 0x55, 0x02, 0x3c, 0xeb, 0x94, 0x33, 0xa1, 0x35,
  0x79, 0x57, 0x65, 0xd5, 0x35, 0x40, 0x5d, 0x1a, 0xa8, 0xad, 0xc0, 0x36, 0xff, 0x72, 0x13, 0xda,
  0x55, 0x00, 0x61, 0x95, 0x34, 0xa0, 0x4d, 0xe8, 0xe2, 0xc9, 0xdb, 0x3f, 0xd5, 0xc9, 0xdf, 0x50,
  0x1d, 0xf9, 0x15, 0xc0, 0x5e, 0xfd, 0x3c, 0x0c, 0xc5, 0xe9, 0xf7, 0x4d, 0x52, 0xb5, 0x28, 0xc4,
  0x46, 0x64, 0xd3, 0xfa, 0xb4, 0x8a, 0x00, 0x7a, 0x89, 0xe6, 0x59, 0xfd, 0x80, 0xa4, 0x00, 0xec,
  0x3d, 0x94, 0xa4, 0x32, 0xe2, 0xcb, 0x20, 0x14, 0xf6, 0xf7, 0xeb, 0x7b, 0x02, 0xa0, 0x52, 0x71,
  0x54, 0xfb, 0x70, 0x3d, 0xd9, 0x0f, 0x45, 0x95, 0x17, 0x43, 0xa5, 0xe4, 0x0f, 0xc9, 0x41, 0xfd,
  0x32, 0x54, 0x00, 0x4d, 0x66, 0x43, 0x50, 0x2d, 0x17, 0x08, 0x2f, 0xdb, 0x3f, 0xe0, 0x17, 0xe9,
  0x84, 0xd5, 0x00, 0x50, 0xf2, 0xb3, 0x34, 0xf6, 0xe4, 0xb1, 0x06, 0x08, 0xab, 0x91, 0x09, 0x00,
  0x4d, 0xed, 0x4e, 0xeb, 0xcf, 0x3a, 0xe8, 0x5d, 0x4c, 0xb5, 0x2a, 0x04, 0x52, 0x2c, 0x3f, 0x8b,
  0x41, 0x53, 0x12, 0xea, 0x25, 0xe5, 0x6a, 0xc4, 0x21, 0xc0, 0x2e, 0x5c, 0xdf, 0xd4, 0x00, 0x50,
  0x85, 0x5c, 0x06, 0xfa, 0x2c, 0xbe, 0x44, 0x01, 0xac, 0x8e, 0x40, 0x1a, 0x58, 0x7f, 0x15, 0x80,
  0xa1, 0x80, 0x1f, 0xea, 0x4f, 0x52, 0x05, 0xe0, 0x97, 0x0a, 0x92, 0x75, 0x77, 0x00, 0x1d, 0x82,
  0x36, 0x7f, 0xa9, 0x9f, 0x5b, 0xfb, 0x40, 0x92, 0xe3, 0xfe, 0xd6, 0xba, 0x08, 0x45, 0x65, 0x5c,
  0xe3, 0x32, 0x99, 0x94, 0x33, 0x01, 0xf8, 0x3b, 0xc9, 0xd6, 0xd9, 0xfe, 0x91, 0x01, 0x85, 0x07,
  0xdd, 0xa4, 0x8e, 0x33, 0x01, 0xd8, 0x7a, 0xfd, 0xba, 0xaa, 0x20, 0xdd, 0x38, 0xa6, 0x1c, 0x98,
  0xec, 0xd6, 0xeb, 0x97, 0x9d, 0x30, 0x03, 0x40, 0xd9, 0x9b, 0xef, 0xc9, 0xfa, 0x5a, 0x90, 0x02,
  0x20, 0x9d, 0x7d, 0xbe, 0x3c, 0x29, 0x36, 0xd3, 0x19, 0x6f, 0xc7, 0x5d, 0x5f, 0x02, 0xf4, 0x18,
  0x2b, 0x09, 0xdf, 0xb6, 0x79, 0x1d, 0x9d, 0xee, 0x0a, 0x37, 0x27, 0xda, 0x48, 0xb3, 0xb6, 0x7e,
  0x15, 0x05, 0x50, 0xf4, 0x60, 0x04, 0xdf, 0xe7, 0x74, 0xde, 0x56, 0x0e, 0x00, 0x16, 0x00, 0x73,
  0xe4, 0xe6, 0x7a, 0x10, 0xa8, 0x4d, 0x42, 0x7a, 0xbb, 0xb7, 0x92, 0xbf, 0xce, 0x52, 0x98, 0x3a,
  0xeb, 0x24, 0xf3, 0x92, 0xf1, 0xe6, 0x43, 0x5a, 0x11, 0xda, 0x6a, 0xa3, 0x9f, 0x0f, 0x31, 0x4e,
  0x84, 0x21, 0x1d, 0x26, 0x3f, 0xdb, 0xcb, 0xa8, 0xf6, 0x23, 0x67, 0x5f, 0xf3, 0xfe, 0x97, 0xf8,
  0x81, 0xb1, 0xb9, 0x93, 0x66, 0x9f, 0x16, 0x71, 0x3f, 0xd3, 0xc4, 0xe4, 0xef, 0x5f, 0xd6, 0xeb,
  0x7f, 0x74, 0xc2, 0x50, 0x59, 0x50, 0x76, 0xb2, 0xf9, 0xde, 0x09, 0xe4, 0xce, 0x66, 0x7b, 0xa7,
  0xb6, 0xe3, 0xb1, 0x6d, 0xf3, 0x88, 0x11, 0x26, 0xf4, 0xe6, 0xe5, 0x5d, 0xfd, 0x5c, 0xcb, 0xef,
  0x78, 0x4f, 0x7d, 0x5a, 0x59, 0xdf, 0x7b, 0x42, 0x80, 0xec, 0x33, 0xf5, 0x86, 0xb0, 0xe0, 0x38,
  0x10, 0x29, 0x24, 0x8d, 0x83, 0xbb, 0xbb, 0xfd, 0x03, 0x29, 0x3e, 0x01, 0x0f, 0x00, 0x23, 0xd8,
  0xb8, 0x0f, 0x04, 0x00, 0x5b, 0xf0, 0x32, 0xe7, 0x39, 0xb1, 0xe8, 0x79, 0xf1, 0x6c, 0x7f, 0xff,
  0xd9, 0x63, 0x7d, 0x6c, 0x98, 0xeb, 0x3d, 0xf5, 0x86, 0x12, 0xe0, 0x1e, 0x92, 0x11, 0x35, 0x9f,
  0x88, 0x2c, 0x2a, 0x3e, 0xfb, 0x6f, 0xf7, 0x7c, 0x7f, 0x7f, 0x7f, 0xda, 0x21, 0x66, 0xff, 0x7b,
  0x00, 0x12, 0x47, 0x60, 0xae, 0x40, 0x56, 0x9d, 0x4b, 0x61, 0xde, 0x61, 0xc6, 0xae, 0x77, 0x46,
  0xf1, 0x07, 0xb3, 0x2e, 0xa6, 0xac, 0xfb, 0xff, 0xb8, 0x08, 0xb5, 0xfd, 0xe8, 0x4d, 0x6d, 0x34,
  0x07, 0x61, 0x95, 0x0c, 0xf8, 0x69, 0xac, 0x32, 0x16, 0x14, 0x76, 0x36, 0xf7, 0xa6, 0xe7, 0x6f,
  0x2f, 0xff, 0xb1, 0xd7, 0x21, 0xa1, 0xdd, 0xff, 0x34, 0x67, 0xc9, 0x4b, 0x20, 0xd4, 0x44, 0xc1,
  0x64, 0x3f, 0x34, 0xec, 0x9d, 0xb1, 0x29, 0x7f, 0x5e, 0x99, 0xbf, 0x75, 0xfa, 0xbb, 0x7b, 0x07,
  0xe7, 0xbc, 0x4d, 0x2f, 0xfe, 0xd4, 0xe2, 0x23, 0x05, 0xe4, 0xae, 0xd9, 0x11, 0xc9, 0x50, 0xa2,
  0x35, 0x51, 0xdb, 0x40, 0xed, 0x23, 0xb3, 0x8d, 0x77, 0x93, 0x76, 0x21, 0xdb, 0x1e, 0x6e, 0x07,
  0xaa, 0x4d, 0xa7, 0xa9, 0xf4, 0x7b, 0xfc, 0xd0, 0xbc, 0x30, 0xb3, 0x99, 0x7a, 0x1e, 0x40, 0xba,
  0x18, 0x52, 0x5e, 0xfa, 0x0d, 0xab, 0x61, 0xc1, 0x59, 0xdb, 0xdd, 0xd5, 0xd2, 0x3f, 0x7d, 0xea,
  0x11, 0x5c, 0xb6, 0x1f, 0x0e, 0xf6, 0x2e, 0xac, 0x23, 0xff, 0x4c, 0x2b, 0x20, 0x85, 0x97, 0x74,
  0x56, 0x64, 0x1b, 0x8d, 0x0e, 0x6a, 0xfd, 0x7e, 0xff, 0xa3, 0x3e, 0x6e, 0x1f, 0xfd, 0x9d, 0x37,
  0x49, 0xb8, 0xdb, 0xef, 0x77, 0x42, 0xe5, 0xc9, 0xf6, 0x31, 0x41, 0xc5, 0x01, 0xee, 0x65, 0x85,
  0x22, 0xe3, 0x31, 0x85, 0x0e, 0xad, 0x46, 0x4f, 0xff, 0x0a, 0x80, 0xff, 0x01, 0xf8, 0x01, 0x7b,
  0x07, 0xbc, 0x05, 0x8a, 0xb8, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60,
  0x82
};
static const size_t ICON_PNG_LEN = 5281;

// === Mobile-first Web GUI ===
static const char WEB_HTML[] PROGMEM = R"=====(
<!DOCTYPE html><html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">
<meta name="apple-mobile-web-app-capable" content="yes">
<meta name="mobile-web-app-capable" content="yes">
<meta name="apple-mobile-web-app-title" content="NeoTick">
<meta name="apple-mobile-web-app-status-bar-style" content="black-translucent">
<link rel="manifest" href="/manifest.json">
<link rel="apple-touch-icon" href="/touch-icon.png">
<link rel="icon" type="image/svg+xml" href="/icon.svg">
<title>NeoTick</title>
<style>
:root{--bg:#0f0f23;--card:#1a1a2e;--accent:#44d9e1;--accent2:#6e7dff;--text:#e0e0e0;--text2:#999;--btn:#2d2d44;--success:#4CAF50;--danger:#e74c3c;--work:#4CAF50;--rest:#e74c3c}
*{margin:0;padding:0;box-sizing:border-box;-webkit-tap-highlight-color:transparent}
body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;background:var(--bg);color:var(--text);min-height:100vh;overflow-x:hidden;overscroll-behavior-y:contain}
.hdr{background:#0a0a1a;padding:22px 16px;text-align:center;position:relative;overflow:hidden;border-bottom:1px solid #1a1a2e}
.hdr::before{content:'';position:absolute;top:-50%;left:-50%;width:200%;height:200%;background:conic-gradient(from 0deg,transparent 0%,rgba(68,217,225,.06) 25%,transparent 50%,rgba(110,125,255,.06) 75%,transparent 100%);animation:headerShine 12s linear infinite}
@keyframes headerShine{0%{transform:rotate(0deg)}100%{transform:rotate(360deg)}}
.hdr h1{font-size:28px;color:#fff;font-weight:900;letter-spacing:3px;position:relative;text-transform:uppercase}
.hdr h1 span{color:var(--accent);font-weight:400}
.hdr .sub{font-size:11px;color:rgba(255,255,255,.4);margin-top:4px;position:relative;letter-spacing:1px}
.tabs{display:flex;background:var(--card);position:sticky;top:0;z-index:10;overflow-x:auto;-webkit-overflow-scrolling:touch}
.tab{flex:1 0 auto;padding:14px 10px;text-align:center;cursor:pointer;font-size:12px;font-weight:600;color:var(--text2);border-bottom:3px solid transparent;transition:.2s;white-space:nowrap}
.tab.active{color:var(--accent);border-bottom-color:var(--accent)}
.panel{display:none;padding:16px;padding-bottom:80px}
.panel.active{display:block}
.card{background:var(--card);border-radius:14px;padding:20px;margin-bottom:14px}
.card h3{font-size:13px;color:var(--accent);margin-bottom:14px;text-transform:uppercase;letter-spacing:1.5px;font-weight:700}
.big-time{font-size:56px;font-weight:800;text-align:center;font-family:'SF Mono','Courier New',monospace;color:#fff;letter-spacing:2px;padding:8px 0}
.big-time .sec{font-size:28px;color:var(--accent);vertical-align:baseline}
.big-time .blink{animation:blink 1s step-end infinite}
@keyframes blink{50%{opacity:.3}}
.sw-time{font-size:44px;font-weight:700;text-align:center;font-family:'SF Mono','Courier New',monospace;color:var(--accent);padding:16px 0}
.btn-row{display:flex;gap:10px;justify-content:center;flex-wrap:wrap;margin-top:14px}
.btn{padding:14px 28px;border:none;border-radius:12px;font-size:15px;cursor:pointer;font-weight:700;transition:.15s;touch-action:manipulation}
.btn:active{transform:scale(.96)}
.btn-primary{background:var(--accent);color:#000}
.btn-danger{background:var(--danger);color:#fff}
.btn-secondary{background:var(--btn);color:var(--text)}.btn-accent{background:#6e7dff;color:#fff}
.colors{display:grid;grid-template-columns:repeat(6,1fr);gap:8px;margin-top:10px;max-width:320px}
.color-dot{width:100%;max-width:44px;aspect-ratio:1;border-radius:50%;cursor:pointer;border:3px solid transparent;transition:.15s;touch-action:manipulation}
.color-dot:hover,.color-dot.active{border-color:#fff;transform:scale(1.15)}
.slider-row{display:flex;align-items:center;gap:12px;margin-top:12px}
.slider-row label{min-width:70px;font-size:13px;color:var(--text2)}
.slider-row input[type=range]{flex:1;height:6px;accent-color:var(--accent);-webkit-appearance:none;background:#333;border-radius:3px;outline:none}
.slider-row input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;width:24px;height:24px;border-radius:50%;background:var(--accent);cursor:pointer}
.slider-row .val{min-width:32px;text-align:right;font-size:14px;font-weight:600}
select{width:100%;padding:12px;border-radius:10px;border:1px solid #333;background:var(--btn);color:var(--text);font-size:15px;margin-top:8px;-webkit-appearance:none;appearance:none}
.radio-group{display:flex;gap:14px;margin-top:10px;flex-wrap:wrap}
.radio-group label{display:flex;align-items:center;gap:6px;cursor:pointer;font-size:14px;touch-action:manipulation}
.status{font-size:11px;color:var(--text2);text-align:center;padding:6px}
.dot{display:inline-block;width:8px;height:8px;border-radius:50%;margin-right:4px}
.dot.on{background:var(--success)}.dot.off{background:var(--danger)}
.num-input{display:flex;gap:8px;justify-content:center;align-items:center;margin:14px 0;flex-wrap:wrap}
.num-input input{width:56px;padding:10px;text-align:center;border-radius:10px;border:1px solid #333;background:var(--btn);color:var(--text);font-size:18px;font-weight:600}
.num-input span{font-size:13px;color:var(--text2)}
.custom-color{margin-top:12px;display:flex;align-items:center;gap:10px}
.custom-color input[type=color]{width:48px;height:40px;border:none;border-radius:10px;cursor:pointer;background:transparent}
.dst-rule{background:rgba(255,255,255,.03);border-radius:10px;padding:14px;margin-top:12px;border:1px solid #222}
.dst-rule h4{font-size:12px;color:var(--accent);margin-bottom:10px;font-weight:700}
.dst-rule .row{display:flex;gap:6px;align-items:center;margin-top:6px;flex-wrap:wrap}
.dst-rule select,.dst-rule input{padding:8px;border-radius:8px;border:1px solid #333;background:var(--btn);color:var(--text);font-size:13px}
.tab-phase{font-size:18px;font-weight:700;text-align:center;padding:10px;border-radius:10px;margin-bottom:10px}
.tab-phase.work{background:rgba(76,175,80,.15);color:var(--work)}
.tab-phase.rest{background:rgba(231,76,60,.15);color:var(--rest)}
.tab-phase.done{background:rgba(68,217,225,.15);color:var(--accent)}
.tab-info{text-align:center;font-size:13px;color:var(--text2);margin-top:8px}
.toggle-row{display:flex;align-items:center;justify-content:space-between;padding:8px 0}
.toggle-row span{font-size:14px}
.toggle{position:relative;width:48px;height:28px;cursor:pointer;touch-action:manipulation}
.toggle input{opacity:0;width:0;height:0}
.toggle .slider{position:absolute;inset:0;background:#444;border-radius:14px;transition:.2s}
.toggle .slider:before{content:'';position:absolute;width:22px;height:22px;left:3px;bottom:3px;background:#fff;border-radius:50%;transition:.2s}
.toggle input:checked+.slider{background:var(--accent)}
.toggle input:checked+.slider:before{transform:translateX(20px)}
.wifi-badge{display:inline-flex;align-items:center;gap:4px;font-size:11px;color:var(--text2);margin-top:4px}
.seg-wrap{display:flex;justify-content:center;align-items:center;gap:4px;padding:12px 0;overflow:hidden}
.seg-digit{position:relative;width:28px;height:50px}
.seg-digit span{position:absolute;background:var(--card);border-radius:2px;transition:background .15s}
.seg-digit span.on{background:var(--clr,var(--accent))}
.seg-digit .a{top:0;left:3px;width:22px;height:4px}
.seg-digit .b{top:3px;right:0;width:4px;height:21px}
.seg-digit .c{bottom:3px;right:0;width:4px;height:21px}
.seg-digit .d{bottom:0;left:3px;width:22px;height:4px}
.seg-digit .e{bottom:3px;left:0;width:4px;height:21px}
.seg-digit .f{top:3px;left:0;width:4px;height:21px}
.seg-digit .g{top:23px;left:3px;width:22px;height:4px}
.seg-colon{display:flex;flex-direction:column;gap:12px;justify-content:center;align-items:center;width:8px;height:50px}
.seg-colon i{width:6px;height:6px;border-radius:50%;background:var(--accent)}
@media(min-width:400px){.seg-digit{width:36px;height:64px}.seg-digit .a,.seg-digit .d,.seg-digit .g{left:4px;width:28px;height:5px}.seg-digit .b,.seg-digit .c{right:0;width:5px}.seg-digit .e,.seg-digit .f{left:0;width:5px}.seg-digit .b,.seg-digit .f{top:4px;height:26px}.seg-digit .c,.seg-digit .e{bottom:4px;height:26px}.seg-digit .g{top:29px}.seg-colon{width:10px;height:64px}.seg-colon i{width:7px;height:7px}}
.seg-bar{position:sticky;top:42px;z-index:9;background:var(--bg);padding:8px 0;border-bottom:1px solid #1a1a2e}
.wheel{display:flex;justify-content:center;align-items:center;gap:4px;margin:14px 0}
.wc{height:120px;width:56px;overflow-y:scroll;scroll-snap-type:y mandatory;-webkit-overflow-scrolling:touch;border-radius:10px;background:var(--btn);position:relative;padding:40px 0;scroll-padding:40px 0}
.wc div{height:40px;display:flex;align-items:center;justify-content:center;scroll-snap-align:center;font-size:20px;font-weight:600;color:var(--text)}
.wc-wrap{position:relative;display:inline-block}
.wc-wrap::after{content:'';position:absolute;top:50%;left:2px;right:2px;height:40px;transform:translateY(-50%);border:2px solid var(--accent);border-radius:8px;pointer-events:none;z-index:2}
.wc-label{font-size:12px;color:var(--text2);text-align:center;margin-top:2px}
.wheel-sep{font-size:24px;font-weight:700;color:var(--accent);padding:0 4px}
.seg-bar.anim .seg-digit span{animation:segAnim .3s infinite alternate}
@keyframes segAnim{0%{background:var(--accent)}50%{background:#6e7dff}100%{background:#ff6e7d}}
.seg-bar.paused .seg-wrap,.seg-bar.paused .big-time{animation:pausePulse 2s ease-in-out infinite}
@keyframes pausePulse{0%,100%{opacity:1}50%{opacity:.15}}
.sec-hdr{cursor:pointer;display:flex;justify-content:space-between;align-items:center;padding:14px 18px;background:var(--card);border-radius:14px;margin-bottom:2px}
.sec-hdr h3{margin:0;font-size:13px;color:var(--accent);text-transform:uppercase;letter-spacing:1.5px;font-weight:700}
.sec-hdr .arr{color:var(--text2);font-size:14px;transition:transform .2s}
.sec-hdr.open .arr{transform:rotate(180deg)}
.sec-body{display:none;background:var(--card);border-radius:0 0 14px 14px;padding:0 18px 18px;margin-top:-12px;margin-bottom:14px}
.sec-body.show{display:block}
.toast{position:fixed;top:50px;left:50%;transform:translateX(-50%);background:rgba(68,217,225,.9);color:#000;padding:6px 18px;border-radius:20px;font-size:12px;font-weight:700;z-index:99;opacity:0;transition:opacity .2s;pointer-events:none}
.toast.show{opacity:1}
</style>
</head>
<body>
<div class="hdr">
  <h1>NEO<span>TICK</span></h1>
  <div class="sub"><a href="https://www.instagram.com/ai.garage_" target="_blank" style="color:rgba(255,255,255,.4);text-decoration:none;letter-spacing:1px">by The AI Garage</a></div>
</div>
<div class="tabs">
  <div class="tab active" data-tab="clock">Clock</div>
  <div class="tab" data-tab="stopwatch">Stopwatch</div>
  <div class="tab" data-tab="timer">Timer</div>
  <div class="tab" data-tab="tabata">Tabata</div>
  <div class="tab" data-tab="pomodoro">Pomodoro</div>
  <div class="tab" data-tab="settings">Settings</div>
</div>
<div class="seg-bar"><div class="seg-wrap" id="segDisp"><div class="seg-digit" id="sd0"></div><div class="seg-digit" id="sd1"></div><div class="seg-colon"><i></i><i></i></div><div class="seg-digit" id="sd2"></div><div class="seg-digit" id="sd3"></div></div><div class="big-time" id="timeDisp">--<span class="blink">:</span>--<span class="sec">:--</span></div></div>

<div class="panel active" id="clock">
  <div class="card">
    <div class="status"><span class="dot" id="syncDot"></span><span id="syncText">Syncing...</span>
      <span class="wifi-badge"><span class="dot" id="wifiDot"></span><span id="wifiText">WiFi</span></span>
    </div>
  </div>
  <div class="card">
    <h3>Display Format</h3>
    <div class="toggle-row">
      <span id="fmtLabel">HH:MM</span>
      <label class="toggle"><input type="checkbox" id="mmssToggle" onchange="toggleMMSS()"><span class="slider"></span></label>
    </div>
  </div>
  <div class="card">
    <h3>Transition Animation</h3>
    <div class="toggle-row">
      <span id="animLabel">Fade on digit change</span>
      <label class="toggle"><input type="checkbox" id="animToggle" checked onchange="toggleAnim()"><span class="slider"></span></label>
    </div>
  </div>
  <div class="card" style="text-align:center">
    <button class="btn btn-primary" onclick="send({cmd:'animate'})">LED Test</button>
  </div>
</div>

<div class="panel" id="stopwatch">
  <div class="card">
    <div class="sw-time" id="swDisp">00:00.0</div>
    <div class="btn-row">
      <button class="btn" id="swToggle" onclick="swToggle()">Start</button>
      <button class="btn btn-accent" id="swBcastBtn" onclick="swToggle(true)" style="display:none;font-size:11px;padding:8px 12px">All Watches</button>
      <button class="btn btn-secondary" id="swReset2" onclick="send({cmd:'sw',action:'reset'})" style="display:none">Reset</button>
    </div>
  </div>
</div>

<div class="panel" id="timer">
  <div class="card">
    <div class="sw-time" id="timerDisp">01:00</div>
    <div class="wheel" id="timerSetRow"><div><div class="wc-wrap"><div class="wc" id="timerMinW"></div></div><div class="wc-label">min</div></div><div class="wheel-sep">:</div><div><div class="wc-wrap"><div class="wc" id="timerSecW"></div></div><div class="wc-label">sec</div></div></div>
    <div class="btn-row">
      <button class="btn btn-secondary" id="tmSetBtn" onclick="tmSet()">Set</button>
      <button class="btn" id="tmToggle" onclick="tmToggle()">Start</button>
      <button class="btn btn-accent" id="tmBcastBtn" onclick="tmToggle(true)" style="display:none;font-size:11px;padding:8px 12px">All Watches</button>
    </div>
  </div>
</div>

<div class="panel" id="tabata">
  <div class="card" style="padding:12px 16px">
    <div style="display:flex;align-items:center;justify-content:space-between;gap:8px;flex-wrap:wrap">
      <div style="flex:1;min-width:120px">
        <div class="tab-phase" id="tabPhase" style="font-size:15px;padding:6px;margin-bottom:4px">READY</div>
        <div class="sw-time" id="tabDisp" style="font-size:36px;padding:4px 0">00:20</div>
        <div class="tab-info" id="tabInfo" style="font-size:12px;margin-top:2px">Interval: - / -</div>
      </div>
      <div style="display:flex;flex-direction:column;gap:6px">
        <button class="btn" id="tabToggle" onclick="tabToggle()" style="padding:10px 24px;font-size:14px">Start</button>
        <button class="btn btn-accent" id="tabBcastBtn" onclick="tabToggle(true)" style="display:none;font-size:11px;padding:6px 12px">All Watches</button>
        <button class="btn btn-secondary" id="tabReset2" onclick="send({cmd:'tabata',action:'reset'})" style="display:none;padding:8px 20px;font-size:12px">Reset</button>
      </div>
    </div>
    <div id="tabSummary" style="display:none;text-align:center;font-size:13px;color:var(--text2);margin-top:8px;padding-top:8px;border-top:1px solid #222">
      Work: <strong id="tabSumWork" style="color:var(--work)">20s</strong> &middot;
      Rest: <strong id="tabSumRest" style="color:var(--rest)">10s</strong> &middot;
      Intervals: <strong id="tabSumInt" style="color:var(--accent)">8</strong>
    </div>
  </div>
  <div class="card" id="tabCfg">
    <h3>Tabata Settings</h3>
    <div style="display:flex;gap:24px;justify-content:center;align-items:flex-start;flex-wrap:wrap">
      <div style="text-align:center"><div style="font-size:11px;color:var(--work);margin-bottom:2px">Work</div><div style="display:flex;gap:2px;align-items:center"><div class="wc-wrap"><div class="wc" id="tabWorkMinW" style="width:44px;height:100px"></div></div><span style="font-size:11px;color:var(--text2)">:</span><div class="wc-wrap"><div class="wc" id="tabWorkSecW" style="width:44px;height:100px"></div></div></div><div style="font-size:10px;color:var(--text2)">min : sec</div></div>
      <div style="text-align:center"><div style="font-size:11px;color:var(--rest);margin-bottom:2px">Rest</div><div style="display:flex;gap:2px;align-items:center"><div class="wc-wrap"><div class="wc" id="tabRestMinW" style="width:44px;height:100px"></div></div><span style="font-size:11px;color:var(--text2)">:</span><div class="wc-wrap"><div class="wc" id="tabRestSecW" style="width:44px;height:100px"></div></div></div><div style="font-size:10px;color:var(--text2)">min : sec</div></div>
      <div style="text-align:center"><div style="font-size:11px;color:var(--accent);margin-bottom:2px">Rounds</div><div class="wc-wrap"><div class="wc" id="tabIntW" style="width:44px;height:100px"></div></div><div style="font-size:10px;color:var(--text2)">&nbsp;</div></div>
    </div>
    <div style="display:flex;gap:8px;margin-top:12px;justify-content:center;flex-wrap:wrap">
      <span style="font-size:12px;color:var(--text2)">Work:</span><select id="tabWC" style="width:auto;padding:4px 8px;font-size:12px;border-radius:6px;border:1px solid #333;background:var(--btn);color:var(--text)"></select>
      <span style="font-size:12px;color:var(--text2)">Rest:</span><select id="tabRC" style="width:auto;padding:4px 8px;font-size:12px;border-radius:6px;border:1px solid #333;background:var(--btn);color:var(--text)"></select>
    </div>
    <div class="btn-row" style="margin-top:12px">
      <button class="btn btn-primary" style="font-size:13px;padding:10px 20px" onclick="saveTabata()">Save</button>
    </div>
    <div style="margin-top:12px;border-top:1px solid #222;padding-top:10px">
      <h3 style="font-size:12px;margin-bottom:8px">Presets</h3>
      <div id="tabPresetList" style="margin-bottom:8px"></div>
      <select id="tabPresetSel" style="display:none"></select>
      <div style="display:flex;gap:6px;align-items:center">
        <input type="text" id="tabPresetName" maxlength="15" placeholder="Preset name" style="flex:1;padding:6px;border-radius:6px;border:1px solid #333;background:var(--btn);color:var(--text);font-size:12px">
        <button class="btn btn-primary" style="padding:6px 10px;font-size:11px" onclick="saveTabPreset()">Save Preset</button>
      </div>
    </div>
  </div>
</div>


<div class="panel" id="pomodoro">
  <div class="card">
    <div class="tab-phase" id="pomPhase">READY</div>
    <div class="sw-time" id="pomDisp">25:00</div>
    <div class="tab-info" id="pomInfo">Interval: 1 / 4</div>
    <div class="btn-row">
      <button class="btn" id="pomToggle" onclick="pomToggle()">Start</button>
      <button class="btn btn-accent" id="pomBcastBtn" onclick="pomToggle(true)" style="display:none;font-size:11px;padding:8px 12px">All Watches</button>
      <button class="btn btn-secondary" id="pomReset2" onclick="send({cmd:'pom',action:'reset'})" style="display:none">Reset</button>
    </div>
  </div>
  <div class="card">
    <h3>Pomodoro Settings</h3>
    <div class="slider-row"><label>Intervals</label><input type="range" id="pomIntSlider" min="1" max="8" value="4"><span class="val" id="pomIntVal">4</span></div>
    <div class="btn-row" style="margin-top:10px"><button class="btn btn-primary" onclick="savePomInt()">Save</button></div>
  </div>
</div>

<div class="panel" id="settings">
  <div class="sec-hdr" onclick="togSec(this)"><h3>Color &amp; Brightness</h3><span class="arr">&#9660;</span></div>
  <div class="sec-body">
    <div class="colors" id="colorGrid"></div>
    <div class="custom-color">
      <span style="font-size:13px;color:var(--text2)">Custom:</span>
      <input type="color" id="customColor" value="#00ff00">
      <button class="btn btn-secondary" style="padding:10px 16px;font-size:13px" onclick="applyCustomColor()">Apply</button>
    </div>
    <div class="slider-row" style="margin-top:14px">
      <label>Brightness</label>
      <input type="range" id="brightSlider" min="5" max="230" value="100">
      <span class="val" id="brightVal">100</span>
    </div>
    <div style="margin-top:14px">
      <h3 style="font-size:12px;margin-bottom:8px">Color Mode</h3>
      <div class="radio-group">
        <label><input type="radio" name="clrMode" value="0" checked onchange="send({cmd:'colormode',value:0})">Static</label>
        <label><input type="radio" name="clrMode" value="1" onchange="send({cmd:'colormode',value:1})">Rainbow</label>
        <label><input type="radio" name="clrMode" value="2" onchange="send({cmd:'colormode',value:2})">Crazy</label>
        <label><input type="radio" name="clrMode" value="3" onchange="send({cmd:'colormode',value:3})">Pulse</label>
      </div>
    </div>
  </div>

  <div class="sec-hdr" onclick="togSec(this)"><h3>Timezone</h3><span class="arr">&#9660;</span></div>
  <div class="sec-body">
    <select id="tzSelect" onchange="setTimezone()"></select>
  </div>

  <div class="sec-hdr" onclick="togSec(this)"><h3>Night Shift & Colon LEDs</h3><span class="arr">&#9660;</span></div>
  <div class="sec-body">
    <div class="toggle-row"><span>Colon LEDs (seconds dots)</span><label class="toggle"><input type="checkbox" id="colonToggle" checked onchange="send({cmd:'colon',enabled:this.checked})"><span class="slider"></span></label></div>
    <div style="font-size:11px;color:var(--text2);margin:4px 0 12px">Auto-off during night shift</div>
    <div class="toggle-row"><span>Auto-dim at night</span><label class="toggle"><input type="checkbox" id="nsToggle" onchange="saveNightShift()"><span class="slider"></span></label></div>
    <div class="slider-row" style="margin-top:10px"><label>Start</label><select id="nsStart" onchange="saveNightShift()" style="width:80px"></select><label>End</label><select id="nsEnd" onchange="saveNightShift()" style="width:80px"></select></div>
    <div class="slider-row"><label>Brightness</label><input type="range" id="nsBright" min="5" max="80" value="15" onchange="saveNightShift()"><span class="val" id="nsBrightVal">15</span></div>
  </div>

  <div class="sec-hdr" onclick="togSec(this)"><h3>Buzzer</h3><span class="arr">&#9660;</span></div>
  <div class="sec-body">
    <div class="radio-group">
      <label><input type="radio" name="buzz" value="0" onchange="send({cmd:'buzzer',level:0})">Off</label>
      <label><input type="radio" name="buzz" value="1" onchange="send({cmd:'buzzer',level:1})">Low</label>
      <label><input type="radio" name="buzz" value="2" checked onchange="send({cmd:'buzzer',level:2})">High</label>
    </div>
    <div class="toggle-row" style="margin-top:10px"><span>Clockwork chime (hourly)</span><label class="toggle"><input type="checkbox" id="cwToggle" onchange="send({cmd:'clockwork',enabled:this.checked})"><span class="slider"></span></label></div>
    <div style="font-size:11px;color:var(--text2);margin:2px 0 10px">Chimes the hour count. Silent during night shift.</div>
    <div style="text-align:center"><button class="btn btn-secondary" style="padding:8px 16px;font-size:12px" onclick="send({cmd:'buzztest'})">Test Buzzer</button></div>
  </div>


  <div class="sec-hdr" onclick="togSec(this)"><h3>Info Display</h3><span class="arr">&#9660;</span></div>
  <div class="sec-body">
    <div class="toggle-row"><span>Show date</span><label class="toggle"><input type="checkbox" id="dateToggle" onchange="saveInfo()"><span class="slider"></span></label></div>
    <div class="toggle-row"><span>Show temperature</span><label class="toggle"><input type="checkbox" id="tempToggle" onchange="saveInfo()"><span class="slider"></span></label></div>
    <div class="slider-row"><label>Interval</label><input type="range" id="dateIntSlider" min="10" max="120" value="30" onchange="saveInfo()"><span class="val" id="dateIntVal">30</span><span style="font-size:11px;color:var(--text2)">sec</span></div>
    <div style="margin-top:8px">
      <div class="radio-group">
        <label><input type="radio" name="tempType" value="0" checked onchange="saveInfo()">Actual</label>
        <label><input type="radio" name="tempType" value="1" onchange="saveInfo()">Feels like</label>
      </div>
      <div class="toggle-row"><span>Color by temperature</span><label class="toggle"><input type="checkbox" id="tempClrToggle" onchange="saveInfo()"><span class="slider"></span></label></div>
      <div id="tempReadout" style="font-size:12px;color:var(--accent);margin-top:6px"></div>
      <div style="margin-top:6px">
        <select id="locSelect" style="width:100%;padding:8px;border-radius:8px;border:1px solid #333;background:var(--btn);color:var(--text);font-size:13px" onchange="setLoc()">
          <option value="auto">Auto (IP)</option>
          <option value="other">Other (lat/lon)...</option>
        </select>
        <div id="locManual" style="display:none;margin-top:6px;gap:6px;align-items:center">
          <input type="number" id="locLat" placeholder="Lat" step="0.01" style="width:45%;padding:6px;border-radius:6px;border:1px solid #333;background:var(--btn);color:var(--text);font-size:12px">
          <input type="number" id="locLon" placeholder="Lon" step="0.01" style="width:45%;padding:6px;border-radius:6px;border:1px solid #333;background:var(--btn);color:var(--text);font-size:12px">
          <button class="btn btn-primary" style="padding:6px 10px;font-size:11px" onclick="saveManualLoc()">Set</button>
        </div>
      </div>
      <div id="locStatus" style="font-size:11px;color:var(--text2);margin-top:4px"></div>
    </div>
  </div>


  <div class="sec-hdr" onclick="togSec(this)"><h3>Birthdays</h3><span class="arr">&#9660;</span></div>
  <div class="sec-body">
    <div style="font-size:11px;color:var(--text2);margin-bottom:10px;line-height:1.5">On a birthday, the watch scrolls "HAPPY BDAY [name]" with a celebration animation.</div>
    <div class="slider-row" style="margin-bottom:10px"><label>Repeat every</label><select id="bdayIntv" onchange="send({cmd:'bday_interval',value:+this.value})" style="padding:6px 10px;border-radius:8px;border:1px solid #333;background:var(--btn);color:var(--text);font-size:13px"><option value="1">1 min</option><option value="2">2 min</option><option value="3">3 min</option><option value="4">4 min</option><option value="5">5 min</option><option value="6">6 min</option><option value="7">7 min</option><option value="8">8 min</option><option value="9">9 min</option><option value="10">10 min</option><option value="12">12 min</option><option value="15">15 min</option><option value="20">20 min</option><option value="30">30 min</option><option value="40">40 min</option><option value="50">50 min</option><option value="60" selected>60 min</option><option value="90">90 min</option><option value="120">120 min</option></select></div>
    <div class="slider-row" style="margin-bottom:10px"><label>Scroll times</label><select id="bdayScrl" onchange="send({cmd:'bday_scrollcount',value:+this.value})" style="padding:6px 10px;border-radius:8px;border:1px solid #333;background:var(--btn);color:var(--text);font-size:13px"><option value="1" selected>1×</option><option value="2">2×</option><option value="3">3×</option><option value="4">4×</option><option value="5">5×</option></select></div>
    <div class="slider-row" style="margin-bottom:10px"><label>Play song</label><label class="tog"><input type="checkbox" id="bdayBuzz" onchange="send({cmd:'bday_buzzer',enabled:this.checked})"><span class="tog-sl"></span></label></div>
    <div class="slider-row" style="margin-bottom:10px"><label>Scroll speed</label><select id="bdaySpd" onchange="send({cmd:'bday_scrollspeed',value:+this.value})" style="padding:6px 10px;border-radius:8px;border:1px solid #333;background:var(--btn);color:var(--text);font-size:13px"><option value="1">Very slow</option><option value="2">Slow</option><option value="3" selected>Normal</option><option value="4">Fast</option><option value="5">Very fast</option></select></div>
    <div id="bdayList" style="margin-bottom:8px"></div>
    <div style="display:flex;gap:6px;flex-wrap:wrap;align-items:center">
      <input type="text" id="bdayName" maxlength="15" placeholder="Name" style="width:80px;padding:6px;border-radius:6px;border:1px solid #333;background:var(--btn);color:var(--text);font-size:12px">
      <input type="number" id="bdayDay" min="1" max="31" placeholder="DD" style="width:44px;padding:6px;border-radius:6px;border:1px solid #333;background:var(--btn);color:var(--text);font-size:12px">
      <input type="number" id="bdayMon" min="1" max="12" placeholder="MM" style="width:44px;padding:6px;border-radius:6px;border:1px solid #333;background:var(--btn);color:var(--text);font-size:12px">
      <button class="btn btn-primary" style="padding:6px 12px;font-size:12px" onclick="addBday()">Add</button>
    </div>
  </div>

  <div class="sec-hdr" onclick="togSec(this)"><h3>DST Rules</h3><span class="arr">&#9660;</span></div>
  <div class="sec-body">
    <div class="radio-group">
      <label><input type="radio" name="dst" value="0" onchange="setDST(0)">Off</label>
      <label><input type="radio" name="dst" value="1" checked onchange="setDST(1)">Custom</label>
      <label><input type="radio" name="dst" value="2" onchange="setDST(2)">Always On</label>
    </div>
    <div id="dstRules">
      <div class="dst-rule"><h4>Start (winter &rarr; summer)</h4><div class="row"><select id="dsFL"><option value="1">Last</option><option value="0">First</option></select><select id="dsDow"></select><span style="color:var(--text2)">of</span><select id="dsMon"></select><span style="color:var(--text2)">at</span><input type="number" id="dsHour" value="2" min="0" max="23" style="width:44px"><span style="color:var(--text2)">:00</span></div></div>
      <div class="dst-rule"><h4>End (summer &rarr; winter)</h4><div class="row"><select id="deFL"><option value="1">Last</option><option value="0">First</option></select><select id="deDow"></select><span style="color:var(--text2)">of</span><select id="deMon"></select><span style="color:var(--text2)">at</span><input type="number" id="deHour" value="2" min="0" max="23" style="width:44px"><span style="color:var(--text2)">:00</span></div></div>
      <div class="btn-row" style="margin-top:10px"><button class="btn btn-primary" style="font-size:12px" onclick="saveDSTRules()">Save</button><button class="btn btn-secondary" style="font-size:12px" onclick="resetDSTIsrael()">Israel Default</button></div>
    </div>
  </div>

  <div class="sec-hdr" onclick="togSec(this)"><h3>Watch Name</h3><span class="arr">&#9660;</span></div>
  <div class="sec-body">
    <div style="font-size:11px;color:var(--text2);margin-bottom:10px;line-height:1.5">Used to identify this watch on the network (neotick-&lt;name&gt;.local) and among peers.</div>
    <div style="display:flex;gap:6px;align-items:center">
      <input type="text" id="devName" maxlength="16" placeholder="Watch name" oninput="updateDevAddr()" style="flex:1;padding:8px;border-radius:8px;border:1px solid #333;background:var(--btn);color:var(--text);font-size:14px">
      <button class="btn btn-primary" style="padding:8px 16px;font-size:13px" onclick="saveDevName()">Save</button>
    </div>
    <div id="devAddr" style="font-size:12px;margin-top:8px;color:var(--text2)"></div>
    <div style="font-size:10px;color:var(--text2);margin-top:6px">Letters, numbers and dashes only (spaces become dashes).</div>
    <div id="nameWarn" style="display:none;margin-top:8px;font-size:12px;color:var(--danger);background:rgba(231,76,60,.1);border:1px solid rgba(231,76,60,.3);border-radius:8px;padding:8px">&#9888; Another watch on the network has this same name. Give each watch a unique name so they don't clash.</div>
  </div>

  <div class="sec-hdr" onclick="togSec(this)"><h3>Multi-Watch Sync</h3><span class="arr">&#9660;</span></div>
  <div class="sec-body">
    <div style="font-size:11px;color:var(--text2);margin-bottom:10px;line-height:1.5">Copy this watch's settings &ndash; brightness, color, night shift, birthdays, timezone, weather and more &ndash; to every other NeoTick on the network. Each watch keeps its own name.</div>
    <button class="btn btn-primary" id="syncCfgBtn" style="display:none;width:100%;padding:10px" onclick="syncAll()">Sync All Watches With This Config</button>
    <div id="syncNoPeers" style="font-size:11px;color:var(--text2)">No other watches detected on the network.</div>
    <div id="peerList" style="margin-top:10px"></div>
  </div>

  <div class="sec-hdr" onclick="togSec(this)"><h3>Firmware Update</h3><span class="arr">&#9660;</span></div>
  <div class="sec-body" id="otaSec">
    <div style="background:rgba(231,76,60,.1);border:1px solid rgba(231,76,60,.3);border-radius:10px;padding:12px;margin-bottom:12px">
      <div style="font-size:13px;color:var(--danger);font-weight:700;margin-bottom:4px">&#9888; Developer Only</div>
      <div style="font-size:11px;color:var(--text2);line-height:1.5">Uploading incorrect firmware can brick your device. Do not use unless you know what you are doing. Do not disconnect power during update.</div>
    </div>
    <div id="otaForm">
      <div style="margin-bottom:10px"><input type="password" id="otaPass" placeholder="Developer password" style="width:100%;padding:10px;border-radius:8px;border:1px solid #333;background:var(--btn);color:var(--text);font-size:14px"></div>
      <div style="margin-bottom:10px"><input type="file" id="otaFile" accept=".bin" style="font-size:13px;color:var(--text2)"></div>
      <button class="btn btn-danger" style="width:100%;padding:12px;font-size:14px" onclick="startOTA()">Upload Firmware</button>
    </div>
    <div id="otaProgress" style="display:none;text-align:center">
      <div style="font-size:16px;font-weight:700;color:var(--accent);margin-bottom:10px" id="otaStatus">Uploading...</div>
      <div style="background:#333;border-radius:6px;height:8px;overflow:hidden;margin-bottom:8px"><div id="otaBar" style="background:var(--accent);height:100%;width:0%;transition:width .2s"></div></div>
      <div style="font-size:12px;color:var(--text2)" id="otaPct">0%</div>
    </div>
  </div>

  <div class="card">
    <h3>WiFi</h3>
    <p style="font-size:14px;color:var(--text2)">SSID: <strong id="wifiSSID">--</strong></p>
    <p style="font-size:14px;color:var(--text2);margin-top:6px">IP: <strong id="wifiIP">--</strong></p>
    <p style="font-size:14px;color:var(--text2);margin-top:6px" id="rssiLine">Signal: --</p>
    <p style="font-size:14px;color:var(--text2);margin-top:6px" id="tempLine">CPU Temp: --</p>
    <div class="btn-row" style="margin-top:10px"><button class="btn btn-danger" style="font-size:12px;padding:8px 14px" onclick="resetWifi()">Reset WiFi</button></div>
  </div>

  <div class="card">
    <div class="status">v3.0 &middot; NeoTick &middot; <a href="https://www.instagram.com/ai.garage_" target="_blank" style="color:var(--accent);text-decoration:none">The AI Garage</a></div>
  </div>
</div>
<div class="toast" id="toast">Saving...</div>

<script>
const C=[
  {n:"Red",c:"#FF0000"},{n:"Green",c:"#008000"},{n:"Blue",c:"#0000FF"},
  {n:"Yellow",c:"#FFFF00"},{n:"Cyan",c:"#00FFFF"},{n:"Magenta",c:"#FF00FF"},
  {n:"Orange",c:"#FFA500"},{n:"Purple",c:"#800080"},{n:"Aqua",c:"#00FFFF"},
  {n:"Lime",c:"#00FF00"},{n:"Indigo",c:"#4B0082"},{n:"Teal",c:"#008080"},
  {n:"Turquoise",c:"#40E0D0"},{n:"Gold",c:"#FFD700"},{n:"Maroon",c:"#800000"},
  {n:"Olive",c:"#808000"},{n:"Navy",c:"#000080"},{n:"SkyBlue",c:"#87CEEB"},
  {n:"Coral",c:"#FF7F50"},{n:"Lavender",c:"#E6E6FA"},{n:"Silver",c:"#C0C0C0"},
  {n:"Pink",c:"#FFC0CB"},{n:"White",c:"#FFFFFF"}
];
const DAYS=["Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"];
const MON=["","Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"];
const TZ=[
  ["UTC-12 Baker Is.",-43200],["UTC-11 Samoa",-39600],["UTC-10 Hawaii",-36000],
  ["UTC-9 Alaska",-32400],["UTC-8 Pacific US",-28800],["UTC-7 Mountain US",-25200],
  ["UTC-6 Central US",-21600],["UTC-5 Eastern US",-18000],["UTC-4 Atlantic",-14400],
  ["UTC-3 Buenos Aires",-10800],["UTC-2 Mid-Atlantic",-7200],["UTC-1 Azores",-3600],
  ["UTC+0 London/GMT",0],["UTC+1 Paris/Berlin",3600],["UTC+2 Israel/Helsinki",7200],
  ["UTC+3 Moscow",10800],["UTC+3:30 Tehran",12600],["UTC+4 Dubai",14400],
  ["UTC+5 Karachi",18000],["UTC+5:30 Mumbai",19800],["UTC+6 Dhaka",21600],
  ["UTC+7 Bangkok",25200],["UTC+8 Singapore",28800],["UTC+9 Tokyo",32400],
  ["UTC+9:30 Adelaide",34200],["UTC+10 Sydney",36000],["UTC+11 Solomon",39600],
  ["UTC+12 Auckland",43200]
];
let ws,st={},firstState=true,wheelsInit=false,lastMode=-1,saving=false,prev={};
const SEG=[0x7E,0x30,0x6D,0x79,0x33,0x5B,0x5F,0x70,0x7F,0x7B];
const SEG_DEG=0x63;
const SEGS='abcdefg';
function initSegs(){for(let i=0;i<4;i++){const el=document.getElementById('sd'+i);el.innerHTML='';SEGS.split('').forEach(s=>{const sp=document.createElement('span');sp.className=s;el.appendChild(sp);});}}
function setDigit(idx,val){const el=document.getElementById('sd'+idx);if(!el)return;const bits=val>=0&&val<=9?SEG[val]:0;const spans=el.querySelectorAll('span');SEGS.split('').forEach((s,i)=>{spans[i].classList.toggle('on',!!(bits&(0x40>>i)));});}
function setDigitRaw(idx,bits){const el=document.getElementById('sd'+idx);if(!el)return;const spans=el.querySelectorAll('span');SEGS.split('').forEach((s,i)=>{spans[i].classList.toggle('on',!!(bits&(0x40>>i)));});}
const SEG_MINUS=0x01;
function updateSeg(){if(st.dv===undefined)return;const v=st.dv;if(st.db){setDigit(0,-1);setDigit(1,-1);setDigit(2,-1);setDigit(3,-1);}else if(st.dt){var neg=v<0,abs=Math.abs(v);if(abs>99)abs=99;if(neg){setDigitRaw(0,SEG_MINUS);if(abs>=10){setDigit(1,Math.floor(abs/10));setDigit(2,abs%10);setDigitRaw(3,SEG_DEG);}else{setDigit(1,abs);setDigitRaw(2,SEG_DEG);setDigit(3,-1);}}else{if(abs>=10){setDigit(0,Math.floor(abs/10));setDigit(1,abs%10);}else{setDigit(0,-1);setDigit(1,abs);}setDigitRaw(2,SEG_DEG);setDigit(3,-1);}}else{setDigit(0,Math.floor(v/1000)%10);setDigit(1,Math.floor(v/100)%10);setDigit(2,Math.floor(v/10)%10);setDigit(3,v%10);}}
function hslStr(h,s,l){return 'hsl('+h+','+s+'%,'+l+'%)';}
var crazyHues=[0,0,0,0],lastCrazyT=0,pulseHue=0,pulsePhaseStart=0,pulseTransitioning=false;
function animSegColors(){var now=Date.now();if(st.clrMode===2){if(now-lastCrazyT>200){lastCrazyT=now;for(var i=0;i<4;i++)crazyHues[i]=Math.floor(Math.random()*360);}for(var i=0;i<4;i++)document.getElementById('sd'+i).style.setProperty('--clr',hslStr(crazyHues[i],100,50));}else if(st.clrMode===3){if(!pulsePhaseStart)pulsePhaseStart=now;var h;if(!pulseTransitioning){h=pulseHue;if(now-pulsePhaseStart>=23000){pulseTransitioning=true;pulsePhaseStart=now;}}else{var el=now-pulsePhaseStart;if(el>=3000){pulseHue=(pulseHue+25)%360;h=pulseHue;pulseTransitioning=false;pulsePhaseStart=now;}else{var t=el/3000;var e=t*t*(3-2*t);h=(pulseHue+Math.floor(25*e))%360;}}for(var i=0;i<4;i++)document.getElementById('sd'+i).style.setProperty('--clr',hslStr(h,100,50));}}
function makeWheel(id,max){const el=document.getElementById(id);el.innerHTML='';for(let i=0;i<=max;i++){const d=document.createElement('div');d.textContent=String(i);el.appendChild(d);}}
function setWheel(id,val){const el=document.getElementById(id);setTimeout(()=>{el.scrollTop=val*40;},50);}
function getWheel(id){return Math.max(0,Math.round(document.getElementById(id).scrollTop/40));}
function init(){
  initSegs();
  makeWheel('timerMinW',59);makeWheel('timerSecW',59);
  makeWheel('tabWorkMinW',10);makeWheel('tabWorkSecW',59);
  makeWheel('tabRestMinW',10);makeWheel('tabRestSecW',59);
  makeWheel('tabIntW',20);
  setWheel('timerMinW',1);setWheel('timerSecW',0);
  document.querySelectorAll('.tab').forEach(t=>{
    t.onclick=()=>{
      const dest=t.dataset.tab;
      const m={clock:0,stopwatch:1,timer:2,tabata:3,pomodoro:4,settings:-1};
      const destMode=m[dest];
      if(destMode!==undefined&&destMode>=0){
        const running=(st.swRun&&dest!=='stopwatch')||(st.tmRun&&dest!=='timer')||(st.tabRun&&dest!=='tabata');
        if(running){
          const what=st.swRun?'Stopwatch':st.tmRun?'Timer':'Tabata';
          if(!confirm(what+' is running. Switching will stop it. Continue?'))return;
          if(st.swRun)send({cmd:'sw',action:'stop'});
          if(st.tmRun)send({cmd:'timer',action:'stop'});
          if(st.tabRun)send({cmd:'tabata',action:'stop'});
        }
      }
      document.querySelectorAll('.tab,.panel').forEach(e=>e.classList.remove('active'));
      t.classList.add('active');document.getElementById(dest).classList.add('active');
      if(destMode>=0)send({cmd:'mode',value:destMode});
    };
  });
  const g=document.getElementById('colorGrid');
  C.forEach((c,i)=>{const d=document.createElement('div');d.className='color-dot';d.style.background=c.c;d.title=c.n;d.onclick=()=>send({cmd:'color',index:i});g.appendChild(d);});
  const bs=document.getElementById('brightSlider');
  bs.oninput=()=>{document.getElementById('brightVal').textContent=bs.value;};
  bs.onchange=()=>send({cmd:'brightness',value:+bs.value});
  const tz=document.getElementById('tzSelect');
  TZ.forEach(([l,o])=>{const op=document.createElement('option');op.value=o;op.textContent=l;tz.appendChild(op);});
  ['dsDow','deDow'].forEach(id=>{const s=document.getElementById(id);DAYS.forEach((d,i)=>{const o=document.createElement('option');o.value=i;o.textContent=d;s.appendChild(o);});});
  ['dsMon','deMon'].forEach(id=>{const s=document.getElementById(id);for(let i=1;i<=12;i++){const o=document.createElement('option');o.value=i;o.textContent=MON[i];s.appendChild(o);}});
  ['tabWC','tabRC'].forEach(id=>{const s=document.getElementById(id);C.forEach((c,i)=>{const o=document.createElement('option');o.value=i;o.textContent=c.n;s.appendChild(o);});});
  document.getElementById('tabWC').value=1;document.getElementById('tabRC').value=0;
  ['nsStart','nsEnd'].forEach(id=>{const s=document.getElementById(id);for(let i=0;i<24;i++){const o=document.createElement('option');o.value=i;o.textContent=P(i)+':00';s.appendChild(o);}});
  document.getElementById('nsStart').value=22;document.getElementById('nsEnd').value=7;
  document.getElementById('nsBright').oninput=function(){document.getElementById('nsBrightVal').textContent=this.value;};
  document.getElementById('pomIntSlider').oninput=function(){document.getElementById('pomIntVal').textContent=this.value;};
  document.getElementById('dateIntSlider').oninput=function(){document.getElementById('dateIntVal').textContent=this.value;};
  initCities();
  connectWS();
}
function connectWS(){
  const h=location.hostname||'4.3.2.1';
  ws=new WebSocket('ws://'+h+'/ws');
  ws.onmessage=e=>{try{var d=JSON.parse(e.data);if(d.toast!==undefined){showToast(d.toast);return;}st=d;if(st.full&&saving){saving=false;document.getElementById('toast').classList.remove('show');}updateUI();}catch(x){}};
  ws.onclose=()=>setTimeout(connectWS,2000);
  ws.onerror=()=>ws.close();
}
function send(o){if(ws&&ws.readyState===1)ws.send(JSON.stringify(o));}
function sendSave(o){saving=true;document.getElementById('toast').classList.add('show');send(o);}
function togSec(el){el.classList.toggle('open');el.nextElementSibling.classList.toggle('show');}
function P(n){return String(n).padStart(2,'0');}
function updateUI(){var _sy=window.pageYOffset;
  if(firstState&&st.mode!==undefined){
    firstState=false;
    lastMode=st.mode;
    const tabs=['clock','stopwatch','timer','tabata','pomodoro'];
    const dest=tabs[st.mode]||'clock';
    document.querySelectorAll('.tab,.panel').forEach(e=>e.classList.remove('active'));
    document.querySelector('.tab[data-tab="'+dest+'"]').classList.add('active');
    document.getElementById(dest).classList.add('active');
  }
  if(st.mode!==undefined&&st.mode!==lastMode){
    lastMode=st.mode;
    const tabs=['clock','stopwatch','timer','tabata','pomodoro'];
    const dest=tabs[st.mode]||'clock';
    document.querySelectorAll('.tab,.panel').forEach(e=>e.classList.remove('active'));
    document.querySelector('.tab[data-tab="'+dest+'"]').classList.add('active');
    document.getElementById(dest).classList.add('active');
  }
  if(st.dv!==undefined){
    const t=document.getElementById('timeDisp');
    const v=st.dv;
    const d0=Math.floor(v/1000)%10,d1=Math.floor(v/100)%10,d2=Math.floor(v/10)%10,d3=v%10;
    var th;
    if(st.db){th='<span style="opacity:.3">--:--</span>';}
    else{th=P(d0*10+d1)+'<span class="blink">:</span>'+P(d2*10+d3);
      if(st.mode===0)th+=('<span class="sec">:'+P(st.s)+'</span>');
      if(st.mode===1)th+=('<span class="sec">.'+Math.floor(((st.swMs||0)%1000)/100)+'</span>');
    }
    if(prev.th!==th){prev.th=th;t.innerHTML=th;}
    if(prev.dv!==v||prev.db!==st.db){prev.dv=v;prev.db=st.db;updateSeg();}
    if(st.clrMode>=2){prev.clrMode=st.clrMode;animSegColors();t.style.color='var(--accent)';}else{if(prev.clrMode>=2){for(var i=0;i<4;i++)document.getElementById('sd'+i).style.removeProperty('--clr');prev.clrMode=st.clrMode;}if(st.clr&&prev.clr!==st.clr){prev.clr=st.clr;document.getElementById('segDisp').style.setProperty('--clr',st.clr);t.style.color=st.clr;}}
  }
  if(st.anim!==prev.anim){prev.anim=st.anim;document.querySelector('.seg-bar').classList.toggle('anim',!!st.anim);}
  var paused=(!st.swRun&&st.swMs>0&&st.mode===1)||(!st.tmRun&&!st.tmDone&&st.tmMs>0&&st.tmMs<st.tmDur&&st.mode===2)||(st.tabPaused&&st.mode===3);
  if(paused!==prev.paused){prev.paused=paused;document.querySelector('.seg-bar').classList.toggle('paused',paused);}
  if(st.synced!==prev.synced){prev.synced=st.synced;const sd=document.getElementById('syncDot'),stx=document.getElementById('syncText');if(st.synced){sd.className='dot on';stx.textContent='NTP OK';}else{sd.className='dot off';stx.textContent='Syncing...';}}
  if(st.wifiLost!==prev.wifiLost){prev.wifiLost=st.wifiLost;const wd=document.getElementById('wifiDot'),wt=document.getElementById('wifiText');if(st.wifiLost){wd.className='dot off';wt.textContent='WiFi Lost';}else{wd.className='dot on';wt.textContent='WiFi OK';}}
  if(st.full)document.querySelectorAll('.color-dot').forEach((d,i)=>d.classList.toggle('active',i===st.colorIdx));
  if(st.bright!==undefined&&st.full){document.getElementById('brightSlider').value=st.bright;document.getElementById('brightVal').textContent=st.bright;}
  if(st.mmss!==undefined&&st.full){
    document.getElementById('mmssToggle').checked=st.mmss;
    document.getElementById('fmtLabel').textContent=st.mmss?'MM:SS':'HH:MM';
  }
  if(st.swMs!==undefined){
    const ms=st.swMs,s=Math.floor(ms/1000),m=Math.floor(s/60);
    var swTxt=P(m)+':'+P(s%60)+'.'+Math.floor((ms%1000)/100);if(prev.swTxt!==swTxt){prev.swTxt=swTxt;document.getElementById('swDisp').textContent=swTxt;}
    var swBt,swBc;if(st.swRun){swBt='Stop';swBc='btn btn-danger';}else if(st.swMs>0){swBt='Resume';swBc='btn btn-primary';}else{swBt='Start';swBc='btn btn-primary';}
    if(prev.swBt!==swBt){prev.swBt=swBt;const b=document.getElementById('swToggle');b.textContent=swBt;b.className=swBc;}
    var swRstVis=(!st.swRun&&st.swMs>0)?'':'none';if(prev.swRst!==swRstVis){prev.swRst=swRstVis;document.getElementById('swReset2').style.display=swRstVis;}
  }
  if(st.tmMs!==undefined){
    var tmTxt;if(!st.tmRun&&(!st.tmMs||st.tmMs<=0)&&!st.tmDone){const m=getWheel('timerMinW'),s=getWheel('timerSecW');tmTxt=P(m)+':'+P(s);}else{const ms=Math.max(0,st.tmMs),s=Math.ceil(ms/1000),m=Math.floor(s/60);tmTxt=P(m)+':'+P(s%60);}
    if(prev.tmTxt!==tmTxt){prev.tmTxt=tmTxt;document.getElementById('timerDisp').textContent=tmTxt;}
    var tmBt,tmBc;if(st.tmRun){tmBt='Stop';tmBc='btn btn-danger';}else if(st.tmMs>0&&!st.tmDone){tmBt='Resume';tmBc='btn btn-primary';}else{tmBt='Start';tmBc='btn btn-primary';}
    if(prev.tmBt!==tmBt){prev.tmBt=tmBt;const b=document.getElementById('tmToggle');b.textContent=tmBt;b.className=tmBc;}
    if(st.tmRun!==prev.tmRun2){prev.tmRun2=st.tmRun;document.getElementById('timerSetRow').style.display=st.tmRun?'none':'';document.getElementById('tmSetBtn').style.display=st.tmRun?'none':'';}
  }
  if(st.tabMs!==undefined){
    const ms=Math.max(0,st.tabMs),s=Math.ceil(ms/1000),m=Math.floor(s/60);
    var tabTxt=P(m)+':'+P(s%60);if(prev.tabTxt!==tabTxt){prev.tabTxt=tabTxt;document.getElementById('tabDisp').textContent=tabTxt;}
    var phCls,phTxt;if(st.tabDone){phCls='tab-phase done';phTxt='DONE!';}else if(st.tabRun){phCls=st.tabWork?'tab-phase work':'tab-phase rest';phTxt=st.tabWork?'WORK':'REST';}else{phCls='tab-phase';phTxt='READY';}
    if(prev.phCls!==phCls){prev.phCls=phCls;const ph=document.getElementById('tabPhase');ph.className=phCls;ph.textContent=phTxt;}
    var tabInf='Interval: '+st.tabInt+' / '+(st.tabTotal||'?');if(prev.tabInf!==tabInf){prev.tabInf=tabInf;document.getElementById('tabInfo').textContent=tabInf;}
    var bTxt,bCls;if(st.tabRun){bTxt='Stop';bCls='btn btn-danger';}else{bTxt='Start';bCls='btn btn-primary';}
    if(prev.tabBtn!==bTxt){prev.tabBtn=bTxt;const b=document.getElementById('tabToggle');b.textContent=bTxt;b.className=bCls;}
    if(st.tabRun!==prev.tabRun||st.tabDone!==prev.tabDone){prev.tabRun=st.tabRun;prev.tabDone=st.tabDone;document.getElementById('tabReset2').style.display=(!st.tabRun&&(st.tabMs>0||st.tabDone))?'':'none';document.getElementById('tabCfg').style.display=st.tabRun?'none':'';}
  }
  if(st.tz!==undefined&&st.full)document.getElementById('tzSelect').value=st.tz;
  if(st.dst!==undefined&&st.full){document.querySelector('input[name=dst][value="'+st.dst+'"]').checked=true;document.getElementById('dstRules').style.display=st.dst==1?'':'none';}
  if(st.dsFL!==undefined&&st.full){
    document.getElementById('dsFL').value=st.dsFL?'1':'0';document.getElementById('dsDow').value=st.dsDow;
    document.getElementById('dsMon').value=st.dsMon;document.getElementById('dsHour').value=st.dsH;
    document.getElementById('deFL').value=st.deFL?'1':'0';document.getElementById('deDow').value=st.deDow;
    document.getElementById('deMon').value=st.deMon;document.getElementById('deHour').value=st.deH;
  }
  if(st.tbWork!==undefined&&!wheelsInit){
    wheelsInit=true;
    setWheel('tabWorkMinW',Math.floor(st.tbWork/60));setWheel('tabWorkSecW',st.tbWork%60);
    setWheel('tabRestMinW',Math.floor(st.tbRest/60));setWheel('tabRestSecW',st.tbRest%60);
    setWheel('tabIntW',st.tbInt2);document.getElementById('tabWC').value=st.tbWC;document.getElementById('tabRC').value=st.tbRC;
  }
  if(st.tbWork!==undefined){
    function fmtDur(s){return s>=60?(Math.floor(s/60)+'m'+((s%60)?((s%60)+'s'):'')):(s+'s');}
    document.getElementById('tabSumWork').textContent=fmtDur(st.tbWork);
    document.getElementById('tabSumRest').textContent=fmtDur(st.tbRest);
    document.getElementById('tabSumInt').textContent=st.tbInt2;
    var tsv=(st.tabRun||st.tabDone)?'block':'none';if(prev.tsv!==tsv){prev.tsv=tsv;document.getElementById('tabSummary').style.display=tsv;}
  }
  if(st.animTr!==undefined&&st.full){document.getElementById('animToggle').checked=st.animTr;}
  if(st.clrMode!==undefined&&st.full){var r2=document.querySelector('input[name=clrMode][value="'+st.clrMode+'"]');if(r2)r2.checked=true;}
  if(st.pomMs!==undefined){
    var ms=Math.max(0,st.pomMs),s=Math.ceil(ms/1000),m=Math.floor(s/60);
    var pomTxt=P(m)+':'+P(s%60);if(prev.pomTxt!==pomTxt){prev.pomTxt=pomTxt;document.getElementById('pomDisp').textContent=pomTxt;}
    var ppCls,ppTxt;if(st.pomDone){ppCls='tab-phase done';ppTxt='DONE!';}else if(st.pomRun){ppCls=st.pomWork?'tab-phase work':'tab-phase rest';ppTxt=st.pomWork?'FOCUS':'BREAK';}else{ppCls='tab-phase';ppTxt='READY';}
    if(prev.ppCls!==ppCls){prev.ppCls=ppCls;var ph=document.getElementById('pomPhase');ph.className=ppCls;ph.textContent=ppTxt;}
    var pomInf='Interval: '+st.pomInt+' / '+(st.pomTotal||4);if(prev.pomInf!==pomInf){prev.pomInf=pomInf;document.getElementById('pomInfo').textContent=pomInf;}
    var pBt;if(st.pomRun){pBt='Stop';}else{pBt='Start';}
    if(prev.pBt!==pBt){prev.pBt=pBt;var b=document.getElementById('pomToggle');b.textContent=pBt;b.className=st.pomRun?'btn btn-danger':'btn btn-primary';}
    if(st.pomRun!==prev.pomRun||st.pomDone!==prev.pomDone){prev.pomRun=st.pomRun;prev.pomDone=st.pomDone;document.getElementById('pomReset2').style.display=(!st.pomRun&&(st.pomMs>0||st.pomDone))?'':'none';}
  }
  if(st.pomTotal!==undefined&&st.full){document.getElementById('pomIntSlider').value=st.pomTotal;document.getElementById('pomIntVal').textContent=st.pomTotal;}
  if(st.dateEn!==undefined&&st.full){document.getElementById('dateToggle').checked=st.dateEn;document.getElementById('dateIntSlider').value=st.dateInt||30;document.getElementById('dateIntVal').textContent=st.dateInt||30;document.getElementById('tempToggle').checked=st.tempEn;var tr=document.querySelector('input[name=tempType][value="'+(st.tempFL?'1':'0')+'"]');if(tr)tr.checked=true;document.getElementById('tempClrToggle').checked=st.tempClr;}
  if(st.curTemp!==undefined){var el=document.getElementById('tempReadout');var txt='';if(st.curTemp!==null){txt=st.curTemp.toFixed(1)+'°C actual | '+st.curFL.toFixed(1)+'°C feels like';if(st.wLoc)txt+='\nLocation: '+st.wLoc;el.innerHTML=txt.replace('\n','<br>');}else{el.textContent='';}}
  if(st.full){var sel=document.getElementById('locSelect');if(st.wLat){var key=st.wLat.toFixed(2)+','+st.wLon.toFixed(2);sel.value=key;if(!sel.value||sel.value==='auto'){sel.value='other';document.getElementById('locManual').style.display='flex';document.getElementById('locLat').value=st.wLat;document.getElementById('locLon').value=st.wLon;}document.getElementById('locStatus').textContent='Location set'+(st.wLoc?' ('+st.wLoc+')':'');}else{sel.value='auto';}}

  if(st.colonEn!==undefined&&st.full)document.getElementById('colonToggle').checked=st.colonEn;
  if(st.buzzLv!==undefined&&st.full){var r=document.querySelector('input[name=buzz][value="'+st.buzzLv+'"]');if(r)r.checked=true;}
  if(st.cwBuzz!==undefined&&st.full)document.getElementById('cwToggle').checked=st.cwBuzz;
  if(st.devName!==undefined&&st.full){var dn=document.getElementById('devName');if(dn&&document.activeElement!==dn)dn.value=st.devName;updateDevAddr();}
  if(st.full){var nw=document.getElementById('nameWarn');if(nw)nw.style.display=st.nameConflict?'':'none';
    var pl=document.getElementById('peerList');if(pl){if(st.peerList&&st.peerList.length){pl.innerHTML='<div style="font-size:11px;color:var(--text2);margin-bottom:4px">Watches on this network:</div>'+st.peerList.map(function(p){var nm=(p.n||p.ip);return '<div style="font-size:13px;padding:2px 0">&#128337; <a href="http://'+p.ip+'/" style="color:var(--accent);text-decoration:none">'+nm+'</a> <span style="color:var(--text2);font-size:11px">'+p.ip+'</span></div>';}).join('');}else{pl.innerHTML='';}}}
  if(st.rssi!==undefined){var r=st.rssi,q=r>-50?'Excellent':r>-65?'Good':r>-75?'Weak':'Poor',cl=r>-50?'var(--success)':r>-65?'var(--accent)':r>-75?'#FFA500':'var(--danger)';document.getElementById('rssiLine').innerHTML='Signal: <strong style="color:'+cl+'">'+r+' dBm ('+q+')</strong>';}
  if(st.bdIntv!==undefined&&st.full){var s=document.getElementById('bdayIntv');if(s)s.value=st.bdIntv;}
  if(st.bdScrl!==undefined&&st.full){var s=document.getElementById('bdayScrl');if(s)s.value=st.bdScrl;}
  if(st.bdSpd!==undefined&&st.full){var s=document.getElementById('bdaySpd');if(s)s.value=st.bdSpd;}
  if(st.bdBuzz!==undefined&&st.full)document.getElementById('bdayBuzz').checked=st.bdBuzz;
  if(st.bdays){var bl=document.getElementById('bdayList');bl.innerHTML='';st.bdays.forEach(function(b,i){bl.innerHTML+='<div style="display:flex;justify-content:space-between;align-items:center;padding:4px 0;font-size:13px"><span>'+b.n+' - '+P(b.d)+'/'+P(b.m)+'</span><button class="btn btn-danger" style="padding:4px 10px;font-size:11px" onclick="delBday('+i+')">X</button></div>';});}
  if(st.tabPresets){var sel=document.getElementById('tabPresetSel');sel.innerHTML='';var pl=document.getElementById('tabPresetList');pl.innerHTML='';st.tabPresets.forEach(function(p,i){if(p.n){var o=document.createElement('option');o.value=i;o.textContent=p.n;sel.appendChild(o);pl.innerHTML+='<div style="display:flex;justify-content:space-between;align-items:center;padding:6px 8px;margin-bottom:4px;background:rgba(255,255,255,.03);border-radius:8px;font-size:13px"><span style="color:var(--text)">'+p.n+' <span style="color:var(--text2);font-size:11px">'+p.w+'s / '+p.r+'s / '+p.i+'r</span></span><span style="display:flex;gap:4px"><button class="btn btn-secondary" style="padding:4px 10px;font-size:11px" onclick="loadTabPresetIdx('+i+')">Load</button><button class="btn btn-danger" style="padding:4px 8px;font-size:11px" onclick="delTabPresetIdx('+i+')">X</button></span></div>';}});if(!pl.innerHTML)pl.innerHTML='<div style="font-size:12px;color:var(--text2);padding:4px">No presets saved</div>';}
  if(st.nsEn!==undefined&&st.full){
    document.getElementById('nsToggle').checked=st.nsEn;
    document.getElementById('nsStart').value=st.nsStart;
    document.getElementById('nsEnd').value=st.nsEnd;
    document.getElementById('nsBright').value=st.nsBright;
    document.getElementById('nsBrightVal').textContent=st.nsBright;
  }
  if(st.temp!==undefined&&st.temp!==prev.temp){prev.temp=st.temp;var tc=st.temp,tq=tc<50?'Normal':tc<60?'Warm':tc<65?'Hot':'THROTTLED',tcl=tc<50?'var(--success)':tc<60?'#FFA500':'var(--danger)';var tl='CPU: <strong style="color:'+tcl+'">'+tc+'&deg;C ('+tq+')</strong>';if(st.thermThrot)tl+=' <span style="color:var(--danger);font-size:11px">&#9888; Brightness reduced</span>';document.getElementById('tempLine').innerHTML=tl;}
  if(st.ssid)document.getElementById('wifiSSID').textContent=st.ssid;
  if(st.ip)document.getElementById('wifiIP').textContent=st.ip;
  var hasPeers=st.peers&&st.peers>0;
  if(hasPeers!==prev.hasPeers){prev.hasPeers=hasPeers;['swBcastBtn','tmBcastBtn','tabBcastBtn','pomBcastBtn','syncCfgBtn'].forEach(function(id){var e=document.getElementById(id);if(e)e.style.display=hasPeers?'':'none';});var np=document.getElementById('syncNoPeers');if(np)np.style.display=hasPeers?'none':'';}
  if(st.bcasting&&!prev.bcasting){prev.bcasting=true;document.getElementById('timeDisp').insertAdjacentHTML('afterend','<div id="bcastBadge" style="text-align:center;font-size:11px;color:#6e7dff;font-weight:700;margin-top:4px">BROADCASTING TO ALL</div>');}
  if(!st.bcasting&&prev.bcasting){prev.bcasting=false;var bb=document.getElementById('bcastBadge');if(bb)bb.remove();}
  if(window.pageYOffset!==_sy)window.scrollTo(0,_sy);
}
function swToggle(bc){
  if(st.swRun) send({cmd:'sw',action:'stop'});
  else send({cmd:'sw',action:'start',broadcast:!!bc});
}
function tmSet(){var d=(getWheel('timerMinW')*60+getWheel('timerSecW'))*1000;send({cmd:'timer',action:'set',duration:d});}
function tmToggle(bc){
  if(st.tmRun) send({cmd:'timer',action:'stop'});
  else if(st.tmMs>0&&!st.tmDone) send({cmd:'timer',action:'start',broadcast:!!bc});
  else send({cmd:'timer',action:'start',duration:(getWheel('timerMinW')*60+getWheel('timerSecW'))*1000,broadcast:!!bc});
}
function tabToggle(bc){
  if(st.tabRun) send({cmd:'tabata',action:'stop'});
  else send({cmd:'tabata',action:'start',broadcast:!!bc});
}

function saveTabata(){var ws=getWheel('tabWorkMinW')*60+getWheel('tabWorkSecW'),rs=getWheel('tabRestMinW')*60+getWheel('tabRestSecW');send({cmd:'tabata_cfg',work:ws||20,rest:rs||10,intervals:getWheel('tabIntW')||8,workColor:+document.getElementById('tabWC').value,restColor:+document.getElementById('tabRC').value});}
function toggleAnim(){send({cmd:'animtoggle',value:document.getElementById('animToggle').checked});}
function setTimezone(){sendSave({cmd:'timezone',value:+document.getElementById('tzSelect').value});}
function setDST(v){sendSave({cmd:'dst',value:v});}
function toggleMMSS(){send({cmd:'clockfmt',mmss:document.getElementById('mmssToggle').checked});}
function saveDSTRules(){sendSave({cmd:'dst_rules',dsFL:document.getElementById('dsFL').value==='1',dsDow:+document.getElementById('dsDow').value,dsMon:+document.getElementById('dsMon').value,dsH:+document.getElementById('dsHour').value,deFL:document.getElementById('deFL').value==='1',deDow:+document.getElementById('deDow').value,deMon:+document.getElementById('deMon').value,deH:+document.getElementById('deHour').value});}
function resetDSTIsrael(){sendSave({cmd:'dst_reset_israel'});}
function resetWifi(){if(confirm('Reset WiFi? Watch will restart.'))send({cmd:'resetwifi'});}
function applyCustomColor(){const h=document.getElementById('customColor').value;send({cmd:'customcolor',r:parseInt(h.substr(1,2),16),g:parseInt(h.substr(3,2),16),b:parseInt(h.substr(5,2),16)});}
function saveNightShift(){sendSave({cmd:'nightshift',enabled:document.getElementById('nsToggle').checked,start:+document.getElementById('nsStart').value,end:+document.getElementById('nsEnd').value,bright:+document.getElementById('nsBright').value});}
function pomToggle(bc){if(st.pomRun)send({cmd:'pom',action:'stop'});else send({cmd:'pom',action:'start',broadcast:!!bc});}
function savePomInt(){send({cmd:'pom_cfg',intervals:+document.getElementById('pomIntSlider').value});}
const CITIES=[['Jerusalem',31.77,35.22],['Tel Aviv',32.08,34.78],['Haifa',32.79,34.99],['Beer Sheva',31.25,34.79],['Rishon LeZion',31.96,34.80],['Petah Tikva',32.09,34.89],['Ashdod',31.80,34.65],['Netanya',32.33,34.86],['Holon',32.02,34.78],['Bnei Brak',32.09,34.83],['Ramat Gan',32.07,34.82],['Rehovot',31.90,34.81],['Ashkelon',31.67,34.57],['Bat Yam',32.02,34.75],['Herzliya',32.16,34.84],['Kfar Saba',32.18,34.91],['Hadera',32.44,34.92],['Modiin',31.90,35.01],['Nazareth',32.70,35.30],['Eilat',29.56,34.95],['Raanana',32.18,34.87],['Tiberias',32.79,35.53],['Acre',32.93,35.07],['Nahariya',33.01,35.10],['Kiryat Gat',31.61,34.76],['Afula',32.61,35.29],['Carmiel',32.91,35.30],['Arad',31.26,35.21]];
function initCities(){var s=document.getElementById('locSelect');var other=s.lastChild;CITIES.sort(function(a,b){return a[0].localeCompare(b[0]);}).forEach(function(c){var o=document.createElement('option');o.value=c[1].toFixed(2)+','+c[2].toFixed(2);o.textContent=c[0];s.insertBefore(o,other);});}
function setLoc(){var s=document.getElementById('locSelect'),v=s.value;document.getElementById('locManual').style.display=v==='other'?'flex':'none';if(v==='auto'){sendSave({cmd:'setloc',lat:0,lon:0,city:''});document.getElementById('locStatus').textContent='Using auto-detect';}else if(v!=='other'){var p=v.split(','),name=s.options[s.selectedIndex].textContent;sendSave({cmd:'setloc',lat:parseFloat(p[0]),lon:parseFloat(p[1]),city:name});document.getElementById('locStatus').textContent='Set: '+name;}}
function saveManualLoc(){var la=parseFloat(document.getElementById('locLat').value),lo=parseFloat(document.getElementById('locLon').value);if(la&&lo){sendSave({cmd:'setloc',lat:la,lon:lo});document.getElementById('locStatus').textContent='Saved ('+la.toFixed(2)+', '+lo.toFixed(2)+')';}}
function saveInfo(){sendSave({cmd:'datedisp',enabled:document.getElementById('dateToggle').checked,interval:+document.getElementById('dateIntSlider').value,tempEn:document.getElementById('tempToggle').checked,feelsLike:document.querySelector('input[name=tempType]:checked').value==='1',tempClr:document.getElementById('tempClrToggle').checked});}
function addBday(){var n=document.getElementById('bdayName').value,d=+document.getElementById('bdayDay').value,m=+document.getElementById('bdayMon').value;if(n&&d&&m)sendSave({cmd:'bday_add',name:n,day:d,month:m});document.getElementById('bdayName').value='';}
function mdnsHost(n){n=(n||'').toLowerCase().replace(/[^a-z0-9-]/g,'-').replace(/^-+|-+$/g,'');if(!n)n='watch';return n.indexOf('neotick-')===0?n:'neotick-'+n;}
function updateDevAddr(){var v=document.getElementById('devName').value;var url='http://'+mdnsHost(v)+'.local';document.getElementById('devAddr').innerHTML='This watch: <a href="'+url+'" style="color:var(--accent);text-decoration:none">'+url+'</a>';}
function saveDevName(){var v=document.getElementById('devName').value.trim();if(v)sendSave({cmd:'devname',value:v});}
function syncAll(){if(confirm('Copy this watch\'s settings to all other watches on the network?'))send({cmd:'synccfg'});}
function showToast(m){var t=document.getElementById('toast');t.textContent=m;t.classList.add('show');clearTimeout(window._tt);window._tt=setTimeout(function(){t.classList.remove('show');t.textContent='Saving...';},2500);}
function delBday(i){sendSave({cmd:'bday_del',index:i});}
function loadTabPreset(){sendSave({cmd:'tab_preset_load',index:+document.getElementById('tabPresetSel').value});}
function saveTabPreset(){var n=document.getElementById('tabPresetName').value;if(!n)return;var ws=getWheel('tabWorkMinW')*60+getWheel('tabWorkSecW'),rs=getWheel('tabRestMinW')*60+getWheel('tabRestSecW');sendSave({cmd:'tab_preset_save',name:n,work:ws||20,rest:rs||10,intervals:getWheel('tabIntW')||8});}
function delTabPreset(){var i=+document.getElementById('tabPresetSel').value;sendSave({cmd:'tab_preset_del',index:i});}
function loadTabPresetIdx(i){sendSave({cmd:'tab_preset_load',index:i});}
function delTabPresetIdx(i){sendSave({cmd:'tab_preset_del',index:i});}
function startOTA(){
  var pass=document.getElementById('otaPass').value;
  if(pass!=='neotick2024'){alert('Wrong password');return;}
  var file=document.getElementById('otaFile').files[0];
  if(!file){alert('Select a .bin file first');return;}
  if(!file.name.endsWith('.bin')){alert('Only .bin files are allowed');return;}
  if(!confirm('Are you sure? The watch will restart after update.')){return;}
  document.getElementById('otaForm').style.display='none';
  document.getElementById('otaProgress').style.display='block';
  var xhr=new XMLHttpRequest();
  var form=new FormData();form.append('firmware',file);
  xhr.upload.onprogress=function(e){if(e.lengthComputable){var pct=Math.round(e.loaded/e.total*100);document.getElementById('otaBar').style.width=pct+'%';document.getElementById('otaPct').textContent=pct+'%';document.getElementById('otaStatus').textContent='Uploading... '+pct+'%';}};
  xhr.onload=function(){if(xhr.responseText==='FAIL'){document.getElementById('otaStatus').textContent='Update Failed!';document.getElementById('otaStatus').style.color='var(--danger)';document.getElementById('otaPct').textContent='The file may be corrupted. Please try again.';setTimeout(function(){document.getElementById('otaForm').style.display='block';document.getElementById('otaProgress').style.display='none';},3000);}else{document.getElementById('otaBar').style.width='100%';document.getElementById('otaStatus').textContent='Update Successful!';document.getElementById('otaStatus').style.color='var(--success)';var cd=5;document.getElementById('otaPct').textContent='Watch will restart in '+cd+'s. Do not disconnect power.';var ti=setInterval(function(){cd--;document.getElementById('otaPct').textContent='Watch will restart in '+cd+'s. Do not disconnect power.';if(cd<=0){clearInterval(ti);document.getElementById('otaPct').textContent='Restarting... Please wait and reconnect.';}},1000);}};
  xhr.onerror=function(){document.getElementById('otaStatus').textContent='Upload Error';document.getElementById('otaStatus').style.color='var(--danger)';document.getElementById('otaPct').textContent='Network error. Check connection.';};
  xhr.open('POST','/update?pass='+encodeURIComponent(pass));xhr.send(form);
}
init();
</script>
</body></html>
)=====";

// ======================== Stopwatch ========================
void WebUI::stopwatchStart() { if (!m_swRunning) { m_swStartTime = millis(); m_swRunning = true; } }  // resumes from accumulated
void WebUI::stopwatchRestart() { m_swAccumulated = 0; m_swStartTime = millis(); m_swRunning = true; }  // fresh start
void WebUI::stopwatchStop() { if (m_swRunning) { m_swAccumulated += millis() - m_swStartTime; m_swRunning = false; } }
void WebUI::stopwatchReset() { m_swRunning = false; m_swAccumulated = 0; m_swStartTime = 0; }
unsigned long WebUI::getStopwatchElapsed() const { return m_swRunning ? m_swAccumulated + (millis() - m_swStartTime) : m_swAccumulated; }

// ======================== Timer ========================
void WebUI::timerSet(unsigned long durationMs) { m_timerDuration = durationMs; m_timerRemaining = durationMs; m_timerDone = false; }
void WebUI::timerStart() { if (!m_timerRunning && m_timerRemaining > 0) { m_timerStartTime = millis(); m_timerRunning = true; m_timerDone = false; } }
void WebUI::timerStop() { if (m_timerRunning) { unsigned long e = millis() - m_timerStartTime; m_timerRemaining = (e >= m_timerRemaining) ? 0 : m_timerRemaining - e; m_timerRunning = false; } }
void WebUI::timerReset() { m_timerRunning = false; m_timerRemaining = m_timerDuration; m_timerDone = false; }
long WebUI::getTimerRemaining() const { if (m_timerRunning) { unsigned long e = millis() - m_timerStartTime; return (e >= m_timerRemaining) ? 0 : (long)(m_timerRemaining - e); } return (long)m_timerRemaining; }

// ======================== Tabata ========================
void WebUI::tabataStart() {
    if (m_tabDone) tabataReset();
    if (!m_tabRunning) {
        m_tabRunning = true; m_tabWorkPhase = true;
        if (m_tabCurrentInterval < 1) m_tabCurrentInterval = 1;
        m_tabPhaseDuration = (unsigned long)m_settings->tabata.workSec * 1000;
        m_tabPhaseStart = millis();
    }
}
void WebUI::tabataStop() { if (m_tabRunning) { unsigned long e = millis() - m_tabPhaseStart; m_tabPhaseDuration = (e >= m_tabPhaseDuration) ? 0 : m_tabPhaseDuration - e; m_tabRunning = false; } }
void WebUI::tabataReset() { m_tabRunning = false; m_tabDone = false; m_tabWorkPhase = true; m_tabCurrentInterval = 1; m_tabPhaseDuration = (unsigned long)m_settings->tabata.workSec * 1000; m_tabPhaseStart = 0; }
long WebUI::getTabataPhaseRemaining() const { if (!m_tabRunning) return (long)m_tabPhaseDuration; unsigned long e = millis() - m_tabPhaseStart; return (e >= m_tabPhaseDuration) ? 0 : (long)(m_tabPhaseDuration - e); }
void WebUI::tabataAdvance() {
    if (!m_tabRunning || getTabataPhaseRemaining() > 0) return;
    m_tabPhaseChanged = true;  // signal main.cpp to buzz
    if (m_tabWorkPhase) {
        m_tabWorkPhase = false; m_tabPhaseDuration = (unsigned long)m_settings->tabata.restSec * 1000; m_tabPhaseStart = millis();
    } else {
        m_tabCurrentInterval++;
        if (m_tabCurrentInterval > m_settings->tabata.intervals) { m_tabRunning = false; m_tabDone = true; m_tabPhaseDuration = 0; }
        else { m_tabWorkPhase = true; m_tabPhaseDuration = (unsigned long)m_settings->tabata.workSec * 1000; m_tabPhaseStart = millis(); }
    }
}

// ======================== Pomodoro ========================
void WebUI::pomodoroStart() {
    if (m_pomDone) pomodoroReset();
    if (!m_pomRunning) {
        m_pomRunning = true; m_pomWorkPhase = true;
        if (m_pomCurrentInterval < 1) m_pomCurrentInterval = 1;
        m_pomPhaseDuration = (unsigned long)POMODORO_WORK_SEC * 1000;
        m_pomPhaseStart = millis();
    }
}
void WebUI::pomodoroStop() {
    if (m_pomRunning) { unsigned long e = millis() - m_pomPhaseStart; m_pomPhaseDuration = (e >= m_pomPhaseDuration) ? 0 : m_pomPhaseDuration - e; m_pomRunning = false; }
}
void WebUI::pomodoroReset() {
    m_pomRunning = false; m_pomDone = false; m_pomWorkPhase = true; m_pomCurrentInterval = 1;
    m_pomPhaseDuration = (unsigned long)POMODORO_WORK_SEC * 1000; m_pomPhaseStart = 0;
}
long WebUI::getPomodoroPhaseRemaining() const {
    if (!m_pomRunning) return (long)m_pomPhaseDuration;
    unsigned long e = millis() - m_pomPhaseStart;
    return (e >= m_pomPhaseDuration) ? 0 : (long)(m_pomPhaseDuration - e);
}
void WebUI::pomodoroAdvance() {
    if (!m_pomRunning || getPomodoroPhaseRemaining() > 0) return;
    if (m_pomWorkPhase) {
        m_pomWorkPhase = false; m_pomPhaseDuration = (unsigned long)POMODORO_BREAK_SEC * 1000; m_pomPhaseStart = millis();
    } else {
        m_pomCurrentInterval++;
        if (m_pomCurrentInterval > m_settings->pomodoroIntervals) { m_pomRunning = false; m_pomDone = true; m_pomPhaseDuration = 0; }
        else { m_pomWorkPhase = true; m_pomPhaseDuration = (unsigned long)POMODORO_WORK_SEC * 1000; m_pomPhaseStart = millis(); }
    }
}

// ======================== JSON Helpers ========================
static int extractInt(const String& json, const char* key) {
    String s = String("\"") + key + "\""; int p = json.indexOf(s); if (p < 0) return -9999;
    p = json.indexOf(':', p); if (p < 0) return -9999; p++;
    while (p < (int)json.length() && (json[p] == ' ' || json[p] == '"')) p++;
    bool neg = false; if (p < (int)json.length() && json[p] == '-') { neg = true; p++; }
    String n; while (p < (int)json.length() && json[p] >= '0' && json[p] <= '9') n += json[p++];
    if (n.length() == 0) return -9999; int v = n.toInt(); return neg ? -v : v;
}
static String extractString(const String& json, const char* key) {
    String s = String("\"") + key + "\""; int p = json.indexOf(s); if (p < 0) return "";
    int a = json.indexOf('"', p + s.length() + 1), b = json.indexOf('"', a + 1);
    return (a < 0 || b < 0) ? "" : json.substring(a + 1, b);
}
static bool extractBool(const String& json, const char* key) {
    String s = String("\"") + key + "\""; int p = json.indexOf(s); if (p < 0) return false;
    int c = json.indexOf(',', p), e = json.indexOf('}', p);
    int end = (c >= 0 && c < e) ? c : e;
    return json.substring(p, end).indexOf("true") >= 0;
}
static float extractFloat(const String& json, const char* key) {
    String s = String("\"") + key + "\""; int p = json.indexOf(s); if (p < 0) return 0;
    p = json.indexOf(':', p); if (p < 0) return 0; p++;
    while (p < (int)json.length() && json[p] == ' ') p++;
    String n; while (p < (int)json.length() && (json[p] == '-' || json[p] == '.' || (json[p] >= '0' && json[p] <= '9'))) n += json[p++];
    return n.toFloat();
}

// ======================== WebSocket Handler ========================
void WebUI::handleWebSocketMessage(AsyncWebSocketClient* client, uint8_t* data, size_t len) {
    String msg; msg.reserve(len);
    for (size_t i = 0; i < len; i++) msg += (char)data[i];
    int cs = msg.indexOf("\"cmd\""); if (cs < 0) return;
    int vs = msg.indexOf('"', cs + 6), ve = msg.indexOf('"', vs + 1);
    if (vs < 0 || ve < 0) return;
    String cmd = msg.substring(vs + 1, ve);

    if (cmd == "color") { int i = extractInt(msg, "index"); if (i >= 0 && i < m_display->getColorCount()) { m_display->setColorByIndex(i); m_settings->colorIndex = i; m_pendingSave = millis(); } }
    else if (cmd == "customcolor") { int r = extractInt(msg, "r"), g = extractInt(msg, "g"), b = extractInt(msg, "b"); if (r >= 0 && r <= 255 && g >= 0 && g <= 255 && b >= 0 && b <= 255) { m_display->setColor(CRGB(r, g, b)); m_settings->colorIndex = -1; m_settings->customR = r; m_settings->customG = g; m_settings->customB = b; m_pendingSave = millis(); } }
    else if (cmd == "brightness") { int v = extractInt(msg, "value"); if (v >= 0 && v <= MAX_BRIGHTNESS) { m_display->setBrightness(v); m_settings->brightness = v; m_pendingSave = millis(); } }
    else if (cmd == "mode") { int v = extractInt(msg, "value"); if (v >= 0 && v <= 4) m_mode = (DisplayMode)v; }
    else if (cmd == "clockfmt") { m_settings->clockShowMMSS = extractBool(msg, "mmss"); m_pendingSave = millis(); }
    else if (cmd == "sw") { String a = extractString(msg, "action"); if (a == "start") { stopwatchStart(); if (extractBool(msg, "broadcast") && m_heartbeat) m_heartbeat->startBroadcast(MODE_STOPWATCH, true, 0, true, 1, 1, false); } else if (a == "restart") { stopwatchRestart(); if (extractBool(msg, "broadcast") && m_heartbeat) m_heartbeat->startBroadcast(MODE_STOPWATCH, true, 0, true, 1, 1, false); } else if (a == "stop") { stopwatchStop(); if (m_heartbeat) m_heartbeat->stopBroadcast(); } else if (a == "reset") { stopwatchReset(); if (m_heartbeat) m_heartbeat->stopBroadcast(); } }
    else if (cmd == "timer") { String a = extractString(msg, "action"); if (a == "start") { int d = extractInt(msg, "duration"); if (d > 0) timerSet(d); timerStart(); if (extractBool(msg, "broadcast") && m_heartbeat) m_heartbeat->startBroadcast(MODE_TIMER, true, getTimerRemaining(), true, 1, 1, false); } else if (a == "set") { int d = extractInt(msg, "duration"); if (d > 0) timerSet(d); } else if (a == "stop") { timerStop(); if (m_heartbeat) m_heartbeat->stopBroadcast(); } else if (a == "reset") { timerReset(); if (m_heartbeat) m_heartbeat->stopBroadcast(); } }
    else if (cmd == "tabata") { String a = extractString(msg, "action"); if (a == "start") { tabataStart(); if (extractBool(msg, "broadcast") && m_heartbeat) m_heartbeat->startBroadcast(MODE_TABATA, true, getTabataPhaseRemaining(), true, 1, m_settings->tabata.intervals, false); } else if (a == "stop") { tabataStop(); if (m_heartbeat) m_heartbeat->stopBroadcast(); } else if (a == "reset") { tabataReset(); if (m_heartbeat) m_heartbeat->stopBroadcast(); } }
    else if (cmd == "tabata_cfg") { int w = extractInt(msg, "work"), r = extractInt(msg, "rest"), n = extractInt(msg, "intervals"), wc = extractInt(msg, "workColor"), rc = extractInt(msg, "restColor"); if (w > 0) m_settings->tabata.workSec = w; if (r > 0) m_settings->tabata.restSec = r; if (n > 0) m_settings->tabata.intervals = n; if (wc >= 0) m_settings->tabata.workColorIdx = wc; if (rc >= 0) m_settings->tabata.restColorIdx = rc; m_pendingSave = millis(); tabataReset(); }
    else if (cmd == "timezone") { long v = (long)extractInt(msg, "value"); m_timeMgr->setTimezoneOffset(v); m_settings->timezoneOffset = v; m_pendingSave = millis(); }
    else if (cmd == "dst") { int v = extractInt(msg, "value"); m_timeMgr->setDSTMode(v); m_settings->dstMode = v; m_pendingSave = millis(); }
    else if (cmd == "dst_rules") { DSTRule s, e; s.isLast = extractBool(msg, "dsFL"); s.dayOfWeek = extractInt(msg, "dsDow"); s.month = extractInt(msg, "dsMon"); s.hour = extractInt(msg, "dsH"); e.isLast = extractBool(msg, "deFL"); e.dayOfWeek = extractInt(msg, "deDow"); e.month = extractInt(msg, "deMon"); e.hour = extractInt(msg, "deH"); m_settings->dstStart = s; m_settings->dstEnd = e; m_timeMgr->setDSTRules(s, e); m_pendingSave = millis(); }
    else if (cmd == "dst_reset_israel") { m_settings->dstStart = DST_ISRAEL_START; m_settings->dstEnd = DST_ISRAEL_END; m_timeMgr->resetDSTToIsrael(); m_pendingSave = millis(); }
    else if (cmd == "animate") { m_animationRequested = true; }
    else if (cmd == "colon") { m_settings->colonLedsEnabled = extractBool(msg, "enabled"); m_pendingSave = millis(); }
    else if (cmd == "colormode") { int v = extractInt(msg, "value"); if (v >= 0 && v <= 3) { m_settings->colorMode = v; m_pendingSave = millis(); } }
    else if (cmd == "animtoggle") { m_settings->animateTransitions = extractBool(msg, "value"); m_pendingSave = millis(); }
    else if (cmd == "nightshift") { m_settings->nightShiftEnabled = extractBool(msg, "enabled"); int sh = extractInt(msg, "start"), eh = extractInt(msg, "end"), nb = extractInt(msg, "bright"); if (sh >= 0 && sh <= 23) m_settings->nightShiftStartHour = sh; if (eh >= 0 && eh <= 23) m_settings->nightShiftEndHour = eh; if (nb >= 0 && nb <= MAX_BRIGHTNESS) m_settings->nightShiftBrightness = nb; m_pendingSave = millis(); }
    else if (cmd == "pom") { String a = extractString(msg, "action"); if (a == "start") { pomodoroStart(); if (extractBool(msg, "broadcast") && m_heartbeat) m_heartbeat->startBroadcast(MODE_POMODORO, true, getPomodoroPhaseRemaining(), true, 1, m_settings->pomodoroIntervals, false); } else if (a == "stop") { pomodoroStop(); if (m_heartbeat) m_heartbeat->stopBroadcast(); } else if (a == "reset") { pomodoroReset(); if (m_heartbeat) m_heartbeat->stopBroadcast(); } }
    else if (cmd == "pom_cfg") { int n = extractInt(msg, "intervals"); if (n > 0 && n <= 8) { m_settings->pomodoroIntervals = n; m_pendingSave = millis(); } }
    else if (cmd == "datedisp") { m_settings->showDateEnabled = extractBool(msg, "enabled"); m_settings->showTempEnabled = extractBool(msg, "tempEn"); m_settings->tempFeelsLike = extractBool(msg, "feelsLike"); m_settings->tempColorByValue = extractBool(msg, "tempClr"); int iv = extractInt(msg, "interval"); if (iv > 0) m_settings->showDateIntervalSec = iv; m_pendingSave = millis(); }
    else if (cmd == "setloc") { m_settings->weatherLat = extractFloat(msg, "lat"); m_settings->weatherLon = extractFloat(msg, "lon"); String cn = extractString(msg, "city"); extern char weatherCity[32]; if (cn.length()) { strncpy(weatherCity, cn.c_str(), 31); weatherCity[31] = 0; } else if (m_settings->weatherLat == 0) { weatherCity[0] = 0; } m_pendingSave = millis(); extern volatile bool weatherFetchNow; weatherFetchNow = true; }
    else if (cmd == "buzzer") { int lv = extractInt(msg, "level"); if (lv >= 0 && lv <= 2) { m_settings->buzzerLevel = lv; m_pendingSave = millis(); } }
    else if (cmd == "buzztest") { m_buzzerTestRequested = true; }
    else if (cmd == "clockwork") { m_settings->clockworkBuzzer = extractBool(msg, "enabled"); m_pendingSave = millis(); }
    else if (cmd == "devname") { String n = extractString(msg, "value"); if (n.length() && m_heartbeat) m_heartbeat->setDeviceName(n); }

    else if (cmd == "bday_interval") { int v = extractInt(msg, "value"); if (v >= 1 && v <= 240) { m_settings->birthdayIntervalMins = v; m_pendingSave = millis(); } }
    else if (cmd == "bday_buzzer") { m_settings->birthdayBuzzer = extractBool(msg, "enabled"); m_pendingSave = millis(); }
    else if (cmd == "bday_scrollcount") { int v = extractInt(msg, "value"); if (v >= 1 && v <= 5) { m_settings->birthdayScrollCount = v; m_pendingSave = millis(); } }
    else if (cmd == "bday_scrollspeed") { int v = extractInt(msg, "value"); if (v >= 1 && v <= 5) { m_settings->birthdayScrollSpeed = v; m_pendingSave = millis(); } }
    else if (cmd == "bday_add") { int idx = m_settings->birthdayCount; if (idx < MAX_BIRTHDAYS) { Birthday b; String n = extractString(msg, "name"); strncpy(b.name, n.c_str(), 15); b.name[15] = 0; b.day = extractInt(msg, "day"); b.month = extractInt(msg, "month"); m_configStore->saveBirthday(idx, b); m_settings->birthdayCount = idx + 1; Preferences p; p.begin(PREFS_NS, false); p.putUChar("bdCnt", m_settings->birthdayCount); p.end(); } if (m_ws) m_ws->textAll(buildStateJSON()); return; }
    else if (cmd == "bday_del") { int i = extractInt(msg, "index"); if (i >= 0 && i < m_settings->birthdayCount) { for (int j = i; j < m_settings->birthdayCount - 1; j++) { Birthday b; m_configStore->loadBirthday(j + 1, b); m_configStore->saveBirthday(j, b); } m_settings->birthdayCount--; Birthday empty; m_configStore->saveBirthday(m_settings->birthdayCount, empty); Preferences p; p.begin(PREFS_NS, false); p.putUChar("bdCnt", m_settings->birthdayCount); p.end(); } if (m_ws) m_ws->textAll(buildStateJSON()); return; }
    else if (cmd == "tab_preset_save") { String n = extractString(msg, "name"); int w = extractInt(msg, "work"), r = extractInt(msg, "rest"), iv = extractInt(msg, "intervals"); TabataPreset p; strncpy(p.name, n.c_str(), 15); p.name[15] = 0; p.workSec = w; p.restSec = r; p.intervals = iv; for (int i = 0; i < MAX_TABATA_PRESETS; i++) { TabataPreset ex; m_configStore->loadTabataPreset(i, ex); if (ex.name[0] == 0) { m_configStore->saveTabataPreset(i, p); break; } } if (m_ws) m_ws->textAll(buildStateJSON()); return; }
    else if (cmd == "tab_preset_del") { int i = extractInt(msg, "index"); if (i >= 0 && i < MAX_TABATA_PRESETS) { TabataPreset empty; m_configStore->saveTabataPreset(i, empty); } if (m_ws) m_ws->textAll(buildStateJSON()); return; }
    else if (cmd == "tab_preset_load") { int i = extractInt(msg, "index"); TabataPreset p; m_configStore->loadTabataPreset(i, p); if (p.name[0]) { m_settings->tabata.workSec = p.workSec; m_settings->tabata.restSec = p.restSec; m_settings->tabata.intervals = p.intervals; m_pendingSave = millis(); tabataReset(); } if (m_ws) m_ws->textAll(buildStateJSON()); return; }
    else if (cmd == "synccfg") { m_syncRequested = true; return; }
    else if (cmd == "resetwifi") { Preferences p; p.begin(NVS_NAMESPACE, false); p.remove("ssid"); p.remove("pass"); p.end(); delay(500); ESP.restart(); }
    if (m_ws) m_ws->textAll(buildStateJSON());
}

// ======================== State Broadcast ========================
// Fast state: only dynamic values that change every second (~300 bytes)
String WebUI::buildFastJSON() {
    String j = "{\"h\":"; j += m_timeMgr->getHours();
    j += ",\"m\":"; j += m_timeMgr->getMinutes();
    j += ",\"s\":"; j += m_timeMgr->getSeconds();
    j += ",\"dv\":"; j += m_displayValue;
    j += ",\"dt\":"; j += m_displayTemp ? "true" : "false";
    j += ",\"db\":"; j += m_displayBlank ? "true" : "false";
    j += ",\"mode\":"; j += (int)m_mode;
    CRGB c = m_display->activeColor();
    char hex[8]; snprintf(hex, sizeof(hex), "#%02X%02X%02X", c.r, c.g, c.b);
    j += ",\"clr\":\""; j += hex; j += "\"";
    j += ",\"swMs\":"; j += getStopwatchElapsed();
    j += ",\"swRun\":"; j += m_swRunning ? "true" : "false";
    j += ",\"tmMs\":"; j += getTimerRemaining();
    j += ",\"tmRun\":"; j += m_timerRunning ? "true" : "false";
    j += ",\"tmDone\":"; j += m_timerDone ? "true" : "false";
    j += ",\"tmDur\":"; j += m_timerDuration;
    j += ",\"tabMs\":"; j += getTabataPhaseRemaining();
    j += ",\"tabRun\":"; j += m_tabRunning ? "true" : "false";
    j += ",\"tabWork\":"; j += m_tabWorkPhase ? "true" : "false";
    j += ",\"tabInt\":"; j += m_tabCurrentInterval;
    j += ",\"tabDone\":"; j += m_tabDone ? "true" : "false";
    j += ",\"tabPaused\":"; j += (!m_tabRunning && !m_tabDone && m_tabPhaseStart > 0) ? "true" : "false";
    j += ",\"pomMs\":"; j += getPomodoroPhaseRemaining();
    j += ",\"pomRun\":"; j += m_pomRunning ? "true" : "false";
    j += ",\"pomWork\":"; j += m_pomWorkPhase ? "true" : "false";
    j += ",\"pomInt\":"; j += m_pomCurrentInterval;
    j += ",\"pomDone\":"; j += m_pomDone ? "true" : "false";
    j += ",\"anim\":"; j += m_animating ? "true" : "false";
    j += ",\"synced\":"; j += m_timeMgr->isTimeSynced() ? "true" : "false";
    j += ",\"wifiLost\":"; j += (WiFi.status() != WL_CONNECTED) ? "true" : "false";
    j += ",\"clrMode\":"; j += m_settings->colorMode;
    int cpuTemp = (int)temperatureRead();
    j += ",\"temp\":"; j += cpuTemp;
    if (cpuTemp >= THERMAL_THROTTLE_TEMP) { j += ",\"thermThrot\":true"; }
    if (m_heartbeat) {
        j += ",\"peers\":"; j += m_heartbeat->getPeerCount();
        if (m_heartbeat->isBroadcasting()) j += ",\"bcasting\":true";
    }
    j += "}";
    return j;
}

// Full state: everything including settings (sent on connect + after settings change)
String WebUI::buildStateJSON() {
    String j = buildFastJSON();
    // Remove closing brace and append settings
    j.remove(j.length() - 1);
    j += ",\"full\":true";
    j += ",\"colorIdx\":"; j += m_display->getColorIndex();
    j += ",\"bright\":"; j += m_settings->brightness;
    j += ",\"mmss\":"; j += m_settings->clockShowMMSS ? "true" : "false";
    j += ",\"tabTotal\":"; j += m_settings->tabata.intervals;
    j += ",\"tz\":"; j += m_timeMgr->getTimezoneOffset();
    j += ",\"dst\":"; j += m_timeMgr->getDSTMode();
    j += ",\"dsFL\":"; j += m_settings->dstStart.isLast ? "true" : "false";
    j += ",\"dsDow\":"; j += m_settings->dstStart.dayOfWeek;
    j += ",\"dsMon\":"; j += m_settings->dstStart.month;
    j += ",\"dsH\":"; j += m_settings->dstStart.hour;
    j += ",\"deFL\":"; j += m_settings->dstEnd.isLast ? "true" : "false";
    j += ",\"deDow\":"; j += m_settings->dstEnd.dayOfWeek;
    j += ",\"deMon\":"; j += m_settings->dstEnd.month;
    j += ",\"deH\":"; j += m_settings->dstEnd.hour;
    j += ",\"tbWork\":"; j += m_settings->tabata.workSec;
    j += ",\"tbRest\":"; j += m_settings->tabata.restSec;
    j += ",\"tbInt2\":"; j += m_settings->tabata.intervals;
    j += ",\"tbWC\":"; j += m_settings->tabata.workColorIdx;
    j += ",\"tbRC\":"; j += m_settings->tabata.restColorIdx;
    j += ",\"animTr\":"; j += m_settings->animateTransitions ? "true" : "false";
    j += ",\"nsEn\":"; j += m_settings->nightShiftEnabled ? "true" : "false";
    j += ",\"nsStart\":"; j += m_settings->nightShiftStartHour;
    j += ",\"nsEnd\":"; j += m_settings->nightShiftEndHour;
    j += ",\"nsBright\":"; j += m_settings->nightShiftBrightness;
    j += ",\"pomTotal\":"; j += m_settings->pomodoroIntervals;
    j += ",\"dateEn\":"; j += m_settings->showDateEnabled ? "true" : "false";
    j += ",\"dateInt\":"; j += m_settings->showDateIntervalSec;
    j += ",\"tempEn\":"; j += m_settings->showTempEnabled ? "true" : "false";
    j += ",\"tempFL\":"; j += m_settings->tempFeelsLike ? "true" : "false";
    j += ",\"tempClr\":"; j += m_settings->tempColorByValue ? "true" : "false";
    if (!isnan(m_currentTemp)) { j += ",\"curTemp\":"; j += String(m_currentTemp, 1); j += ",\"curFL\":"; j += String(m_currentFeelsLike, 1); }
    else { j += ",\"curTemp\":null,\"curFL\":null"; }
    if (m_weatherCity.length() > 0) { j += ",\"wLoc\":\""; j += m_weatherCity; j += "\""; }
    if (m_settings->weatherLat != 0) { j += ",\"wLat\":"; j += String(m_settings->weatherLat, 2); j += ",\"wLon\":"; j += String(m_settings->weatherLon, 2); }
    j += ",\"colonEn\":"; j += m_settings->colonLedsEnabled ? "true" : "false";
    j += ",\"buzzLv\":"; j += m_settings->buzzerLevel;
    j += ",\"cwBuzz\":"; j += m_settings->clockworkBuzzer ? "true" : "false";
    j += ",\"devName\":\""; j += (m_heartbeat ? m_heartbeat->getDeviceName() : String("")); j += "\"";
    if (m_heartbeat) {
        j += ",\"nameConflict\":"; j += m_heartbeat->hasNameConflict() ? "true" : "false";
        j += ",\"peerList\":[";
        for (int i = 0; i < m_heartbeat->getPeerCount(); i++) {
            const PeerInfo& p = m_heartbeat->getPeer(i);
            if (i > 0) j += ",";
            j += "{\"n\":\""; j += p.name; j += "\",\"ip\":\""; j += p.ip; j += "\"}";
        }
        j += "]";
    }

    j += ",\"rssi\":"; j += WiFi.RSSI();
    j += ",\"bdIntv\":"; j += m_settings->birthdayIntervalMins;
    j += ",\"bdBuzz\":"; j += m_settings->birthdayBuzzer ? "true" : "false";
    j += ",\"bdScrl\":"; j += m_settings->birthdayScrollCount;
    j += ",\"bdSpd\":";  j += m_settings->birthdayScrollSpeed;
    j += ",\"bdays\":[";
    for (int i = 0; i < m_settings->birthdayCount && i < MAX_BIRTHDAYS; i++) {
        Birthday b; m_configStore->loadBirthday(i, b);
        if (i > 0) j += ",";
        j += "{\"n\":\""; j += b.name; j += "\",\"d\":"; j += b.day; j += ",\"m\":"; j += b.month; j += "}";
    }
    j += "]";
    j += ",\"tabPresets\":[";
    for (int i = 0; i < MAX_TABATA_PRESETS; i++) {
        TabataPreset p; m_configStore->loadTabataPreset(i, p);
        if (i > 0) j += ",";
        j += "{\"n\":\""; j += p.name; j += "\",\"w\":"; j += p.workSec; j += ",\"r\":"; j += p.restSec; j += ",\"i\":"; j += p.intervals; j += "}";
    }
    j += "]";
    j += ",\"ssid\":\""; j += WiFi.SSID(); j += "\"";
    j += ",\"ip\":\""; j += WiFi.localIP().toString(); j += "\"";
    j += "}";
    return j;
}

// ======================== Multi-Watch Config Sync ========================
// Syncable settings only (no live/identity fields). Pushed to peers on demand.
String WebUI::buildConfigJSON() {
    const WatchSettings& s = *m_settings;
    String j = "{";
    j += "\"colorIdx\":";  j += s.colorIndex;
    j += ",\"customR\":";  j += s.customR;
    j += ",\"customG\":";  j += s.customG;
    j += ",\"customB\":";  j += s.customB;
    j += ",\"clrMode\":";  j += s.colorMode;
    j += ",\"bright\":";   j += s.brightness;
    j += ",\"mmss\":";     j += s.clockShowMMSS ? "true" : "false";
    j += ",\"animTr\":";   j += s.animateTransitions ? "true" : "false";
    j += ",\"tz\":";       j += s.timezoneOffset;
    j += ",\"dst\":";      j += s.dstMode;
    j += ",\"dsFL\":";     j += s.dstStart.isLast ? "true" : "false";
    j += ",\"dsDow\":";    j += s.dstStart.dayOfWeek;
    j += ",\"dsMon\":";    j += s.dstStart.month;
    j += ",\"dsH\":";      j += s.dstStart.hour;
    j += ",\"deFL\":";     j += s.dstEnd.isLast ? "true" : "false";
    j += ",\"deDow\":";    j += s.dstEnd.dayOfWeek;
    j += ",\"deMon\":";    j += s.dstEnd.month;
    j += ",\"deH\":";      j += s.dstEnd.hour;
    j += ",\"tbWork\":";   j += s.tabata.workSec;
    j += ",\"tbRest\":";   j += s.tabata.restSec;
    j += ",\"tbInt\":";    j += s.tabata.intervals;
    j += ",\"tbWC\":";     j += s.tabata.workColorIdx;
    j += ",\"tbRC\":";     j += s.tabata.restColorIdx;
    j += ",\"nsEn\":";     j += s.nightShiftEnabled ? "true" : "false";
    j += ",\"nsStart\":";  j += s.nightShiftStartHour;
    j += ",\"nsEnd\":";    j += s.nightShiftEndHour;
    j += ",\"nsBright\":"; j += s.nightShiftBrightness;
    j += ",\"pomTotal\":"; j += s.pomodoroIntervals;
    j += ",\"dateEn\":";   j += s.showDateEnabled ? "true" : "false";
    j += ",\"dateInt\":";  j += s.showDateIntervalSec;
    j += ",\"tempEn\":";   j += s.showTempEnabled ? "true" : "false";
    j += ",\"tempFL\":";   j += s.tempFeelsLike ? "true" : "false";
    j += ",\"tempClr\":";  j += s.tempColorByValue ? "true" : "false";
    j += ",\"wLat\":";     j += String(s.weatherLat, 4);
    j += ",\"wLon\":";     j += String(s.weatherLon, 4);
    j += ",\"wCity\":\""; j += m_weatherCity; j += "\"";  // label (manual coords don't auto-derive it)
    j += ",\"colonEn\":";  j += s.colonLedsEnabled ? "true" : "false";
    j += ",\"buzzLv\":";   j += s.buzzerLevel;
    j += ",\"cwBuzz\":";   j += s.clockworkBuzzer ? "true" : "false";
    j += ",\"bdIntv\":";   j += s.birthdayIntervalMins;
    j += ",\"bdScrl\":";   j += s.birthdayScrollCount;
    j += ",\"bdSpd\":";    j += s.birthdayScrollSpeed;
    j += ",\"bdBuzz\":";   j += s.birthdayBuzzer ? "true" : "false";
    j += ",\"bdays\":[";
    for (int i = 0; i < s.birthdayCount && i < MAX_BIRTHDAYS; i++) {
        Birthday b; m_configStore->loadBirthday(i, b);
        if (i > 0) j += ",";
        j += "{\"n\":\""; j += b.name; j += "\",\"d\":"; j += b.day; j += ",\"m\":"; j += b.month; j += "}";
    }
    j += "]}";
    return j;
}

// Apply a config payload received from another watch. Identity (device name) is never touched.
void WebUI::applyConfigJSON(const String& body) {
    JsonDocument doc;
    if (deserializeJson(doc, body)) return;
    WatchSettings& s = *m_settings;

    // Identity guard: a config push must NEVER change this watch's name / mDNS identity.
    // It doesn't today (the name isn't part of WatchSettings or the payload), but snapshot
    // it and restore below so this can never regress if a name field is added later.
    String keepName = (m_heartbeat ? m_heartbeat->getDeviceName() : String(""));

    s.colorIndex          = doc["colorIdx"]  | s.colorIndex;
    s.customR             = doc["customR"]   | s.customR;
    s.customG             = doc["customG"]   | s.customG;
    s.customB             = doc["customB"]   | s.customB;
    s.colorMode           = doc["clrMode"]   | s.colorMode;
    s.brightness          = doc["bright"]    | s.brightness;
    s.clockShowMMSS       = doc["mmss"]      | s.clockShowMMSS;
    s.animateTransitions  = doc["animTr"]    | s.animateTransitions;
    s.timezoneOffset      = doc["tz"]        | s.timezoneOffset;
    s.dstMode             = doc["dst"]       | s.dstMode;
    s.dstStart.isLast     = doc["dsFL"]      | s.dstStart.isLast;
    s.dstStart.dayOfWeek  = doc["dsDow"]     | s.dstStart.dayOfWeek;
    s.dstStart.month      = doc["dsMon"]     | s.dstStart.month;
    s.dstStart.hour       = doc["dsH"]       | s.dstStart.hour;
    s.dstEnd.isLast       = doc["deFL"]      | s.dstEnd.isLast;
    s.dstEnd.dayOfWeek    = doc["deDow"]     | s.dstEnd.dayOfWeek;
    s.dstEnd.month        = doc["deMon"]     | s.dstEnd.month;
    s.dstEnd.hour         = doc["deH"]       | s.dstEnd.hour;
    s.tabata.workSec      = doc["tbWork"]    | s.tabata.workSec;
    s.tabata.restSec      = doc["tbRest"]    | s.tabata.restSec;
    s.tabata.intervals    = doc["tbInt"]     | s.tabata.intervals;
    s.tabata.workColorIdx = doc["tbWC"]      | s.tabata.workColorIdx;
    s.tabata.restColorIdx = doc["tbRC"]      | s.tabata.restColorIdx;
    s.nightShiftEnabled   = doc["nsEn"]      | s.nightShiftEnabled;
    s.nightShiftStartHour = doc["nsStart"]   | s.nightShiftStartHour;
    s.nightShiftEndHour   = doc["nsEnd"]     | s.nightShiftEndHour;
    s.nightShiftBrightness= doc["nsBright"]  | s.nightShiftBrightness;
    s.pomodoroIntervals   = doc["pomTotal"]  | s.pomodoroIntervals;
    s.showDateEnabled     = doc["dateEn"]    | s.showDateEnabled;
    s.showDateIntervalSec = doc["dateInt"]   | s.showDateIntervalSec;
    s.showTempEnabled     = doc["tempEn"]    | s.showTempEnabled;
    s.tempFeelsLike       = doc["tempFL"]    | s.tempFeelsLike;
    s.tempColorByValue    = doc["tempClr"]   | s.tempColorByValue;
    s.weatherLat          = doc["wLat"]      | s.weatherLat;
    s.weatherLon          = doc["wLon"]      | s.weatherLon;
    s.colonLedsEnabled    = doc["colonEn"]   | s.colonLedsEnabled;
    s.buzzerLevel         = doc["buzzLv"]    | s.buzzerLevel;
    s.clockworkBuzzer     = doc["cwBuzz"]    | s.clockworkBuzzer;
    s.birthdayIntervalMins= doc["bdIntv"]    | s.birthdayIntervalMins;
    s.birthdayScrollCount = doc["bdScrl"]    | s.birthdayScrollCount;
    s.birthdayScrollSpeed = doc["bdSpd"]     | s.birthdayScrollSpeed;
    s.birthdayBuzzer      = doc["bdBuzz"]    | s.birthdayBuzzer;

    // Birthdays: replace the whole list
    if (doc["bdays"].is<JsonArray>()) {
        JsonArray arr = doc["bdays"].as<JsonArray>();
        int cnt = 0;
        for (JsonObject bo : arr) {
            if (cnt >= MAX_BIRTHDAYS) break;
            Birthday b;
            const char* n = bo["n"] | "";
            strncpy(b.name, n, sizeof(b.name) - 1); b.name[sizeof(b.name) - 1] = 0;
            b.day   = bo["d"] | 0;
            b.month = bo["m"] | 0;
            m_configStore->saveBirthday(cnt, b);
            cnt++;
        }
        s.birthdayCount = cnt;
    }

    // Apply live to the running objects
    m_display->setBrightness(s.brightness);
    if (s.colorIndex >= 0) m_display->setColorByIndex(s.colorIndex);
    else m_display->setColor(CRGB(s.customR, s.customG, s.customB));
    m_timeMgr->setTimezoneOffset(s.timezoneOffset);
    m_timeMgr->setDSTMode(s.dstMode);
    m_timeMgr->setDSTRules(s.dstStart, s.dstEnd);

    // Weather: copy the city label (manual coords don't auto-derive it) and force an
    // immediate re-fetch so the synced location takes effect now, not in ~10 minutes.
    {
        const char* city = doc["wCity"] | "";
        extern char weatherCity[32];
        strncpy(weatherCity, city, sizeof(weatherCity) - 1);
        weatherCity[sizeof(weatherCity) - 1] = 0;
        m_weatherCity = city;
        extern volatile bool weatherFetchNow;
        weatherFetchNow = true;
    }

    // Restore identity if anything above touched it — config sync must not rename this watch.
    if (m_heartbeat && m_heartbeat->getDeviceName() != keepName) {
        m_heartbeat->setDeviceName(keepName);
    }

    m_configStore->save(s);  // single NVS write (includes bdCnt)
    m_syncFlashUntil = millis() + 3000;  // show "SYNC" on the digits so it's visibly received
    Serial.println("[sync] applied config from peer (name preserved)");
}

// Host side: push this watch's config to every known peer (blocking HTTP, runs from main loop).
void WebUI::doConfigSync() {
    if (!m_heartbeat) return;
    m_syncFlashUntil = millis() + 3000;  // flash "SYNC" on this watch too
    String body = buildConfigJSON();
    int total = m_heartbeat->getPeerCount();
    int ok = 0;
    for (int i = 0; i < total; i++) {
        const PeerInfo& p = m_heartbeat->getPeer(i);
        if (p.ip.length() == 0) continue;
        HTTPClient http;
        http.setConnectTimeout(800);   // fail fast on an unreachable peer (keeps loop responsive)
        http.setTimeout(1500);
        http.begin("http://" + p.ip + "/applyconfig");
        http.addHeader("Content-Type", "application/json");
        http.addHeader("X-Sync-Token", SYNC_TOKEN);
        int code = http.POST(body);
        if (code == 200) ok++;
        http.end();
    }
    Serial.printf("[sync] pushed config to %d/%d peers\n", ok, total);
    if (m_ws && m_ws->count() > 0) {
        String t = "{\"toast\":\"Synced "; t += ok; t += "/"; t += total; t += " watches\"}";
        m_ws->textAll(t);
    }
}

// Regular broadcast: fast JSON only. Full state sent on connect + after commands.
void WebUI::broadcastState() { if (!m_ws || m_ws->count() == 0) return; m_ws->textAll(buildFastJSON()); }

// ======================== Setup ========================
void WebUI::begin(LedDisplay* display, TimeManager* timeMgr, ConfigStore* configStore, WatchSettings* settings, WifiManager* wifiMgr, Heartbeat* heartbeat) {
    m_display = display; m_timeMgr = timeMgr; m_configStore = configStore; m_settings = settings; m_wifiMgr = wifiMgr; m_heartbeat = heartbeat;
    m_tabPhaseDuration = (unsigned long)m_settings->tabata.workSec * 1000;
    m_server = new AsyncWebServer(WEB_PORT);
    m_ws = new AsyncWebSocket(WS_PATH);
    m_ws->onEvent([this](AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type, void*, uint8_t* data, size_t len) {
        if (type == WS_EVT_CONNECT) {
            Serial.printf("WS #%u connected\n", client->id());
            client->text(buildStateJSON());
        }
        else if (type == WS_EVT_DISCONNECT) {
            Serial.printf("WS #%u disconnected\n", client->id());
        }
        else if (type == WS_EVT_DATA) { handleWebSocketMessage(client, data, len); }
    });
    m_server->addHandler(m_ws);

    // Always serve the normal watch UI
    m_server->on("/", HTTP_GET, [this](AsyncWebServerRequest* r) {
        size_t htmlLen = strlen_P(WEB_HTML);
        AsyncWebServerResponse* resp = r->beginChunkedResponse("text/html",
            [htmlLen](uint8_t* buffer, size_t maxLen, size_t index) -> size_t {
                if (index >= htmlLen) return 0;
                size_t len = std::min(maxLen, htmlLen - index);
                memcpy_P((char*)buffer, WEB_HTML + index, len);
                return len;
            });
        r->send(resp);
    });
    extern const char* getLogBuffer();
    m_server->on("/icon.svg", HTTP_GET, [](AsyncWebServerRequest* r) {
        r->send(200, "image/svg+xml", "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 100 100'><rect width='100' height='100' rx='20' fill='#0f0f23'/><text x='50' y='62' text-anchor='middle' font-family='monospace' font-size='36' font-weight='bold' fill='#44d9e1'>12:34</text></svg>");
    });
    m_server->on("/touch-icon.png", HTTP_GET, [](AsyncWebServerRequest* r) {
        AsyncWebServerResponse* resp = r->beginResponse_P(200, "image/png", ICON_PNG, ICON_PNG_LEN);
        resp->addHeader("Cache-Control", "no-cache");
        r->send(resp);
    });
    m_server->on("/manifest.json", HTTP_GET, [](AsyncWebServerRequest* r) {
        r->send(200, "application/manifest+json", "{\"name\":\"NeoTick\",\"short_name\":\"NeoTick\",\"display\":\"standalone\",\"background_color\":\"#0f0f23\",\"theme_color\":\"#0f0f23\",\"start_url\":\"/\",\"icons\":[{\"src\":\"/touch-icon.png\",\"sizes\":\"192x192\",\"type\":\"image/png\"}]}");
    });
    m_server->on("/logs", HTTP_GET, [](AsyncWebServerRequest* r) { r->send(200, "text/plain", getLogBuffer()); });
    // Receive a config push from another watch on the network
    m_server->on("/applyconfig", HTTP_POST,
        [this](AsyncWebServerRequest* r) {
            String* body = (String*)r->_tempObject;
            // Require the shared token so a stray device can't overwrite our settings.
            bool authed = r->hasHeader("X-Sync-Token") &&
                          r->getHeader("X-Sync-Token")->value() == SYNC_TOKEN;
            if (!authed) {
                if (body) { delete body; r->_tempObject = nullptr; }
                r->send(403, "text/plain", "FORBIDDEN");
                return;
            }
            if (body) { applyConfigJSON(*body); delete body; r->_tempObject = nullptr; }
            if (m_ws && m_ws->count() > 0) m_ws->textAll(buildStateJSON());
            r->send(200, "text/plain", "OK");
        },
        NULL,
        [](AsyncWebServerRequest* r, uint8_t* data, size_t len, size_t index, size_t total) {
            if (index == 0) { r->_tempObject = new String(); ((String*)r->_tempObject)->reserve(total + 1); }
            String* body = (String*)r->_tempObject;
            if (body) for (size_t i = 0; i < len; i++) (*body) += (char)data[i];
        });
    m_server->on("/update", HTTP_GET, [](AsyncWebServerRequest* r) {
        r->send(200, "text/html", "<html><body style='background:#0f0f23;color:#e0e0e0;font-family:sans-serif;text-align:center;padding:40px'><h2>Firmware Update</h2><p style='color:#e74c3c'>Developer Only - Use main UI for guided update</p><form id='f' method='POST' enctype='multipart/form-data'><input type='password' id='p' placeholder='Password' style='margin:10px;padding:8px'><br><input type='file' name='firmware' style='margin:10px'><br><input type='button' value='Upload' onclick=\"f.action='/update?pass='+encodeURIComponent(p.value);f.submit()\" style='padding:12px 24px;font-size:16px;cursor:pointer'></form></body></html>");
    });
    m_server->on("/update", HTTP_POST,
        [](AsyncWebServerRequest* r) {
            bool ok = !Update.hasError() && otaMagicFound && otaPasswordOK;
            r->send(200, "text/plain", ok ? "OK" : "FAIL");
            if (ok) { delay(500); ESP.restart(); }
        },
        [](AsyncWebServerRequest* r, String filename, size_t index, uint8_t* data, size_t len, bool final) {
            if (!index) {
                // Check password from query param
                otaPasswordOK = r->hasParam("pass") && r->getParam("pass")->value() == OTA_PASSWORD;
                otaMagicFound = false;
                otaTailLen = 0;
                if (!otaPasswordOK) return;
                Update.begin(UPDATE_SIZE_UNKNOWN);
            }
            if (!otaPasswordOK) return;

            // Search for magic marker in uploaded data
            if (!otaMagicFound) {
                // Check overlap from previous chunk boundary
                if (otaTailLen > 0 && len > 0) {
                    uint8_t buf[OTA_MAGIC_LEN * 2];
                    memcpy(buf, otaTail, otaTailLen);
                    size_t copyLen = min(len, (size_t)(OTA_MAGIC_LEN - 1));
                    memcpy(buf + otaTailLen, data, copyLen);
                    size_t searchLen = otaTailLen + copyLen;
                    for (size_t i = 0; i + OTA_MAGIC_LEN <= searchLen; i++) {
                        if (memcmp(buf + i, OTA_MAGIC, OTA_MAGIC_LEN) == 0) { otaMagicFound = true; break; }
                    }
                }
                // Search current chunk
                if (!otaMagicFound) {
                    for (size_t i = 0; i + OTA_MAGIC_LEN <= len; i++) {
                        if (memcmp(data + i, OTA_MAGIC, OTA_MAGIC_LEN) == 0) { otaMagicFound = true; break; }
                    }
                }
                // Save tail for cross-boundary check
                otaTailLen = min(len, (size_t)(OTA_MAGIC_LEN - 1));
                memcpy(otaTail, data + len - otaTailLen, otaTailLen);
            }

            Update.write(data, len);
            if (final) {
                if (otaMagicFound) Update.end(true);
                else Update.abort();
            }
        }
    );
    m_server->begin();
    Serial.println("Web UI started on port 80");
}

void WebUI::update() {
    tabataAdvance();
    pomodoroAdvance();
    if (m_timerRunning && getTimerRemaining() <= 0) { m_timerRunning = false; m_timerRemaining = 0; m_timerDone = true; }

    // Deferred NVS save: batch all changes, write once 2 seconds after last change
    unsigned long now = millis();
    if (m_pendingSave > 0 && now - m_pendingSave >= NVS_SAVE_DELAY_MS) {
        m_pendingSave = 0;
        m_configStore->save(*m_settings);  // single NVS write for all settings
    }

    // Multi-watch config push (deferred out of the WS handler to avoid blocking it)
    if (m_syncRequested) { m_syncRequested = false; doConfigSync(); }

    unsigned long iv = (m_swRunning || m_tabRunning) ? WS_BROADCAST_FAST_MS : WS_BROADCAST_SLOW_MS;
    if (now - m_lastBroadcast >= iv) { m_lastBroadcast = now; broadcastState(); m_ws->cleanupClients(); }
    if (now - m_lastFullBroadcast >= WS_FULL_BROADCAST_MS) { m_lastFullBroadcast = now; if (m_ws && m_ws->count() > 0) m_ws->textAll(buildStateJSON()); }
}
