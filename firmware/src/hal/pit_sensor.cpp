#include "pit_sensor.h"

#include <Arduino.h>
#include <cmath>

namespace hal {

#ifdef PIT_SENSOR_MAX31855
// ------------------------------------------------------------------ MAX31855 backend

PitSensor::PitSensor(int cs, int mosi, int miso, int sck) : cs_(cs), mosi_(mosi), miso_(miso), sck_(sck) {}

const char* PitSensor::backendName() { return "MAX31855"; }

bool PitSensor::begin(float min_valid_c, float max_valid_c) {
    min_valid_c_ = min_valid_c;
    max_valid_c_ = max_valid_c;
    pinMode(cs_, OUTPUT);
    digitalWrite(cs_, HIGH);
    spi_ = &SPI;
    spi_->begin(sck_, miso_, mosi_, cs_);
    // Presence test: an unpowered or missing chip leaves MISO floating; all-ones or all-zeros frames are rejected.
    uint32_t f = readFrame();
    present_ = (f != 0xFFFFFFFFu) && (f != 0x00000000u);
    filter_.reset();
    return present_;
}

uint32_t PitSensor::readFrame() {
    spi_->beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
    digitalWrite(cs_, LOW);
    delayMicroseconds(1);
    uint32_t v = 0;
    for (int i = 0; i < 4; ++i) v = (v << 8) | spi_->transfer(0x00);
    digitalWrite(cs_, HIGH);
    spi_->endTransaction();
    return v;
}

PitReading PitSensor::read() {
    PitReading r{};
    if (!present_) {
        r.ok = false;
        r.fault = control::Fault::SENSOR_OPEN;
        return r;
    }
    uint32_t f = readFrame();
    r.fault_bits = static_cast<uint8_t>(f & 0x07u);  // bit0 OC, bit1 SCG, bit2 SCV
    int16_t tc = static_cast<int16_t>((f >> 18) & 0x3FFFu);
    if (tc & 0x2000) tc |= static_cast<int16_t>(0xC000);  // sign-extend 14-bit
    int16_t cj = static_cast<int16_t>((f >> 4) & 0x0FFFu);
    if (cj & 0x0800) cj |= static_cast<int16_t>(0xF000);  // sign-extend 12-bit
    r.raw_c = tc * 0.25f + offset_c_;
    r.cold_junction_c = cj * 0.0625f;

    const bool fault_flag = (f >> 16) & 0x1u;
    if (fault_flag && (r.fault_bits & 0x01u)) {
        r.ok = false;
        r.fault = control::Fault::SENSOR_OPEN;
    } else if (fault_flag && (r.fault_bits & 0x06u)) {
        r.ok = false;
        r.fault = control::Fault::SENSOR_SHORT;
    } else if (fault_flag || f == 0xFFFFFFFFu || r.raw_c < min_valid_c_ || r.raw_c > max_valid_c_) {
        r.ok = false;
        r.fault = control::Fault::SENSOR_RANGE;
    } else {
        r.ok = true;
        r.fault = control::Fault::NONE;
        filter_.push(r.raw_c);
    }
    r.filtered_c = filter_.ready() ? filter_.value() : r.raw_c;
    return r;
}

PitSensor::SelfTest PitSensor::selfTest() {
    SelfTest t{};
    t.has_register_test = false;  // read-only chip, nothing to write
    begin(min_valid_c_, max_valid_c_);  // always re-probe: a chip unpowered after boot must read NOT DETECTED
    t.present = present_;
    if (!present_) return t;
    const uint32_t f = readFrame();
    t.fault_bits = static_cast<uint8_t>(f & 0x07u);
    int16_t tc = static_cast<int16_t>((f >> 18) & 0x3FFFu);
    if (tc & 0x2000) tc |= static_cast<int16_t>(0xC000);
    int16_t cj = static_cast<int16_t>((f >> 4) & 0x0FFFu);
    if (cj & 0x0800) cj |= static_cast<int16_t>(0xF000);
    t.tc_c = tc * 0.25f;
    t.cj_c = cj * 0.0625f;
    return t;
}

#else
// ------------------------------------------------------------------ MAX31856 backend

PitSensor::PitSensor(int cs, int mosi, int miso, int sck)
    : dev_(static_cast<int8_t>(cs), static_cast<int8_t>(mosi), static_cast<int8_t>(miso), static_cast<int8_t>(sck)) {}

const char* PitSensor::backendName() { return "MAX31856"; }

bool PitSensor::begin(float min_valid_c, float max_valid_c) {
    min_valid_c_ = min_valid_c;
    max_valid_c_ = max_valid_c;
    present_ = dev_.begin();
    if (!present_) return false;
    dev_.setThermocoupleType(MAX31856_TCTYPE_K);
    // Adafruit begin() returns true with no chip answering. An unpowered MAX31856 reads every register as
    // 0x00 (seen 2026-10-04, doc 05), so require the type K just written to read back before calling it present.
    present_ = dev_.getThermocoupleType() == MAX31856_TCTYPE_K;
    if (!present_) return false;
    dev_.setNoiseFilter(MAX31856_NOISE_FILTER_60HZ);  // US mains
    dev_.setConversionMode(MAX31856_CONTINUOUS);
    filter_.reset();
    return true;
}

PitReading PitSensor::read() {
    PitReading r{};
    if (!present_) {
        r.ok = false;
        r.fault = control::Fault::SENSOR_OPEN;
        return r;
    }

    r.fault_bits = dev_.readFault();
    float tc_c = dev_.readThermocoupleTemperature();
    r.cold_junction_c = dev_.readCJTemperature();

    // The chip resets when the fan supply switches on (bench 2026-10-27): it comes back at power-on defaults with
    // conversions off and reads 0.00 C on both channels. Re-run begin() and wait one conversion before calling it
    // open; an unpowered chip fails begin()'s type-K readback and still faults below.
    if (r.fault_bits == 0 && tc_c == 0.0f && r.cold_junction_c == 0.0f && begin(min_valid_c_, max_valid_c_)) {
        log_w("MAX31856 read all zero, re-initialised");
        delay(200);  // first continuous conversion, 60 Hz filter; well inside the 5 s watchdog
        r.fault_bits = dev_.readFault();
        tc_c = dev_.readThermocoupleTemperature();
        r.cold_junction_c = dev_.readCJTemperature();
    }
    r.raw_c = tc_c + offset_c_;

    // A MAX31856 that loses power after boot reads every register as 0x00: no fault bit, 0.00 C on both
    // channels (bench 2026-10-10). A live chip never reads exactly 0.000 on both, so treat that as open.
    if (r.fault_bits == 0 && tc_c == 0.0f && r.cold_junction_c == 0.0f) {
        present_ = false;
        r.ok = false;
        r.fault = control::Fault::SENSOR_OPEN;
    } else if (r.fault_bits & MAX31856_FAULT_OPEN) {
        r.ok = false;
        r.fault = control::Fault::SENSOR_OPEN;
    } else if (r.fault_bits & MAX31856_FAULT_OVUV) {
        r.ok = false;
        r.fault = control::Fault::SENSOR_SHORT;
    } else if ((r.fault_bits & (MAX31856_FAULT_TCRANGE | MAX31856_FAULT_CJRANGE)) || std::isnan(r.raw_c) ||
               r.raw_c < min_valid_c_ || r.raw_c > max_valid_c_) {
        r.ok = false;
        r.fault = control::Fault::SENSOR_RANGE;
    } else {
        r.ok = true;
        r.fault = control::Fault::NONE;
        filter_.push(r.raw_c);
    }

    r.filtered_c = filter_.ready() ? filter_.value() : r.raw_c;
    return r;
}

PitSensor::SelfTest PitSensor::selfTest() {
    SelfTest t{};
    t.has_register_test = true;
    begin(min_valid_c_, max_valid_c_);  // always re-probe: a chip unpowered after boot must read NOT DETECTED
    t.present = present_;
    if (!present_) return t;
    // Register readback: CR1 thermocouple-type field is writable on a real MAX31856 only.
    dev_.setThermocoupleType(MAX31856_TCTYPE_J);
    const bool j_ok = dev_.getThermocoupleType() == MAX31856_TCTYPE_J;
    dev_.setThermocoupleType(MAX31856_TCTYPE_K);
    const bool k_ok = dev_.getThermocoupleType() == MAX31856_TCTYPE_K;
    t.register_ok = j_ok && k_ok;
    delay(200);  // continuous mode: let one conversion complete with type K restored
    t.fault_bits = dev_.readFault();
    t.tc_c = dev_.readThermocoupleTemperature();
    t.cj_c = dev_.readCJTemperature();
    return t;
}
#endif

}  // namespace hal
