#include <Arduino.h>

class BitBangAHT21ELI {
    public:
        BitBangAHT21ELI(int sdaPin, int sclPin);
        bool aht_get_data(float* temperature, float* humidity);
        void checkIfSensorIsConnected();
    private:
        int _sdaPin;
        int _sclPin;
        void i2c_init();
        void sda_low();
        void sda_high();
        void scl_low();
        void scl_high();
        void i2c_start();
        void i2c_stop();
        bool i2c_write_byte(uint8_t byte);
        bool aht_init();
        uint8_t i2c_read_byte(bool ack);
        bool aht_write(uint8_t* data, uint8_t len);
        bool aht_read(uint8_t* data, uint8_t len);
        uint8_t aht_get_status();
};

