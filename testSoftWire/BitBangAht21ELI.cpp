#include "BitBangAHT21ELI.h"

// AHT21 default I2C address
#define AHTX0_I2CADDR_DEFAULT 0x38
#define AHTX0_CMD_CALIBRATE 0xE1
#define AHTX0_CMD_SOFTRESET 0xBA
#define AHTX0_CMD_TRIGGER 0xAC
#define AHTX0_STATUS_BUSY 0x80
#define AHTX0_STATUS_CALIBRATED 0x08
#define I2C_DELAY 5

BitBangAHT21ELI::BitBangAHT21ELI(int sdaPin, int sclPin) {
    _sdaPin = sdaPin;
    _sclPin = sclPin;
    aht_init();
}

// Bit-banged I2C timing (microseconds)

// ----- Bit-banged I2C functions -----
void BitBangAHT21ELI::i2c_init() {
    pinMode(_sdaPin, INPUT_PULLUP);
    pinMode(_sclPin, INPUT_PULLUP);
    digitalWrite(_sdaPin, HIGH);
    digitalWrite(_sclPin, HIGH);
}

void BitBangAHT21ELI::sda_low() {
    pinMode(_sdaPin, OUTPUT);
    digitalWrite(_sdaPin, LOW);
}

void BitBangAHT21ELI::sda_high() {
    pinMode(_sdaPin, INPUT_PULLUP);
}

void BitBangAHT21ELI::scl_low() {
    pinMode(_sclPin, OUTPUT);
    digitalWrite(_sclPin, LOW);
}

void BitBangAHT21ELI::scl_high() {
    pinMode(_sclPin, INPUT_PULLUP);
    // Wait for clock stretching
    while (digitalRead(_sclPin) == LOW) {
        delayMicroseconds(1);
    }
}

void BitBangAHT21ELI::i2c_start() {
    sda_high();
    delayMicroseconds(I2C_DELAY);
    scl_high();
    delayMicroseconds(I2C_DELAY);
    sda_low();
    delayMicroseconds(I2C_DELAY);
    scl_low();
    delayMicroseconds(I2C_DELAY);
}

void BitBangAHT21ELI::i2c_stop() {
    sda_low();
    delayMicroseconds(I2C_DELAY);
    scl_high();
    delayMicroseconds(I2C_DELAY);
    sda_high();
    delayMicroseconds(I2C_DELAY);
}

bool BitBangAHT21ELI::i2c_write_byte(uint8_t byte) {
    for (int i = 7; i >= 0; i--) {
        if (byte & (1 << i)) {
            sda_high();
        } else {
            sda_low();
        }
        delayMicroseconds(I2C_DELAY);
        scl_high();
        delayMicroseconds(I2C_DELAY);
        scl_low();
        delayMicroseconds(I2C_DELAY);
    }
    // Read ACK
    sda_high();
    delayMicroseconds(I2C_DELAY);
    scl_high();
    delayMicroseconds(I2C_DELAY);
    bool ack = (digitalRead(_sdaPin) == LOW);
    scl_low();
    delayMicroseconds(I2C_DELAY);
    
    return ack;
}

uint8_t BitBangAHT21ELI::i2c_read_byte(bool ack) {
    uint8_t byte = 0;
    sda_high();
    
    for (int i = 7; i >= 0; i--) {
        delayMicroseconds(I2C_DELAY);
        scl_high();
        delayMicroseconds(I2C_DELAY);
        if (digitalRead(_sdaPin)) {
            byte |= (1 << i);
        }
        scl_low();
    }
    
    // Send ACK/NACK
    if (ack) {
        sda_low();
    } else {
        sda_high();
    }
    delayMicroseconds(I2C_DELAY);
    scl_high();
    delayMicroseconds(I2C_DELAY);
    scl_low();
    delayMicroseconds(I2C_DELAY);
    sda_high();
    
    return byte;
}

// ----- I2C Scanner -----
void BitBangAHT21ELI::checkIfSensorIsConnected() {
    Serial.println("\nScanning I2C bus...");
    int devicesFound = 0;
    
    for (uint8_t addr = 1; addr < 127; addr++) {
        i2c_start();
        bool ack = i2c_write_byte(addr << 1); // Write address
        i2c_stop();
        
        if (ack) {
            Serial.print("Device found at address 0x");
            if (addr < 16) Serial.print("0");
            Serial.print(addr, HEX);
            Serial.println(" !");
            devicesFound++;
        }
        delay(1);
    }
    
    if (devicesFound == 0) {
        Serial.println("No I2C devices found!");
        Serial.println("\nCheck:");
        Serial.println("1. Wiring (SDA, SCL, VCC, GND)");
        Serial.println("2. Pull-up resistors (4.7k-10k on SDA and SCL)");
        Serial.println("3. Sensor power");
    } else {
        Serial.print("Found ");
        Serial.print(devicesFound);
        Serial.println(" device(s)");
    }
}

// ----- AHT21 functions -----
bool BitBangAHT21ELI::aht_write(uint8_t* data, uint8_t len) {
    i2c_start();
    
    if (!i2c_write_byte(AHTX0_I2CADDR_DEFAULT << 1)) { // Write address
        i2c_stop();
        Serial.println("No ACK on address write");
        return false;
    }
    
    for (uint8_t i = 0; i < len; i++) {
        if (!i2c_write_byte(data[i])) {
            i2c_stop();
            Serial.print("No ACK on data byte ");
            Serial.println(i);
            return false;
        }
    }
    
    i2c_stop();
    return true;
}

bool BitBangAHT21ELI::aht_read(uint8_t* data, uint8_t len) {
    i2c_start();
    
    if (!i2c_write_byte((AHTX0_I2CADDR_DEFAULT << 1) | 1)) { // Read address
        i2c_stop();
        Serial.println("No ACK on address read");
        return false;
    }
    
    for (uint8_t i = 0; i < len; i++) {
        data[i] = i2c_read_byte(i < (len - 1)); // ACK all but last byte
    }
    
    i2c_stop();
    return true;
}

uint8_t BitBangAHT21ELI::aht_get_status() {
    uint8_t ret;
    if (!aht_read(&ret, 1)) {
        return 0xFF;
    }
    return ret;
}

bool BitBangAHT21ELI::aht_init() {
    i2c_init();
    Serial.println("Bit-banged I2C initialized");
    Serial.print("SDA: GPIO ");
    Serial.println(_sdaPin);
    Serial.print("SCL: GPIO ");
    Serial.println(_sclPin);
    
    delay(100);
    checkIfSensorIsConnected();
    
    Serial.println("\nInitializing AHT21...");
    delay(40); // Power-up time

    // Soft reset
    uint8_t cmd = AHTX0_CMD_SOFTRESET;
    Serial.println("Sending soft reset...");
    if (!aht_write(&cmd, 1)) {
        Serial.println("Failed to send soft reset");
        return false;
    }
    delay(20);

    // Check status
    Serial.println("Reading status...");
    uint8_t status = aht_get_status();
    if (status == 0xFF) {
        Serial.println("Cannot read status!");
        return false;
    }
    
    Serial.print("Status: 0x");
    Serial.println(status, HEX);

    // Wait until not busy
    int timeout = 100;
    while ((aht_get_status() & AHTX0_STATUS_BUSY) && timeout > 0) {
        delay(10);
        timeout--;
    }

    // Send calibration
    uint8_t cal_cmd[3] = {AHTX0_CMD_CALIBRATE, 0x08, 0x00};
    Serial.println("Sending calibration...");
    if (!aht_write(cal_cmd, 3)) {
        Serial.println("Failed to send calibration");
        return false;
    }

    delay(10);

    // Wait for calibration
    timeout = 100;
    while ((aht_get_status() & AHTX0_STATUS_BUSY) && timeout > 0) {
        delay(10);
        timeout--;
    }

    status = aht_get_status();
    Serial.print("Final status: 0x");
    Serial.println(status, HEX);

    if (!(status & AHTX0_STATUS_CALIBRATED)) {
        Serial.println("Warning: Not calibrated");
    }

    Serial.println("Init complete!");
    return true;
}

bool BitBangAHT21ELI::aht_get_data(float* temperature, float* humidity) {
    // Trigger measurement
    uint8_t cmd[3] = {AHTX0_CMD_TRIGGER, 0x33, 0x00};
    if (!aht_write(cmd, 3)) {
        Serial.println("Failed to trigger");
        return false;
    }

    delay(80);

    // Wait until ready
    int timeout = 100;
    while ((aht_get_status() & AHTX0_STATUS_BUSY) && timeout > 0) {
        delay(10);
        timeout--;
    }

    if (timeout == 0) {
        Serial.println("Measurement timeout");
        return false;
    }

    // Read data
    uint8_t data[7];
    if (!aht_read(data, 7)) {
        Serial.println("Read failed");
        return false;
    }

    // Print raw data
    Serial.print("Raw: ");
    for(int i = 0; i < 7; i++) {
        if (data[i] < 0x10) Serial.print("0");
        Serial.print(data[i], HEX);
        Serial.print(" ");
    }
    Serial.println();

    // Convert humidity
    uint32_t h = data[1];
    h <<= 8;
    h |= data[2];
    h <<= 4;
    h |= data[3] >> 4;
    *humidity = ((float)h * 100.0) / 1048576.0;

    // Convert temperature
    uint32_t t = data[3] & 0x0F;
    t <<= 8;
    t |= data[4];
    t <<= 8;
    t |= data[5];
    *temperature = ((float)t * 200.0 / 1048576.0) - 50.0;

    return true;
}

