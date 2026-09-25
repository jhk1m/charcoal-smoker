// Serial bench console. Lets the bench session change the values that bring-up exists to measure
// (PWM frequency, min-start duty, max duty, PID gains, probe offset, setpoint) without a reflash,
// and run the state machine against a simulated probe when nothing is wired up yet.
//
// Runs inside the control task, which is the only writer to the blower and the state machine, so
// commands are applied directly with no queue. Input is polled non-blocking, one line at a time.
#pragma once

#include <cstdint>

#include "control/state_machine.h"
#include "hal/blower.h"
#include "hal/pit_sensor.h"
#include "storage/settings.h"

namespace ui {

struct ConsoleDeps {
    control::StateMachine* sm;
    hal::Blower* blower;
    hal::PitSensor* pit;
    storage::Settings* settings;
    storage::SettingsStore* store;
    control::Thresholds (*makeThresholds)(const storage::Settings&);  // main.cpp owns the mapping
    void (*setCookActive)(bool);
};

class Console {
public:
    void begin(const ConsoleDeps& d);

    // Call once per control tick; processes whatever complete lines have arrived.
    void poll();

    bool csvEnabled() const { return csv_; }

    // Simulated probe: when active the control loop uses this temperature instead of the sensor,
    // so the state machine can be exercised with nothing wired to the board.
    bool simActive() const { return sim_; }
    float simTempC() const { return sim_c_; }
    void simStep(float dt_s);

private:
    void handleLine(char* line);
    void help() const;
    void dump() const;

    ConsoleDeps d_{};
    char buf_[96] = {};
    uint8_t len_ = 0;
    bool csv_ = true;
    bool sim_ = false;
    float sim_c_ = 20.0f;
    float sim_rate_c_per_s_ = 0.0f;
};

}  // namespace ui
