#pragma once

#include <Arduino.h>
#include "settings.h"

class AutoTimer {
public:
  // Timer states
  enum TimerState {
    INACTIVE,    // Timer is not running
    READY,       // Ready to start (weight detected but not stable yet)
    RUNNING,     // Timer is running
    STOPPED,     // Timer has stopped
    FINISHED     // Shot finished
  };
  
  // Initialize the auto timer
  static void init();
  
  // Update the timer based on current weight and settings
  // Should be called in the main loop
  static void update(float currentWeight);
  
  // Force start the timer
  static void startTimer();
  
  // Force stop the timer
  static void stopTimer();
  
  // Reset the timer
  static void resetTimer();
  
  // Check if timer is running
  static bool isRunning() {
    return state == RUNNING;
  }
  
  // Get current timer state
  static TimerState getState();
  
  // Get elapsed time in seconds
  static float getElapsedTime();
  
  // Get elapsed time as formatted string (MM:SS.s)
  static String getFormattedTime();
  
  // Set sensitivity for auto-start detection (override settings)
  static void setSensitivity(int sensitivity);
  
  // Set auto timer mode (override settings)
  static void setMode(AutoTimerMode mode);
  
  // Get current timer value in milliseconds
  static unsigned long getTimerValue();
  
private:
  static TimerState state;
  static unsigned long startTime;  // Start time in milliseconds
  static unsigned long stopTime;   // Stop time in milliseconds
  static float lastWeight;         // Previous weight reading
  static float peakWeight;         // Highest weight detected during shot
  static float startWeight;        // Weight when timer started
  static float weightChangeRate;   // Current rate of weight change
  static float weightThreshold;    // Weight change needed to start timer
  static bool hasDetectedShot;     // Flag indicating if a shot has been detected
  
  // Settings
  static AutoTimerMode timerMode;
  static SensitivityLevel sensitivity;
  static unsigned long lastUpdateTime;
  
  // Helpers
  static void checkForAutoStart(float currentWeight);
  static void checkForAutoStop(float currentWeight);
  static bool isWeightIncreasing(float currentWeight);
  static bool isWeightStable(float currentWeight);
  static void updateWeightChangeRate(float currentWeight);
}; 