#include "drivers/ADS1256.h"

// Constructor
ADS1256::ADS1256(int cs_pin, int drdy_pin, int reset_pin, SPIClass *spi) {
    _spi = spi;
    _cs_pin = cs_pin;
    _drdy_pin = drdy_pin;
    _reset_pin = reset_pin;
    _vref = 2.5; // Default reference voltage
    _gain = ADS1256_GAIN_1;
    _gainValue = 1.0;
}

// Initialize ADS1256
bool ADS1256::begin(uint8_t gain, uint8_t dataRate) {
    // Initialize pins
    pinMode(_cs_pin, OUTPUT);
    pinMode(_drdy_pin, INPUT_PULLUP);
    
    if (_reset_pin != -1) {
        pinMode(_reset_pin, OUTPUT);
    }
    
    // Initialize SPI
    _spi->begin();
    csHigh();
    
    // Hardware reset if reset pin is defined
    if (_reset_pin != -1) {
        digitalWrite(_reset_pin, LOW);
        delay(10);
        digitalWrite(_reset_pin, HIGH);
        delay(10);
    }
    
    // Wait for chip to settle
    delay(50);
    
    // Software reset
    csLow();
    sendCommand(ADS1256_CMD_RESET);
    delay(10);
    
    // Wait for DRDY to indicate reset completed
    waitDRDY();
    
    // Check if communication works by reading the STATUS register
    uint8_t status = readRegister(ADS1256_REG_STATUS);
    if ((status & 0x03) == 0) {
        // Device is not responding correctly
        csHigh();
        return false;
    }
    
    // Set gain and data rate
    setGain(gain);
    setDataRate(dataRate);
    
    // Enable buffer - set bit 1 in STATUS register
    writeRegister(ADS1256_REG_STATUS, 0x02);
    
    // Self calibration
    sendCommand(ADS1256_CMD_SELFCAL);
    waitDRDY();
    
    csHigh();
    return true;
}

// Read register value
uint8_t ADS1256::readRegister(uint8_t reg) {
    csLow();
    _spi->transfer(ADS1256_CMD_RREG | reg); // RREG command + register address
    _spi->transfer(0x00);                   // Read one register only
    delayMicroseconds(10);                  // t6 delay, min 50 * tCLKIN
    uint8_t value = _spi->transfer(0xFF);   // Read data
    csHigh();
    return value;
}

// Write to register
void ADS1256::writeRegister(uint8_t reg, uint8_t value) {
    csLow();
    _spi->transfer(ADS1256_CMD_WREG | reg); // WREG command + register address
    _spi->transfer(0x00);                   // Write one register only
    _spi->transfer(value);                  // Write data
    csHigh();
}

// Set channel single-ended or differential
void ADS1256::setChannel(uint8_t channel_p, uint8_t channel_n) {
    uint8_t mux_value = (channel_p << 4) | channel_n;
    writeRegister(ADS1256_REG_MUX, mux_value);
    
    // Synchronize the ADC
    csLow();
    sendCommand(ADS1256_CMD_SYNC);
    delayMicroseconds(10);
    sendCommand(ADS1256_CMD_WAKEUP);
    csHigh();
    
    // Allow channel to settle
    delayMicroseconds(100);
}

// Set channel with simplified interface
void ADS1256::setChannelDiff(uint8_t channel, bool differential) {
    uint8_t p_channel, n_channel;
    
    if (channel > 7) {
        // Invalid channel
        return;
    }
    
    // Set up positive channel
    switch (channel) {
        case 0: p_channel = ADS1256_MUXP_AIN0; break;
        case 1: p_channel = ADS1256_MUXP_AIN1; break;
        case 2: p_channel = ADS1256_MUXP_AIN2; break;
        case 3: p_channel = ADS1256_MUXP_AIN3; break;
        case 4: p_channel = ADS1256_MUXP_AIN4; break;
        case 5: p_channel = ADS1256_MUXP_AIN5; break;
        case 6: p_channel = ADS1256_MUXP_AIN6; break;
        case 7: p_channel = ADS1256_MUXP_AIN7; break;
        default: p_channel = ADS1256_MUXP_AINCOM; break;
    }
    
    // For differential, use next channel. For single-ended, use AINCOM
    if (differential) {
        // For differential, pair channels 0-1, 2-3, 4-5, 6-7
        n_channel = channel & 0x01 ? channel - 1 : channel + 1;
    } else {
        n_channel = ADS1256_MUXN_AINCOM;
    }
    
    setChannel(p_channel >> 4, n_channel);
}

// Set PGA gain
void ADS1256::setGain(uint8_t gain) {
    if (gain > ADS1256_GAIN_64) {
        gain = ADS1256_GAIN_1;
    }
    
    // Store gain for voltage conversion
    _gain = gain;
    
    // Calculate gain value (1, 2, 4, 8, 16, 32, 64)
    _gainValue = 1 << gain;
    
    // Get current ADCON register value, update just the gain bits (bits 0-2)
    uint8_t adcon = readRegister(ADS1256_REG_ADCON);
    adcon &= 0xF8;  // Clear gain bits
    adcon |= gain;  // Set new gain
    
    writeRegister(ADS1256_REG_ADCON, adcon);
}

// Set data rate
void ADS1256::setDataRate(uint8_t drate) {
    writeRegister(ADS1256_REG_DRATE, drate);
}

// Send command
void ADS1256::sendCommand(uint8_t command) {
    _spi->transfer(command);
}

// Read conversion result from current channel
int32_t ADS1256::readCurrentChannel() {
    csLow();
    sendCommand(ADS1256_CMD_RDATA);
    delayMicroseconds(10); // t6 delay, min 50 * tCLKIN
    
    int32_t value = readData();
    csHigh();
    
    return value;
}

// Read from specified channel
int32_t ADS1256::readChannel(uint8_t channel, bool differential) {
    // Set the channel
    setChannelDiff(channel, differential);
    
    // Wait for the DRDY pin to go low
    waitDRDY();
    
    // Read the conversion result
    return readCurrentChannel();
}

// Wait for DRDY pin to go low
void ADS1256::waitDRDY() {
    while (digitalRead(_drdy_pin) == HIGH) {
        delayMicroseconds(10);
    }
}

// Self calibration
void ADS1256::calibrateSelf() {
    csLow();
    sendCommand(ADS1256_CMD_SELFCAL);
    waitDRDY();
    csHigh();
}

// Offset calibration
void ADS1256::calibrateOffset() {
    csLow();
    sendCommand(ADS1256_CMD_SELFOCAL);
    waitDRDY();
    csHigh();
}

// Gain calibration
void ADS1256::calibrateGain() {
    csLow();
    sendCommand(ADS1256_CMD_SELFGCAL);
    waitDRDY();
    csHigh();
}

// Reset the ADS1256
void ADS1256::reset() {
    csLow();
    sendCommand(ADS1256_CMD_RESET);
    delay(5); // Give time for reset to complete
    csHigh();
    delay(5);
}

// Convert raw value to voltage
float ADS1256::rawToVoltage(int32_t rawValue) {
    return (float)rawValue * (_vref / (8388608.0 * _gainValue));
}

// Read all 4 load cells
bool ADS1256::readLoadCells(float values[4]) {
    for (int i = 0; i < 4; i++) {
        waitDRDY();
        int32_t raw = readChannel(i, true); // Read differential channels 0-1, 2-3, 4-5, 6-7
        values[i] = rawToVoltage(raw);
    }
    return true;
}

// Chip select low
void ADS1256::csLow() {
    digitalWrite(_cs_pin, LOW);
}

// Chip select high
void ADS1256::csHigh() {
    digitalWrite(_cs_pin, HIGH);
}

// SPI transfer
uint8_t ADS1256::spiTransfer(uint8_t data) {
    return _spi->transfer(data);
}

// Read conversion result
int32_t ADS1256::readData() {
    int32_t value = 0;
    
    // 24-bit value, MSB first
    value |= (_spi->transfer(0xFF) << 16);
    value |= (_spi->transfer(0xFF) << 8);
    value |= _spi->transfer(0xFF);
    
    // Convert to signed value
    if (value & 0x800000) {
        value |= 0xFF000000;
    }
    
    return value;
} 