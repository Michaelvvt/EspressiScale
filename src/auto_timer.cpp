#include "auto_timer.h"
#include "settings.h"

// Static member initialization
AutoTimer::TimerState AutoTimer::state = AutoTimer::INACTIVE;
unsigned long AutoTimer::startTime = 0;
unsigned long AutoTimer::stopTime = 0;
float AutoTimer::lastWeight = 0.0f;
float AutoTimer::peakWeight = 0.0f;
float AutoTimer::startWeight = 0.0f;
float AutoTimer::weightChangeRate = 0.0f;
float AutoTimer::weightThreshold = 0.0f;
bool AutoTimer::hasDetectedShot = false;
AutoTimerMode AutoTimer::timerMode = AutoTimerMode::OFF;
SensitivityLevel AutoTimer::sensitivity = SensitivityLevel::LEVEL_MEDIUM;
unsigned long AutoTimer::lastUpdateTime = 0;

// Constants for auto-detection
constexpr float WEIGHT_RATE_RUNNING_AVG_FACTOR = 0.3f;  // For smoothing weight change rate
constexpr unsigned long MIN_STABLE_TIME_MS = 1000;      // Minimum stable time before auto-stop
constexpr unsigned long DEBOUNCE_TIME_MS = 200;         // Debounce time for weight changes
constexpr float MIN_SHOT_WEIGHT = 0.5f;                 // Minimum weight to consider a shot

void AutoTimer::init() {
  resetTimer();
  // Load settings
  timerMode = Settings::autoTimerMode;
  sensitivity = Settings::sensitivity;
  
  // Set threshold based on sensitivity
  weightThreshold = Settings::getSensitivityThreshold();
}

void AutoTimer::update(float currentWeight) {
  // Skip if disabled
  if (timerMode == AutoTimerMode::OFF) {
    return;
  }
  
  unsigned long currentTime = millis();
  
  // Update weight change rate
  if (currentTime - lastUpdateTime > 50) { // Update every 50ms
    updateWeightChangeRate(currentWeight);
    lastUpdateTime = currentTime;
  }
  
  // Process based on current state
  switch (state) {
    case INACTIVE:
      // Check for weight change that might indicate shot starting
      checkForAutoStart(currentWeight);
      break;
      
    case READY:
      // Check if weight increase continues, and start timer if so
      if (isWeightIncreasing(currentWeight)) {
        startWeight = currentWeight;
        startTime = currentTime;
        state = RUNNING;
      } else if (!isWeightIncreasing(currentWeight) && currentTime - lastUpdateTime > 1000) {
        // If no increase for a while, go back to inactive
        state = INACTIVE;
      }
      break;
      
    case RUNNING:
      // Track peak weight
      if (currentWeight > peakWeight) {
        peakWeight = currentWeight;
      }
      
      // Check if we should auto-stop
      checkForAutoStop(currentWeight);
      break;
      
    case STOPPED:
    case FINISHED:
      // No automatic state transitions from these states
      break;
  }
  
  // Update last weight
  lastWeight = currentWeight;
}

void AutoTimer::startTimer() {
  startTime = millis();
  startWeight = lastWeight;
  peakWeight = lastWeight;
  state = RUNNING;
  hasDetectedShot = true;
}

void AutoTimer::stopTimer() {
  if (state == RUNNING) {
    stopTime = millis();
    state = STOPPED;
  }
}

void AutoTimer::resetTimer() {
  state = INACTIVE;
  startTime = 0;
  stopTime = 0;
  lastWeight = 0.0f;
  peakWeight = 0.0f;
  startWeight = 0.0f;
  weightChangeRate = 0.0f;
  hasDetectedShot = false;
  lastUpdateTime = 0;
}

AutoTimer::TimerState AutoTimer::getState() {
  return state;
}

float AutoTimer::getElapsedTime() {
  if (state == INACTIVE) {
    return 0.0f;
  } else if (state == RUNNING) {
    return (millis() - startTime) / 1000.0f;
  } else {
    return (stopTime - startTime) / 1000.0f;
  }
}

String AutoTimer::getFormattedTime() {
  float seconds = getElapsedTime();
  int minutes = static_cast<int>(seconds) / 60;
  seconds = seconds - (minutes * 60);
  
  char buffer[10];
  sprintf(buffer, "%02d:%05.2f", minutes, seconds);
  return String(buffer);
}

void AutoTimer::setSensitivity(int newSensitivity) {
  sensitivity = static_cast<SensitivityLevel>(newSensitivity);
  
  // Update threshold based on new sensitivity
  weightThreshold = Settings::getSensitivityThreshold();
}

void AutoTimer::setMode(AutoTimerMode newMode) {
  timerMode = newMode;
  if (timerMode == AutoTimerMode::OFF && state != INACTIVE) {
    resetTimer();
  }
}

void AutoTimer::checkForAutoStart(float currentWeight) {
  // Only check for auto-start if enabled
  if (timerMode != AutoTimerMode::MANUAL && timerMode != AutoTimerMode::AUTO) {
    return;
  }
  
  // Check if weight is increasing significantly
  if (currentWeight > MIN_SHOT_WEIGHT && 
      weightChangeRate > weightThreshold &&
      !hasDetectedShot) {
    // Mark that we're considering starting a timer
    state = READY;
  }
}

void AutoTimer::checkForAutoStop(float currentWeight) {
  // Only check for auto-stop if enabled
  if (timerMode != AutoTimerMode::MANUAL && timerMode != AutoTimerMode::AUTO) {
    return;
  }
  
  static unsigned long stableStartTime = 0;
  
  // Check if weight is stable
  if (isWeightStable(currentWeight)) {
    if (stableStartTime == 0) {
      stableStartTime = millis();
    } else if (millis() - stableStartTime > MIN_STABLE_TIME_MS) {
      // Weight has been stable for enough time, stop timer
      stopTime = millis();
      state = FINISHED;
      stableStartTime = 0;
    }
  } else {
    stableStartTime = 0;
  }
}

bool AutoTimer::isWeightIncreasing(float currentWeight) {
  return weightChangeRate > 0.05f; // Consider weight increasing if change rate is positive
}

bool AutoTimer::isWeightStable(float currentWeight) {
  // Weight is stable if change rate is very small
  return abs(weightChangeRate) < 0.02f;
}

void AutoTimer::updateWeightChangeRate(float currentWeight) {
  static float prevChangeRate = 0.0f;
  
  // Calculate instantaneous change rate
  float instantRate = (currentWeight - lastWeight) * 20.0f; // * 20 to convert to per second
  
  // Apply running average for smoothing
  weightChangeRate = (instantRate * WEIGHT_RATE_RUNNING_AVG_FACTOR) + 
                     (prevChangeRate * (1.0f - WEIGHT_RATE_RUNNING_AVG_FACTOR));
  
  // Save for next iteration
  prevChangeRate = weightChangeRate;
}

unsigned long AutoTimer::getTimerValue() {
  if (state == INACTIVE) {
    return 0;
  } else if (state == RUNNING) {
    return millis() - startTime;
  } else {
    return stopTime - startTime;
  }
} 