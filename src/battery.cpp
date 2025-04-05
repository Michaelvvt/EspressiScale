#include <Arduino.h>
#include "esp_adc_cal.h"
#include "battery.h"
#include "lvgl.h"

// Battery UI elements
static lv_obj_t* batteryContainer = nullptr;
static lv_obj_t* batteryOutline = nullptr;
static lv_obj_t* batteryFill = nullptr;
static lv_obj_t* batteryTerminal = nullptr;
static lv_obj_t* percentageLabel = nullptr;
static lv_obj_t* chargingIcon = nullptr;

// Battery state variables
static uint8_t currentPercentage = 100;
static bool isCurrentlyCharging = false;
static bool isVisible = true;
static bool showPercentageText = true;

// Battery dimensions for UI
#define BATTERY_WIDTH 26
#define BATTERY_HEIGHT 14
#define TERMINAL_WIDTH 2
#define TERMINAL_HEIGHT 8

// For ADC reading averaging
#define SAMPLE_COUNT 5
static int samples[SAMPLE_COUNT] = {0};
static int sampleIndex = 0;

// Battery parameters
static float fullVoltage = 4.2f;     // Voltage when battery is full
static float emptyVoltage = 3.3f;    // Voltage when battery is empty
static float voltageOffset = 0.0f;   // Calibration offset

// Function to read and calibrate ADC values
uint32_t readADC_Cal(int ADC_Raw) {
    esp_adc_cal_characteristics_t adc_chars;
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, &adc_chars);
    return (esp_adc_cal_raw_to_voltage(ADC_Raw, &adc_chars));
}

// Read averaged battery ADC value
int readAveragedVoltageADC() {
    // Read new sample
    int newSample = analogRead(PIN_BAT_ADC);
    
    // Add to circular buffer
    samples[sampleIndex] = newSample;
    sampleIndex = (sampleIndex + 1) % SAMPLE_COUNT;
    
    // Calculate average
    long sum = 0;
    for (int i = 0; i < SAMPLE_COUNT; i++) {
        sum += samples[i];
    }
    
    return sum / SAMPLE_COUNT;
}

void setupBattery() {
    pinMode(PIN_BAT_EN, OUTPUT);
    digitalWrite(PIN_BAT_EN, HIGH);
    
    // Initialize averaging array
    for (int i = 0; i < SAMPLE_COUNT; i++) {
        samples[i] = 0;
    }
    
    // Initialize charging detection pin if available
    if (PIN_CHARGING >= 0) {
        pinMode(PIN_CHARGING, INPUT);
    }
}

float getBatteryVoltage() {
    // Use the ESP-specific calibrated reading and voltage divider
    uint32_t voltage_raw = readADC_Cal(readAveragedVoltageADC());
    float voltage = (voltage_raw * 2.0f / 1000.0f) + voltageOffset;
    return voltage;
}

uint8_t getBatteryPercentage() {
    float voltage = getBatteryVoltage();
    
    // Calculate percentage based on voltage range
    float range = fullVoltage - emptyVoltage;
    if (range <= 0) range = 0.9f; // Avoid division by zero
    
    float normalizedVoltage = voltage - emptyVoltage;
    int percentage = (normalizedVoltage / range) * 100.0f;
    
    // Clamp percentage
    if (percentage > 100) percentage = 100;
    if (percentage < 0) percentage = 0;
    
    return percentage;
}

BatteryLevel getBatteryLevel() {
    if (isCurrentlyCharging) return BATTERY_CHARGING;
    
    uint8_t percentage = getBatteryPercentage();
    if (percentage <= 5) return BATTERY_CRITICAL;
    if (percentage <= 19) return BATTERY_LOW;
    if (percentage <= 49) return BATTERY_MEDIUM;
    return BATTERY_HIGH;
}

bool isBatteryCharging() {
    if (PIN_CHARGING >= 0) {
        // Adjust depending on hardware (HIGH or LOW when charging)
        return digitalRead(PIN_CHARGING) == HIGH;
    }
    return false; // If charging detection not supported
}

void drawBatteryIndicator(lv_obj_t* parent, int16_t x, int16_t y, bool showPercentage) {
    showPercentageText = showPercentage;
    currentPercentage = getBatteryPercentage();
    isCurrentlyCharging = isBatteryCharging();
    isVisible = true;
    
    batteryContainer = lv_obj_create(parent);
    lv_obj_remove_style_all(batteryContainer);
    lv_obj_set_style_bg_opa(batteryContainer, LV_OPA_TRANSP, 0);
    lv_obj_set_size(batteryContainer, BATTERY_WIDTH + 40, BATTERY_HEIGHT + 10);
    lv_obj_set_pos(batteryContainer, x, y);
    
    if (showPercentageText) {
        percentageLabel = lv_label_create(batteryContainer);
        lv_obj_set_style_text_font(percentageLabel, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(percentageLabel, lv_color_white(), 0);
        lv_obj_align(percentageLabel, LV_ALIGN_LEFT_MID, 0, 0);
        lv_label_set_text_fmt(percentageLabel, "%d%%", currentPercentage);
    }
    
    batteryOutline = lv_obj_create(batteryContainer);
    lv_obj_remove_style_all(batteryOutline);
    lv_obj_set_style_bg_opa(batteryOutline, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(batteryOutline, 1, 0);
    lv_obj_set_style_border_color(batteryOutline, lv_color_white(), 0);
    lv_obj_set_style_radius(batteryOutline, 3, 0);
    
    if (showPercentageText) {
        lv_obj_align_to(batteryOutline, percentageLabel, LV_ALIGN_OUT_RIGHT_MID, 5, 0);
    } else {
        lv_obj_align(batteryOutline, LV_ALIGN_CENTER, 0, 0);
    }
    
    lv_obj_set_size(batteryOutline, BATTERY_WIDTH, BATTERY_HEIGHT);
    
    batteryTerminal = lv_obj_create(batteryContainer);
    lv_obj_remove_style_all(batteryTerminal);
    lv_obj_set_style_bg_color(batteryTerminal, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(batteryTerminal, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(batteryTerminal, 1, 0);
    lv_obj_set_size(batteryTerminal, TERMINAL_WIDTH, TERMINAL_HEIGHT);
    lv_obj_align_to(batteryTerminal, batteryOutline, LV_ALIGN_OUT_RIGHT_MID, 0, 0);
    
    batteryFill = lv_obj_create(batteryOutline);
    lv_obj_remove_style_all(batteryFill);
    
    // Set color based on battery level
    BatteryLevel level = getBatteryLevel();
    lv_color_t fillColor;
    
    switch (level) {
        case BATTERY_CRITICAL:
            fillColor = lv_color_make(255, 0, 0); // Red
            break;
        case BATTERY_LOW:
            fillColor = lv_color_make(255, 165, 0); // Orange
            break;
        case BATTERY_MEDIUM:
        case BATTERY_HIGH:
        case BATTERY_CHARGING:
            fillColor = lv_color_make(0, 200, 0); // Green
            break;
        default:
            fillColor = lv_color_make(0, 200, 0);
    }
    
    lv_obj_set_style_bg_color(batteryFill, fillColor, 0);
    lv_obj_set_style_bg_opa(batteryFill, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(batteryFill, 2, 0);
    lv_obj_set_style_border_width(batteryFill, 0, 0);
    
    int fillWidth = ((BATTERY_WIDTH - 4) * currentPercentage) / 100;
    if (fillWidth < 2 && currentPercentage > 0) fillWidth = 2;
    
    lv_obj_set_size(batteryFill, fillWidth, BATTERY_HEIGHT - 4);
    lv_obj_align(batteryFill, LV_ALIGN_LEFT_MID, 2, 0);
    
    chargingIcon = lv_label_create(batteryOutline);
    lv_obj_set_style_text_font(chargingIcon, lv_theme_default_get()->font_small, 0);
    lv_obj_set_style_text_color(chargingIcon, lv_color_black(), 0);
    lv_label_set_text(chargingIcon, LV_SYMBOL_CHARGE);
    lv_obj_center(chargingIcon);
    
    if (isCurrentlyCharging) {
        lv_obj_clear_flag(chargingIcon, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(chargingIcon, LV_OBJ_FLAG_HIDDEN);
    }
}

void updateBatteryIndicator() {
    if (!batteryContainer || !isVisible) return;
    
    currentPercentage = getBatteryPercentage();
    isCurrentlyCharging = isBatteryCharging();
    
    // Update percentage text if displayed
    if (showPercentageText && percentageLabel) {
        lv_label_set_text_fmt(percentageLabel, "%d%%", currentPercentage);
    }
    
    // Update fill amount
    int fillWidth = ((BATTERY_WIDTH - 4) * currentPercentage) / 100;
    if (fillWidth < 2 && currentPercentage > 0) fillWidth = 2;
    
    lv_obj_set_size(batteryFill, fillWidth, BATTERY_HEIGHT - 4);
    
    // Update fill color based on level
    BatteryLevel level = getBatteryLevel();
    lv_color_t fillColor;
    
    switch (level) {
        case BATTERY_CRITICAL:
            fillColor = lv_color_make(255, 0, 0); // Red
            break;
        case BATTERY_LOW:
            fillColor = lv_color_make(255, 165, 0); // Orange
            break;
        case BATTERY_MEDIUM:
        case BATTERY_HIGH:
        case BATTERY_CHARGING:
            fillColor = lv_color_make(0, 200, 0); // Green
            break;
        default:
            fillColor = lv_color_make(0, 200, 0);
    }
    
    lv_obj_set_style_bg_color(batteryFill, fillColor, 0);
    
    // Show/hide charging icon
    if (isCurrentlyCharging) {
        lv_obj_clear_flag(chargingIcon, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(chargingIcon, LV_OBJ_FLAG_HIDDEN);
    }
}
