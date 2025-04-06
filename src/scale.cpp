#include <HX711.h>
#include <Arduino.h>
#include "pin_config.h" // Include pin configuration header
#include "settings.h"   // Added for scale type settings
#include "drivers/ADS1256.h" // Include our ADS1256 driver

// Use pin definitions from pin_config.h
float calibration_factor = 4220.0; // Put your own calibration factor here
float ads1256_calibration_factors[4] = {4220.0, 4220.0, 4220.0, 4220.0}; // Calibration factors for each ADS1256 channel

// Global instances
HX711 scale;
ADS1256 *ads1256 = nullptr; // Pointer to ADS1256 instance, initialized in setupADS1256

// Flag to track setup status
bool isHX711Initialized = false;
bool isADS1256Initialized = false;

// Raw readings from each load cell (ADS1256 mode)
float cellReadings[4] = {0.0, 0.0, 0.0, 0.0};
float cellOffsets[4] = {0.0, 0.0, 0.0, 0.0};

// Function declarations for ADS1256 to fix compilation order
void tareADS1256();
float updateADS1256();
float getRawReadingADS1256();
void calibrateADS1256(float knownWeight);

// HX711 implementation
bool setupHX711() {
  pinMode(LOADCELL_POWER_PIN, OUTPUT);
  digitalWrite(LOADCELL_POWER_PIN, HIGH);
  
  // Allow time for the load cell amplifier to power up
  delay(100);
  
  // HX711 begin() returns void, so we need to check is_ready() instead
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  
  scale.set_gain();
  scale.set_scale(calibration_factor);
  
  // Try to read from the scale to verify it's working
  if (!scale.is_ready()) {
    Serial.println("HX711 not responding. Check wiring.");
    isHX711Initialized = false;
    return false;
  }
  
  scale.tare();
  Serial.println("HX711 scale initialized successfully");
  isHX711Initialized = true;
  return true;
}

void tareHX711() {
  int times = 20;
  long sum = 0;
  long lastSum = 0;
  boolean finished = false;
  int stableCounter = 0;
  const long stabilityTolerance = 10; // Tolerance threshold for stability

  for (byte i = 0; i < times && !finished; i++) {
    sum = scale.read();
    if (abs(sum - lastSum) < stabilityTolerance) {
      stableCounter++;
      if (stableCounter >= 3) { // Require 3 consecutive stable readings
        finished = true;
      }

    }
    else {
      stableCounter = 0;
    }
    lastSum = sum;
    scale.set_offset(sum); // Set the scale to 0.0
    delay(5); // Small delay to allow readings to stabilize
  }
}

float updateHX711() {
  return scale.get_units();
}

float getRawReadingHX711() {
  // Returns the raw, uncalibrated reading from the scale
  return scale.read_average(5) - scale.get_offset();
}

// ADS1256 implementation
bool setupADS1256() {
  // Initialize the ADS1256
  if (ads1256 != nullptr) {
    delete ads1256;
  }
  
  ads1256 = new ADS1256(ADS1256_CS_PIN, ADS1256_DRDY_PIN, ADS1256_RESET_PIN);
  
  if (!ads1256->begin()) {
    isADS1256Initialized = false;
    return false;
  }
  
  // Set reference voltage to 5V (adjust if your reference is different)
  ads1256->setRefVoltage(5.0);
  
  // Set data rate (adjust as needed for your application)
  ads1256->setDataRate(ADS1256_DRATE_100SPS);
  
  // Load saved calibration factors from EEPROM
  Settings::loadADS1256CalibrationFactors(ads1256_calibration_factors);
  
  // Perform initial tare
  tareADS1256();
  
  isADS1256Initialized = true;
  return true;
}

void tareADS1256() {
  // Read multiple samples for more stable offset
  const int numSamples = 10;
  float sumReadings[4] = {0};
  
  // Read each cell multiple times
  for (int i = 0; i < numSamples; i++) {
    float readings[4];
    
    if (ads1256->readLoadCells(readings)) {
      for (int cell = 0; cell < 4; cell++) {
        sumReadings[cell] += readings[cell];
      }
    }
    delay(10); // Short delay between readings
  }
  
  // Calculate average offset for each cell
  for (int cell = 0; cell < 4; cell++) {
    cellOffsets[cell] = sumReadings[cell] / numSamples;
    Serial.printf("Cell %d offset: %.6f\n", cell, cellOffsets[cell]);
  }
}

float updateADS1256() {
  if (!isADS1256Initialized || ads1256 == nullptr) {
    return 0.0f;
  }
  
  const int MAX_RETRIES = 3;
  const int RETRY_DELAY_MS = 50;
  float totalWeight = 0.0f;
  bool readingSuccess = false;
  
  for (int retry = 0; retry < MAX_RETRIES && !readingSuccess; retry++) {
    // If retry, add a short delay
    if (retry > 0) {
      delay(RETRY_DELAY_MS);
    }
    
    try {
      float cellReadingsTemp[4];
      
      // Try to read all load cells
      if (ads1256->readLoadCells(cellReadingsTemp)) {
        // Copy readings to main array
        for (int cell = 0; cell < 4; cell++) {
          cellReadings[cell] = cellReadingsTemp[cell];
        }
        readingSuccess = true;
      } else if (retry == MAX_RETRIES - 1) {
        // Last retry failed, return last successful reading
        break;
      }
    } catch (...) {
      // Catch any exceptions during reading
      if (retry == MAX_RETRIES - 1) {
        // Last retry failed, return last successful reading
        break;
      }
    }
  }
  
  // Process readings and calculate weight
  if (readingSuccess) {
    totalWeight = 0.0f;
    
    for (int cell = 0; cell < 4; cell++) {
      // Apply offsets and calibration
      float calibratedCellValue = (cellReadings[cell] - cellOffsets[cell]) / ads1256_calibration_factors[cell];
      totalWeight += calibratedCellValue;
    }
  }
  
  return totalWeight;
}

float getRawReadingADS1256() {
  if (!isADS1256Initialized || ads1256 == nullptr) {
    return 0.0f;
  }
  
  const int MAX_RETRIES = 3;
  const int RETRY_DELAY_MS = 50;
  float totalRaw = 0.0f;
  bool readingSuccess = false;
  
  for (int retry = 0; retry < MAX_RETRIES && !readingSuccess; retry++) {
    // If retry, add a short delay
    if (retry > 0) {
      delay(RETRY_DELAY_MS);
    }
    
    try {
      float tempReadings[4];
      
      // Try to read all load cells
      if (ads1256->readLoadCells(tempReadings)) {
        // Sum up raw values
        totalRaw = 0.0f;
        for (int cell = 0; cell < 4; cell++) {
          totalRaw += tempReadings[cell];
        }
        readingSuccess = true;
      } else if (retry == MAX_RETRIES - 1) {
        // Last retry failed, return 0
        return 0.0f;
      }
    } catch (...) {
      // Catch any exceptions during reading
      if (retry == MAX_RETRIES - 1) {
        // Last retry failed, return 0
        return 0.0f;
      }
    }
  }
  
  return totalRaw;
}

// Forward declarations for ADS1256 functions - Fix the return type
void tareADS1256();
float updateADS1256();
float getRawReadingADS1256();
void calibrateADS1256(float knownWeight);

void calibrateADS1256(float knownWeight) {
    if (!isADS1256Initialized || ads1256 == nullptr) {
        return;
    }
    
    // First tare the scale to set the zero point
    tareADS1256();
    
    // Wait for stability after taring
    delay(1000);
    
    // Take multiple readings with the known weight
    const int numReadings = 10;
    float rawReadings[4] = {0};
    
    // Collect readings from each cell
    for (int i = 0; i < numReadings; i++) {
        float currentReadings[4];
        ads1256->readLoadCells(currentReadings);
        
        for (int cell = 0; cell < 4; cell++) {
            rawReadings[cell] += (currentReadings[cell] - cellOffsets[cell]);
        }
        
        delay(100); // Short delay between readings
    }
    
    // Average the readings
    for (int cell = 0; cell < 4; cell++) {
        rawReadings[cell] /= numReadings;
    }
    
    // Calculate new calibration factors based on known weight
    float totalRaw = rawReadings[0] + rawReadings[1] + rawReadings[2] + rawReadings[3];
    
    if (totalRaw != 0) {
        // Calculate the contribution of each cell to the total
        float cellContribution[4];
        for (int cell = 0; cell < 4; cell++) {
            cellContribution[cell] = rawReadings[cell] / totalRaw;
        }
        
        // Distribute the known weight to each cell according to its contribution
        for (int cell = 0; cell < 4; cell++) {
            // Only update if cell has a significant reading
            if (abs(rawReadings[cell]) > 0.01) {
                // Cell calibration = Raw reading / (cell's portion of known weight)
                float cellPortion = knownWeight * cellContribution[cell];
                ads1256_calibration_factors[cell] = rawReadings[cell] / cellPortion;
            }
        }
        
        // Save calibration factors to EEPROM
        Settings::saveADS1256CalibrationFactors(ads1256_calibration_factors);
    }
}

// Function to calibrate the scale with a known weight
void calibrateScale(float knownWeight) {
    if (Settings::scaleType == ScaleType::HX711) {
        // Existing HX711 calibration code could go here
        // Currently not implemented
    } else if (Settings::scaleType == ScaleType::ADS1256) {
        calibrateADS1256(knownWeight);
    }
}

// Interface functions that select the right implementation
bool setupScale() {
  // Choose implementation based on settings
  if (Settings::isADS1256()) {
    return setupADS1256();
  } else {
    return setupHX711();
  }
}

void tareScale() {
  // Choose implementation based on settings
  if (Settings::isADS1256()) {
    tareADS1256();
  } else {
    tareHX711();
  }
}

float updateScale() {
  // Choose implementation based on settings
  if (Settings::isADS1256()) {
    return updateADS1256();
  } else {
    return updateHX711();
  }
}

float getRawReading() {
  // Choose implementation based on settings
  if (Settings::isADS1256()) {
    return getRawReadingADS1256();
  } else {
    return getRawReadingHX711();
  }
}

// Add function to power down the ADS1256
void powerDownADS1256() {
  if (isADS1256Initialized && ads1256 != nullptr) {
    // Put ADS1256 in standby mode
    ads1256->sendCommand(ADS1256_CMD_STANDBY);
    
    // Power down the load cells
    pinMode(LOADCELL_POWER_PIN, OUTPUT);
    digitalWrite(LOADCELL_POWER_PIN, LOW);
  }
}

// Add function to power up the ADS1256
void powerUpADS1256() {
  if (ads1256 != nullptr) {
    // Power up the load cells
    pinMode(LOADCELL_POWER_PIN, OUTPUT);
    digitalWrite(LOADCELL_POWER_PIN, HIGH);
    
    // Allow time for load cells to stabilize
    delay(100);
    
    // Wake up the ADS1256
    ads1256->sendCommand(ADS1256_CMD_WAKEUP);
    
    // Reinitialize if necessary
    if (!isADS1256Initialized) {
      setupADS1256();
    }
  }
}

// Update the reinitializeScale function to include proper power management
void reinitializeScale() {
  // Free previous resources
  if (ads1256 != nullptr) {
    // Power down before deleting
    powerDownADS1256();
    delete ads1256;
    ads1256 = nullptr;
  }
  
  // Power down any scale on pins
  pinMode(LOADCELL_POWER_PIN, OUTPUT);
  digitalWrite(LOADCELL_POWER_PIN, LOW);
  
  // Reinitialize
  setupScale();
  
  // Tare to reset zero point
  tareScale();
}
