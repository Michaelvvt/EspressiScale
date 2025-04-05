#pragma once

#include "Arduino.h"
#include "lvgl.h"
#include "settings.h"

/**
 * Calibration types
 */
enum CalibrationMode : uint8_t {
  CALIBRATION_BASIC = 0,        // Single point calibration
  CALIBRATION_MULTI_PT = 1,     // Multi-point calibration
  CALIBRATION_LINEARITY = 2,    // Linearity assessment
  CALIBRATION_REPEATABILITY = 3,// Repeatability test
  CALIBRATION_DRIFT = 4,        // Drift analysis
  CALIBRATION_TARE = 5,         // Tare verification
  CALIBRATION_TEST = 6,         // Test existing calibration
  CALIBRATION_MENU = 7          // Calibration menu screen
};

/**
 * Calibration quality indicators
 */
enum CalibrationQuality : uint8_t {
  QUALITY_EXCELLENT = 0,
  QUALITY_GOOD = 1,
  QUALITY_FAIR = 2,
  QUALITY_POOR = 3
};

/**
 * Structure to hold calibration results and metrics
 */
struct CalibrationResult {
  float linearFactor;               // Linear calibration factor
  float nonLinearCoefficients[3];   // Polynomial coefficients (if used)
  bool useNonLinear;                // Whether to use non-linear calibration
  float errorEstimate;              // Estimated error in calibration
  float repeatabilityError;         // Measured repeatability error
  CalibrationQuality quality;       // Overall quality assessment
};

/**
 * Calibration system for the EspressiScale
 */
class Calibration {
public:
  /**
   * Initialize the calibration system
   */
  static void init();
  
  /**
   * Start the calibration process
   * 
   * This launches the calibration UI and begins the selected
   * calibration process.
   */
  static void start();
  
  /**
   * Check if calibration is currently active
   * 
   * @return true if calibration UI is active
   */
  static bool isActive();
  
  /**
   * Update the calibration system
   * 
   * This should be called in the main loop to handle
   * calibration UI and processing when active.
   */
  static void update();
  
  /**
   * Apply calibration to a raw weight reading
   * 
   * @param rawWeight Raw weight from load cell
   * @return Calibrated weight in grams
   */
  static float applyCalibration(float rawWeight);
  
  /**
   * Get the current calibration status text
   * 
   * @return String describing calibration status
   */
  static const char* getStatusText();
  
private:
  // State variables
  static bool calibrationActive;
  static CalibrationMode currentMode;
  static int currentStep;
  static int totalSteps;
  static int selectedPoints;
  static float calibrationWeights[10];  // Increased array size for more points
  static float rawReadings[10];         // Increased array size for more points
  static float repeatabilityReadings[5]; // For repeatability test (5 measurements)
  static CalibrationResult result;
  static float selectedRefWeight;       // Selected reference weight value
  
  // Linearity test variables
  static bool inAscendingPhase;
  static float linearityWeights[6];     // 3 points ascending + 3 points descending
  static float linearityReadings[6];    // Corresponding raw readings
  
  // Drift test variables
  static unsigned long driftStartTime;
  static float initialDriftReading;
  static float maxDrift;
  
  // Tare test variables
  static float tareTestWeights[3];
  static float tareMeasuredWeights[3];
  
  // Current calibration values
  static float currentRawReading;
  static float currentRefWeight;
  
  // UI components
  static lv_obj_t* leftPanel;
  static lv_obj_t* rightPanel;
  static lv_obj_t* titleLabel;
  static lv_obj_t* instructionLabel;
  static lv_obj_t* readingLabel;
  static lv_obj_t* progressLabel;
  static lv_obj_t* actionButton;
  static lv_obj_t* backButton;
  static lv_obj_t* secondaryButton;     // For options that need a third button
  static lv_obj_t* weightOptions[7];    // Increased to support more weight options
  static lv_obj_t* menuButtons[6];      // Buttons for calibration menu
  static int selectedWeightIndex;
  
  // Common weight values in grams
  static const float COMMON_WEIGHTS[];
  
  // UI Methods
  static void createUI();
  static void updateUI();
  static void clearUI();
  static void showCalibrationMenu();
  static void showCalibrationTypeSelect();
  static void showBasicCalibrationStep(int step);
  static void showMultiPointSetup();
  static void showMultiPointStep(int point);
  static void showLinearityStep(int step);
  static void showRepeatabilityStep(int step);
  static void showDriftTest();
  static void showTareVerificationStep(int step);
  static void showWeightSelection();
  static void showResults();
  static void showTestMode();
  
  // Calibration methods
  static void startBasicCalibration();
  static void startMultiPointCalibration();
  static void startLinearityTest();
  static void startRepeatabilityTest();
  static void startDriftTest();
  static void startTareVerification();
  static void startTestMode();
  static void calculateCalibration();
  static void evaluateCalibration();
  static void evaluateLinearity();
  static void evaluateRepeatability();
  static void evaluateDrift();
  static void evaluateTareVerification();
  static void saveCalibration();
  static void cancelCalibration();
  
  // Function to update drift test
  static void updateDriftTest();
  
  // Helper for weight selection
  static void updateWeightSelectionUI();
  static void selectWeight(int index);
  
  // Event handlers
  static void onActionButtonClicked(lv_event_t* e);
  static void onBackButtonClicked(lv_event_t* e);
  static void onSecondaryButtonClicked(lv_event_t* e);
  static void onWeightOptionClicked(lv_event_t* e);
  static void onMenuButtonClicked(lv_event_t* e);
  
  // Mathematical helpers
  static float calculateLinearFactor();
  static void calculatePolynomialCoefficients();
  static float calculateErrorEstimate();
  static float calculateStandardDeviation(float* values, int count);
  static CalibrationQuality determineQuality();
}; 