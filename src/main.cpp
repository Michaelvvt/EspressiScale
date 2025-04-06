#include "arduino.h"
#include <battery.h>
#include <scale.h>
#include <filter.h>
#include "jd9613.h"
#include "lvgl.h"
#include "pin_config.h"
#include "SPI.h"
#include "time.h"
#include "sntp.h"
#define TOUCH_MODULES_CST_SELF
#include "TouchLib.h"
#include "Wire.h"
#include "wifiManager.h"
#include <PrettyOTA.h>
#include "ble_service.h"
#include "settings.h"
#include "menu_system.h"
#include "auto_timer.h"
#include "lvgl_fonts.h"
#include "FS.h"
#include "EEPROM.h"
#include "Update.h"
#include "esp_bt.h"
#include "HX711.h"
#include "NimBLEDevice.h"
#include "calibration.h"

// Load cell power control pin (from scale.cpp)
#define LOADCELL_POWER_PIN 6

#ifndef BOARD_HAS_PSRAM
#error "Please turn on PSRAM option to OPI PSRAM"
#endif

WiFiManager wifiManager;
String header;

AsyncWebServer  server(80); // Server on port 80 (HTTP)
PrettyOTA       OTAUpdates;

// These are the actual physical dimensions
const uint16_t screenWidth = 294 * 2; // Two screens side by side
const uint16_t screenHeight = 126;
static const size_t lv_buffer_size = screenWidth * screenHeight * sizeof(lv_color_t);
static lv_disp_draw_buf_t draw_buf;
static lv_color_t *buf = NULL;
lv_obj_t *label_weight = NULL;
lv_obj_t *label_timer = NULL; // New label for timer
lv_obj_t *label_unit = NULL;  // Label for weight unit

// For inactivity and deep sleep management
static unsigned long last_activity_time = 0; // Last activity time
static float lastWeight = 0; // Last weight value

static EventGroupHandle_t touch_eg;
#define GET_TOUCH_INT _BV(1)

extern uint8_t espressiscale_left_map[];
extern uint8_t espressiscale_right_map[];

TouchLib touch(Wire, PIN_IIC_SDA, PIN_IIC_SCL, CTS820_SLAVE_ADDRESS);

bool batteryIndicatorInitialized = false; // Track if battery indicator is initialized

void my_print(const char *buf)
{
  Serial.printf(buf);
  Serial.flush();
}

inline void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
  uint32_t h = (area->y2 - area->y1 + 1);
  
  int _w1 = 294 - area->x1;
  int _w2 = area->x2 - 294 + 1;
  
  if (_w1 > 0)
  {
    TFT_CS_0_L;
    lcd_PushColors_SoftRotation(area->x1,
                area->y1,
                _w1,
                h,
                (uint16_t *)&color_p->full,
                  2); // Horizontal display
    TFT_CS_0_H;
  }
  if (_w2 > 0)
  {
    TFT_CS_1_L;
    lcd_PushColors_SoftRotation(0,
                area->y1,
                _w2,
                h,
                (uint16_t *)&color_p->full,
                  1); // Horizontal display
    TFT_CS_1_H;
  }
  
  lv_disp_flush_ready(disp);
}

static void lv_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data)
{
  if (touch.read())
  {
    TP_Point t = touch.getPoint(0);
    int16_t x = 126 - t.x;
    int16_t y = x;
    x = t.y;
    data->point.x = x;
    t.x = x;
    data->point.y = y;
    t.y = y;

    /* Adjust black shadow areas. */
    if (t.x > 326)
      data->point.x = t.x - 32;

    if (t.x > 294 && t.x < 326)
      data->state = LV_INDEV_STATE_REL;
    else
      data->state = LV_INDEV_STATE_PR;
    }
  else
  {
    data->state = LV_INDEV_STATE_REL;
  }
}

void startWifi(void * parameter){
  wifiManager.setConnectRetries(10);
  // Set timeout for the configuration portal
  wifiManager.setConfigPortalTimeout(300); // 5 minutes timeout
  
  // Use a default password for the AP for better security
  wifiManager.autoConnect("EspressiScale", "Espress1Scale");

  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  OTAUpdates.Begin(&server);
  server.begin();
  OTAUpdates.OverwriteAppVersion("1.0.0");
  
  vTaskDelete(NULL);
}

// Timer control functions for BLE
void startTimer();
void stopTimer();
void resetTimer();

// Deep sleep helper
void enterDeepSleep();

// Function to update BLE protocol when settings change
void updateBLEProtocolFromSettings() {
  // Use Settings::bleProtocol directly as it's now the same enum type as BLEProtocolMode
  setBLEProtocolMode(static_cast<BLEProtocolMode>(Settings::bleProtocol));
}

// Timer control function implementations
void startTimer() {
  // Use the AutoTimer class to start the timer
  AutoTimer::startTimer();
  last_activity_time = millis(); // Reset the activity timer
  Serial.println("Timer started via BLE");
}

void stopTimer() {
  // Use the AutoTimer class to stop the timer
  AutoTimer::stopTimer();
  last_activity_time = millis(); // Reset the activity timer
  Serial.println("Timer stopped via BLE");
}

void resetTimer() {
  // Use the AutoTimer class to reset the timer
  AutoTimer::resetTimer();
  last_activity_time = millis(); // Reset the activity timer
  Serial.println("Timer reset via BLE");
}

// Update UI based on current settings
void updateUI() {
  // Clear the screen
  lv_obj_clean(lv_scr_act());
  
  // Set the background color to black
  lv_obj_set_style_bg_color(lv_scr_act(), lv_color_black(), LV_PART_MAIN);
  
  // Create a label to display the weight
  label_weight = lv_label_create(lv_scr_act());
  lv_obj_set_style_text_font(label_weight, &lv_font_montserrat_48, LV_PART_MAIN);
  lv_obj_set_style_text_color(label_weight, lv_color_white(), LV_PART_MAIN);
  
  // Create label for weight unit
  label_unit = lv_label_create(lv_scr_act());
  lv_obj_set_style_text_font(label_unit, &lv_font_montserrat_28, LV_PART_MAIN);
  lv_obj_set_style_text_color(label_unit, lv_color_white(), LV_PART_MAIN);
  
  // Create a label to display the timer
  label_timer = lv_label_create(lv_scr_act());
  lv_obj_set_style_text_font(label_timer, &lv_font_montserrat_48, LV_PART_MAIN);
  lv_obj_set_style_text_color(label_timer, lv_color_white(), LV_PART_MAIN);
  
  // Position based on current units setting
  const char* unitStr = Settings::getUnitString();
  lv_label_set_text(label_unit, unitStr);
  
  // Add battery indicator to the top-right corner
  if (!batteryIndicatorInitialized) {
    drawBatteryIndicator(lv_scr_act(), lv_disp_get_physical_hor_res(NULL) - 80, 5, true);
    batteryIndicatorInitialized = true;
  } else {
    updateBatteryIndicator();
  }
  
  // Layout: weight on right screen, timer on left screen
  lv_obj_align(label_weight, LV_ALIGN_RIGHT_MID, -60, 0);
  lv_obj_align(label_unit, LV_ALIGN_RIGHT_MID, -10, 0);
  lv_obj_align(label_timer, LV_ALIGN_LEFT_MID, 10, 0);
}

void setup()
{
  touch_eg = xEventGroupCreate();

  esp_sleep_enable_ext0_wakeup(GPIO_NUM_12, 0); // Touch interrupt is connected to GPIO 12

  Serial.begin(921600);
  Serial.println("EspressiScale starting up");
  
  // Initialize scale hardware
  bool scaleInitSuccess = setupScale();
  if (!scaleInitSuccess) {
    Serial.println("WARNING: Scale initialization failed!");
    // Continue anyway, but display an error on screen later
  }
  
  // Initialize battery management
  setupBattery();
  
  // Initialize settings from EEPROM
  Settings::init();
  
  // Initialize the auto-timer
  AutoTimer::init();
  
  // Initialize display
  jd9613_init();
  
  // Display startup image
  TFT_CS_0_L;
  lcd_PushColors(0, 0, 294, 126, (uint16_t *)espressiscale_right_map, 1);
  TFT_CS_0_H;
  TFT_CS_1_L;
  lcd_PushColors(0, 0, 294, 126, (uint16_t *)espressiscale_left_map, 3);
  TFT_CS_1_H;
  
  // Show startup image for 2 seconds (reduced from 3)
  delay(2000);

  lv_init();

  buf = (lv_color_t *)ps_malloc(lv_buffer_size);

  assert(buf);

  lv_disp_draw_buf_init(&draw_buf, buf, NULL, screenWidth * screenHeight);

  /*Initialize the display*/
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);

  /*Set the resolution of the display*/
  disp_drv.hor_res = screenWidth;
  disp_drv.ver_res = screenHeight;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  disp_drv.full_refresh = 1;

  lv_disp_drv_register(&disp_drv);

  // Initialize touch
  Wire.begin(PIN_IIC_SDA, PIN_IIC_SCL);
  touch.init();
  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = lv_touchpad_read;
  lv_indev_drv_register(&indev_drv);

  // Setup BLE with the stored protocol setting
  setupBLE(static_cast<BLEProtocolMode>(Settings::bleProtocol));
  
  // Initialize menu system
  MenuSystem::init();
  
  // Create the initial UI
  updateUI();
  
  // Initialize the last activity time
  last_activity_time = millis();
  
  // Start WiFi in a separate task to avoid blocking the main loop
  xTaskCreatePinnedToCore(
    startWifi,     // Function to run
    "startWifi",   // Task name
    10000,         // Stack size
    NULL,          // Task parameter
    1,             // Task priority
    NULL,          // Task handle
    0              // Task core
  );
}

void loop()
{
  static unsigned long lastDisplayUpdateTime = 0;
  const unsigned long DISPLAY_UPDATE_INTERVAL = 100; // Update display every 100ms
  
  // Update the menu system (checks for gestures and handles menu if active)
  MenuSystem::update();
  
  // If menu is active, skip normal app logic
  if (MenuSystem::isActive()) {
    lv_task_handler();
    delay(10);
    return;
  }
  
  // Read filtered weight
  float currentWeight = medianFilter();
  
  // Update auto-timer with current weight if enabled
  if (Settings::isAutoTimerEnabled()) {
    AutoTimer::update(currentWeight);
  }
  
  // Get timer value from AutoTimer
  unsigned long timerVal = AutoTimer::getTimerValue();
  
  // Update BLE with current weight and timer (regardless of display refresh)
  float displayWeight = Settings::convertToSelectedUnit(currentWeight);
  updateBLEWeight(currentWeight); // Always send weight in grams over BLE
  updateBLETimer(timerVal / 1000.0f); // Convert ms to seconds for BLE
  
  // Only update display at the specified interval for efficiency
  unsigned long currentTime = millis();
  if (currentTime - lastDisplayUpdateTime >= DISPLAY_UPDATE_INTERVAL) {
    // Update the weight and timer display
    char weight_str[16];
    snprintf(weight_str, sizeof(weight_str), "%.1f", displayWeight);
    lv_label_set_text(label_weight, weight_str);
    
    // Display the timer in MM:SS format
    lv_label_set_text(label_timer, AutoTimer::getFormattedTime().c_str());
    
    // Update last display refresh time
    lastDisplayUpdateTime = currentTime;
  }
  
  // Handle touch interactions for the main screen
  if (touch.read())
  {
    // Any touch interaction should reset the activity timer
    last_activity_time = currentTime;
    
    TP_Point t = touch.getPoint(0);
    int16_t x = t.y; // Adjusted to match the screen orientation
    
    // Check which side of the screen was touched
    if (x > screenWidth / 2) {
      // Right screen touched - toggle timer
      if (AutoTimer::isRunning()) {
        AutoTimer::stopTimer();
        Serial.println("Timer stopped via touch");
      } else {
        AutoTimer::startTimer();
        Serial.println("Timer started via touch");
      }
      
      // Force display update immediately after touch
      lastDisplayUpdateTime = 0;
      
      // Debounce delay
      delay(200);
    } else {
      // Left screen touched - reset timer and tare scale
      AutoTimer::resetTimer();
      xTaskCreate( // Use a task to prevent blocking the loop
        [] (void * parameter) {
          tareScale(); // Tare the scale
          vTaskDelete(NULL); // Delete task when done
        },
        "TareTask", // Task name
        10000,      // Stack size
        NULL,       // Task parameter
        1,          // Task priority
        NULL        // Task handle
      );
      Serial.println("Tared and timer reset via touch");
      
      // Force display update immediately after touch
      lastDisplayUpdateTime = 0;
    }
  }
  
  // Check if the weight has changed significantly (indicating activity)
  if (abs(currentWeight - lastWeight) >= 0.5f) {
    last_activity_time = currentTime; // Reset the activity timer
  }
  
  // Update last weight value
  lastWeight = currentWeight;
  
  // Check battery status periodically
  static unsigned long lastBatteryUpdateTime = 0;
  if (currentTime - lastBatteryUpdateTime >= 60000) { // Update every minute (60000ms)
    lastBatteryUpdateTime = currentTime;
    updateBatteryIndicator();
    
    // Log battery info
    float batteryVoltage = getBatteryVoltage();
    uint8_t batteryPercentage = getBatteryPercentage();
    Serial.printf("Battery: %.2fV (%d%%)\n", batteryVoltage, batteryPercentage);
    
    // Check if battery is critically low by voltage or percentage
    if (batteryVoltage < 3.0f || batteryPercentage <= 5) // Critical level
    {
      Serial.printf("Battery voltage is low: %.2fV (%d%%). Entering deep sleep...\n", 
                   batteryVoltage, batteryPercentage);
      // Display low battery message before going to deep sleep
      lv_obj_clean(lv_scr_act());
      lv_obj_t* low_bat = lv_label_create(lv_scr_act());
      lv_obj_set_style_text_font(low_bat, &lv_font_montserrat_28, LV_PART_MAIN);
      lv_label_set_text(low_bat, "LOW BATTERY");
      lv_obj_center(low_bat);
      lv_refr_now(NULL); // Refresh the display immediately
      delay(2000); // Wait for 2 seconds to show the message
      
      enterDeepSleep();
    }
  }
  
  // Check for inactivity
  if (!AutoTimer::isRunning() && 
      currentTime - last_activity_time >= Settings::getSleepTimeoutMs()) {
    Serial.println("Entering deep sleep due to inactivity...");
    enterDeepSleep();
  }
  
  // Process BLE tasks
  processBLE();
  
  // Always process LVGL tasks
  lv_task_handler();
  
  // Yield for a small period to prevent CPU hogging
  delay(10);
}

// Helper function to properly enter deep sleep
void enterDeepSleep() {
  // Check if BLE is connected
  if (isBLEConnected()) {
    Serial.println("BLE client connected, notifying before disconnecting...");
    
    // Show a message on screen that we're disconnecting
    lv_obj_clean(lv_scr_act());
    lv_obj_t* disconnect_label = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(disconnect_label, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(disconnect_label, lv_color_white(), LV_PART_MAIN);
    lv_label_set_text(disconnect_label, "Disconnecting BLE...");
    lv_obj_align(disconnect_label, LV_ALIGN_CENTER, 0, 0);
    lv_refr_now(NULL);
    
    // Small delay to ensure client receives any final updates
    delay(500);
  }

  // 1. Finish any pending operations
  // 2. Close BLE connections
  NimBLEDevice::deinit(true);
  
  // 3. Power down peripherals
  digitalWrite(LOADCELL_POWER_PIN, LOW); // Power down load cell
  
  // 4. Flush the screen to black before going to deep sleep
  lv_obj_clean(lv_scr_act());
  lv_obj_set_style_bg_color(lv_scr_act(), lv_color_black(), LV_PART_MAIN);
  lv_refr_now(NULL); // Refresh the display immediately
  
  // 5. Display a "sleeping" message
  lv_obj_t* sleep_label = lv_label_create(lv_scr_act());
  lv_obj_set_style_text_font(sleep_label, &lv_font_montserrat_16, LV_PART_MAIN);
  lv_obj_set_style_text_color(sleep_label, lv_color_white(), LV_PART_MAIN);
  lv_label_set_text(sleep_label, "Sleeping. Touch to wake.");
  lv_obj_align(sleep_label, LV_ALIGN_CENTER, 0, 0);
  lv_refr_now(NULL);
  
  // 6. Brief delay to ensure message is displayed
  delay(500);
  
  // 7. Finally enter deep sleep
  esp_deep_sleep_start();
}