#pragma once

/*
#define WIFI_SSID             "SSID"
#define WIFI_PASSWORD         "Password"

#define WIFI_CONNECT_WAIT_MAX (30 * 1000)

#define NTP_SERVER1           "pool.ntp.org"
#define NTP_SERVER2           "time.nist.gov"
#define GMT_OFFSET_SEC        0
#define DAY_LIGHT_OFFSET_SEC  0
#define GET_TIMEZONE_API      "https://ipapi.co/timezone/"
*/

#define LV_DELAY(x)                                                                                                                                  \
  do {                                                                                                                                               \
    uint32_t t = x;                                                                                                                                  \
    while (t--) {                                                                                                                                    \
      lv_timer_handler();                                                                                                                            \
      delay(1);                                                                                                                                      \
    }                                                                                                                                                \
  } while (0);

// Only define these if not already defined in the board's pins_arduino.h
#ifndef TFT_DC
#define TFT_DC        16
#endif

#ifndef TFT_RES
#define TFT_RES       15
#endif

#ifndef TFT_CS_0
#define TFT_CS_0      13
#endif

#ifndef TFT_CS_1
#define TFT_CS_1      14
#endif

#ifndef TFT_MOSI
#define TFT_MOSI      18
#endif

#ifndef TFT_SCK
#define TFT_SCK       17
#endif

#define PIN_IIC_SCL   11
#define PIN_IIC_SDA   10
#define PIN_TOUCH_INT 12
#define PIN_TOUCH_RES 9

// Battery and power management pins
#define PIN_BAT_ADC   2      // Battery voltage measurement ADC pin
#define PIN_BAT_EN    15     // Battery enable pin
#define PIN_CHARGING  -1     // Charging status pin (if available, set to actual pin or leave as -1)

// Load cell pins for HX711
#define LOADCELL_DOUT_PIN  4     // HX711 data pin 
#define LOADCELL_SCK_PIN   3     // HX711 clock pin
#define LOADCELL_POWER_PIN 6     // Power control pin for load cell

// ADS1256 pins - using existing SPI pins where possible
#define ADS1256_CS_PIN   5       // ADS1256 chip select pin
#define ADS1256_DRDY_PIN 7       // ADS1256 data ready pin
#define ADS1256_RESET_PIN 8      // ADS1256 reset pin
// We can reuse SPI pins for MOSI/MISO/SCK shared with the display