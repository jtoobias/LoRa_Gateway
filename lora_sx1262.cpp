#include "lora_sx1262.hpp"

JtSX1262::JtSX1262(uint8_t nss, uint8_t rst, uint8_t busy, uint8_t dio1, SPIClass& spi)
  : _nss(nss), _rst(rst), _busy(busy), _dio1(dio1), _spi(spi) {}

void JtSX1262::begin() {
    pinMode(_nss, OUTPUT);
    pinMode(_rst, OUTPUT);
    pinMode(_busy, INPUT);
    pinMode(_dio1, INPUT);

    digitalWrite(_nss, HIGH);
    reset();
    wakeUp();
}

uint8_t JtSX1262::readStatus() {
    uint8_t status = 0x00;

    digitalWrite(_nss, LOW);
    _spi.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
    _spi.transfer(0xC0);   // GetStatus command
    _spi.transfer(0x00);   // Dummy byte
    status = _spi.transfer(0x00); // Status byte
    _spi.endTransaction();
    digitalWrite(_nss, HIGH);

    return status;
}

void JtSX1262::reset() {
    digitalWrite(_rst, LOW);
    delay(20);
    digitalWrite(_rst, HIGH);
    delay(10);
}

void JtSX1262::wakeUp() {
    digitalWrite(_nss, LOW);
    _spi.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
    _spi.transfer(0xC0);  // SetSleep opcode (for wakeup)
    _spi.transfer(0x00);  // dummy
    _spi.endTransaction();
    digitalWrite(_nss, HIGH);
    // waitWhileBusy();
}

void JtSX1262::writeCommand(uint8_t opcode, uint8_t* buffer, uint8_t size) {
    digitalWrite(_nss, LOW);
    _spi.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
    _spi.transfer(opcode);
    for (uint8_t i = 0; i < size; i++) {
        _spi.transfer(buffer[i]);
    }
    _spi.endTransaction();
    digitalWrite(_nss, HIGH);
    // waitWhileBusy();
}

void JtSX1262::readCommand(uint8_t opcode, uint8_t* buffer, uint8_t size) {
    digitalWrite(_nss, LOW);
    _spi.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
    _spi.transfer(opcode);
    _spi.transfer(0x00); // dummy
    for (uint8_t i = 0; i < size; i++) {
        buffer[i] = _spi.transfer(0x00);
    }
    _spi.endTransaction();
    digitalWrite(_nss, HIGH);
    // waitWhileBusy();
}

void JtSX1262::waitWhileBusy() {
    while (digitalRead(_busy) == HIGH) delayMicroseconds(10);
}

void JtSX1262::transmit(uint8_t* data, uint8_t length) {
    // just an example: write to buffer at 0x00
    uint8_t addr = 0x00;
    writeCommand(0x0E, &addr, 1);  // SetBufferBaseAddress TX

    // write payload
    digitalWrite(_nss, LOW);
    _spi.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
    _spi.transfer(0x0E);  // WriteBuffer
    _spi.transfer(0x00);  // offset
    for (uint8_t i = 0; i < length; i++) {
        _spi.transfer(data[i]);
    }
    _spi.endTransaction();
    digitalWrite(_nss, HIGH);
    waitWhileBusy();

    // trigger TX mode
    uint8_t tx_params[] = {0x00, 0x00, 0x00}; // no timeout
    writeCommand(0x83, tx_params, 3); // SetTx
}

bool JtSX1262::receive(uint8_t* buffer, uint8_t* length) {
    uint8_t irq_status[2];
    Serial.println("Read IRQ");
    readCommand(0x15, irq_status, 2); // GetIrqStatus

    // Combine the two bytes
    uint16_t irq = (irq_status[0] << 8) | irq_status[1];

    Serial.print("IRQ: ");
    Serial.println(irq,HEX);
    // Check if RX_DONE is set (bit 1)
    if (irq & 0x0002) {
        // Read RX buffer status
        uint8_t rx_buf_status[2];
        readCommand(0x13, rx_buf_status, 2); // GetRxBufferStatus
        uint8_t payload_len = rx_buf_status[0];
        uint8_t offset = rx_buf_status[1];
        Serial.print("Payload Length: ");
        Serial.println(payload_len);
        Serial.print("Offset: ");
        Serial.println(offset);

        // Read payload
        digitalWrite(_nss, LOW);
        _spi.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
        _spi.transfer(0x1E); // ReadBuffer
        _spi.transfer(offset); // offset
        _spi.transfer(0x00); // dummy byte
        
        Serial.println("Read: ");
        for (uint8_t i = 0; i < payload_len; i++) {
            buffer[i] = _spi.transfer(0x00);
        }
        _spi.endTransaction();
        digitalWrite(_nss, HIGH);

        *length = payload_len;
        Serial.print("Length: ");
        Serial.println(*length);

        // Clear IRQs
        uint8_t clear_irq[] = { 0xFF, 0xFF };
        writeCommand(0x97, clear_irq, 2); // ClearIrqStatus
        Serial.println("Clear: ");

        return true;
    }

    return false;
}

void JtSX1262::setFrequency(uint32_t frequency) {
    uint8_t freq[3];
    freq[0] = (frequency >> 16) & 0xFF;
    freq[1] = (frequency >> 8) & 0xFF;
    freq[2] = frequency & 0xFF;
    writeCommand(0x86, freq, 3);  // SetRfFrequency
}

void JtSX1262::setModulationParams(uint8_t spreadingFactor, uint8_t bandwidth, uint8_t codingRate) {
    uint8_t modParams[4];
    modParams[0] = spreadingFactor;  // SF6 to SF12
    modParams[1] = bandwidth;        // 125 kHz, 250 kHz, etc.
    modParams[2] = codingRate;      // 4/5, 4/6, etc.
    modParams[3] = 0x00;            // Reserved (set to 0)
    writeCommand(0x8A, modParams, 4);  // SetModulationParams
}
void JtSX1262::setTxPower(int8_t power) {
    uint8_t txPower[2];
    txPower[0] = power;  // TX power in dBm (e.g., 14 for 14 dBm)
    txPower[1] = 0xFF;    // Max ramp-up time
    writeCommand(0x8F, txPower, 2);  // SetTxPower
}
void JtSX1262::setLowPowerMode() {
    uint8_t sleepParams[1] = { 0x00 };  // Sleep mode
    writeCommand(0x84, sleepParams, 1);  // SetSleep
}
