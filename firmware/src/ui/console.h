// Serial bench console. Lets the bench session change the values that bring-up exists to measure
// (PWM frequency, min-start duty, max duty, PID gains, probe offset, setpoint) without a reflash,
// and run the state machine against a simulated probe when nothing is wired up yet.
//
// Runs inside the control task, which is the only writer to the blower and the state machine, so
// commands are applied directly with no queue. Input is polled non-blocking, one line at a time.
#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <cstdint>

#include "control/fan_monitor.h"
#include "control/state_machine.h"
#include "hal/blower.h"
#include "hal/pit_sensor.h"
#include "storage/settings.h"

namespace ui {

struct ConsoleDeps {
    control::StateMachine* sm;
    control::FanMonitor* fan;
    hal::Blower* blower;
    hal::PitSensor* pit;
    storage::Settings* settings;
    storage::SettingsStore* store;
    control::Thresholds (*makeThresholds)(const storage::Settings&);  // main.cpp owns the mapping
    void (*setCookActive)(bool);
    void (*remoteOut)(const char*, size_t);  // console replies for a remote (BLE) line; nullptr = serial only
};

class Console {
public:
    void begin(const ConsoleDeps& d);

    // Call once per control tick; processes whatever complete lines have arrived (serial, then injected).
    void poll();

    // Hand in a complete line from another task (BLE). Queued, run inside poll() on the control task; replies go
    // to serial AND to remoteOut. Returns false (line dropped) when the queue of 4 is full.
    bool inject(const char* line);

    bool csvEnabled() const { return csv_; }

    // Simulated probe: when active the control loop uses this temperature instead of the sensor,
    // so the state machine can be exercised with nothing wired to the board.
    bool simActive() const { return sim_; }
    float simTempC() const { return sim_c_; }
    void simStep(float dt_s);

private:
    void handleLine(char* line);
    size_t emit(const uint8_t* b, size_t n);  // every console reply goes through here

    // Print target for the console. Mirrors to Serial; while a remote line runs, also to remoteOut.
    struct Out : public Print {
        Console* c = nullptr;
        size_t write(uint8_t b) override { return c->emit(&b, 1); }
        size_t write(const uint8_t* b, size_t n) override { return c->emit(b, n); }
    } mutable out_;  // help() and dump() are const
    bool remote_ = false;
    QueueHandle_t inject_q_ = nullptr;
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
