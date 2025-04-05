#include "ble_service.h"
#include "arduino.h"
#include "scale.h"

/**
 * BLE Service Implementation for EspressiScale
 * 
 * This file implements the BLE functionality defined in ble_service.h.
 * It creates a BLE server that allows clients to:
 * - Receive weight measurements via notifications
 * - Receive timer values via notifications
 * - Send commands to control the scale (tare, timer functions)
 */

// Global BLE server and characteristics pointers
NimBLEServer* pServer = nullptr;
NimBLEService* pService = nullptr;
NimBLECharacteristic* pWeightCharacteristic = nullptr;
NimBLECharacteristic* pTimerCharacteristic = nullptr;
NimBLECharacteristic* pCommandCharacteristic = nullptr;

// Create server callbacks instance
EspressiScaleServerCallbacks* pServerCallbacks = nullptr;

// Create command callbacks instance
CommandCallbacks* pCommandCallbacks = nullptr;

/**
 * External references to scale control functions defined in main.cpp
 * These functions are called when BLE commands are received
 */
extern void tareScale();      // Zeroes the scale
extern void startTimer();     // Starts or resumes the timer
extern void stopTimer();      // Pauses the timer
extern void resetTimer();     // Resets the timer to zero

/**
 * BLEServerCallbacks constructor
 * Initializes the connected state to false
 */
EspressiScaleServerCallbacks::EspressiScaleServerCallbacks() {
  _connected = false;
}

/**
 * Called when a client connects to the BLE server
 * 
 * Updates the connection state and logs the connection event
 * 
 * @param pServer Pointer to the BLE server
 */
void EspressiScaleServerCallbacks::onConnect(NimBLEServer* pServer) {
  _connected = true;
  Serial.println("BLE client connected");
}

/**
 * Called when a client disconnects from the BLE server
 * 
 * Updates the connection state, logs the event, and restarts advertising
 * to allow new clients to connect.
 * 
 * @param pServer Pointer to the BLE server
 */
void EspressiScaleServerCallbacks::onDisconnect(NimBLEServer* pServer) {
  _connected = false;
  Serial.println("BLE client disconnected");
  
  // Restart advertising when client disconnects
  NimBLEDevice::startAdvertising();
}

/**
 * CommandCallbacks constructor
 */
CommandCallbacks::CommandCallbacks() {
  // Constructor (no initialization needed)
}

/**
 * Process commands received from BLE clients
 * 
 * This function is called when a client writes to the command characteristic.
 * It interprets the command code and calls the appropriate function to
 * execute the requested action.
 * 
 * @param pCharacteristic Pointer to the characteristic that received the write
 */
void CommandCallbacks::onWrite(NimBLECharacteristic* pCharacteristic) {
  std::string value = pCharacteristic->getValue();
  
  if (value.length() > 0) {
    uint8_t command = value[0];
    
    switch (static_cast<BLECommand>(command)) {
      case BLECommand::TARE:
        Serial.println("BLE Command: TARE");
        tareScale();
        break;
      case BLECommand::START_TIMER:
        Serial.println("BLE Command: START_TIMER");
        startTimer();
        break;
      case BLECommand::STOP_TIMER:
        Serial.println("BLE Command: STOP_TIMER");
        stopTimer();
        break;
      case BLECommand::RESET_TIMER:
        Serial.println("BLE Command: RESET_TIMER");
        resetTimer();
        break;
      default:
        Serial.println("Unknown BLE command received");
        break;
    }
  }
}

/**
 * Initialize the BLE service
 * 
 * This function sets up the complete BLE server infrastructure:
 * 1. Initializes the NimBLE device with the name "EspressiScale"
 * 2. Creates a BLE server
 * 3. Sets up connection callbacks
 * 4. Creates the service and characteristics
 * 5. Sets up command handling
 * 6. Starts advertising
 * 
 * This should be called once during device initialization.
 */
void setupBLE() {
  Serial.println("Initializing BLE...");
  
  // Initialize NimBLE device
  NimBLEDevice::init("EspressiScale");
  
  // Create the server
  pServer = NimBLEDevice::createServer();
  
  // Set server callbacks
  pServerCallbacks = new EspressiScaleServerCallbacks();
  pServer->setCallbacks(pServerCallbacks);
  
  // Create the service
  pService = pServer->createService(ESPRESSISCALE_SERVICE_UUID);
  
  // Create characteristics
  pWeightCharacteristic = pService->createCharacteristic(
    ESPRESSISCALE_WEIGHT_CHAR_UUID,
    NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
  );
  
  pTimerCharacteristic = pService->createCharacteristic(
    ESPRESSISCALE_TIMER_CHAR_UUID,
    NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
  );
  
  pCommandCharacteristic = pService->createCharacteristic(
    ESPRESSISCALE_COMMAND_CHAR_UUID,
    NIMBLE_PROPERTY::WRITE
  );
  
  // Set command characteristic callbacks
  pCommandCallbacks = new CommandCallbacks();
  pCommandCharacteristic->setCallbacks(pCommandCallbacks);
  
  // Start the service
  pService->start();
  
  // Start advertising
  NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(ESPRESSISCALE_SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);  // functions that help with iPhone connections issue
  pAdvertising->setMaxPreferred(0x12);
  
  NimBLEDevice::startAdvertising();
  
  Serial.println("BLE initialized, advertising started");
}

/**
 * Send weight updates to connected clients
 * 
 * This function updates the weight characteristic with the current weight value
 * and sends a notification to all connected clients that have enabled notifications.
 * 
 * @param weight Current weight in grams
 */
void updateBLEWeight(float weight) {
  if (pWeightCharacteristic != nullptr) {
    pWeightCharacteristic->setValue(weight);
    pWeightCharacteristic->notify();
  }
}

/**
 * Send timer updates to connected clients
 * 
 * This function updates the timer characteristic with the current timer value
 * and sends a notification to all connected clients that have enabled notifications.
 * 
 * @param timer Current timer value in seconds
 */
void updateBLETimer(float timer) {
  if (pTimerCharacteristic != nullptr) {
    pTimerCharacteristic->setValue(timer);
    pTimerCharacteristic->notify();
  }
}

/**
 * Process any BLE tasks in the main loop
 * 
 * This function is a placeholder for any BLE-related processing that might
 * need to happen in the main application loop. Currently, NimBLE handles
 * most tasks internally, so this function is empty.
 * 
 * It should be called regularly in the main loop.
 */
void processBLE() {
  // Handle any BLE tasks that need to be processed in the main loop
  // Currently, this is empty as NimBLE handles most tasks internally
} 