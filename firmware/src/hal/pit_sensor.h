// Pit thermocouple front end. Two backends, selected at build time:
//   default            MAX31856 via Adafruit_MAX31856 1.2.8 (software SPI, 4-wire, configurable, fault register)
//   -DPIT_SENSOR_MAX31855   MAX31855 read directly over hardware SPI (3-wire read-only, 32-bit frame)
//
// MAX31856, VERIFIED against Adafruit_MAX31856.h (v1.2.8):
//   Adafruit_MAX31856(int8_t cs, int8_t mosi, int8_t miso, int8_t clk)
//   bool begin(); void setThermocoupleType(...); void setConversionMode(...); uint8_t readFault();
//   float readThermocoupleTemperature(); float readCJTemperature(); void setNoiseFilter(...)
//
// MAX31855 frame (Maxim MAX31855 datasheet, "Memory Map"): D31..D18 thermocouple temp, signed 14-bit, 0.25 C/LSB;
//   D16 fault; D15..D4 internal (cold junction) temp, signed 12-bit, 0.0625 C/LSB; D2 SCV (short to VCC);
//   D1 SCG (short to GND); D0 OC (open circuit). Read with CS low, SPI mode 0, up to 5 MHz.
#pragma once

#include "control/median_filter.h"
#include "control/state_machine.h"

#ifdef PIT_SENSOR_MAX31855
#include <SPI.h>
#else
#include <Adafruit_MAX31856.h>
#endif

namespace hal {

struct PitReading {
    float raw_c;
    float filtered_c;
    float cold_junction_c;
    uint8_t fault_bits;
    bool ok;
    control::Fault fault;  // mapped fault for the state machine
};

class PitSensor {
public:
    PitSensor(int cs, int mosi, int miso, int sck);

    bool begin(float min_valid_c, float max_valid_c);

    // Poll once per control tick. Non-blocking on both backends.
    PitReading read();

    // Calibration offset added to the raw reading before the validity checks and the filter.
    void setOffset(float c) { offset_c_ = c; }
    float offset() const { return offset_c_; }

    bool present() const { return present_; }
    static const char* backendName();

    // Incoming-inspection identity check (doc 09 section 2a). Re-probes the chip, then on the MAX31856 writes
    // thermocouple type J, reads it back, restores K: a relabeled MAX31855 has no writable registers and fails.
    // Also returns one raw sample so the caller can judge the shorted-input test (tc vs cold junction).
    struct SelfTest {
        bool present;
        bool has_register_test;  // false on the MAX31855 backend
        bool register_ok;        // J readback matched, K restored
        float tc_c;              // uncorrected thermocouple reading
        float cj_c;
        uint8_t fault_bits;
    };
    SelfTest selfTest();

private:
#ifdef PIT_SENSOR_MAX31855
    int cs_, mosi_, miso_, sck_;
    SPIClass* spi_ = nullptr;
    uint32_t readFrame();
#else
    Adafruit_MAX31856 dev_;
#endif
    control::MedianFilter<5> filter_;
    float offset_c_ = 0.0f;
    float min_valid_c_ = -20.0f;
    float max_valid_c_ = 400.0f;
    bool present_ = false;
};

}  // namespace hal
