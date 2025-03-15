#include "arduino.h"
#include <HX711.h>
#include <battery.h>
#include <scale.h>
#include <filter.h>
#include <Wifi.h>
#include <config.h>
#include <ota.h>
#include "jd9613.h"
#include "lvgl.h"
#include "pin_config.h"
#include "SPI.h"
#include "time.h"
#include "sntp.h"
#define TOUCH_MODULES_CST_SELF
#include "TouchLib.h"
#include "Wire.h"

#ifndef BOARD_HAS_PSRAM
#error "Please turn on PSRAM option to OPI PSRAM"
#endif

WiFiServer server(80);
String header;

static const uint16_t screenWidth = 294 * 2; // screenWidth = 294 * 2;
static const uint16_t screenHeight = 126;
static const size_t lv_buffer_size = screenWidth * screenHeight * sizeof(lv_color_t);
static lv_disp_draw_buf_t draw_buf;
static lv_color_t *buf = NULL;
lv_obj_t *label_weight = NULL;
lv_obj_t *label_timer = NULL; // New label for timer


static EventGroupHandle_t touch_eg;
#define GET_TOUCH_INT _BV(1)

extern uint8_t espressiscale_left_map[];
extern uint8_t espressiscale_right_map[];

TouchLib touch(Wire, PIN_IIC_SDA, PIN_IIC_SCL, CTS820_SLAVE_ADDRESS);

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

  /*Change the following line to your display resolution*/
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

  // Connect to Wi-Fi network with SSID and password
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  unsigned long startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 30000) // 30 seconds timeout
  {
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("Failed to connect to WiFi. Restarting...");
    ESP.restart();
  }
  // Print local IP address and start web server
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  server.begin();

  setupOTA();

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
  tareScale(); // Tare the scale before starting the loop (Had some issues with scale taring the wrong value)
}

void loop()
{
  // Read filtered weight
  float currentWeight = medianFilter();

  // Update the label with the current weight
  char weight_str[16];
  snprintf(weight_str, sizeof(weight_str), "%.1f g", currentWeight);
  lv_label_set_text(label_weight, weight_str);

  // Placeholder timer value
  static int timer = 0; // Initialize timer to 0
  static bool timer_running = false; // Timer running state
  static unsigned long last_update = 0; // Last update time

  if (touch.read())
  {
    TP_Point t = touch.getPoint(0);
    int16_t x = t.y; // Adjusted to match the screen orientation

    if (x > screenWidth / 2)
    {
      timer_running = !timer_running; // Toggle timer state
      delay(300); // Debounce delay
    }
    else
    {
      timer = 0; // Reset timer
      tareScale(); // Tare the scale
      Serial.println("Tared and timer reset via touch");
    }
  }

  if (timer_running)
  {
    unsigned long current_time = millis();
    if (current_time - last_update >= 1000) // Update every second
    {
      timer++;
      last_update = current_time;
    }
  }

  // Update the label with the timer value
  char timer_str[16];
  snprintf(timer_str, sizeof(timer_str), "%d s", timer);
  lv_label_set_text(label_timer, timer_str);
  
  handleOTA();

  // Handle LittlevGL tasks
  lv_task_handler();
  delay(5);
  static unsigned long last_activity_time = 0; // Last activity time
  static float lastWeight = currentWeight; // Last weight value

  // Check if the timer is not running and weight hasn't changed significantly
  if (!timer_running && abs(currentWeight - lastWeight) < 1.0)
  {
    unsigned long current_time = millis();
    if (current_time - last_activity_time >= 300000) // 5 minutes
    {
      Serial.println("Entering deep sleep due to inactivity...");
      // Flush the screen to black before going to deep sleep
      lv_obj_clean(lv_scr_act());
      lv_obj_set_style_bg_color(lv_scr_act(), lv_color_black(), LV_PART_MAIN);
      lv_refr_now(NULL); // Refresh the display immediately
      esp_deep_sleep_start();
    }
  }
  else
  {
    last_activity_time = millis(); // Reset the activity timer
  }

  lastWeight = currentWeight; // Update the last weight value

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
}