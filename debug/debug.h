#pragma once

/*
 * debug.h
 * 
 * Lightweight logging utility for ESP32/Arduino projects.
 * 
 * Provides logging macros with log levels (DEBUG, INFO, WARN, ERROR),
 * formatted output including timestamp (in ms) and function name.
 * 
 * ----------------------------------
 * 🔧 USAGE:
 * 
 * 1. Include this file in your project:
 *    #include "debug.h"
 * 
 * 2. Set the desired log level **before** including the file:
 *    #define LOG_LEVEL LOG_LEVEL_DEBUG  // Options: DEBUG, INFO, WARN, ERROR, NONE
 *    #include "debug.h"
 * 
 * 3. Use the log macros in your code:
 *    LOG_DEBUG("Counter value: %d", counter);
 *    LOG_INFO("System started");
 *    LOG_WARN("Battery low");
 *    LOG_ERROR("Failed to connect to WiFi");
 * 
 * 4. Logs below the defined level will be excluded at compile time.
 * 
 * ----------------------------------
 * ✅ Example output:
 * [1243 ms] [INFO ] setup(): WiFi connected
 * [1872 ms] [ERROR] loop(): Sensor read failed
 * 
 */
#ifndef DEBUG_H
#define DEBUG_H

#include "esp_log.h"

#define LOG_TAG "APP"

#if USE_LOG_COLORS
  #define COLOR_RED     "\033[31m"
  #define COLOR_YELLOW  "\033[33m"
  #define COLOR_GREEN   "\033[32m"
  #define COLOR_CYAN    "\033[36m"
  #define COLOR_RESET   "\033[0m"
#else
  #define COLOR_RED     ""
  #define COLOR_YELLOW  ""
  #define COLOR_GREEN   ""
  #define COLOR_CYAN    ""
  #define COLOR_RESET   ""
#endif
// Core formatter

#define MY_LOG_FORMAT(color, letter, fmt, ...) \
    ESP_LOG##letter(LOG_TAG, color  fmt, \
                     ##__VA_ARGS__)


/*#define MY_LOG_FORMAT(color, letter, fmt, ...) \
    ESP_LOG##letter(LOG_TAG, color "[%-20s:%-4d] " fmt COLOR_RESET, __FUNCTION__, __LINE__, ##__VA_ARGS__) */

// Log level wrappers
#define LOG_ERROR(fmt, ...)   MY_LOG_FORMAT(COLOR_RED,     E, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)    MY_LOG_FORMAT(COLOR_YELLOW,  W, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)    MY_LOG_FORMAT(COLOR_GREEN,   I, fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...)   MY_LOG_FORMAT(COLOR_CYAN,    D, fmt, ##__VA_ARGS__)

#endif // DEBUG_H

