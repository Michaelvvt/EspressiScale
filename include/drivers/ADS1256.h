#pragma once

#include <Arduino.h>
#include <SPI.h>

// ADS1256 Register Map
#define ADS1256_REG_STATUS     0x00
#define ADS1256_REG_MUX        0x01
#define ADS1256_REG_ADCON      0x02
#define ADS1256_REG_DRATE      0x03
#define ADS1256_REG_IO         0x04
#define ADS1256_REG_OFC0       0x05
#define ADS1256_REG_OFC1       0x06
#define ADS1256_REG_OFC2       0x07
#define ADS1256_REG_FSC0       0x08
#define ADS1256_REG_FSC1       0x09
#define ADS1256_REG_FSC2       0x0A

// ADS1256 Commands
#define ADS1256_CMD_WAKEUP     0x00
#define ADS1256_CMD_RDATA      0x01
#define ADS1256_CMD_RDATAC     0x03
#define ADS1256_CMD_SDATAC     0x0F
#define ADS1256_CMD_RREG       0x10
#define ADS1256_CMD_WREG       0x50
#define ADS1256_CMD_SELFCAL    0xF0
#define ADS1256_CMD_SELFOCAL   0xF1
#define ADS1256_CMD_SELFGCAL   0xF2
#define ADS1256_CMD_SYSOCAL    0xF3
#define ADS1256_CMD_SYSGCAL    0xF4
#define ADS1256_CMD_SYNC       0xFC
#define ADS1256_CMD_STANDBY    0xFD
#define ADS1256_CMD_RESET      0xFE

// Data Rates
#define ADS1256_DRATE_30000SPS 0xF0
#define ADS1256_DRATE_15000SPS 0xE0
#define ADS1256_DRATE_7500SPS  0xD0
#define ADS1256_DRATE_3750SPS  0xC0
#define ADS1256_DRATE_2000SPS  0xB0
#define ADS1256_DRATE_1000SPS  0xA1
#define ADS1256_DRATE_500SPS   0x92
#define ADS1256_DRATE_100SPS   0x82
#define ADS1256_DRATE_60SPS    0x72
#define ADS1256_DRATE_50SPS    0x63
#define ADS1256_DRATE_30SPS    0x53
#define ADS1256_DRATE_25SPS    0x43
#define ADS1256_DRATE_15SPS    0x33
#define ADS1256_DRATE_10SPS    0x23
#define ADS1256_DRATE_5SPS     0x13
#define ADS1256_DRATE_2_5SPS   0x03

// PGA Gain Settings
#define ADS1256_GAIN_1         0x00
#define ADS1256_GAIN_2         0x01
#define ADS1256_GAIN_4         0x02
#define ADS1256_GAIN_8         0x03
#define ADS1256_GAIN_16        0x04
#define ADS1256_GAIN_32        0x05
#define ADS1256_GAIN_64        0x06

// Channel definition
#define ADS1256_MUXP_AIN0      0x00
#define ADS1256_MUXP_AIN1      0x10
#define ADS1256_MUXP_AIN2      0x20
#define ADS1256_MUXP_AIN3      0x30
#define ADS1256_MUXP_AIN4      0x40
#define ADS1256_MUXP_AIN5      0x50
#define ADS1256_MUXP_AIN6      0x60
#define ADS1256_MUXP_AIN7      0x70
#define ADS1256_MUXP_AINCOM    0x80

#define ADS1256_MUXN_AIN0      0x00
#define ADS1256_MUXN_AIN1      0x01
#define ADS1256_MUXN_AIN2      0x02
#define ADS1256_MUXN_AIN3      0x03
#define ADS1256_MUXN_AIN4      0x04
#define ADS1256_MUXN_AIN5      0x05
#define ADS1256_MUXN_AIN6      0x06
#define ADS1256_MUXN_AIN7      0x07
#define ADS1256_MUXN_AINCOM    0x08

// Class for the ADS1256 ADC
class ADS1256 {
public:
    // Constructor
    ADS1256(int cs_pin, int drdy_pin, int reset_pin = -1, SPIClass *spi = &SPI);
    
    // Initialize the ADS1256
    bool begin(uint8_t gain = ADS1256_GAIN_1, uint8_t dataRate = ADS1256_DRATE_100SPS);
    
    // Read from register
    uint8_t readRegister(uint8_t reg);
    
    // Write to register
    void writeRegister(uint8_t reg, uint8_t value);
    
    // Set channel and gain
    void setChannel(uint8_t channel_p, uint8_t channel_n);
    
    // Set mux channel using channel pair
    void setChannelDiff(uint8_t channel, bool differential = false);
    
    // Set gain
    void setGain(uint8_t gain);
    
    // Set data rate
    void setDataRate(uint8_t drate);
    
    // Send command
    void sendCommand(uint8_t command);
    
    // Read data from current channel
    int32_t readCurrentChannel();
    
    // Read data from specified channel
    int32_t readChannel(uint8_t channel, bool differential = false);
    
    // Wait for DRDY pin to go low
    void waitDRDY();
    
    // Calibration methods
    void calibrateSelf();
    void calibrateOffset();
    void calibrateGain();
    
    // Reset the ADS1256
    void reset();
    
    // Convert raw value to voltage
    float rawToVoltage(int32_t rawValue);
    
    // Read from all 4 load cells
    bool readLoadCells(float values[4]);
    
    // Set VREF value
    void setVref(float vref) { _vref = vref; }
    
    // Get current VREF value
    float getVref() { return _vref; }
    
private:
    SPIClass *_spi;
    int _cs_pin;
    int _drdy_pin;
    int _reset_pin;
    uint8_t _gain;
    float _vref;
    float _gainValue;
    
    // Chip select control
    void csLow();
    void csHigh();
    
    // SPI transfer
    uint8_t spiTransfer(uint8_t data);
    
    // Read conversion result
    int32_t readData();
    
    // Convert raw value based on current gain
    float convertRawToVoltage(int32_t rawValue);
}; 