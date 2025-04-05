#pragma once

#include <NimBLEDevice.h>
#include <NimBLEServer.h>
#include <NimBLEUtils.h>

/**
 * BLE Service Implementation for EspressiScale
 * 
 * This module implements the Bluetooth Low Energy (BLE) server functionality
 * for the EspressiScale, allowing it to communicate wirelessly with client devices
 * such as mobile apps or coffee machine controllers (like Gaggiuino).
 * 
 * The implementation uses NimBLE, a lightweight BLE stack for ESP32.
 */

/**
 * Service and characteristic UUIDs for EspressiScale
 * 
 * These UUIDs identify the BLE service and characteristics offered by the scale.
 * They must match the UUIDs used by client applications to establish communication.
 * The UUIDs are custom-defined for EspressiScale and match those defined in the
 * esp-arduino-ble-scales library for client-side implementation.
 */
#define ESPRESSISCALE_SERVICE_UUID         "19B10000-E8F2-537E-4F6C-D104768A1214"
#define ESPRESSISCALE_WEIGHT_CHAR_UUID     "19B10001-E8F2-537E-4F6C-D104768A1214"
#define ESPRESSISCALE_TIMER_CHAR_UUID      "19B10002-E8F2-537E-4F6C-D104768A1214"
#define ESPRESSISCALE_COMMAND_CHAR_UUID    "19B10003-E8F2-537E-4F6C-D104768A1214"

/**
 * Command codes for controlling the scale
 * 
 * These codes define the actions that can be triggered via BLE.
 * When a client writes one of these command values to the command characteristic,
 * the corresponding action is executed on the scale.
 * 
 * These codes must match those defined in client applications (esp-arduino-ble-scales library).
 */
enum class BLECommand : uint8_t {
  TARE = 0x01,        // Zero the scale
  START_TIMER = 0x02, // Start or resume the timer
  STOP_TIMER = 0x03,  // Pause the timer
  RESET_TIMER = 0x04  // Reset the timer to zero
};

/**
 * Callback class for handling BLE server events
 * 
 * This class handles connection and disconnection events from client devices.
 * It maintains a connection state flag and automatically restarts advertising
 * when a client disconnects.
 */
class EspressiScaleServerCallbacks : public NimBLEServerCallbacks {
public:
  /**
   * Constructor initializes the connection state
   */
  EspressiScaleServerCallbacks();
  
  /**
   * Called when a client connects to the server
   * 
   * @param pServer Pointer to the NimBLEServer instance
   */
  void onConnect(NimBLEServer* pServer);
  
  /**
   * Called when a client disconnects from the server
   * 
   * @param pServer Pointer to the NimBLEServer instance
   */
  void onDisconnect(NimBLEServer* pServer);
  
  /**
   * Check if a client is currently connected
   * 
   * @return true if a client is connected, false otherwise
   */
  bool isConnected() const { return _connected; }

private:
  bool _connected = false; // Connection state flag
};

/**
 * Callback class for handling command characteristic writes
 * 
 * This class processes commands received from client devices by
 * interpreting the value written to the command characteristic
 * and calling the appropriate scale functions.
 */
class CommandCallbacks : public NimBLECharacteristicCallbacks {
public:
  /**
   * Constructor
   */
  CommandCallbacks();
  
  /**
   * Called when a client writes to the command characteristic
   * 
   * @param pCharacteristic Pointer to the NimBLECharacteristic instance
   */
  void onWrite(NimBLECharacteristic* pCharacteristic);
};

/**
 * Initialize the BLE service for EspressiScale
 * 
 * This function sets up the BLE server, creates the service and characteristics,
 * initializes callbacks, and starts advertising.
 * It should be called once during application startup.
 */
void setupBLE();

/**
 * Update the weight characteristic with a new value
 * 
 * This function sends the current weight to connected clients by
 * updating the characteristic value and sending a notification.
 * 
 * @param weight Current weight in grams
 */
void updateBLEWeight(float weight);

/**
 * Update the timer characteristic with a new value
 * 
 * This function sends the current timer value to connected clients by
 * updating the characteristic value and sending a notification.
 * 
 * @param timer Current timer value in seconds
 */
void updateBLETimer(float timer);

/**
 * Process any BLE-related tasks that need to be handled in the main loop
 * 
 * This function should be called regularly in the main application loop
 * to handle any BLE tasks that need periodic attention.
 */
void processBLE(); 