#include "calibration.h"
#include "scale.h"
#include "filter.h"
#include <EEPROM.h>
#include "lvgl_fonts.h"

// Forward declarations for external functions
extern void updateUI();
extern float getRawReading(); // Gets raw uncalibrated reading

// EEPROM layout for calibration
#define CALIB_VALID_KEY 0xA6    // Validation byte for calibration
#define CALIB_KEY_ADDR 32       // Address of calibration validation byte
#define CALIB_DATA_ADDR 33      // Start address for calibration data

// Static member initialization
bool Calibration::calibrationActive = false;
CalibrationMode Calibration::currentMode = CALIBRATION_MENU;
int Calibration::currentStep = 0;
int Calibration::totalSteps = 0;
int Calibration::selectedPoints = 3; // Default for multi-point
float Calibration::calibrationWeights[10] = {0};
float Calibration::rawReadings[10] = {0};
float Calibration::repeatabilityReadings[5] = {0};
CalibrationResult Calibration::result = {0};
float Calibration::currentRawReading = 0;
float Calibration::currentRefWeight = 0;
float Calibration::selectedRefWeight = 0;

// Linearity test variables
bool Calibration::inAscendingPhase = true;
float Calibration::linearityWeights[6] = {0};
float Calibration::linearityReadings[6] = {0};

// Drift test variables
unsigned long Calibration::driftStartTime = 0;
float Calibration::initialDriftReading = 0;
float Calibration::maxDrift = 0;

// Tare test variables
float Calibration::tareTestWeights[3] = {0};
float Calibration::tareMeasuredWeights[3] = {0};

// UI components
lv_obj_t* Calibration::leftPanel = nullptr;
lv_obj_t* Calibration::rightPanel = nullptr;
lv_obj_t* Calibration::titleLabel = nullptr;
lv_obj_t* Calibration::instructionLabel = nullptr;
lv_obj_t* Calibration::readingLabel = nullptr;
lv_obj_t* Calibration::progressLabel = nullptr;
lv_obj_t* Calibration::actionButton = nullptr;
lv_obj_t* Calibration::backButton = nullptr;
lv_obj_t* Calibration::secondaryButton = nullptr;
lv_obj_t* Calibration::weightOptions[7] = {nullptr};
lv_obj_t* Calibration::menuButtons[6] = {nullptr};
int Calibration::selectedWeightIndex = 0;

// Common weight values in grams
const float Calibration::COMMON_WEIGHTS[] = {10.0, 20.0, 50.0, 100.0, 200.0, 500.0, 1000.0};

void Calibration::init() {
  // Check if calibration data exists in EEPROM
  EEPROM.begin(128); // Ensure EEPROM is initialized
  
  byte validKey = EEPROM.read(CALIB_KEY_ADDR);
  
  if (validKey == CALIB_VALID_KEY) {
    // Load calibration data
    int addr = CALIB_DATA_ADDR;
    
    // Read whether to use non-linear calibration
    result.useNonLinear = EEPROM.read(addr++) != 0;
    
    // Read linear calibration factor (4 bytes float)
    EEPROM.get(addr, result.linearFactor);
    addr += sizeof(float);
    
    // Read non-linear coefficients (3 floats)
    for (int i = 0; i < 3; i++) {
      EEPROM.get(addr, result.nonLinearCoefficients[i]);
      addr += sizeof(float);
    }
    
    // Read error estimate
    EEPROM.get(addr, result.errorEstimate);
    addr += sizeof(float);
    
    // Read repeatability error
    EEPROM.get(addr, result.repeatabilityError);
    addr += sizeof(float);
    
    // Read quality
    result.quality = static_cast<CalibrationQuality>(EEPROM.read(addr++));
  } else {
    // Set default calibration values
    result.useNonLinear = false;
    result.linearFactor = 1.0f;
    result.nonLinearCoefficients[0] = 0.0f; // a (quadratic)
    result.nonLinearCoefficients[1] = 1.0f; // b (linear)
    result.nonLinearCoefficients[2] = 0.0f; // c (constant)
    result.errorEstimate = 0.0f;
    result.repeatabilityError = 0.0f;
    result.quality = QUALITY_FAIR;
  }
}

void Calibration::start() {
  calibrationActive = true;
  currentStep = 0;
  currentMode = CALIBRATION_MENU;
  
  // Create main UI
  createUI();
  
  // Show calibration menu
  showCalibrationMenu();
}

bool Calibration::isActive() {
  return calibrationActive;
}

void Calibration::update() {
  if (!calibrationActive) return;
  
  // Update the current raw reading
  currentRawReading = getRawReading();
  
  // Update reading display if available
  if (readingLabel != nullptr && !lv_obj_has_flag(readingLabel, LV_OBJ_FLAG_HIDDEN)) {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%.1fg", applyCalibration(currentRawReading));
    lv_label_set_text(readingLabel, buffer);
  }
  
  // Special handling for drift test
  if (currentMode == CALIBRATION_DRIFT && currentStep == 1) {
    updateDriftTest();
  }
}

float Calibration::applyCalibration(float rawWeight) {
  if (result.useNonLinear) {
    // Apply quadratic calibration: ax^2 + bx + c
    float a = result.nonLinearCoefficients[0];
    float b = result.nonLinearCoefficients[1];
    float c = result.nonLinearCoefficients[2];
    return a * rawWeight * rawWeight + b * rawWeight + c;
  } else {
    // Apply linear calibration
    return rawWeight * result.linearFactor;
  }
}

const char* Calibration::getStatusText() {
  static char buffer[32];
  
  switch (result.quality) {
    case QUALITY_EXCELLENT:
      snprintf(buffer, sizeof(buffer), "Calibration: Excellent");
      break;
    case QUALITY_GOOD:
      snprintf(buffer, sizeof(buffer), "Calibration: Good");
      break;
    case QUALITY_FAIR:
      snprintf(buffer, sizeof(buffer), "Calibration: Fair");
      break;
    case QUALITY_POOR:
      snprintf(buffer, sizeof(buffer), "Calibration: Poor");
      break;
    default:
      snprintf(buffer, sizeof(buffer), "Calibration: Unknown");
      break;
  }
  
  return buffer;
}

// UI Methods
void Calibration::createUI() {
  // Clean up existing UI if any
  clearUI();
  
  // Create dual panel layout
  // Left panel for instructions
  leftPanel = lv_obj_create(lv_scr_act());
  lv_obj_set_size(leftPanel, 294, 126);
  lv_obj_set_pos(leftPanel, 0, 0);
  lv_obj_set_style_bg_color(leftPanel, lv_color_black(), LV_PART_MAIN);
  lv_obj_set_style_border_width(leftPanel, 0, LV_PART_MAIN);
  
  // Right panel for interaction
  rightPanel = lv_obj_create(lv_scr_act());
  lv_obj_set_size(rightPanel, 294, 126);
  lv_obj_set_pos(rightPanel, 294, 0);
  lv_obj_set_style_bg_color(rightPanel, lv_color_black(), LV_PART_MAIN);
  lv_obj_set_style_border_width(rightPanel, 0, LV_PART_MAIN);
  
  // Create title and instruction labels on left panel
  titleLabel = lv_label_create(leftPanel);
  lv_obj_set_style_text_font(titleLabel, &lv_font_montserrat_24, LV_PART_MAIN);
  lv_obj_set_style_text_color(titleLabel, lv_color_white(), LV_PART_MAIN);
  lv_obj_align(titleLabel, LV_ALIGN_TOP_MID, 0, 10);
  
  instructionLabel = lv_label_create(leftPanel);
  lv_obj_set_style_text_font(instructionLabel, &lv_font_montserrat_16, LV_PART_MAIN);
  lv_obj_set_style_text_color(instructionLabel, lv_color_white(), LV_PART_MAIN);
  lv_obj_set_width(instructionLabel, 270);
  lv_obj_align(instructionLabel, LV_ALIGN_CENTER, 0, 10);
  
  progressLabel = lv_label_create(leftPanel);
  lv_obj_set_style_text_font(progressLabel, &lv_font_montserrat_16, LV_PART_MAIN);
  lv_obj_set_style_text_color(progressLabel, lv_color_white(), LV_PART_MAIN);
  lv_obj_align(progressLabel, LV_ALIGN_BOTTOM_MID, 0, -10);
  
  // Create back button
  backButton = lv_btn_create(rightPanel);
  lv_obj_set_size(backButton, 100, 40);
  lv_obj_align(backButton, LV_ALIGN_BOTTOM_MID, 0, -10);
  lv_obj_add_event_cb(backButton, onBackButtonClicked, LV_EVENT_CLICKED, NULL);
  
  lv_obj_t* backLabel = lv_label_create(backButton);
  lv_label_set_text(backLabel, "BACK");
  lv_obj_center(backLabel);
}

void Calibration::updateUI() {
  // Update UI based on current mode and step
  switch (currentMode) {
    case CALIBRATION_MENU:
      showCalibrationMenu();
      break;
    case CALIBRATION_BASIC:
      showBasicCalibrationStep(currentStep);
      break;
    case CALIBRATION_MULTI_PT:
      if (currentStep == 0) {
        showMultiPointSetup();
      } else {
        showMultiPointStep(currentStep - 1);
      }
      break;
    case CALIBRATION_LINEARITY:
      showLinearityStep(currentStep);
      break;
    case CALIBRATION_REPEATABILITY:
      showRepeatabilityStep(currentStep);
      break;
    case CALIBRATION_DRIFT:
      showDriftTest();
      break;
    case CALIBRATION_TARE:
      showTareVerificationStep(currentStep);
      break;
    case CALIBRATION_TEST:
      showTestMode();
      break;
    default:
      // Default to menu
      showCalibrationMenu();
      break;
  }
}

void Calibration::clearUI() {
  // Delete all UI elements if they exist
  if (leftPanel != nullptr) {
    lv_obj_del(leftPanel);
    leftPanel = nullptr;
  }
  
  if (rightPanel != nullptr) {
    lv_obj_del(rightPanel);
    rightPanel = nullptr;
  }
  
  // Reset pointers to child elements
  titleLabel = nullptr;
  instructionLabel = nullptr;
  readingLabel = nullptr;
  progressLabel = nullptr;
  actionButton = nullptr;
  backButton = nullptr;
  secondaryButton = nullptr;
  
  // Reset weight options
  for (int i = 0; i < 7; i++) {
    weightOptions[i] = nullptr;
  }
  
  // Reset menu buttons
  for (int i = 0; i < 6; i++) {
    menuButtons[i] = nullptr;
  }
}

void Calibration::showCalibrationTypeSelect() {
  // Update left panel
  lv_label_set_text(titleLabel, "CALIBRATION TYPE");
  lv_label_set_text(instructionLabel, "Choose calibration method");
  lv_label_set_text(progressLabel, "");
  
  // Create three options on the right panel
  const char* options[] = {"BASIC", "MULTI-PT", "TEST"};
  const char* descriptions[] = {"Single point", "Multiple points", "Verify"};
  
  for (int i = 0; i < 3; i++) {
    lv_obj_t* optionBtn = lv_btn_create(rightPanel);
    lv_obj_set_size(optionBtn, 120, 50);
    lv_obj_align(optionBtn, LV_ALIGN_TOP_MID, 0, 15 + i * 60);
    lv_obj_set_style_bg_color(optionBtn, lv_color_hex(0x0066CC), LV_PART_MAIN);
    
    lv_obj_t* optionLabel = lv_label_create(optionBtn);
    lv_label_set_text(optionLabel, options[i]);
    lv_obj_center(optionLabel);
    
    lv_obj_t* descLabel = lv_label_create(rightPanel);
    lv_obj_set_style_text_font(descLabel, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_label_set_text(descLabel, descriptions[i]);
    lv_obj_align(descLabel, LV_ALIGN_TOP_MID, 0, 70 + i * 60);
    
    // Set user data to identify which button was clicked
    lv_obj_set_user_data(optionBtn, (void*)(intptr_t)i);
    lv_obj_add_event_cb(optionBtn, [](lv_event_t* e) {
      lv_obj_t* btn = lv_event_get_target(e);
      int mode = (int)(intptr_t)lv_obj_get_user_data(btn);
      
      switch (mode) {
        case CALIBRATION_BASIC:
          Calibration::startBasicCalibration();
          break;
        case CALIBRATION_MULTI_PT:
          Calibration::showMultiPointSetup();
          break;
        case CALIBRATION_TEST:
          Calibration::startTestMode();
          break;
      }
    }, LV_EVENT_CLICKED, NULL);
  }
  
  // Hide the back button initially to prevent accidental exit
  lv_obj_add_flag(backButton, LV_OBJ_FLAG_HIDDEN);
  
  // Add direct exit button
  lv_obj_t* exitBtn = lv_btn_create(rightPanel);
  lv_obj_set_size(exitBtn, 100, 30);
  lv_obj_set_style_bg_color(exitBtn, lv_color_hex(0xCC0000), LV_PART_MAIN);
  lv_obj_align(exitBtn, LV_ALIGN_BOTTOM_MID, 0, -5);
  lv_obj_add_event_cb(exitBtn, [](lv_event_t* e) {
    Calibration::cancelCalibration();
  }, LV_EVENT_CLICKED, NULL);
  
  lv_obj_t* exitLabel = lv_label_create(exitBtn);
  lv_label_set_text(exitLabel, "EXIT");
  lv_obj_center(exitLabel);
}

// Basic calibration implementation
void Calibration::startBasicCalibration() {
  currentMode = CALIBRATION_BASIC;
  currentStep = 0;
  totalSteps = 3;
  
  // Show first step
  showBasicCalibrationStep(0);
}

void Calibration::showBasicCalibrationStep(int step) {
  currentStep = step;
  
  // Clear any previous options
  for (int i = 0; i < 7; i++) {
    if (weightOptions[i] != nullptr) {
      lv_obj_del(weightOptions[i]);
      weightOptions[i] = nullptr;
    }
  }
  
  // Common UI updates
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "STEP %d OF %d", currentStep + 1, totalSteps);
  lv_label_set_text(progressLabel, buffer);
  
  lv_obj_clear_flag(backButton, LV_OBJ_FLAG_HIDDEN);
  lv_label_set_text(titleLabel, "BASIC CALIBRATION");
  
  // Create or update action button
  if (actionButton != nullptr) {
    lv_obj_del(actionButton);
  }
  
  actionButton = lv_btn_create(rightPanel);
  lv_obj_set_size(actionButton, 120, 50);
  lv_obj_align(actionButton, LV_ALIGN_TOP_MID, 0, 20);
  lv_obj_set_style_bg_color(actionButton, lv_color_hex(0x0066CC), LV_PART_MAIN);
  lv_obj_add_event_cb(actionButton, onActionButtonClicked, LV_EVENT_CLICKED, NULL);
  
  lv_obj_t* actionLabel = lv_label_create(actionButton);
  
  // Create reading label
  if (readingLabel == nullptr) {
    readingLabel = lv_label_create(rightPanel);
    lv_obj_set_style_text_font(readingLabel, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(readingLabel, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(readingLabel, LV_ALIGN_CENTER, 0, 0);
  }
  
  // Step-specific UI
  switch (step) {
    case 0: // Remove weight
      lv_label_set_text(instructionLabel, "Remove all weight\nfrom the scale");
      lv_label_set_text(actionLabel, "TARE");
      lv_obj_center(actionLabel);
      break;
      
    case 1: // Add reference weight
      lv_label_set_text(instructionLabel, "Place a known weight\non the scale");
      lv_label_set_text(actionLabel, "CONFIRM");
      lv_obj_center(actionLabel);
      
      // Record tare value
      rawReadings[0] = currentRawReading;
      break;
      
    case 2: // Enter weight value
      lv_label_set_text(instructionLabel, "Select or adjust\nthe actual weight");
      
      // Record weight reading
      rawReadings[1] = currentRawReading;
      
      // Hide action button
      lv_obj_add_flag(actionButton, LV_OBJ_FLAG_HIDDEN);
      
      // Show weight selection
      showWeightSelection();
      break;
  }
}

void Calibration::showWeightSelection() {
  // Find closest standard weight to our reading
  float calibratedReading = applyCalibration(currentRawReading);
  int closestIndex = 0;
  float minDiff = 1000000.0;
  
  for (int i = 0; i < sizeof(COMMON_WEIGHTS) / sizeof(COMMON_WEIGHTS[0]); i++) {
    float diff = fabs(calibratedReading - COMMON_WEIGHTS[i]);
    if (diff < minDiff) {
      minDiff = diff;
      closestIndex = i;
    }
  }
  
  // Create weight options
  const int numOptions = 4;
  int startIdx = max(0, closestIndex - numOptions / 2);
  
  if (startIdx + numOptions > sizeof(COMMON_WEIGHTS) / sizeof(COMMON_WEIGHTS[0])) {
    startIdx = sizeof(COMMON_WEIGHTS) / sizeof(COMMON_WEIGHTS[0]) - numOptions;
  }
  
  // Create weight option buttons
  for (int i = 0; i < numOptions; i++) {
    int weightIdx = startIdx + i;
    float weight = COMMON_WEIGHTS[weightIdx];
    
    weightOptions[i] = lv_obj_create(rightPanel);
    lv_obj_set_size(weightOptions[i], 80, 40);
    
    // Calculate position (vertical list)
    int yPos = 10 + i * 50;
    if (i == numOptions / 2) { // Highlight middle option
      selectedWeightIndex = i;
      selectedRefWeight = weight;
      lv_obj_set_style_bg_color(weightOptions[i], lv_color_hex(0x0066CC), LV_PART_MAIN);
      lv_obj_set_style_bg_opa(weightOptions[i], LV_OPA_COVER, LV_PART_MAIN);
    } else {
      lv_obj_set_style_bg_opa(weightOptions[i], LV_OPA_TRANSP, LV_PART_MAIN);
      lv_obj_set_style_border_width(weightOptions[i], 1, LV_PART_MAIN);
      lv_obj_set_style_border_color(weightOptions[i], lv_color_white(), LV_PART_MAIN);
    }
    
    lv_obj_align(weightOptions[i], LV_ALIGN_TOP_MID, 0, yPos);
    
    // Create weight label
    lv_obj_t* weightLabel = lv_label_create(weightOptions[i]);
    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%.1fg", weight);
    lv_label_set_text(weightLabel, buffer);
    lv_obj_center(weightLabel);
    
    // Set user data for the option
    lv_obj_set_user_data(weightOptions[i], (void*)(intptr_t)i);
    
    // Add click event
    lv_obj_add_event_cb(weightOptions[i], onWeightOptionClicked, LV_EVENT_CLICKED, NULL);
  }
  
  // Add Save button
  lv_obj_t* saveBtn = lv_btn_create(rightPanel);
  lv_obj_set_size(saveBtn, 120, 40);
  lv_obj_align(saveBtn, LV_ALIGN_BOTTOM_MID, 0, -10);
  lv_obj_set_style_bg_color(saveBtn, lv_color_hex(0x00CC66), LV_PART_MAIN);
  
  lv_obj_t* saveLabel = lv_label_create(saveBtn);
  lv_label_set_text(saveLabel, "SAVE");
  lv_obj_center(saveLabel);
  
  // Add save click event
  lv_obj_add_event_cb(saveBtn, [](lv_event_t* e) {
    // Save the selected weight
    Calibration::calibrationWeights[1] = Calibration::selectedRefWeight;
    
    // Calculate calibration
    Calibration::calculateCalibration();
    
    // Show results
    Calibration::showResults();
  }, LV_EVENT_CLICKED, NULL);
}

void Calibration::onWeightOptionClicked(lv_event_t* e) {
  lv_obj_t* clicked = lv_event_get_target(e);
  int optionIndex = (int)(intptr_t)lv_obj_get_user_data(clicked);
  
  // Update selected weight
  selectWeight(optionIndex);
}

void Calibration::selectWeight(int index) {
  // Update selection
  selectedWeightIndex = index;
  
  // Update UI to reflect selection
  for (int i = 0; i < 7; i++) {
    if (weightOptions[i] != nullptr) {
      if (i == selectedWeightIndex) {
        lv_obj_set_style_bg_color(weightOptions[i], lv_color_hex(0x0066CC), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(weightOptions[i], LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_width(weightOptions[i], 0, LV_PART_MAIN);
        
        // Get the text from the child label
        lv_obj_t* label = lv_obj_get_child(weightOptions[i], 0);
        const char* weightText = lv_label_get_text(label);
        
        // Parse the weight value
        float weight = 0;
        sscanf(weightText, "%f", &weight);
        selectedRefWeight = weight;
      } else {
        lv_obj_set_style_bg_opa(weightOptions[i], LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(weightOptions[i], 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(weightOptions[i], lv_color_white(), LV_PART_MAIN);
      }
    }
  }
}

void Calibration::calculateCalibration() {
  // Handle different calibration modes
  switch (currentMode) {
    case CALIBRATION_BASIC:
      // Simple single-point calibration
      result.linearFactor = currentRefWeight / currentRawReading;
      result.useNonLinear = false;
      result.quality = QUALITY_FAIR; // Basic is always just "fair"
      
      // Save the calibration
      saveCalibration();
      
      // Show results
      showResults();
      break;
      
    case CALIBRATION_MULTI_PT:
      // Perform multi-point calibration calculation
      if (selectedPoints > 2) {
        calculatePolynomialCoefficients();
        result.errorEstimate = calculateErrorEstimate();
        result.quality = determineQuality();
      } else {
        // Simple linear fit for 2 points
        result.linearFactor = calculateLinearFactor();
        result.useNonLinear = false;
        result.quality = QUALITY_GOOD;
      }
      
      // Save the calibration
      saveCalibration();
      
      // Show results
      showResults();
      break;
      
    default:
      // Just return to the menu for other modes
      currentMode = CALIBRATION_MENU;
      Calibration::updateUI();
      break;
  }
}

float Calibration::calculateLinearFactor() {
  float zeroReading = rawReadings[0];
  float weightReading = rawReadings[1];
  float referenceWeight = calibrationWeights[1];
  
  // Calculate slope: weight / (reading - zero)
  float deltaReading = weightReading - zeroReading;
  
  if (fabs(deltaReading) < 0.001f) {
    // Avoid division by zero
    return 1.0f;
  }
  
  return referenceWeight / deltaReading;
}

void Calibration::calculatePolynomialCoefficients() {
  // Simple case for 3 points - quadratic fit
  // For more points, we would use a more sophisticated method (e.g., least squares)
  
  if (selectedPoints == 3) {
    // Three points exactly determine a quadratic
    float x1 = rawReadings[0];
    float x2 = rawReadings[1];
    float x3 = rawReadings[2];
    
    float y1 = calibrationWeights[0];
    float y2 = calibrationWeights[1];
    float y3 = calibrationWeights[2];
    
    // Calculate quadratic coefficients (ax^2 + bx + c)
    float denominator = (x1-x2) * (x1-x3) * (x2-x3);
    
    if (fabs(denominator) < 0.001f) {
      // If points are too close, fall back to linear
      result.nonLinearCoefficients[0] = 0.0f;  // a
      result.nonLinearCoefficients[1] = result.linearFactor;  // b
      result.nonLinearCoefficients[2] = 0.0f;  // c
    } else {
      float a = ((y1*(x2-x3) + y2*(x3-x1) + y3*(x1-x2)) / denominator);
      float b = ((y1*(x3*x3 - x2*x2) + y2*(x1*x1 - x3*x3) + y3*(x2*x2 - x1*x1)) / denominator);
      float c = ((y1*(x2*x3*x3 - x3*x2*x2) + y2*(x3*x1*x1 - x1*x3*x3) + y3*(x1*x2*x2 - x2*x1*x1)) / denominator);
      
      result.nonLinearCoefficients[0] = a;  // Quadratic term
      result.nonLinearCoefficients[1] = b;  // Linear term
      result.nonLinearCoefficients[2] = c;  // Constant term
    }
  } else {
    // For more or fewer points, use linear model
    result.nonLinearCoefficients[0] = 0.0f;  // a
    result.nonLinearCoefficients[1] = result.linearFactor;  // b
    result.nonLinearCoefficients[2] = 0.0f;  // c
  }
}

float Calibration::calculateErrorEstimate() {
  // Calculate the average error across all calibration points
  float totalError = 0.0f;
  
  for (int i = 0; i < selectedPoints; i++) {
    float actualWeight = calibrationWeights[i];
    float estimatedWeight = applyCalibration(rawReadings[i]);
    totalError += fabs(estimatedWeight - actualWeight);
  }
  
  return totalError / selectedPoints;
}

CalibrationQuality Calibration::determineQuality() {
  // Evaluate quality based on error and repeatability
  
  // Higher of the two errors
  float maxError = max(result.errorEstimate, result.repeatabilityError);
  
  // Determine quality based on percentage error
  // Using the highest calibration weight as reference
  float maxWeight = 0;
  for (int i = 0; i < selectedPoints; i++) {
    maxWeight = max(maxWeight, calibrationWeights[i]);
  }
  
  // Error as percentage of max weight
  float errorPercent = (maxWeight > 0.001f) ? (maxError * 100.0f / maxWeight) : 100.0f;
  
  if (errorPercent < 0.1f) {
    return QUALITY_EXCELLENT;
  } else if (errorPercent < 0.3f) {
    return QUALITY_GOOD;
  } else if (errorPercent < 0.5f) {
    return QUALITY_FAIR;
  } else {
    return QUALITY_POOR;
  }
}

void Calibration::saveCalibration() {
  // Store calibration data in EEPROM
  EEPROM.begin(128); // Ensure EEPROM is initialized
  
  // Write validation key
  EEPROM.write(CALIB_KEY_ADDR, CALIB_VALID_KEY);
  
  // Write calibration data
  int addr = CALIB_DATA_ADDR;
  
  // Write whether to use non-linear calibration
  EEPROM.write(addr++, result.useNonLinear ? 1 : 0);
  
  // Write linear calibration factor (4 bytes float)
  EEPROM.put(addr, result.linearFactor);
  addr += sizeof(float);
  
  // Write non-linear coefficients (3 floats)
  for (int i = 0; i < 3; i++) {
    EEPROM.put(addr, result.nonLinearCoefficients[i]);
    addr += sizeof(float);
  }
  
  // Write error estimate
  EEPROM.put(addr, result.errorEstimate);
  addr += sizeof(float);
  
  // Write repeatability error
  EEPROM.put(addr, result.repeatabilityError);
  addr += sizeof(float);
  
  // Write quality
  EEPROM.write(addr++, static_cast<uint8_t>(result.quality));
  
  // Commit changes
  EEPROM.commit();
  
  Serial.println("Calibration saved to EEPROM");
  
  // Show the calibration results screen
  Calibration::updateUI();
}

void Calibration::cancelCalibration() {
  // Exit without saving
  calibrationActive = false;
  
  // Clear UI
  clearUI();
  
  // Return to main UI
  ::updateUI();
}

void Calibration::showResults() {
  // Update UI to show calibration results
  lv_label_set_text(titleLabel, "CALIBRATION COMPLETE");
  
  // Show calibration quality and error
  char buffer[256];
  const char* qualityStr = "";
  
  switch (result.quality) {
    case QUALITY_EXCELLENT: qualityStr = "Excellent"; break;
    case QUALITY_GOOD: qualityStr = "Good"; break;
    case QUALITY_FAIR: qualityStr = "Fair"; break;
    case QUALITY_POOR: qualityStr = "Poor"; break;
    default: qualityStr = "Unknown"; break;
  }
  
  snprintf(buffer, sizeof(buffer), 
           "Accuracy: %s\nError: ±%.1fg\n\nMethod: %s\nModel: %s",
           qualityStr,
           result.errorEstimate,
           (currentMode == CALIBRATION_BASIC) ? "Basic" : "Multi-point",
           result.useNonLinear ? "Quadratic" : "Linear");
  
  lv_label_set_text(instructionLabel, buffer);
  lv_label_set_text(progressLabel, "");
  
  // Clear right panel
  lv_obj_t* children[10];
  uint32_t childCount = lv_obj_get_child_cnt(rightPanel);
  
  for (uint32_t i = 0; i < childCount && i < 10; i++) {
    children[i] = lv_obj_get_child(rightPanel, i);
  }
  
  for (uint32_t i = 0; i < childCount && i < 10; i++) {
    lv_obj_del(children[i]);
  }
  
  // Add test button
  lv_obj_t* testBtn = lv_btn_create(rightPanel);
  lv_obj_set_size(testBtn, 120, 40);
  lv_obj_align(testBtn, LV_ALIGN_TOP_MID, 0, 20);
  lv_obj_set_style_bg_color(testBtn, lv_color_hex(0x0066CC), LV_PART_MAIN);
  
  lv_obj_t* testLabel = lv_label_create(testBtn);
  lv_label_set_text(testLabel, "TEST");
  lv_obj_center(testLabel);
  
  lv_obj_add_event_cb(testBtn, [](lv_event_t* e) {
    // Start test mode
    Calibration::startTestMode();
  }, LV_EVENT_CLICKED, NULL);
  
  // Add done button
  lv_obj_t* doneBtn = lv_btn_create(rightPanel);
  lv_obj_set_size(doneBtn, 120, 40);
  lv_obj_align(doneBtn, LV_ALIGN_BOTTOM_MID, 0, -20);
  lv_obj_set_style_bg_color(doneBtn, lv_color_hex(0x00CC66), LV_PART_MAIN);
  
  lv_obj_t* doneLabel = lv_label_create(doneBtn);
  lv_label_set_text(doneLabel, "DONE");
  lv_obj_center(doneLabel);
  
  lv_obj_add_event_cb(doneBtn, [](lv_event_t* e) {
    // Exit calibration
    Calibration::cancelCalibration();
  }, LV_EVENT_CLICKED, NULL);
}

void Calibration::onActionButtonClicked(lv_event_t* e) {
  // Different behavior based on current mode and step
  switch (currentMode) {
    case CALIBRATION_BASIC:
      // Process basic calibration steps
      if (currentStep == 0) {
        // Zero point - record raw reading
        rawReadings[0] = currentRawReading;
        calibrationWeights[0] = 0;
        showBasicCalibrationStep(1);
      } else if (currentStep == 1) {
        // Show weight selection
        showWeightSelection();
      } else if (currentStep == 2) {
        // Calculate and save calibration
        calculateCalibration();
        showResults();
      } else {
        // Move to next step
        showBasicCalibrationStep(currentStep + 1);
      }
      break;
      
    case CALIBRATION_MULTI_PT:
      // Process multi-point calibration steps
      if (currentStep == 0) {
        // Zero point - record raw reading
        rawReadings[0] = currentRawReading;
        calibrationWeights[0] = 0;
        showMultiPointStep(0);
      } else if (currentStep <= selectedPoints) {
        // Show weight selection for this point
        showWeightSelection();
      } else {
        // Calculate and save calibration
        calculateCalibration();
        showResults();
      }
      break;
      
    case CALIBRATION_LINEARITY:
      // Process linearity test steps
      evaluateLinearity();
      break;
      
    case CALIBRATION_REPEATABILITY:
      // Process repeatability test steps
      evaluateRepeatability();
      break;
      
    case CALIBRATION_DRIFT:
      // Handle drift test
      evaluateDrift();
      break;
      
    case CALIBRATION_TARE:
      // Process tare verification steps
      evaluateTareVerification();
      break;
      
    case CALIBRATION_TEST:
      // Test mode - just return to menu
      currentMode = CALIBRATION_MENU;
      showCalibrationMenu();
      break;
      
    default:
      // Unknown mode, return to menu
      currentMode = CALIBRATION_MENU;
      showCalibrationMenu();
      break;
  }
}

void Calibration::onBackButtonClicked(lv_event_t* e) {
  // Different behavior based on current mode
  switch (currentMode) {
    case CALIBRATION_MENU:
      // Exit calibration system
      cancelCalibration();
      break;
      
    case CALIBRATION_BASIC:
    case CALIBRATION_MULTI_PT:
    case CALIBRATION_LINEARITY:
    case CALIBRATION_REPEATABILITY:
    case CALIBRATION_DRIFT:
    case CALIBRATION_TARE:
    case CALIBRATION_TEST:
      // If at first step, go back to menu
      if (currentStep == 0) {
        // Return to calibration menu
        currentMode = CALIBRATION_MENU;
        showCalibrationMenu();
      } else {
        // Go back one step in the current flow
        currentStep--;
        
        // Update UI based on current mode
        switch (currentMode) {
          case CALIBRATION_BASIC:
            showBasicCalibrationStep(currentStep);
            break;
          case CALIBRATION_MULTI_PT:
            if (currentStep == 0) {
              showMultiPointSetup();
            } else {
              showMultiPointStep(currentStep - 1);
            }
            break;
          case CALIBRATION_LINEARITY:
            showLinearityStep(currentStep);
            break;
          case CALIBRATION_REPEATABILITY:
            showRepeatabilityStep(currentStep);
            break;
          case CALIBRATION_TARE:
            showTareVerificationStep(currentStep);
            break;
          default:
            // For other modes, just cancel and return to menu
            currentMode = CALIBRATION_MENU;
            showCalibrationMenu();
            break;
        }
      }
      break;
      
    default:
      // Unknown mode, return to menu
      currentMode = CALIBRATION_MENU;
      showCalibrationMenu();
      break;
  }
}

void Calibration::startTestMode() {
  currentMode = CALIBRATION_TEST;
  showTestMode();
}

void Calibration::showTestMode() {
  // Clear right panel
  lv_obj_t* children[10];
  uint32_t childCount = lv_obj_get_child_cnt(rightPanel);
  
  for (uint32_t i = 0; i < childCount && i < 10; i++) {
    children[i] = lv_obj_get_child(rightPanel, i);
  }
  
  for (uint32_t i = 0; i < childCount && i < 10; i++) {
    lv_obj_del(children[i]);
  }
  
  // Update left panel
  lv_label_set_text(titleLabel, "TEST CALIBRATION");
  lv_label_set_text(instructionLabel, "Place any weight on the scale\n\nCheck if reading matches expected weight");
  lv_label_set_text(progressLabel, "");
  
  // Create current weight reading
  readingLabel = lv_label_create(rightPanel);
  lv_obj_set_style_text_font(readingLabel, &lv_font_montserrat_24, LV_PART_MAIN);
  lv_obj_align(readingLabel, LV_ALIGN_CENTER, 0, 0);
  
  // Add "Done" button
  lv_obj_t* doneBtn = lv_btn_create(rightPanel);
  lv_obj_set_size(doneBtn, 120, 40);
  lv_obj_align(doneBtn, LV_ALIGN_BOTTOM_MID, 0, -10);
  
  lv_obj_t* doneLabel = lv_label_create(doneBtn);
  lv_label_set_text(doneLabel, "DONE");
  lv_obj_center(doneLabel);
  
  lv_obj_add_event_cb(doneBtn, [](lv_event_t* e) {
    Calibration::showResults();
  }, LV_EVENT_CLICKED, NULL);
}

void Calibration::showCalibrationMenu() {
  // Set up title and instructions
  lv_label_set_text(titleLabel, "CALIBRATION MENU");
  lv_label_set_text(instructionLabel, "Select calibration mode");
  lv_label_set_text(progressLabel, "");
  
  // Change back button to "Exit"
  lv_obj_t* backLabel = lv_obj_get_child(backButton, 0);
  if (backLabel != nullptr) {
    lv_label_set_text(backLabel, "EXIT");
  }
  
  // Create 6 menu buttons for different calibration options
  const char* buttonLabels[] = {
    "Basic Calibration",
    "Multi-point Calibration",
    "Linearity Test",
    "Repeatability Test",
    "Drift Analysis",
    "Tare Verification"
  };
  
  // Create menu buttons in a grid layout
  int buttonWidth = 260;
  int buttonHeight = 40;
  int ySpacing = 10;
  
  for (int i = 0; i < 6; i++) {
    menuButtons[i] = lv_btn_create(rightPanel);
    lv_obj_set_size(menuButtons[i], buttonWidth, buttonHeight);
    lv_obj_set_pos(menuButtons[i], (294 - buttonWidth) / 2, 5 + i * (buttonHeight + ySpacing));
    
    // Set button styles
    lv_obj_set_style_bg_color(menuButtons[i], lv_color_make(0, 100, 120), LV_PART_MAIN);
    lv_obj_set_style_radius(menuButtons[i], 5, LV_PART_MAIN);
    
    // Add click event 
    lv_obj_add_event_cb(menuButtons[i], onMenuButtonClicked, LV_EVENT_CLICKED, (void*)i);
    
    // Add label to button
    lv_obj_t* btnLabel = lv_label_create(menuButtons[i]);
    lv_label_set_text(btnLabel, buttonLabels[i]);
    lv_obj_center(btnLabel);
  }
  
  // Hide readingLabel
  if (readingLabel != nullptr) {
    lv_obj_add_flag(readingLabel, LV_OBJ_FLAG_HIDDEN);
  }
}

void Calibration::onMenuButtonClicked(lv_event_t* e) {
  int idx = (int)lv_event_get_user_data(e);
  
  // Clear menu UI
  for (int i = 0; i < 6; i++) {
    if (menuButtons[i] != nullptr) {
      lv_obj_del(menuButtons[i]);
      menuButtons[i] = nullptr;
    }
  }
  
  // Start appropriate calibration mode
  switch (idx) {
    case 0: // Basic Calibration
      startBasicCalibration();
      break;
    case 1: // Multi-point Calibration
      startMultiPointCalibration();
      break;
    case 2: // Linearity Test
      startLinearityTest();
      break;
    case 3: // Repeatability Test
      startRepeatabilityTest();
      break;
    case 4: // Drift Analysis
      startDriftTest();
      break;
    case 5: // Tare Verification
      startTareVerification();
      break;
  }
}

void Calibration::startLinearityTest() {
  currentMode = CALIBRATION_LINEARITY;
  currentStep = 0;
  totalSteps = 6; // 3 ascending + 3 descending points
  inAscendingPhase = true;
  
  // Reset test variables
  for (int i = 0; i < 6; i++) {
    linearityWeights[i] = 0;
    linearityReadings[i] = 0;
  }
  
  // Start with first step
  showLinearityStep(0);
}

void Calibration::startRepeatabilityTest() {
  currentMode = CALIBRATION_REPEATABILITY;
  currentStep = 0;
  totalSteps = 5; // 5 repetitions
  
  // Reset readings
  for (int i = 0; i < 5; i++) {
    repeatabilityReadings[i] = 0;
  }
  
  // Start with first step
  showRepeatabilityStep(0);
}

void Calibration::startDriftTest() {
  currentMode = CALIBRATION_DRIFT;
  currentStep = 0;
  
  // Reset drift values
  driftStartTime = millis();
  initialDriftReading = 0;
  maxDrift = 0;
  
  // Show drift test UI
  showDriftTest();
}

void Calibration::startTareVerification() {
  currentMode = CALIBRATION_TARE;
  currentStep = 0;
  totalSteps = 3; // 3 different weights to test
  
  // Reset tare test values
  for (int i = 0; i < 3; i++) {
    tareTestWeights[i] = 0;
    tareMeasuredWeights[i] = 0;
  }
  
  // Suggested test weights
  tareTestWeights[0] = 50.0f;
  tareTestWeights[1] = 100.0f;
  tareTestWeights[2] = 200.0f;
  
  // Start with first step
  showTareVerificationStep(0);
}

void Calibration::showLinearityStep(int step) {
  // Set current step
  currentStep = step;
  
  // Set up title and progress indicator
  lv_label_set_text(titleLabel, "LINEARITY TEST");
  
  char progressText[32];
  snprintf(progressText, sizeof(progressText), "Step %d of %d", step + 1, totalSteps);
  lv_label_set_text(progressLabel, progressText);
  
  // Phase indicator
  const char* phaseText = inAscendingPhase ? "Ascending Phase" : "Descending Phase";
  
  // Target weight for this step
  float targetWeight = 0;
  if (inAscendingPhase) {
    // Steps 0, 1, 2 (0g, 50g, 100g)
    targetWeight = step * 50.0f;
  } else {
    // Steps 3, 4, 5 (100g, 50g, 0g)
    targetWeight = (5 - step) * 50.0f;
  }
  
  // Instructions based on the step
  char instructions[128];
  snprintf(instructions, sizeof(instructions), 
          "%s\n\nPlease %s the scale to %.0fg\n\nTarget sequence:\nAscending: 0→50→100g\nDescending: 100→50→0g", 
          phaseText,
          (targetWeight > 0) ? "add weight to" : "remove all weight from",
          targetWeight);
  
  lv_label_set_text(instructionLabel, instructions);
  
  // Create weight reading display
  if (readingLabel == nullptr) {
    readingLabel = lv_label_create(rightPanel);
    lv_obj_set_style_text_font(readingLabel, &lv_font_montserrat_36, LV_PART_MAIN);
    lv_obj_set_style_text_color(readingLabel, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(readingLabel, LV_ALIGN_CENTER, 0, -20);
  } else {
    lv_obj_clear_flag(readingLabel, LV_OBJ_FLAG_HIDDEN);
  }
  
  // Button label text
  const char* buttonText = "CONFIRM WEIGHT";
  
  // Create or update action button
  if (actionButton != nullptr) {
    lv_obj_t* label = lv_obj_get_child(actionButton, 0);
    if (label != nullptr) {
      lv_label_set_text(label, buttonText);
    }
  } else {
    actionButton = lv_btn_create(rightPanel);
    lv_obj_set_size(actionButton, 200, 40);
    lv_obj_align(actionButton, LV_ALIGN_BOTTOM_MID, 0, -60);
    lv_obj_add_event_cb(actionButton, onActionButtonClicked, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t* label = lv_label_create(actionButton);
    lv_label_set_text(label, buttonText);
    lv_obj_center(label);
  }
}

// Handle linearity test progression
void Calibration::evaluateLinearity() {
  int step = currentStep;
  
  // Record the current reading
  float currentWeight = applyCalibration(currentRawReading);
  
  if (inAscendingPhase) {
    // Store ascending data
    linearityWeights[step] = step * 50.0f; // 0, 50, 100
    linearityReadings[step] = currentWeight;
    
    // Check if we've completed ascending phase
    if (step == 2) {
      // Switch to descending phase
      inAscendingPhase = false;
    }
  } else {
    // Store descending data
    int descIdx = step - 3; // Map steps 3,4,5 to indices 0,1,2
    linearityWeights[step] = (2 - descIdx) * 50.0f; // 100, 50, 0
    linearityReadings[step] = currentWeight;
  }
  
  // Move to next step or finish
  if (step < totalSteps - 1) {
    // Continue to next step
    showLinearityStep(step + 1);
  } else {
    // Completed all steps, show results
    showResults();
  }
}

void Calibration::showRepeatabilityStep(int step) {
  // Set current step
  currentStep = step;
  
  // Set up title and progress indicator
  lv_label_set_text(titleLabel, "REPEATABILITY TEST");
  
  char progressText[32];
  snprintf(progressText, sizeof(progressText), "Test %d of %d", step + 1, totalSteps);
  lv_label_set_text(progressLabel, progressText);
  
  // Instructions based on even/odd steps
  // Even steps (0, 2, 4): Add 100g weight
  // Odd steps (1, 3): Remove weight
  bool isAddingWeight = (step % 2 == 0);
  
  char instructions[128];
  if (isAddingWeight) {
    snprintf(instructions, sizeof(instructions), 
            "Place a 100g reference weight\non the scale\n\nThis test checks if the scale\ngives consistent readings\nfor the same weight.");
  } else {
    snprintf(instructions, sizeof(instructions), 
            "Remove the weight from\nthe scale\n\nThis test checks if the scale\nreturns to zero properly.");
  }
  
  lv_label_set_text(instructionLabel, instructions);
  
  // Create weight reading display
  if (readingLabel == nullptr) {
    readingLabel = lv_label_create(rightPanel);
    lv_obj_set_style_text_font(readingLabel, &lv_font_montserrat_36, LV_PART_MAIN);
    lv_obj_set_style_text_color(readingLabel, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(readingLabel, LV_ALIGN_CENTER, 0, -20);
  } else {
    lv_obj_clear_flag(readingLabel, LV_OBJ_FLAG_HIDDEN);
  }
  
  // Show previous readings if available
  if (step > 0) {
    char readings[96] = "";
    for (int i = 0; i < step; i++) {
      if (i % 2 == 0) { // Only show loaded readings
        char reading[16];
        snprintf(reading, sizeof(reading), "%s%.1fg", 
                (i > 0) ? ", " : "",
                repeatabilityReadings[i]);
        strcat(readings, reading);
      }
    }
    
    if (strlen(readings) > 0) {
      char readingsText[128];
      snprintf(readingsText, sizeof(readingsText), "Previous readings: %s", readings);
      
      if (secondaryButton == nullptr) {
        // Create an info label for previous readings
        secondaryButton = lv_label_create(rightPanel);
        lv_obj_set_style_text_font(secondaryButton, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_set_style_text_color(secondaryButton, lv_color_white(), LV_PART_MAIN);
        lv_obj_set_width(secondaryButton, 280);
        lv_obj_align(secondaryButton, LV_ALIGN_BOTTOM_MID, 0, -110);
      }
      
      lv_label_set_text(secondaryButton, readingsText);
    }
  }
  
  // Button label text
  const char* buttonText = "CONFIRM";
  
  // Create or update action button
  if (actionButton != nullptr) {
    lv_obj_t* label = lv_obj_get_child(actionButton, 0);
    if (label != nullptr) {
      lv_label_set_text(label, buttonText);
    }
  } else {
    actionButton = lv_btn_create(rightPanel);
    lv_obj_set_size(actionButton, 200, 40);
    lv_obj_align(actionButton, LV_ALIGN_BOTTOM_MID, 0, -60);
    lv_obj_add_event_cb(actionButton, onActionButtonClicked, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t* label = lv_label_create(actionButton);
    lv_label_set_text(label, buttonText);
    lv_obj_center(label);
  }
}

// Handle repeatability test progression
void Calibration::evaluateRepeatability() {
  int step = currentStep;
  
  // Record the current reading
  float currentWeight = applyCalibration(currentRawReading);
  repeatabilityReadings[step] = currentWeight;
  
  // Move to next step or finish
  if (step < totalSteps - 1) {
    // Continue to next step
    showRepeatabilityStep(step + 1);
  } else {
    // Completed all steps, now calculate standard deviation
    float loadedValues[3] = {
      repeatabilityReadings[0], // First loaded reading
      repeatabilityReadings[2], // Second loaded reading
      repeatabilityReadings[4]  // Third loaded reading
    };
    
    // Calculate standard deviation
    float stdDev = calculateStandardDeviation(loadedValues, 3);
    
    // Store the repeatability error for saving with calibration
    result.repeatabilityError = stdDev;
    
    // Show results
    showResults();
  }
}

float Calibration::calculateStandardDeviation(float* values, int count) {
  if (count <= 1) return 0.0f;
  
  // Calculate mean
  float sum = 0.0f;
  for (int i = 0; i < count; i++) {
    sum += values[i];
  }
  float mean = sum / count;
  
  // Calculate sum of squared differences
  float sqDiffSum = 0.0f;
  for (int i = 0; i < count; i++) {
    float diff = values[i] - mean;
    sqDiffSum += diff * diff;
  }
  
  // Calculate standard deviation
  return sqrt(sqDiffSum / (count - 1));
}

void Calibration::showDriftTest() {
  // Set up title
  lv_label_set_text(titleLabel, "DRIFT ANALYSIS");
  
  // Instructions for drift test
  const char* instructions = 
    "Place a reference weight on\n"
    "the scale and leave undisturbed\n\n"
    "This test monitors for weight\n"
    "drift over time. Leave the weight\n"
    "in place for at least 5 minutes.";
  
  lv_label_set_text(instructionLabel, instructions);
  
  // Create weight reading display
  if (readingLabel == nullptr) {
    readingLabel = lv_label_create(rightPanel);
    lv_obj_set_style_text_font(readingLabel, &lv_font_montserrat_36, LV_PART_MAIN);
    lv_obj_set_style_text_color(readingLabel, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(readingLabel, LV_ALIGN_CENTER, 0, -20);
  } else {
    lv_obj_clear_flag(readingLabel, LV_OBJ_FLAG_HIDDEN);
  }
  
  // Create info label for drift statistics
  if (secondaryButton == nullptr) {
    secondaryButton = lv_label_create(rightPanel);
    lv_obj_set_style_text_font(secondaryButton, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(secondaryButton, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_width(secondaryButton, 280);
    lv_obj_align(secondaryButton, LV_ALIGN_BOTTOM_MID, 0, -110);
    lv_label_set_text(secondaryButton, "Elapsed: 00:00\nInitial: 0.0g\nDrift: 0.0g");
  }
  
  // Start button
  const char* buttonText = "START";
  
  // Create or update action button
  if (actionButton != nullptr) {
    lv_obj_t* label = lv_obj_get_child(actionButton, 0);
    if (label != nullptr) {
      lv_label_set_text(label, buttonText);
    }
  } else {
    actionButton = lv_btn_create(rightPanel);
    lv_obj_set_size(actionButton, 200, 40);
    lv_obj_align(actionButton, LV_ALIGN_BOTTOM_MID, 0, -60);
    lv_obj_add_event_cb(actionButton, onActionButtonClicked, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t* label = lv_label_create(actionButton);
    lv_label_set_text(label, buttonText);
    lv_obj_center(label);
  }
  
  // Progress display
  lv_label_set_text(progressLabel, "Ready to start");
}

void Calibration::evaluateDrift() {
  // If this is the first press (start), record initial weight
  if (currentStep == 0) {
    driftStartTime = millis();
    initialDriftReading = applyCalibration(currentRawReading);
    maxDrift = 0;
    
    // Update button to "STOP"
    lv_obj_t* label = lv_obj_get_child(actionButton, 0);
    if (label != nullptr) {
      lv_label_set_text(label, "STOP");
    }
    
    // Update progress message
    lv_label_set_text(progressLabel, "Test running...");
    
    // Increment step to indicate we're running
    currentStep = 1;
    
    // Return without finishing
    return;
  }
  
  // Otherwise, this is the stop button press - evaluate results
  unsigned long elapsedTime = millis() - driftStartTime;
  float currentWeight = applyCalibration(currentRawReading);
  float totalDrift = currentWeight - initialDriftReading;
  
  // Store drift in results
  result.repeatabilityError = fabsf(totalDrift);
  
  // Show results
  showResults();
}

// Update function for drift test - call this from the main update loop
void Calibration::updateDriftTest() {
  if (currentMode != CALIBRATION_DRIFT || currentStep != 1) return;
  
  // Calculate time and drift
  unsigned long elapsedTime = millis() - driftStartTime;
  float currentWeight = applyCalibration(currentRawReading);
  float currentDrift = currentWeight - initialDriftReading;
  
  // Update max drift if needed
  if (fabsf(currentDrift) > fabsf(maxDrift)) {
    maxDrift = currentDrift;
  }
  
  // Format time as mm:ss
  unsigned long seconds = elapsedTime / 1000;
  unsigned long minutes = seconds / 60;
  seconds %= 60;
  
  // Update the info display
  char driftInfo[64];
  snprintf(driftInfo, sizeof(driftInfo), 
          "Elapsed: %02lu:%02lu\nInitial: %.1fg\nCurrent: %.1fg\nDrift: %+.1fg", 
          minutes, seconds, initialDriftReading, currentWeight, currentDrift);
  
  if (secondaryButton != nullptr) {
    lv_label_set_text(secondaryButton, driftInfo);
  }
}

void Calibration::showTareVerificationStep(int step) {
  // Set current step
  currentStep = step;
  
  // Set up title and progress indicator
  lv_label_set_text(titleLabel, "TARE VERIFICATION");
  
  char progressText[32];
  snprintf(progressText, sizeof(progressText), "Step %d of %d", step + 1, totalSteps);
  lv_label_set_text(progressLabel, progressText);
  
  // Instructions based on the step (3 step process)
  char instructions[128];
  
  if (step == 0) {
    // Step 1: Place first weight
    snprintf(instructions, sizeof(instructions), 
            "Place a %.0fg reference weight\non the scale\n\n"
            "This test verifies that the\ntare function works correctly\n"
            "with different preloaded weights.",
            tareTestWeights[0]);
  } else if (step == 1) {
    // Step 2: Place second weight
    snprintf(instructions, sizeof(instructions), 
            "Tare the scale, then place\na %.0fg reference weight\n\n"
            "Press TARE first, then place\nthe weight and press NEXT.",
            tareTestWeights[1]);
  } else {
    // Step 3: Place third weight
    snprintf(instructions, sizeof(instructions), 
            "Tare the scale, then place\na %.0fg reference weight\n\n"
            "Press TARE first, then place\nthe weight and press NEXT.",
            tareTestWeights[2]);
  }
  
  lv_label_set_text(instructionLabel, instructions);
  
  // Create weight reading display
  if (readingLabel == nullptr) {
    readingLabel = lv_label_create(rightPanel);
    lv_obj_set_style_text_font(readingLabel, &lv_font_montserrat_36, LV_PART_MAIN);
    lv_obj_set_style_text_color(readingLabel, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(readingLabel, LV_ALIGN_CENTER, 0, -20);
  } else {
    lv_obj_clear_flag(readingLabel, LV_OBJ_FLAG_HIDDEN);
  }
  
  // Create tare button for steps 1 and 2
  if (step > 0) {
    if (secondaryButton == nullptr) {
      secondaryButton = lv_btn_create(rightPanel);
      lv_obj_set_size(secondaryButton, 100, 40);
      lv_obj_align(secondaryButton, LV_ALIGN_BOTTOM_LEFT, 20, -60);
      lv_obj_add_event_cb(secondaryButton, onSecondaryButtonClicked, LV_EVENT_CLICKED, NULL);
      
      lv_obj_t* label = lv_label_create(secondaryButton);
      lv_label_set_text(label, "TARE");
      lv_obj_center(label);
    } else if (lv_obj_check_type(secondaryButton, &lv_btn_class)) {
      lv_obj_clear_flag(secondaryButton, LV_OBJ_FLAG_HIDDEN);
      
      // Make sure label says TARE
      lv_obj_t* label = lv_obj_get_child(secondaryButton, 0);
      if (label != nullptr) {
        lv_label_set_text(label, "TARE");
      }
    }
  } else if (secondaryButton != nullptr && lv_obj_check_type(secondaryButton, &lv_btn_class)) {
    lv_obj_add_flag(secondaryButton, LV_OBJ_FLAG_HIDDEN);
  }
  
  // Next button
  const char* buttonText = step < totalSteps - 1 ? "NEXT" : "FINISH";
  
  // Create or update action button
  if (actionButton != nullptr) {
    lv_obj_t* label = lv_obj_get_child(actionButton, 0);
    if (label != nullptr) {
      lv_label_set_text(label, buttonText);
    }
    
    // Position based on whether there's a tare button
    if (step > 0) {
      lv_obj_align(actionButton, LV_ALIGN_BOTTOM_RIGHT, -20, -60);
      lv_obj_set_size(actionButton, 100, 40);
    } else {
      lv_obj_align(actionButton, LV_ALIGN_BOTTOM_MID, 0, -60);
      lv_obj_set_size(actionButton, 200, 40);
    }
  } else {
    actionButton = lv_btn_create(rightPanel);
    lv_obj_set_size(actionButton, 200, 40);
    lv_obj_align(actionButton, LV_ALIGN_BOTTOM_MID, 0, -60);
    lv_obj_add_event_cb(actionButton, onActionButtonClicked, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t* label = lv_label_create(actionButton);
    lv_label_set_text(label, buttonText);
    lv_obj_center(label);
  }
}

void Calibration::evaluateTareVerification() {
  int step = currentStep;
  
  // Record the current reading
  float currentWeight = applyCalibration(currentRawReading);
  tareMeasuredWeights[step] = currentWeight;
  
  // Move to next step or finish
  if (step < totalSteps - 1) {
    // Continue to next step
    showTareVerificationStep(step + 1);
  } else {
    // Completed all steps, show results
    showResults();
  }
}

void Calibration::onSecondaryButtonClicked(lv_event_t* e) {
  // Handle based on the current mode
  switch (currentMode) {
    case CALIBRATION_TARE:
      // Perform tare operation
      tareScale(); // Call the external tare function
      break;
      
    default:
      // Handle other modes if needed
      break;
  }
} 