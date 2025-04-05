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
 * 
 * The implementation supports two modes:
 * - EspressiScale native protocol
 * - Acaia-compatible protocol for wider app compatibility
 */

// Forward declarations of internal functions
void setupEspressiScaleService();
void setupAcaiaService();
uint8_t calculateAcaiaChecksum(const uint8_t* data, size_t length);

// Global BLE server and characteristics pointers
NimBLEServer* pServer = nullptr;
NimBLEService* pService = nullptr;
NimBLECharacteristic* pWeightCharacteristic = nullptr;
NimBLECharacteristic* pTimerCharacteristic = nullptr;
NimBLECharacteristic* pCommandCharacteristic = nullptr;

// Additional characteristics for Acaia protocol
NimBLECharacteristic* pFeatureCharacteristic = nullptr;

// Protocol mode
BLEProtocolMode currentProtocolMode = BLEProtocolMode::ESPRESSISCALE;

// Create server callbacks instance
EspressiScaleServerCallbacks* pServerCallbacks = nullptr;

// Create command callbacks instances
CommandCallbacks* pCommandCallbacks = nullptr;
AcaiaCommandCallbacks* pAcaiaCommandCallbacks = nullptr;

/**
 * External references to scale control functions defined in main.cpp
 * These functions are called when BLE commands are received
 */
extern void tareScale();      // Zeroes the scale
extern void startTimer();     // Starts or resumes the timer
extern void stopTimer();      // Pauses the timer
extern void resetTimer();     // Resets the timer to zero

/**
 * EspressiScaleServerCallbacks constructor
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
 * @param pServer Pointer to the NimBLEServer instance
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
 * @param pServer Pointer to the NimBLEServer instance
 */
void EspressiScaleServerCallbacks::onDisconnect(NimBLEServer* pServer) {
  _connected = false;
  Serial.println("BLE client disconnected");
  
  // Restart advertising when client disconnects
  NimBLEDevice::startAdvertising();
}

/**
 * CommandCallbacks constructor (EspressiScale native protocol)
 */
CommandCallbacks::CommandCallbacks() {
  // Constructor (no initialization needed)
}

/**
 * Process commands received from BLE clients (EspressiScale native protocol)
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
 * AcaiaCommandCallbacks constructor (Acaia-compatible protocol)
 */
AcaiaCommandCallbacks::AcaiaCommandCallbacks() {
  // Constructor (no initialization needed)
}

/**
 * Process commands received from BLE clients (Acaia-compatible protocol)
 * 
 * This function is called when a client writes to the command characteristic.
 * It processes the Acaia protocol packet format, verifies the checksum,
 * and executes the requested command.
 * 
 * @param pCharacteristic Pointer to the characteristic that received the write
 */
void AcaiaCommandCallbacks::onWrite(NimBLECharacteristic* pCharacteristic) {
  std::string value = pCharacteristic->getValue();
  
  if (value.length() < 4) {
    Serial.println("Acaia command too short");
    return;
  }
  
  const uint8_t* data = reinterpret_cast<const uint8_t*>(value.data());
  size_t length = value.length();
  
  // Check for Acaia header byte
  if (data[0] != ACAIA_HEADER_BYTE) {
    Serial.println("Invalid Acaia header");
    return;
  }
  
  // Verify packet checksum
  if (!verifyChecksum(data, length)) {
    Serial.println("Invalid Acaia checksum");
    return;
  }
  
  // Process the command
  processCommand(data, length);
}

/**
 * Verify Acaia packet checksum
 * 
 * @param data Pointer to packet data
 * @param length Length of the packet
 * @return true if checksum is valid, false otherwise
 */
bool AcaiaCommandCallbacks::verifyChecksum(const uint8_t* data, size_t length) {
  if (length < 2) return false;
  
  uint8_t checksum = 0;
  for (size_t i = 0; i < length - 1; i++) {
    checksum ^= data[i]; // XOR checksum
  }
  
  return checksum == data[length - 1];
}

/**
 * Process Acaia command packet
 * 
 * @param data Pointer to packet data
 * @param length Length of the packet
 */
void AcaiaCommandCallbacks::processCommand(const uint8_t* data, size_t length) {
  // Format: header, type, length, command, payload, checksum
  if (length < 5) return;
  
  uint8_t command = data[3];
  
  switch (command) {
    case ACAIA_CMD_TARE:
      Serial.println("Acaia Command: TARE");
      tareScale();
      break;
    case ACAIA_CMD_START_TIMER:
      Serial.println("Acaia Command: START_TIMER");
      startTimer();
      break;
    case ACAIA_CMD_STOP_TIMER:
      Serial.println("Acaia Command: STOP_TIMER");
      stopTimer();
      break;
    case ACAIA_CMD_RESET_TIMER:
      Serial.println("Acaia Command: RESET_TIMER");
      resetTimer();
      break;
    default:
      Serial.print("Unknown Acaia command received: 0x");
      Serial.println(command, HEX);
      break;
  }
}

/**
 * Create a checksum for Acaia packet
 * 
 * @param data Pointer to packet data (excluding checksum byte)
 * @param length Length of the data (excluding checksum byte)
 * @return Calculated checksum byte
 */
uint8_t calculateAcaiaChecksum(const uint8_t* data, size_t length) {
  uint8_t checksum = 0;
  for (size_t i = 0; i < length; i++) {
    checksum ^= data[i]; // XOR checksum
  }
  return checksum;
}

/**
 * Initialize the BLE service
 * 
 * This function sets up the complete BLE server infrastructure based on the
 * selected protocol mode (EspressiScale native or Acaia-compatible).
 * 
 * @param mode Protocol mode to use
 */
void setupBLE(BLEProtocolMode mode) {
  Serial.print("Initializing BLE in ");
  Serial.println(mode == BLEProtocolMode::ESPRESSISCALE ? "EspressiScale mode" : "Acaia-compatible mode");
  
  // Store the current protocol mode
  currentProtocolMode = mode;
  
  // Clean up previous BLE resources if they exist
  if (pServer != nullptr) {
    NimBLEDevice::deinit(true);
  }
  
  // Initialize NimBLE device with appropriate name
  if (mode == BLEProtocolMode::ESPRESSISCALE) {
    NimBLEDevice::init("EspressiScale");
  } else {
    NimBLEDevice::init("Acaia"); // Mimic Acaia device name
  }
  
  // Create the server
  pServer = NimBLEDevice::createServer();
  
  // Set server callbacks
  pServerCallbacks = new EspressiScaleServerCallbacks();
  pServer->setCallbacks(pServerCallbacks);
  
  // Configure services and characteristics based on protocol mode
  if (mode == BLEProtocolMode::ESPRESSISCALE) {
    setupEspressiScaleService();
  } else {
    setupAcaiaService();
  }
  
  // Start advertising
  NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
  
  if (mode == BLEProtocolMode::ESPRESSISCALE) {
    pAdvertising->addServiceUUID(ESPRESSISCALE_SERVICE_UUID);
  } else {
    pAdvertising->addServiceUUID(ACAIA_SERVICE_UUID);
  }
  
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);  // Functions that help with iPhone connections
  pAdvertising->setMaxPreferred(0x12);
  
  NimBLEDevice::startAdvertising();
  
  Serial.println("BLE initialized, advertising started");
}

/**
 * Set up the EspressiScale native protocol service
 */
void setupEspressiScaleService() {
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
}

/**
 * Set up the Acaia-compatible protocol service
 */
void setupAcaiaService() {
  // Create the service
  pService = pServer->createService(ACAIA_SERVICE_UUID);
  
  // Create characteristics
  pWeightCharacteristic = pService->createCharacteristic(
    ACAIA_WEIGHT_CHAR_UUID,
    NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
  );
  
  pFeatureCharacteristic = pService->createCharacteristic(
    ACAIA_FEATURE_CHAR_UUID,
    NIMBLE_PROPERTY::READ
  );
  
  // Feature flags - indicating scale capabilities
  uint8_t featureValue[] = {0x00, 0x00}; // Basic scale features
  pFeatureCharacteristic->setValue(featureValue, sizeof(featureValue));
  
  pCommandCharacteristic = pService->createCharacteristic(
    ACAIA_COMMAND_CHAR_UUID,
    NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::NOTIFY
  );
  
  // Set command characteristic callbacks
  pAcaiaCommandCallbacks = new AcaiaCommandCallbacks();
  pCommandCharacteristic->setCallbacks(pAcaiaCommandCallbacks);
  
  // Start the service
  pService->start();
}

/**
 * Get the current BLE protocol mode
 * 
 * @return Current protocol mode (ESPRESSISCALE or ACAIA)
 */
BLEProtocolMode getBLEProtocolMode() {
  return currentProtocolMode;
}

/**
 * Change the BLE protocol mode
 * 
 * This function changes the protocol mode between EspressiScale native
 * and Acaia-compatible. It restarts the BLE service with the new mode.
 * 
 * @param mode New protocol mode to use
 */
void setBLEProtocolMode(BLEProtocolMode mode) {
  if (mode == currentProtocolMode) {
    return; // No change needed
  }
  
  Serial.print("Changing BLE protocol mode to ");
  Serial.println(mode == BLEProtocolMode::ESPRESSISCALE ? "EspressiScale" : "Acaia");
  
  // Restart BLE with the new mode
  setupBLE(mode);
}

/**
 * Send weight updates to connected clients
 * 
 * This function updates the weight characteristic with the current weight value
 * and sends a notification to all connected clients that have enabled notifications.
 * The notification format depends on the current protocol mode.
 * 
 * @param weight Current weight in grams
 */
void updateBLEWeight(float weight) {
  if (pWeightCharacteristic == nullptr) {
    return;
  }
  
  if (currentProtocolMode == BLEProtocolMode::ESPRESSISCALE) {
    // EspressiScale native protocol - simple float value
    pWeightCharacteristic->setValue(weight);
    pWeightCharacteristic->notify();
  } else {
    // Acaia-compatible protocol - formatted packet
    uint8_t packet[8] = {0};
    
    // Header
    packet[0] = ACAIA_HEADER_BYTE;
    
    // Packet type (weight)
    packet[1] = static_cast<uint8_t>(AcaiaPacketType::WEIGHT);
    
    // Length of payload
    packet[2] = 3;
    
    // Weight value (convert to integer, 10x for one decimal place)
    int16_t weightInt = weight * 10;
    packet[3] = (weightInt >> 8) & 0xFF;  // High byte
    packet[4] = weightInt & 0xFF;         // Low byte
    
    // Unit (0 = grams)
    packet[5] = 0;
    
    // Calculate checksum
    packet[6] = calculateAcaiaChecksum(packet, 6);
    
    pWeightCharacteristic->setValue(packet, sizeof(packet));
    pWeightCharacteristic->notify();
  }
}

/**
 * Send timer updates to connected clients
 * 
 * This function updates the timer characteristic with the current timer value
 * and sends a notification to all connected clients that have enabled notifications.
 * The notification format depends on the current protocol mode.
 * 
 * @param timer Current timer value in seconds
 */
void updateBLETimer(float timer) {
  if (currentProtocolMode == BLEProtocolMode::ESPRESSISCALE) {
    // EspressiScale native protocol
    if (pTimerCharacteristic != nullptr) {
      pTimerCharacteristic->setValue(timer);
      pTimerCharacteristic->notify();
    }
  } else {
    // Acaia-compatible protocol - send timer through command characteristic
    if (pCommandCharacteristic != nullptr) {
      uint8_t packet[7] = {0};
      
      // Header
      packet[0] = ACAIA_HEADER_BYTE;
      
      // Packet type (timer)
      packet[1] = static_cast<uint8_t>(AcaiaPacketType::TIMER);
      
      // Length of payload
      packet[2] = 2;
      
      // Timer value in seconds (integer)
      uint16_t timerInt = timer;
      packet[3] = (timerInt >> 8) & 0xFF;  // High byte
      packet[4] = timerInt & 0xFF;         // Low byte
      
      // Calculate checksum
      packet[5] = calculateAcaiaChecksum(packet, 5);
      
      pCommandCharacteristic->setValue(packet, sizeof(packet));
      pCommandCharacteristic->notify();
    }
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