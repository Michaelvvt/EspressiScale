#include "settings.h"
#include <EEPROM.h>

// EEPROM layout
#define EEPROM_KEY_ADDR 0      // Address for "valid" key
#define EEPROM_VALID_KEY 0xAB  // Key indicating valid settings
#define EEPROM_SETTINGS_ADDR 1 // Start address for settings data
#define EEPROM_SIZE 128        // Increase EEPROM size to store calibration factors
#define EEPROM_ADS1256_CALIB_ADDR 64 // Address for ADS1256 calibration factors
#define EEPROM_ADS1256_CALIB_KEY 0xCD // Key indicating valid ADS1256 calibration

// Define the static member variables
Preferences Settings::preferences;
AutoTimerMode Settings::autoTimerMode = AutoTimerMode::OFF;
SensitivityLevel Settings::sensitivity = SensitivityLevel::LEVEL_MEDIUM;
ScaleType Settings::scaleType = ScaleType::HX711;
BLEProtocolMode Settings::bleProtocol = BLEProtocolMode::ESPRESSISCALE;
BrightnessLevel Settings::brightness = BrightnessLevel::LEVEL_MEDIUM;
SleepTimeout Settings::sleepTimeout = SleepTimeout::FIVE_MIN;
WeightUnit Settings::weightUnit = WeightUnit::GRAM;

bool Settings::validateSettings() {
  bool changed = false;
  
  // Ensure all settings are within valid ranges
  if (static_cast<int>(autoTimerMode) > 2) {
    autoTimerMode = AutoTimerMode::MANUAL;
    changed = true;
  }
  
  if (static_cast<int>(sensitivity) > 2) {
    sensitivity = SensitivityLevel::LEVEL_MEDIUM;
    changed = true;
  }
  
  if (static_cast<int>(scaleType) > 1) {
    scaleType = ScaleType::HX711;
    changed = true;
  }
  
  if (static_cast<int>(bleProtocol) > 1) {
    bleProtocol = BLEProtocolMode::ESPRESSISCALE;
    changed = true;
  }
  
  if (static_cast<int>(brightness) > 3) {
    brightness = BrightnessLevel::LEVEL_MEDIUM;
    changed = true;
  }
  
  if (static_cast<int>(sleepTimeout) > 4) {
    sleepTimeout = SleepTimeout::FIVE_MIN;
    changed = true;
  }
  
  if (static_cast<int>(weightUnit) > 1) {
    weightUnit = WeightUnit::GRAM;
    changed = true;
  }

  return changed;
}

void Settings::loadFromEEPROM() {
  int addr = EEPROM_SETTINGS_ADDR;
  
  // Read all settings
  autoTimerMode = static_cast<AutoTimerMode>(EEPROM.read(addr++));
  sensitivity = static_cast<SensitivityLevel>(EEPROM.read(addr++));
  
  // Check if we have enough data for the new setting
  if (EEPROM.read(EEPROM_KEY_ADDR) == EEPROM_VALID_KEY && addr < EEPROM_SIZE) {
    scaleType = static_cast<ScaleType>(EEPROM.read(addr++));
  } else {
    scaleType = ScaleType::HX711;
  }
  
  // Handle legacy BLEProtocolMode values
  uint8_t rawBleProtocolValue = EEPROM.read(addr++);
  if (rawBleProtocolValue <= 1) {
    bleProtocol = static_cast<BLEProtocolMode>(rawBleProtocolValue);
  } else {
    bleProtocol = BLEProtocolMode::ESPRESSISCALE;
  }
  
  brightness = static_cast<BrightnessLevel>(EEPROM.read(addr++));
  sleepTimeout = static_cast<SleepTimeout>(EEPROM.read(addr++));
  weightUnit = static_cast<WeightUnit>(EEPROM.read(addr++));
  
  // Validate loaded values
  validateSettings();
}

void Settings::init() {
  // Initialize EEPROM
  EEPROM.begin(EEPROM_SIZE);
  
  // Check if EEPROM has been initialized with our settings
  byte validKey = EEPROM.read(EEPROM_KEY_ADDR);
  
  if (validKey == EEPROM_VALID_KEY) {
    // EEPROM has valid data, load it
    loadFromEEPROM();
  } else {
    // First run or EEPROM was cleared, initialize with defaults
    saveAll();
  }
}

void Settings::saveAll() {
  // Write validation key
  EEPROM.write(EEPROM_KEY_ADDR, EEPROM_VALID_KEY);
  
  // Write all settings
  int addr = EEPROM_SETTINGS_ADDR;
  
  EEPROM.write(addr++, static_cast<uint8_t>(autoTimerMode));
  EEPROM.write(addr++, static_cast<uint8_t>(sensitivity));
  EEPROM.write(addr++, static_cast<uint8_t>(scaleType));
  EEPROM.write(addr++, static_cast<uint8_t>(bleProtocol));
  EEPROM.write(addr++, static_cast<uint8_t>(brightness));
  EEPROM.write(addr++, static_cast<uint8_t>(sleepTimeout));
  EEPROM.write(addr++, static_cast<uint8_t>(weightUnit));
  
  // Commit changes to flash
  EEPROM.commit();
}

// Non-inline methods defined in the header
AutoTimerMode Settings::getAutoTimerMode() {
  return autoTimerMode;
}

bool Settings::isAutoTimerAlwaysOn() {
  return autoTimerMode == AutoTimerMode::AUTO;
}

SensitivityLevel Settings::getSensitivity() {
  return sensitivity;
}

BLEProtocolMode Settings::getBLEProtocol() {
  return bleProtocol;
}

int Settings::getBrightnessValue() {
  // Convert enum to actual brightness value (0-255)
  switch (brightness) {
    case BrightnessLevel::LEVEL_LOW:
      return 64;
    case BrightnessLevel::LEVEL_MEDIUM:
      return 128;
    case BrightnessLevel::LEVEL_HIGH:
      return 255;
    case BrightnessLevel::LEVEL_MAX:
      return 255; 
    default:
      return 128;
  }
}

SleepTimeout Settings::getSleepTimeout() {
  return sleepTimeout;
}

WeightUnit Settings::getWeightUnit() {
  return weightUnit;
}

bool Settings::isMetricUnits() {
  return weightUnit == WeightUnit::GRAM;
}

float Settings::convertWeight(float weightGrams) {
  if (weightUnit == WeightUnit::GRAM) {
    return weightGrams;
  } else {
    // Convert to ounces (1g = 0.03527396oz)
    return weightGrams * 0.03527396f;
  }
}

const char* Settings::getWeightUnitString() {
  return weightUnit == WeightUnit::GRAM ? "g" : "oz";
}

// New accessor methods
ScaleType Settings::getScaleType() {
  return scaleType;
}

bool Settings::isADS1256() {
  return scaleType == ScaleType::ADS1256;
}

void Settings::saveADS1256CalibrationFactors(float factors[4]) {
  // Initialize EEPROM if not already done
  EEPROM.begin(EEPROM_SIZE);
  
  // Write validation key
  EEPROM.write(EEPROM_ADS1256_CALIB_ADDR, EEPROM_ADS1256_CALIB_KEY);
  
  // Write calibration factors
  int addr = EEPROM_ADS1256_CALIB_ADDR + 1;
  for (int i = 0; i < 4; i++) {
    EEPROM.put(addr, factors[i]);
    addr += sizeof(float);
  }
  
  // Commit changes to flash
  EEPROM.commit();
}

void Settings::loadADS1256CalibrationFactors(float factors[4]) {
  // Initialize EEPROM if not already done
  EEPROM.begin(EEPROM_SIZE);
  
  // Check for valid key
  byte validKey = EEPROM.read(EEPROM_ADS1256_CALIB_ADDR);
  
  if (validKey == EEPROM_ADS1256_CALIB_KEY) {
    // Read calibration factors
    int addr = EEPROM_ADS1256_CALIB_ADDR + 1;
    for (int i = 0; i < 4; i++) {
      EEPROM.get(addr, factors[i]);
      addr += sizeof(float);
    }
  } else {
    // Initialize with default values if no valid data
    for (int i = 0; i < 4; i++) {
      factors[i] = 1.0f;
    }
  }
} 