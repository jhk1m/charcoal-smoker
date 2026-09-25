// Persistent settings in NVS via Arduino-ESP32 Preferences.
// VERIFIED: https://docs.espressif.com/projects/arduino-esp32/en/latest/tutorials/preferences.html
//   begin(const char* ns, bool readOnly); end(); putFloat/getFloat(key, default); putUChar/getUChar; putUInt/getUInt; isKey; clear
//   namespace and key names max 15 characters.
#pragma once

#include <cstdint>

#include "control/pid.h"

namespace storage {

struct Settings {
    float setpoint_c;
    control::PidGains gains;
    uint8_t blower_min_start_pct;
    uint32_t blower_pwm_freq_hz;
    uint8_t blower_max_pct;
    bool cook_active;      // set while a cook is running; on boot, resume the cook (doc 06 P8)
    float probe_offset_c;  // added to every pit reading; set from the ice / boiling check (doc 06 E14)
    bool lid_detect;       // lid detection enabled (turn off in high wind, per CyberQ and FireBoard advice)
};

class SettingsStore {
public:
    // Loads from NVS, filling any missing key with the compile-time default from config.h.
    Settings load();
    void save(const Settings& s);
    void reset();  // clear namespace

private:
    static constexpr const char* NS = "smoker";  // <= 15 chars
};

}  // namespace storage
