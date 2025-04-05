#pragma once

#include "nimble_config.h" // Include our configuration first to disable problematic macros
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
 * It supports both the native EspressiScale protocol and an Acaia-compatible protocol.
 */

/**
 * BLE protocol mode enumeration
 * 
 * Defines which BLE protocol the scale will use:
 * - ESPRESSISCALE: The native EspressiScale protocol
 * - ACAIA: Acaia-compatible protocol for wider app compatibility
 */
enum class BLEProtocolMode : uint8_t {
  ESPRESSISCALE = 0,  // Native EspressiScale protocol
  ACAIA = 1           // Acaia-compatible protocol
};

/**
 * Service and characteristic UUIDs for EspressiScale native protocol
 */
#define ESPRESSISCALE_SERVICE_UUID         "19B10000-E8F2-537E-4F6C-D104768A1214"
#define ESPRESSISCALE_WEIGHT_CHAR_UUID     "19B10001-E8F2-537E-4F6C-D104768A1214"
#define ESPRESSISCALE_TIMER_CHAR_UUID      "19B10002-E8F2-537E-4F6C-D104768A1214"
#define ESPRESSISCALE_COMMAND_CHAR_UUID    "19B10003-E8F2-537E-4F6C-D104768A1214"

/**
 * Service and characteristic UUIDs for Acaia-compatible protocol
 * 
 * These UUIDs match those used by Acaia scales, allowing the EspressiScale
 * to be recognized by apps that support Acaia scales.
 */
#define ACAIA_SERVICE_UUID                 "00001820-0000-1000-8000-00805f9b34fb"  // Weight Scale service
#define ACAIA_WEIGHT_CHAR_UUID             "00002a9c-0000-1000-8000-00805f9b34fb"  // Weight Measurement
#define ACAIA_FEATURE_CHAR_UUID            "00002a9d-0000-1000-8000-00805f9b34fb"  // Weight Scale Feature
#define ACAIA_COMMAND_CHAR_UUID            "00002a9e-0000-1000-8000-00805f9b34fb"  // Custom for commands

/**
 * Command codes for controlling the scale (EspressiScale native protocol)
 */
enum class BLECommand : uint8_t {
  TARE = 0x01,        // Zero the scale
  START_TIMER = 0x02, // Start or resume the timer
  STOP_TIMER = 0x03,  // Pause the timer
  RESET_TIMER = 0x04  // Reset the timer to zero
};

/**
 * Acaia protocol packet header and command definitions
 */
#define ACAIA_HEADER_BYTE                  0xEF
#define ACAIA_WEIGHT_NOTIFICATION          0x0C
#define ACAIA_CMD_TARE                     0x00
#define ACAIA_CMD_START_TIMER              0x0D
#define ACAIA_CMD_STOP_TIMER               0x0E
#define ACAIA_CMD_RESET_TIMER              0x0F

/**
 * Acaia-compatible packet types
 */
enum class AcaiaPacketType : uint8_t {
  WEIGHT = 5,
  TIMER = 13,
  BUTTON = 11
};

/**
 * Callback class for handling BLE server events
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
 * in EspressiScale native protocol
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
 * Callback class for handling command characteristic writes
 * in Acaia-compatible protocol
 */
class AcaiaCommandCallbacks : public NimBLECharacteristicCallbacks {
public:
  /**
   * Constructor
   */
  AcaiaCommandCallbacks();
  
  /**
   * Called when a client writes to the command characteristic
   * 
   * @param pCharacteristic Pointer to the NimBLECharacteristic instance
   */
  void onWrite(NimBLECharacteristic* pCharacteristic);
  
private:
  /**
   * Verify packet checksum
   * 
   * @param data Pointer to packet data
   * @param length Length of the packet
   * @return true if checksum is valid, false otherwise
   */
  bool verifyChecksum(const uint8_t* data, size_t length);
  
  /**
   * Process Acaia command packet
   * 
   * @param data Pointer to packet data
   * @param length Length of the packet
   */
  void processCommand(const uint8_t* data, size_t length);
};

/**
 * Initialize the BLE service for EspressiScale
 * 
 * This function sets up the BLE server, creates the service and characteristics,
 * initializes callbacks, and starts advertising.
 * It should be called once during application startup.
 * 
 * @param mode Protocol mode to use (ESPRESSISCALE or ACAIA)
 */
void setupBLE(BLEProtocolMode mode = BLEProtocolMode::ESPRESSISCALE);

/**
 * Get the current BLE protocol mode
 * 
 * @return Current protocol mode (ESPRESSISCALE or ACAIA)
 */
BLEProtocolMode getBLEProtocolMode();

/**
 * Change the BLE protocol mode
 * 
 * This function changes the protocol mode between EspressiScale native
 * and Acaia-compatible. It restarts the BLE service with the new mode.
 * 
 * @param mode New protocol mode to use
 */
void setBLEProtocolMode(BLEProtocolMode mode);

/**
 * Update the weight characteristic with a new value
 * 
 * This function sends the current weight to connected clients by
 * updating the characteristic value and sending a notification.
 * The notification format depends on the current protocol mode.
 * 
 * @param weight Current weight in grams
 */
void updateBLEWeight(float weight);

/**
 * Update the timer characteristic with a new value
 * 
 * This function sends the current timer value to connected clients by
 * updating the characteristic value and sending a notification.
 * The notification format depends on the current protocol mode.
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

/**
 * Check if a BLE device is currently connected
 * 
 * @return true if a client is connected, false otherwise
 */
bool isBLEConnected(); 