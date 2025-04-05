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

#ifndef BOARD_HAS_PSRAM
#error "Please turn on PSRAM option to OPI PSRAM"
#endif

WiFiManager wifiManager;
String header;

AsyncWebServer  server(80); // Server on port 80 (HTTP)
PrettyOTA       OTAUpdates;

static const uint16_t screenWidth = 294 * 2; // screenWidth = 294 * 2;
static const uint16_t screenHeight = 126;
static const size_t lv_buffer_size = screenWidth * screenHeight * sizeof(lv_color_t);
static lv_disp_draw_buf_t draw_buf;
static lv_color_t *buf = NULL;
lv_obj_t *label_weight = NULL;
lv_obj_t *label_timer = NULL; // New label for timer

// Timer variables
static int timer = 0; // Initialize timer to 0
static bool timer_running = false; // Timer running state
static unsigned long last_update = 0; // Last update time

// For inactivity and deep sleep management
static unsigned long last_activity_time = 0; // Last activity time
static float lastWeight = 0; // Last weight value

static EventGroupHandle_t touch_eg;
#define GET_TOUCH_INT _BV(1)

extern uint8_t espressiscale_left_map[];
extern uint8_t espressiscale_right_map[];

TouchLib touch(Wire, PIN_IIC_SDA, PIN_IIC_SCL, CTS820_SLAVE_ADDRESS);

// Add these variables at the top of the file, with other variables
static unsigned long touch_start_time = 0;
static bool long_press_detected = false;
static const unsigned long LONG_PRESS_DURATION = 3000; // 3 seconds for long press

void my_print(const char *buf)
{
  Serial.printf(buf);
  Serial.flush();
}

inline void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
  // uint32_t w = (area->x2 - area->x1 + 1);
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
void startTimer() {
  timer_running = true;
  last_activity_time = millis(); // Reset the activity timer
  Serial.println("Timer started via BLE");
}

void stopTimer() {
  timer_running = false;
  last_activity_time = millis(); // Reset the activity timer
  Serial.println("Timer stopped via BLE");
}

void resetTimer() {
  timer = 0;
  timer_running = false;
  updateBLETimer(timer);
  last_activity_time = millis(); // Reset the activity timer
  Serial.println("Timer reset via BLE");
}

void setup()
{
  touch_eg = xEventGroupCreate();

  esp_sleep_enable_ext0_wakeup(GPIO_NUM_12, 0); // Touch interrupt is connected to GPIO 12

  Serial.begin(921600);
  Serial.println("HX711 with median filter and exponential smoothing");
  jd9613_init();
  TFT_CS_0_L;
  lcd_PushColors(0, 0, 294, 126, (uint16_t *)espressiscale_right_map, 1);
  TFT_CS_0_H;
  TFT_CS_1_L;
  lcd_PushColors(0, 0, 294, 126, (uint16_t *)espressiscale_left_map, 3);
  TFT_CS_1_H;
  delay(3000);

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

  Wire.begin(PIN_IIC_SDA, PIN_IIC_SCL);
  touch.init();
  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = lv_touchpad_read;
  lv_indev_drv_register(&indev_drv);

  setupScale();
  setupBattery();
  setupBLE(); // Initialize BLE service with default mode (EspressiScale)

  // Clear the display after showing the logo
  lv_obj_clean(lv_scr_act());

  // Set the background color to black
  lv_obj_set_style_bg_color(lv_scr_act(), lv_color_black(), LV_PART_MAIN);

  // Create a label to display the weight
  label_weight = lv_label_create(lv_scr_act());
  lv_obj_set_style_text_font(label_weight, &lv_font_montserrat_48, LV_PART_MAIN);
  lv_obj_align(label_weight, LV_ALIGN_RIGHT_MID, -10, 0);

  // Create a label to display the timer
  label_timer = lv_label_create(lv_scr_act());
  lv_obj_set_style_text_font(label_timer, &lv_font_montserrat_48, LV_PART_MAIN);
  lv_obj_align(label_timer, LV_ALIGN_LEFT_MID, 10, 0); // Align to the left
  
  // Initialize the last activity time
  last_activity_time = millis();
  
  xTaskCreatePinnedToCore(
    startWifi, // Function to run on this task
    "startWifi", // Task name
    10000, // Stack size
    NULL, // Task parameter
    1, // Task priority
    NULL, // Task handle
    0 // Task core
  );
}

void loop()
{
  // Read filtered weight
  float currentWeight = medianFilter();
  
  // Update BLE with current weight
  updateBLEWeight(currentWeight);

  // Update the label with the current weight
  char weight_str[16];
  snprintf(weight_str, sizeof(weight_str), "%.1f g", currentWeight);
  lv_label_set_text(label_weight, weight_str);

  if (touch.read())
  {
    // Any touch interaction should reset the activity timer
    last_activity_time = millis();
    
    TP_Point t = touch.getPoint(0);
    int16_t x = t.y; // Adjusted to match the screen orientation

    // Start timing for long press detection
    if (touch_start_time == 0) {
      touch_start_time = millis();
      long_press_detected = false;
    }
    
    // Check for long press (3+ seconds)
    if (!long_press_detected && (millis() - touch_start_time > LONG_PRESS_DURATION)) {
      long_press_detected = true;
      
      // Toggle BLE protocol mode
      BLEProtocolMode currentMode = getBLEProtocolMode();
      BLEProtocolMode newMode = (currentMode == BLEProtocolMode::ESPRESSISCALE) ? 
                                BLEProtocolMode::ACAIA : 
                                BLEProtocolMode::ESPRESSISCALE;
      
      // Clear both displays
      lv_obj_clean(lv_scr_act());
      
      // Create label for left display (BLE Mode:)
      lv_obj_t* label_mode_left = lv_label_create(lv_scr_act());
      lv_obj_set_style_text_font(label_mode_left, &lv_font_montserrat_28, LV_PART_MAIN);
      lv_obj_set_width(label_mode_left, 294);  // Width of one display
      lv_obj_align(label_mode_left, LV_ALIGN_LEFT_MID, 10, 0);
      lv_label_set_text(label_mode_left, "BLE Mode:");
      
      // Create label for right display (actual mode)
      lv_obj_t* label_mode_right = lv_label_create(lv_scr_act());
      lv_obj_set_style_text_font(label_mode_right, &lv_font_montserrat_28, LV_PART_MAIN);
      lv_obj_set_width(label_mode_right, 294);  // Width of one display
      lv_obj_align(label_mode_right, LV_ALIGN_RIGHT_MID, -10, 0);
      
      // Set the mode text
      if (newMode == BLEProtocolMode::ESPRESSISCALE) {
        lv_label_set_text(label_mode_right, "EspressiScale");
      } else {
        lv_label_set_text(label_mode_right, "Acaia");
      }
      
      // Apply the change
      setBLEProtocolMode(newMode);
      
      // Refresh the display
      lv_refr_now(NULL);
      delay(2000);
      
      // Recreate the normal UI
      lv_obj_clean(lv_scr_act());
      
      // Set the background color to black
      lv_obj_set_style_bg_color(lv_scr_act(), lv_color_black(), LV_PART_MAIN);
      
      // Create a label to display the weight
      label_weight = lv_label_create(lv_scr_act());
      lv_obj_set_style_text_font(label_weight, &lv_font_montserrat_48, LV_PART_MAIN);
      lv_obj_align(label_weight, LV_ALIGN_RIGHT_MID, -10, 0);
      
      // Create a label to display the timer
      label_timer = lv_label_create(lv_scr_act());
      lv_obj_set_style_text_font(label_timer, &lv_font_montserrat_48, LV_PART_MAIN);
      lv_obj_align(label_timer, LV_ALIGN_LEFT_MID, 10, 0); // Align to the left
    }
    else if (x > screenWidth / 2)
    {
      // Only process normal touch if not a long press
      if (!long_press_detected) {
        timer_running = !timer_running; // Toggle timer state
        if (timer_running) {
          Serial.println("Timer started via touch");
        } else {
          Serial.println("Timer stopped via touch");
        }
        delay(1000); // Debounce delay
      }
    }
    else
    {
      // Only process normal touch if not a long press
      if (!long_press_detected) {
        timer_running = false; // Stop the timer
        timer = 0; // Reset timer
        updateBLETimer(timer); // Update BLE with reset timer
        xTaskCreate( // To prevent halting the loop
          [] (void * parameter) {
            tareScale(); // Tare the scale
            vTaskDelete(NULL); // Delete the task once done
          },
          "TareTask", // Task name
          10000, // Stack size
          NULL, // Task parameter
          1, // Task priority
          NULL // Task handle
        );
        Serial.println("Tared and timer reset via touch");
      }
    }
  }
  else {
    // Reset touch timing when no touch detected
    touch_start_time = 0;
    long_press_detected = false;
  }

  if (timer_running)
  {
    unsigned long current_time = millis();
    if (current_time - last_update >= 1000) // Update every second
    {
      timer++;
      last_update = current_time;
      
      // Update BLE with current timer
      updateBLETimer(timer);
    }
  }

  // Display the timer
  char timer_str[16];
  snprintf(timer_str, sizeof(timer_str), "%d s", timer);
  lv_label_set_text(label_timer, timer_str);
  
  // Check if the weight has changed significantly (indicating activity)
  if (abs(currentWeight - lastWeight) >= 1.0) {
    last_activity_time = millis(); // Reset the activity timer
  }
  
  // Check for inactivity
  if (!timer_running && millis() - last_activity_time >= 300000) // 5 minutes
  {
    Serial.println("Entering deep sleep due to inactivity...");
    // Flush the screen to black before going to deep sleep
    lv_obj_clean(lv_scr_act());
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_black(), LV_PART_MAIN);
    lv_refr_now(NULL); // Refresh the display immediately
    esp_deep_sleep_start();
  }
  
  // Update last weight value
  lastWeight = currentWeight;
  
  // Check battery status
  float batteryStatus = getBatteryVoltage(); // Update the battery status
  
  if (batteryStatus < 3) // Check if battery voltage is below 3V
  {
    Serial.println("Battery voltage is low. Entering deep sleep...");
    // Display low battery message before going to deep sleep
    lv_label_set_text(label_weight, "Low battery");
    lv_refr_now(NULL); // Refresh the display immediately
    delay(2000); // Wait for 2 seconds to show the message
    // Flush the screen to black before going to deep sleep
    lv_obj_clean(lv_scr_act());
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_black(), LV_PART_MAIN);
    lv_refr_now(NULL); // Refresh the display immediately
    esp_deep_sleep_start();
  }
  
  // Process BLE tasks
  processBLE();

  // LVGL task handler
  lv_task_handler();
  delay(10);
}