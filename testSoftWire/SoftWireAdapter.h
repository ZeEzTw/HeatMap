#ifndef SOFTWIREADAPTER_H
#define SOFTWIREADAPTER_H

#include <SoftWire.h>
#include <Wire.h>

class SoftWireAdapter : public TwoWire {
public:
    SoftWire *sw;
    SoftWireAdapter(SoftWire *soft) : TwoWire(0), sw(soft) {}

    bool begin() override { return true; }  // returnează bool, nu void
    void begin(uint8_t) override { sw->begin(); } // pentru compatibilitate cu alte overload-uri
    void beginTransmission(uint8_t address) override { sw->beginTransmission(address); }
    uint8_t endTransmission(bool stopBit = true) override { return sw->endTransmission(stopBit); }
    size_t write(uint8_t data) override { return sw->write(data); }
    uint8_t requestFrom(uint8_t address, uint8_t quantity, bool stopBit = true) override {
        return sw->requestFrom(address, quantity, stopBit);
    }
    int read() override { return sw->read(); }
};

#endif

