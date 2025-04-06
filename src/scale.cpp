#include <HX711.h>
#include <Arduino.h>
#include "pin_config.h" // Include pin configuration header

// Use pin definitions from pin_config.h
float calibration_factor = 4220.0; // Put your own calibration factor here

HX711 scale;

bool setupScale(){
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
    return false;
  }
  
  scale.tare();
  Serial.println("Scale initialized successfully");
  return true;
}

void reTareScale(){
  scale.tare();
}

void tareScale(){
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

float updateScale(){
  return scale.get_units();
}

float getRawReading() {
  // Returns the raw, uncalibrated reading from the scale
  return scale.read_average(5) - scale.get_offset();
}}
