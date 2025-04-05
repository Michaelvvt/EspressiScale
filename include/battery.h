#pragma once

#include <Arduino.h>
#include "lvgl.h"
#include "pin_config.h"

// Battery status levels
enum BatteryLevel {
    BATTERY_CRITICAL,  // 0-5%
    BATTERY_LOW,       // 6-19%
    BATTERY_MEDIUM,    // 20-49%
    BATTERY_HIGH,      // 50-100%
    BATTERY_CHARGING   // Any level while charging
};

// Initialize battery monitoring
void setupBattery();

// Get raw battery voltage in volts
float getBatteryVoltage();

// Get battery percentage (0-100)
uint8_t getBatteryPercentage();

// Get battery level category
BatteryLevel getBatteryLevel();

// Check if battery is charging (if supported by hardware)
bool isBatteryCharging();

// Draw the battery indicator at the specified position
void drawBatteryIndicator(lv_obj_t* parent, int16_t x, int16_t y, bool showPercentage = true);

// Update the battery indicator display
void updateBatteryIndicator();