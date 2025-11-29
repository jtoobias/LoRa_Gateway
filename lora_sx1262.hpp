#ifndef JtSX1262_H
#define JtSX1262_H

#include <Arduino.h>
#include <SPI.h>

class JtSX1262 {
public:
    JtSX1262(uint8_t nss, uint8_t rst, uint8_t busy, uint8_t dio1, SPIClass& spi);

    void begin();
    void reset();
    void transmit(uint8_t* data, uint8_t length);
    bool receive(uint8_t* buffer, uint8_t* length);
    void writeCommand(uint8_t opcode, uint8_t* buffer, uint8_t size);
    void readCommand(uint8_t opcode, uint8_t* buffer, uint8_t size);
    uint8_t readStatus();

    void setFrequency(uint32_t frequency);
    void setModulationParams(uint8_t spreadingFactor, uint8_t bandwidth, uint8_t codingRate);
    void setTxPower(int8_t power);
    void setLowPowerMode();

private:
    uint8_t _nss, _rst, _busy, _dio1;
    SPIClass& _spi;

    void wakeUp();
    void waitWhileBusy();
};

#endif
