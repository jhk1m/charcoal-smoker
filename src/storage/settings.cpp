#include "settings.h"

#include <Preferences.h>

#include "config.h"

namespace storage {

// Keys (<= 15 chars)
static constexpr const char* K_SETPOINT = "setpoint_c";
static constexpr const char* K_KP = "pid_kp";
static constexpr const char* K_KI = "pid_ki";
static constexpr const char* K_KD = "pid_kd";
static constexpr const char* K_MIN_START = "blw_min_pct";
static constexpr const char* K_FREQ = "blw_freq_hz";
static constexpr const char* K_MAX = "blw_max_pct";
static constexpr const char* K_COOK = "cook_active";
static constexpr const char* K_OFFSET = "probe_off_c";
static constexpr const char* K_LID = "lid_detect";

Settings SettingsStore::load() {
    Settings s{};
    Preferences p;
    p.begin(NS, /*readOnly=*/true);
    s.setpoint_c = p.getFloat(K_SETPOINT, cfg::SETPOINT_DEFAULT_C);
    s.gains.kp = p.getFloat(K_KP, cfg::PID_KP_DEFAULT);
    s.gains.ki = p.getFloat(K_KI, cfg::PID_KI_DEFAULT);
    s.gains.kd = p.getFloat(K_KD, cfg::PID_KD_DEFAULT);
    s.blower_min_start_pct = p.getUChar(K_MIN_START, cfg::BLOWER_MIN_START_DUTY_PCT);
    s.blower_pwm_freq_hz = p.getUInt(K_FREQ, cfg::BLOWER_PWM_FREQ_HZ);
    s.blower_max_pct = p.getUChar(K_MAX, cfg::BLOWER_MAX_DUTY_PCT);
    s.cook_active = p.getUChar(K_COOK, 0) != 0;
    s.probe_offset_c = p.getFloat(K_OFFSET, 0.0f);
    s.lid_detect = p.getUChar(K_LID, 1) != 0;
    p.end();

    // Clamp anything a previous firmware may have stored out of range.
    if (s.setpoint_c < cfg::SETPOINT_MIN_C || s.setpoint_c > cfg::SETPOINT_MAX_C) s.setpoint_c = cfg::SETPOINT_DEFAULT_C;
    if (s.blower_max_pct > 100) s.blower_max_pct = 100;
    if (s.blower_min_start_pct >= s.blower_max_pct) s.blower_min_start_pct = cfg::BLOWER_MIN_START_DUTY_PCT;
    if (s.probe_offset_c < -20.0f || s.probe_offset_c > 20.0f) s.probe_offset_c = 0.0f;
    return s;
}

void SettingsStore::save(const Settings& s) {
    Preferences p;
    p.begin(NS, /*readOnly=*/false);
    p.putFloat(K_SETPOINT, s.setpoint_c);
    p.putFloat(K_KP, s.gains.kp);
    p.putFloat(K_KI, s.gains.ki);
    p.putFloat(K_KD, s.gains.kd);
    p.putUChar(K_MIN_START, s.blower_min_start_pct);
    p.putUInt(K_FREQ, s.blower_pwm_freq_hz);
    p.putUChar(K_MAX, s.blower_max_pct);
    p.putUChar(K_COOK, s.cook_active ? 1 : 0);
    p.putFloat(K_OFFSET, s.probe_offset_c);
    p.putUChar(K_LID, s.lid_detect ? 1 : 0);
    p.end();
}

void SettingsStore::reset() {
    Preferences p;
    p.begin(NS, false);
    p.clear();
    p.end();
}

}  // namespace storage
