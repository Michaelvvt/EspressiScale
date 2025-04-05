#pragma once

#include "Arduino.h"
#include "lvgl.h"
#include "lvgl_fonts.h"
#include "TouchLib.h"
#include "settings.h"

// Forward declarations of TouchLib instance and display width/height
extern TouchLib touch;
extern const uint16_t screenWidth;
extern const uint16_t screenHeight;

// Forward declare necessary functions from main.cpp
void updateBLEProtocolFromSettings();
extern void updateUI();

class MenuSystem {
public:
  // Initialize the menu system
  static void init() {
    // Create initial UI components - will be inactive until menu is shown
    createComponents();
    
    // Start in normal (non-menu) mode
    inMenuMode = false;
    currentPage = PAGE_AUTO_TIMER;
    lastMenuActivity = 0;
    
    // Initialize touch tracking
    touchStartX = 0;
    touchStartY = 0;
    touchStartTime = 0;
    bothTouchStartTime = 0;
    optionScrollOffset = 0;
    currentOptionScreen = 0;
  }
  
  // Call in main loop to check for menu gesture
  static void update() {
    // Only process when menu is active
    if (!inMenuMode) return;
    
    // Check for timeout (auto-exit after 30 seconds of inactivity)
    if (millis() - lastMenuActivity > 30000) {
      exitMenu();
      return;
    }
    
    // Handle menu touch interactions
    handleMenuTouch();
  }
  
  // Returns true if menu is currently active
  static bool isActive() {
    return inMenuMode;
  }
  
private:
  // Menu state
  static bool inMenuMode;
  static SettingsPage currentPage;
  static int optionScrollOffset;
  static int currentOptionScreen;
  
  // Touch tracking
  static unsigned long bothTouchStartTime;
  static int16_t touchStartX;
  static int16_t touchStartY;
  static unsigned long touchStartTime;
  static bool isSwipeGesture;
  
  // Menu activity tracking
  static unsigned long lastMenuActivity;
  static const unsigned long MENU_TIMEOUT = 30000; // 30 seconds
  
  // Menu animation tracking
  static bool animationActive;
  
  // Create necessary LVGL objects
  static void createComponents() {
    // This will be populated when menu is shown
  }
  
  // Check for long press on both displays
  static void checkMenuGesture() {
    // Get latest touch position
    int x = 0;
    
    if (!touch.read()) {
      bothTouchStartTime = 0;
      return;
    }
    
    // Get touch position
    if (touch.getPointNum() > 0) {
      TP_Point p = touch.getPoint(0);
      x = p.x;
      lastMenuActivity = millis(); // Update activity timestamp
    } else {
      bothTouchStartTime = 0;
      return;
    }
    
    // Check if there are two touch points (one on each screen)
    bool leftTouched = false;
    bool rightTouched = false;
    
    if (touch.getPointNum() >= 2) {
      // Multiple touches detected, check positions
      for (uint8_t i = 0; i < touch.getPointNum(); i++) {
        TP_Point p = touch.getPoint(i);
        if (p.y < screenWidth / 2) {
          leftTouched = true;
        } else {
          rightTouched = true;
        }
      }
    } else if (millis() % 100 < 50) {
      // For single touch, alternate checking left/right halves
      // This is a workaround as some touch controllers may not report multiple points
      if (x < screenWidth / 2) {
        leftTouched = true;
        rightTouched = touchStartX > screenWidth / 2;
      } else {
        rightTouched = true;
        leftTouched = touchStartX > 0 && touchStartX < screenWidth / 2;
      }
    }
    
    if (leftTouched && rightTouched) {
      if (bothTouchStartTime == 0) {
        bothTouchStartTime = millis();
        touchStartX = x; // For single touch fallback
      } else if (millis() - bothTouchStartTime > 1500) { // 1.5 seconds threshold
        // Toggle menu state
        bothTouchStartTime = 0;
        if (inMenuMode) {
          exitMenu();
        } else {
          enterMenu();
        }
      }
    } else {
      // Reset for new gesture attempt
      bothTouchStartTime = 0;
    }
  }
  
  // Show the menu
  static void enterMenu() {
    inMenuMode = true;
    currentPage = PAGE_AUTO_TIMER;
    optionScrollOffset = 0;
    currentOptionScreen = 0;
    lastMenuActivity = millis();
    
    // Perform transition animation
    fadeTransition(true);
    
    // Draw initial menu
    drawSettingsMenu();
    
    // Show brief help text
    showHelpText();
  }
  
  // Hide the menu
  static void exitMenu() {
    // Save all settings
    Settings::saveAll();
    
    inMenuMode = false;
    
    // Perform exit animation
    fadeTransition(false);
  }
  
  // Fading transition effect
  static void fadeTransition(bool entering) {
    // Clear screen
    lv_obj_clean(lv_scr_act());
    
    // Set black background
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_black(), LV_PART_MAIN);
    
    // Add fade overlay if needed
    if (entering) {
      // Could add fade-in effect
      drawSettingsMenu();
    } else {
      // Return to main screen
      // Call updateUI() in main.cpp
      updateUI();
    }
  }
  
  // Show temporary help text
  static void showHelpText() {
    lv_obj_t* help = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(help, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_set_style_text_align(help, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_width(help, screenWidth);
    lv_obj_align(help, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_label_set_text(help, "Swipe to navigate • Touch to select • Long-press both sides to exit");
    
    // Auto-remove after 3 seconds
    lv_timer_t* timer = lv_timer_create([](lv_timer_t* t) {
      lv_obj_del((lv_obj_t*)t->user_data);
      lv_timer_del(t);
    }, 3000, help);
  }
  
  // Draw the settings menu
  static void drawSettingsMenu() {
    lv_obj_clean(lv_scr_act());
    
    // Draw navigation dots on left display
    drawNavigationDots();
    
    // Draw setting title with adjacent settings
    drawSettingWithAdjacent();
    
    // Draw options for current setting on right display
    drawOptions();
    
    // Draw menu indicator (small gear icon)
    drawMenuIndicator();
  }
  
  // Draw menu indicator icon
  static void drawMenuIndicator() {
    lv_obj_t* indicator = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(indicator, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_label_set_text(indicator, LV_SYMBOL_SETTINGS);
    lv_obj_align(indicator, LV_ALIGN_TOP_LEFT, 10, 10);
  }
  
  // Draw navigation dots on left side
  static void drawNavigationDots() {
    for (int i = 0; i < PAGE_COUNT; i++) {
      lv_obj_t* dot = lv_obj_create(lv_scr_act());
      lv_obj_set_size(dot, 10, 10);
      lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, LV_PART_MAIN);
      
      // Position dots vertically along left edge
      int y_pos = 20 + (screenHeight - 40) * i / (PAGE_COUNT - 1);
      lv_obj_align(dot, LV_ALIGN_LEFT_MID, 15, y_pos - screenHeight/2);
      
      // Current page has filled dot, others are outlines
      if (i == currentPage) {
        lv_obj_set_style_bg_color(dot, lv_color_white(), LV_PART_MAIN);
      } else {
        lv_obj_set_style_bg_color(dot, lv_color_black(), LV_PART_MAIN);
        lv_obj_set_style_border_color(dot, lv_color_white(), LV_PART_MAIN);
        lv_obj_set_style_border_width(dot, 2, LV_PART_MAIN);
      }
    }
  }
  
  // Draw current setting with adjacent settings
  static void drawSettingWithAdjacent() {
    // Background highlight for current setting
    lv_obj_t* highlight = lv_obj_create(lv_scr_act());
    lv_obj_set_size(highlight, 220, 50);
    lv_obj_align(highlight, LV_ALIGN_LEFT_MID, 40, 0);
    lv_obj_set_style_bg_color(highlight, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(highlight, LV_OPA_30, LV_PART_MAIN);
    lv_obj_set_style_radius(highlight, 5, LV_PART_MAIN);
    lv_obj_set_style_border_width(highlight, 0, LV_PART_MAIN);
    
    // Current setting (center, bright)
    lv_obj_t* label_current = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(label_current, &lv_font_montserrat_28, LV_PART_MAIN);
    lv_obj_set_style_text_color(label_current, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_width(label_current, 200);
    lv_obj_align(label_current, LV_ALIGN_LEFT_MID, 50, 0);
    lv_label_set_text(label_current, getSettingName(currentPage));
    
    // Previous setting (above, dimmed)
    SettingsPage prevPage = (currentPage == 0) ? 
                          (SettingsPage)(PAGE_COUNT - 1) : 
                          (SettingsPage)(currentPage - 1);
    
    lv_obj_t* label_prev = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(label_prev, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(label_prev, lv_color_hex(0x888888), LV_PART_MAIN); // Dimmed
    lv_obj_set_width(label_prev, 200);
    lv_obj_align(label_prev, LV_ALIGN_LEFT_MID, 50, -50);
    lv_label_set_text(label_prev, getSettingName(prevPage));
    
    // Next setting (below, dimmed)
    SettingsPage nextPage = (SettingsPage)((currentPage + 1) % PAGE_COUNT);
    
    lv_obj_t* label_next = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(label_next, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(label_next, lv_color_hex(0x888888), LV_PART_MAIN); // Dimmed
    lv_obj_set_width(label_next, 200);
    lv_obj_align(label_next, LV_ALIGN_LEFT_MID, 50, 50);
    lv_label_set_text(label_next, getSettingName(nextPage));
  }
  
  // Draw options for current setting
  static void drawOptions() {
    // Get all options for current setting
    const char** options = getSettingOptions(currentPage);
    int optionCount = getOptionCount(currentPage);
    int selectedOption = getSelectedOption(currentPage);
    
    // Calculate optimal layout
    float optionWidth = min(80.0f, 250.0f / optionCount);
    float spacing = 10;
    float totalWidth = (optionWidth + spacing) * optionCount - spacing;
    
    // Start position (center options)
    float startX = screenWidth / 2 + (294 - totalWidth) / 2;
    
    // Draw all options
    for (int i = 0; i < optionCount; i++) {
      float x = startX + i * (optionWidth + spacing);
      
      // Container for option
      lv_obj_t* cont = lv_obj_create(lv_scr_act());
      lv_obj_set_size(cont, optionWidth, 40);
      lv_obj_set_pos(cont, x, screenHeight/2 - 20);
      
      // Style based on selection state
      if (i == selectedOption) {
        // Selected option - highlighted
        lv_obj_set_style_bg_color(cont, lv_color_hex(0x0066CC), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(cont, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_text_color(cont, lv_color_white(), LV_PART_MAIN);
        lv_obj_set_style_border_width(cont, 0, LV_PART_MAIN);
      } else {
        // Unselected option - outlined
        lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_color(cont, lv_color_white(), LV_PART_MAIN);
        lv_obj_set_style_border_width(cont, 1, LV_PART_MAIN);
        lv_obj_set_style_text_color(cont, lv_color_white(), LV_PART_MAIN);
      }
      
      // Option text
      lv_obj_t* label = lv_label_create(cont);
      lv_obj_set_style_text_font(label, &lv_font_montserrat_16, LV_PART_MAIN);
      lv_label_set_text(label, options[i]);
      lv_obj_center(label);
      
      // Store button data for touch handling
      lv_obj_set_user_data(cont, (void*)(intptr_t)i);
    }
  }
  
  // Handle touch input for menu navigation
  static void handleMenuTouch() {
    if (!touch.read()) {
      touchStartX = 0;
      touchStartY = 0;
      isSwipeGesture = false;
      return;
    }
    
    lastMenuActivity = millis(); // Reset timeout
    
    TP_Point t = touch.getPoint(0);
    int16_t x = t.y; // Adjusted for screen orientation
    int16_t y = t.x; // Vertical position
    
    // Record touch start for swipe detection
    if (touchStartX == 0) {
      touchStartX = x;
      touchStartY = y;
      return;
    }
    
    // Check for swipe gesture
    int16_t deltaX = x - touchStartX;
    int16_t deltaY = y - touchStartY;
    
    // If significant movement detected
    if (!isSwipeGesture && (abs(deltaX) > 30 || abs(deltaY) > 30)) {
      isSwipeGesture = true;
      
      // Determine swipe direction
      if (abs(deltaX) > abs(deltaY)) {
        // Horizontal swipe (for option pages if needed)
        // Not implemented here as we're fitting all options on one screen
      } else {
        // Vertical swipe (for setting navigation)
        // Only process vertical swipes on left screen
        if (touchStartX < screenWidth / 2) {
          if (deltaY < -30) {
            // Swipe up - previous setting
            SettingsPage prevPage = (currentPage == 0) ? 
                                  (SettingsPage)(PAGE_COUNT - 1) : 
                                  (SettingsPage)(currentPage - 1);
            currentPage = prevPage;
            drawSettingsMenu();
          } else if (deltaY > 30) {
            // Swipe down - next setting
            SettingsPage nextPage = (SettingsPage)((currentPage + 1) % PAGE_COUNT);
            currentPage = nextPage;
            drawSettingsMenu();
          }
        }
      }
    }
    // If touch release without swipe, check for option selection
    else if (!isSwipeGesture && touch.getPointNum() == 0) {
      // Only process direct touch on right display
      if (x > screenWidth / 2) {
        lv_obj_t* clicked_obj = lv_indev_search_obj(lv_scr_act(), x, y);
        if (clicked_obj != NULL) {
          // Check if it's one of our option buttons
          int option_idx = (int)(intptr_t)lv_obj_get_user_data(clicked_obj);
          if (option_idx >= 0) {
            setSelectedOption(currentPage, option_idx);
            drawSettingsMenu(); // Redraw to reflect changes
          }
        }
      }
    }
  }
  
  // Recursively search for the object at position
  static lv_obj_t* lv_indev_search_obj(lv_obj_t* obj, lv_coord_t x, lv_coord_t y) {
    // Check if point is within this object
    lv_area_t coords;
    lv_obj_get_coords(obj, &coords);
    
    // Check if point is within this object's area
    if ((coords.x1 <= x && x <= coords.x2) && 
        (coords.y1 <= y && y <= coords.y2)) {
      // Check children in reverse order (to match draw order)
      uint32_t child_cnt = lv_obj_get_child_cnt(obj);
      
      for (int i = child_cnt - 1; i >= 0; i--) {
        lv_obj_t* child = lv_indev_search_obj(lv_obj_get_child(obj, i), x, y);
        if (child) return child;
      }
      
      return obj;
    }
    return NULL;
  }
  
  // Helper function to get setting name
  static const char* getSettingName(SettingsPage page) {
    switch (page) {
      case PAGE_AUTO_TIMER: return "AUTO-TIMER";
      case PAGE_SENSITIVITY: return "SENSITIVITY";
      case PAGE_BLE_PROTOCOL: return "BLE PROTOCOL";
      case PAGE_BRIGHTNESS: return "BRIGHTNESS";
      case PAGE_SLEEP_TIMEOUT: return "SLEEP TIMEOUT";
      case PAGE_UNITS: return "UNITS";
      default: return "UNKNOWN";
    }
  }
  
  // Helper function to get options for a setting
  static const char** getSettingOptions(SettingsPage page) {
    static const char* autoTimerOptions[] = {"OFF", "LOCAL", "ALWAYS"};
    static const char* sensitivityOptions[] = {"LOW", "MEDIUM", "HIGH"};
    static const char* bleProtocolOptions[] = {"DEFAULT", "ACAIA"};
    static const char* brightnessOptions[] = {"LOW", "MEDIUM", "HIGH", "AUTO"};
    static const char* sleepTimeoutOptions[] = {"2 MIN", "5 MIN", "10 MIN"};
    static const char* unitOptions[] = {"GRAMS", "OUNCES"};
    
    switch (page) {
      case PAGE_AUTO_TIMER: return autoTimerOptions;
      case PAGE_SENSITIVITY: return sensitivityOptions;
      case PAGE_BLE_PROTOCOL: return bleProtocolOptions;
      case PAGE_BRIGHTNESS: return brightnessOptions;
      case PAGE_SLEEP_TIMEOUT: return sleepTimeoutOptions;
      case PAGE_UNITS: return unitOptions;
      default: return autoTimerOptions;
    }
  }
  
  // Helper function to get number of options for a setting
  static int getOptionCount(SettingsPage page) {
    switch (page) {
      case PAGE_AUTO_TIMER: return 3;
      case PAGE_SENSITIVITY: return 3;
      case PAGE_BLE_PROTOCOL: return 2;
      case PAGE_BRIGHTNESS: return 4;
      case PAGE_SLEEP_TIMEOUT: return 3;
      case PAGE_UNITS: return 2;
      default: return 3;
    }
  }
  
  // Helper function to get currently selected option
  static int getSelectedOption(SettingsPage page) {
    switch (page) {
      case PAGE_AUTO_TIMER: return static_cast<int>(Settings::autoTimerMode);
      case PAGE_SENSITIVITY: return static_cast<int>(Settings::sensitivity);
      case PAGE_BLE_PROTOCOL: return static_cast<int>(Settings::bleProtocol);
      case PAGE_BRIGHTNESS: return static_cast<int>(Settings::brightness);
      case PAGE_SLEEP_TIMEOUT: return static_cast<int>(Settings::sleepTimeout);
      case PAGE_UNITS: return static_cast<int>(Settings::weightUnit);
      default: return 0;
    }
  }
  
  // Helper function to set selected option
  static void setSelectedOption(SettingsPage page, int option) {
    switch (page) {
      case PAGE_AUTO_TIMER:
        Settings::autoTimerMode = static_cast<AutoTimerMode>(option);
        break;
      case PAGE_SENSITIVITY:
        Settings::sensitivity = static_cast<SensitivityLevel>(option);
        break;
      case PAGE_BLE_PROTOCOL:
        Settings::bleProtocol = static_cast<BLEProtocolMode>(option);
        // Update BLE protocol if needed
        updateBLEProtocolFromSettings();
        break;
      case PAGE_BRIGHTNESS:
        Settings::brightness = static_cast<BrightnessLevel>(option);
        // Update screen brightness if needed
        updateBrightness();
        break;
      case PAGE_SLEEP_TIMEOUT:
        Settings::sleepTimeout = static_cast<SleepTimeout>(option);
        break;
      case PAGE_UNITS:
        Settings::weightUnit = static_cast<WeightUnit>(option);
        break;
    }
    
    // Save settings immediately
    Settings::saveAll();
  }
  
  // Update BLE protocol after settings change
  static void updateBLEProtocol() {
    updateBLEProtocolFromSettings();
  }
  
  // Update screen brightness
  static void updateBrightness() {
    // Would need to be implemented if hardware supports it
  }
};

// Static member initialization
bool MenuSystem::inMenuMode = false;
SettingsPage MenuSystem::currentPage = PAGE_AUTO_TIMER;
int MenuSystem::optionScrollOffset = 0;
int MenuSystem::currentOptionScreen = 0;
unsigned long MenuSystem::bothTouchStartTime = 0;
int16_t MenuSystem::touchStartX = 0;
int16_t MenuSystem::touchStartY = 0;
bool MenuSystem::isSwipeGesture = false;
unsigned long MenuSystem::lastMenuActivity = 0;
bool MenuSystem::animationActive = false;
unsigned long MenuSystem::touchStartTime = 0; 