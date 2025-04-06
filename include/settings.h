#pragma once

#include <Preferences.h>
#include <Arduino.h>
#include <EEPROM.h>
#include "ble_service.h" // Include for BLEProtocolMode definition

// Define all the enum types for settings
enum class AutoTimerMode : uint8_t {
    OFF = 0,
    MANUAL = 1,
    AUTO = 2
};

enum class SensitivityLevel : uint8_t {
    LEVEL_LOW = 0,
    LEVEL_MEDIUM = 1,
    LEVEL_HIGH = 2
};

enum class ScaleType : uint8_t {
    HX711 = 0,
    ADS1256 = 1
};

enum class BrightnessLevel : uint8_t {
    LEVEL_LOW = 0,
    LEVEL_MEDIUM = 1,
    LEVEL_HIGH = 2,
    LEVEL_MAX = 3
};

enum class SleepTimeout : uint8_t {
    NEVER = 0,
    THIRTY_SEC = 1,
    ONE_MIN = 2,
    FIVE_MIN = 3,
    TEN_MIN = 4
};

enum class WeightUnit : uint8_t {
    GRAM = 0,
    OUNCE = 1
};

// Menu page enum
enum SettingsPage {
  PAGE_AUTO_TIMER = 0,
  PAGE_SENSITIVITY = 1,
  PAGE_BLE_PROTOCOL = 2,
  PAGE_BRIGHTNESS = 3,
  PAGE_SLEEP_TIMEOUT = 4,
  PAGE_UNITS = 5,
  PAGE_SCALE_TYPE = 6,
  PAGE_COUNT
};

class Settings {
public:
    // Initialize settings from EEPROM or with defaults if validation fails
    static void init();
    
    // Save all settings to EEPROM
    static void saveAll();
    
    // Helper methods with inline implementation to avoid circular dependencies
    static unsigned long getSleepTimeoutMs() {
        switch(sleepTimeout) {
            case SleepTimeout::THIRTY_SEC: return 30000;
            case SleepTimeout::ONE_MIN: return 60000;
            case SleepTimeout::FIVE_MIN: return 300000;
            case SleepTimeout::TEN_MIN: return 600000;
            case SleepTimeout::NEVER:
            default: return 0;
        }
    }
    
    static float getSensitivityThreshold() {
        switch(sensitivity) {
            case SensitivityLevel::LEVEL_LOW: return 1.0f;
            case SensitivityLevel::LEVEL_MEDIUM: return 0.5f;
            case SensitivityLevel::LEVEL_HIGH: return 0.2f;
            default: return 0.5f;
        }
    }
    
    static float convertToSelectedUnit(float grams) {
        return (weightUnit == WeightUnit::OUNCE) ? grams * 0.03527396f : grams;
    }
    
    static const char* getUnitString() {
        return (weightUnit == WeightUnit::OUNCE) ? "oz" : "g";
    }
    
    static bool isAutoTimerEnabled() {
        return autoTimerMode == AutoTimerMode::AUTO;
    }

    // ADS1256 calibration factors persistence
    static void saveADS1256CalibrationFactors(float factors[4]);
    static void loadADS1256CalibrationFactors(float factors[4]);

    // Static member variables - use extern for declaration only
    static Preferences preferences;
    static AutoTimerMode autoTimerMode;
    static SensitivityLevel sensitivity;
    static ScaleType scaleType;
    static BLEProtocolMode bleProtocol;
    static BrightnessLevel brightness;
    static SleepTimeout sleepTimeout;
    static WeightUnit weightUnit;

    // Other getter methods that are defined in the cpp file
    static AutoTimerMode getAutoTimerMode();
    static bool isAutoTimerAlwaysOn();
    static SensitivityLevel getSensitivity();
    static BLEProtocolMode getBLEProtocol();
    static int getBrightnessValue();
    static SleepTimeout getSleepTimeout();
    static WeightUnit getWeightUnit();
    static bool isMetricUnits();
    static float convertWeight(float weightGrams);
    static const char* getWeightUnitString();
    static ScaleType getScaleType();
    static bool isADS1256();

private:
    // Helper methods
    static bool validateSettings();
    static void loadFromEEPROM();
}; 