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
  pinMode(LOADCELL_POWER_PIN, OUTPUT);
  digitalWrite(LOADCELL_POWER_PIN, HIGH);
  
  // Allow time for load cells to power up
  delay(100);
  
  // Create ADS1256 instance if not already created
  if (ads1256 == nullptr) {
    ads1256 = new ADS1256(ADS1256_CS_PIN, ADS1256_DRDY_PIN, ADS1256_RESET_PIN);
  }
  
  // Initialize SPI and ADS1256
  if (!ads1256->begin(ADS1256_GAIN_1, ADS1256_DRATE_100SPS)) {
    Serial.println("ADS1256 initialization failed. Check wiring.");
    isADS1256Initialized = false;
    return false;
  }
  
  // Set reference voltage (typical value for load cell applications)
  ads1256->setVref(5.0); // Most load cell amplifiers use 5V excitation
  
  // Configure ADS1256 for 4 differential channels (4 load cells)
  ads1256->setDataRate(ADS1256_DRATE_100SPS); // Set 100SPS data rate
  
  // Initial tare to zero the scale
  tareADS1256();
  
  Serial.println("ADS1256 scale initialized successfully");
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
  float totalWeight = 0.0;
  float readings[4];
  
  // Get readings from all 4 load cells
  if (ads1256->readLoadCells(readings)) {
    // Process each cell
    for (int cell = 0; cell < 4; cell++) {
      // Apply offset and calibration
      cellReadings[cell] = (readings[cell] - cellOffsets[cell]) * ads1256_calibration_factors[cell];
      
      // Add to total weight
      totalWeight += cellReadings[cell];
    }
  }
  
  return totalWeight;
}

float getRawReadingADS1256() {
  float totalRaw = 0.0;
  float readings[4];
  
  // Get raw sum of all cells without calibration factor
  if (ads1256->readLoadCells(readings)) {
    for (int cell = 0; cell < 4; cell++) {
      totalRaw += (readings[cell] - cellOffsets[cell]);
    }
  }
  
  return totalRaw;
}

// Forward declarations for ADS1256 functions
void tareADS1256();
void updateADS1256();
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
    }
}

// Function to calibrate the scale with a known weight
void calibrateScale(float knownWeight) {
    if (Settings::scaleType == SCALE_TYPE_HX711) {
        // Existing HX711 calibration code could go here
        // Currently not implemented
    } else if (Settings::scaleType == SCALE_TYPE_ADS1256) {
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

void reinitializeScale() {
  // Free previous resources
  if (ads1256 != nullptr) {
    delete ads1256;
    ads1256 = nullptr;
  }
  
  // Power down current scale
  pinMode(LOADCELL_POWER_PIN, OUTPUT);
  digitalWrite(LOADCELL_POWER_PIN, LOW);
  delay(100); // Give time for power down
  
  // Re-initialize based on current settings
  setupScale();
  
  // Tare the scale after reinitialization
  tareScale();
}
