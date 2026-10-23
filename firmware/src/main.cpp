// charcoal-smoker firmware entry point.
// One FreeRTOS control task at 1 Hz owns the sensor, state machine and blower. BLE callbacks run on the
// NimBLE host task and post commands into a queue; the control task drains it each tick. Nothing but the
// control task ever writes to the blower.

#include <Arduino.h>
#include <algorithm>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include "ble/ble_service.h"
#include "config.h"
#include "control/fan_monitor.h"
#include "control/state_machine.h"
#include "hal/blower.h"
#include "hal/fan_tach.h"
#include "hal/pit_sensor.h"
#include "hal/watchdog.h"
#include "log/event_log.h"
#include "pins.h"
#include "storage/settings.h"
#include "ui/console.h"

#ifndef FW_VERSION
#define FW_VERSION "dev"
#endif

namespace {

// ---------------------------------------------------------------- command queue (BLE -> control task)
enum class CmdType : uint8_t { SETPOINT, MODE, MANUAL_DUTY, TIMER };
struct Cmd {
    CmdType type;
    float f;
    uint8_t u;
};
QueueHandle_t g_cmd_queue = nullptr;

// ---------------------------------------------------------------- singletons
storage::SettingsStore g_store;
storage::Settings g_settings;
hal::Blower g_blower;
hal::FanTach g_tach;
hal::PitSensor g_pit(pins::TC_CS, pins::TC_MOSI, pins::TC_MISO, pins::TC_SCK);
ble::Service g_ble;
evlog::EventLog<cfg::EVENT_LOG_CAPACITY> g_log;
ui::Console g_console;
portMUX_TYPE g_log_mux = portMUX_INITIALIZER_UNLOCKED;

const char* const CSV_HEADER =
    "t_s,state,fault,pit_raw_c,pit_filt_c,cj_c,duty_cmd,duty_applied,tc_fault_bits,warnings,out_ema,rate_c_s,fan_rpm";

control::Thresholds makeThresholds(const storage::Settings& s) {
    control::Thresholds t{};
    t.ignite_exit_margin_c = cfg::IGNITE_EXIT_MARGIN_C;
    t.ignite_duty_pct = cfg::IGNITE_DUTY_PCT;
    t.ignite_timeout_s = cfg::IGNITE_TIMEOUT_S;
    t.ignite_min_rise_c = cfg::IGNITE_MIN_RISE_C;
    t.startup_max_duty_pct = std::min<uint8_t>(cfg::STARTUP_MAX_DUTY_PCT, s.blower_max_pct);
    t.max_duty_pct = s.blower_max_pct;
    t.hold_band_c = cfg::HOLD_BAND_C;
    t.hold_settle_s = cfg::HOLD_SETTLE_S;
    t.recovery_max_s = cfg::RECOVERY_MAX_S;
    t.lid_open_drop_c_per_s = cfg::LID_OPEN_DROP_C_PER_S;
    t.lid_open_total_drop_c = cfg::LID_OPEN_TOTAL_DROP_C;
    t.lid_rate_window_s = cfg::LID_RATE_WINDOW_S;
    t.lid_open_min_s = cfg::LID_OPEN_MIN_S;
    t.lid_open_timeout_s = cfg::LID_OPEN_TIMEOUT_S;
    t.lid_open_duty_pct = cfg::LID_OPEN_DUTY_PCT;
    t.lid_open_recover_band_c = cfg::LID_OPEN_RECOVER_BAND_C;
    t.lid_exit_rise_above_low_c = cfg::LID_EXIT_RISE_ABOVE_LOW_C;
    t.lid_exit_plateau_c_per_s = cfg::LID_EXIT_PLATEAU_C_PER_S;
    t.lid_guard_out_ema_pct = cfg::LID_GUARD_OUT_EMA_PCT;
    t.lid_rising_rate_c_per_s = cfg::LID_RISING_RATE_C_PER_S;
    t.lid_rising_window_s = cfg::LID_RISING_WINDOW_S;
    t.lid_rising_duty_pct = cfg::LID_RISING_DUTY_PCT;
    t.out_ema_alpha = cfg::OUT_EMA_ALPHA;
    t.probe_ema_alpha = cfg::PROBE_EMA_ALPHA;
    t.fuel_low_fall_c = cfg::FUEL_LOW_FALL_C;
    t.fuel_low_window_s = cfg::FUEL_LOW_WINDOW_S;
    t.fuel_low_out_pct = cfg::FUEL_LOW_OUT_PCT;
    t.fuel_low_clear_s = cfg::FUEL_LOW_CLEAR_S;
    t.fire_out_below_sp_c = cfg::FIRE_OUT_BELOW_SP_C;
    t.fire_out_s = cfg::FIRE_OUT_S;
    t.pit_alarm_dev_c = cfg::PIT_ALARM_DEV_C;
    t.pit_alarm_hyst_c = cfg::PIT_ALARM_HYST_C;
    t.creep_above_sp_c = cfg::CREEP_ABOVE_SP_C;
    t.creep_s = cfg::CREEP_S;
    t.pit_over_margin_c = cfg::PIT_OVER_MARGIN_C;
    t.pit_soft_limit_c = cfg::PIT_SOFT_LIMIT_C;
    return t;
}

void setCookActive(bool active) {
    if (g_settings.cook_active == active) return;
    g_settings.cook_active = active;
    g_store.save(g_settings);
}

void logEvent(evlog::EventType type, uint8_t a, uint8_t b, float value) {
    evlog::Event e{};
    e.uptime_s = millis() / 1000;
    e.type = type;
    e.a = a;
    e.b = b;
    e.value = value;
    portENTER_CRITICAL(&g_log_mux);
    g_log.push(e);
    portEXIT_CRITICAL(&g_log_mux);
}

bool popEvent(evlog::Event& out) {
    portENTER_CRITICAL(&g_log_mux);
    const bool ok = g_log.pop(out);
    portEXIT_CRITICAL(&g_log_mux);
    return ok;
}

void post(Cmd c) {
    if (g_cmd_queue) xQueueSend(g_cmd_queue, &c, 0);
}

// ---------------------------------------------------------------- control task
void controlTask(void*) {
    control::StateMachine sm(makeThresholds(g_settings), g_settings.gains);
    sm.setSetpoint(g_settings.setpoint_c);
    sm.setLidDetectEnabled(g_settings.lid_detect);

    // Fan tach supervision (4-wire). The legacy 2-wire blower has no tach, so the checks stay off for it.
    control::FanMonitor fan_mon(
        control::FanLimits{cfg::FAN_RATED_RPM, cfg::FAN_RUNAWAY_MARGIN_PCT, cfg::FAN_STALL_S, cfg::FAN_RUNAWAY_S},
        g_settings.fan_tach_ppr);
    fan_mon.setEnabled(cfg::BLOWER_4WIRE);

    ui::ConsoleDeps cd{};
    cd.sm = &sm;
    cd.fan = &fan_mon;
    cd.blower = &g_blower;
    cd.pit = &g_pit;
    cd.settings = &g_settings;
    cd.store = &g_store;
    cd.makeThresholds = &makeThresholds;
    cd.setCookActive = &setCookActive;
    cd.remoteOut = [](const char* t, size_t n) { g_ble.publishConsole(t, n); };
    g_console.begin(cd);

    // Power-loss resume (doc 06 P8): a cook that was running when power dropped restarts through IGNITE,
    // which exits to RAMP at once if the pit is still hot and enforces the startup cap either way.
    if (g_settings.cook_active) {
        sm.startCook(g_settings.setpoint_c);
        logEvent(evlog::EventType::POWER_RESUME, 0, 0, g_settings.setpoint_c);
        log_w("cook_active set at boot: resuming cook at %.1f C", g_settings.setpoint_c);
    }

    if (hal::Watchdog::lastResetWasWatchdog()) {
        logEvent(evlog::EventType::FAULT, static_cast<uint8_t>(control::Fault::WATCHDOG_RESET), 0, 0.0f);
        log_e("Previous reset was a watchdog reset");
    }
    if (!hal::Watchdog::begin(cfg::WATCHDOG_TIMEOUT_MS)) log_e("TWDT init failed");

    // Reprint the banner from the control task: setup() prints are lost if the terminal attaches late.
    Serial.printf("# charcoal-smoker fw %s hw=%s tc=%s. Type ? for help.\n", FW_VERSION, pins::HW_NAME,
                  hal::PitSensor::backendName());
    Serial.println(CSV_HEADER);

    control::State last_state = sm.state();
    control::Fault last_fault = sm.fault();
    uint8_t last_warn = 0;
    uint16_t last_timer_min = 0xFFFF;
    uint8_t last_duty = 255;
    uint32_t last_tick_ms = millis();
    uint32_t sample_counter = 0;

    TickType_t wake = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(cfg::CONTROL_PERIOD_MS);
    const TickType_t slice = pdMS_TO_TICKS(cfg::BLOWER_SERVICE_MS);

    for (;;) {
        // Sleep to the next tick in short slices, servicing the blower each time, so the start kick ends at
        // BLOWER_KICK_MS instead of at the next 1 Hz tick. Single task, so the driver needs no locking.
        const TickType_t tick_end = wake + period;
        while (static_cast<int32_t>(xTaskGetTickCount() - tick_end) < 0) {
            vTaskDelay(slice);
            g_blower.service(millis());
        }
        wake = tick_end;
        hal::Watchdog::feed();
        const uint32_t now = millis();
        const float dt_s = (now - last_tick_ms) / 1000.0f;
        last_tick_ms = now;

        // 1. Bench console, then BLE commands. Both run in this task, so they may touch the state machine.
        g_console.poll();

        Cmd c;
        while (xQueueReceive(g_cmd_queue, &c, 0) == pdTRUE) {
            switch (c.type) {
                case CmdType::SETPOINT:
                    sm.setSetpoint(c.f);
                    g_settings.setpoint_c = c.f;
                    g_store.save(g_settings);
                    g_ble.publishSetpoint(c.f);
                    logEvent(evlog::EventType::SETPOINT_CHANGE, 0, 0, c.f);
                    break;
                case CmdType::MODE:
                    // 0 = auto cook, 1 = manual, 2 = stop, 3 = clear fault
                    if (c.u == 0) { sm.startCook(sm.setpoint()); setCookActive(sm.state() == control::State::IGNITE); }
                    else if (c.u == 1) { sm.enterManual(0); setCookActive(false); }
                    else if (c.u == 2) { sm.stop(); setCookActive(false); }
                    else if (c.u == 3) sm.clearFault();
                    logEvent(evlog::EventType::MODE_CHANGE, c.u, 0, 0.0f);
                    break;
                case CmdType::MANUAL_DUTY:
                    if (sm.state() == control::State::MANUAL) sm.setManualDuty(c.u);
                    break;
                case CmdType::TIMER:
                    sm.setTimer(static_cast<uint32_t>(c.f) * 60u);
                    break;
            }
        }

        // 2. Sample. The bench console can substitute a simulated probe so the state machine can be
        //    exercised with nothing wired to the board.
        hal::PitReading r = g_pit.read();
        if (g_console.simActive()) {
            g_console.simStep(dt_s);
            r.raw_c = g_console.simTempC();
            r.filtered_c = r.raw_c;
            r.ok = true;
            r.fault = control::Fault::NONE;
            r.fault_bits = 0;
        }
        control::Sample s{r.filtered_c, r.ok, r.fault, dt_s};

        // 2b. Fan tach (4-wire). The pulses counted since the previous tick are judged against the duty the
        //     fan was given over that interval. A fault latches FAULT here, and step 4 then cuts the fan supply.
        //     An unreadable counter reads as 0 pulses, which is a stall: fail safe.
        if (cfg::BLOWER_4WIRE) {
            uint32_t tach_pulses = 0;
            if (!g_tach.take(tach_pulses)) tach_pulses = 0;
            sm.raiseFault(fan_mon.update(g_blower.armed(), g_blower.appliedPercent(), tach_pulses, dt_s));
        }

        // 3. Step the state machine.
        const control::Command cmd = sm.tick(s);

        // 4. Actuate. FAULT/IDLE always force OFF regardless of cmd.duty_pct.
        if (cmd.state == control::State::FAULT || cmd.state == control::State::IDLE) g_blower.off();
        else g_blower.setDutyPercent(cmd.duty_pct, dt_s);
        if (cmd.state == control::State::FAULT) setCookActive(false);
        g_ble.service(now);

        // 5. Publish.
        g_ble.publishPit(r.filtered_c);
        if (g_blower.commandedPercent() != last_duty) {
            last_duty = g_blower.commandedPercent();
            g_ble.publishDuty(last_duty);
        }
        if (cmd.state != last_state) {
            last_state = cmd.state;
            g_ble.publishState(cmd.state);
            logEvent(evlog::EventType::STATE_CHANGE, static_cast<uint8_t>(cmd.state), last_duty, r.filtered_c);
            log_i("state -> %s", control::toString(cmd.state));
        }
        if (cmd.fault != last_fault) {
            last_fault = cmd.fault;
            g_ble.publishFault(cmd.fault);
            logEvent(evlog::EventType::FAULT, static_cast<uint8_t>(cmd.fault), 0, r.filtered_c);
            log_w("fault -> %s (bits 0x%02x)", control::toString(cmd.fault), r.fault_bits);
        }
        // Timer: minutes left, rounded up, so a reader sees the countdown and 0 when it is done or unset. Until
        // 2026-10-22 the read value stayed at whatever was last WRITTEN, so a phone showed a timer that was not running.
        const uint16_t timer_min = static_cast<uint16_t>((sm.timerS() + 59u) / 60u);
        if (timer_min != last_timer_min) {
            last_timer_min = timer_min;
            g_ble.publishTimer(timer_min);
        }
        if (cmd.warnings != last_warn) {
            last_warn = cmd.warnings;
            g_ble.publishWarnings(cmd.warnings);
            logEvent(evlog::EventType::WARNING, cmd.warnings, static_cast<uint8_t>(cmd.state), r.filtered_c);
            log_w("warnings -> 0x%02x", cmd.warnings);
        }
        if (++sample_counter % 10 == 0) {  // one telemetry record every 10 s
            logEvent(evlog::EventType::SAMPLE, last_duty, static_cast<uint8_t>(cmd.state), r.filtered_c);
        }

        // Serial telemetry (CSV) for bench logging / step-response capture. 'v' on the console mutes it.
        //    The same line goes out on BLE 000e whenever a peer is connected; 'v' mutes serial only.
        char csv[160];
        snprintf(csv, sizeof(csv), "%lu,%s,%s,%.2f,%.2f,%.2f,%u,%u,0x%02x,0x%02x,%.1f,%.3f,%.0f\n", static_cast<unsigned long>(now / 1000),
                 control::toString(cmd.state), control::toString(cmd.fault), r.raw_c, r.filtered_c,
                 r.cold_junction_c, g_blower.commandedPercent(), g_blower.appliedPercent(), r.fault_bits,
                 cmd.warnings, sm.outputEma(), sm.rateCPerS(), fan_mon.rpm());
        if (g_console.csvEnabled()) Serial.print(csv);
        g_ble.publishTelemetry(csv);
    }
}

}  // namespace

// ---------------------------------------------------------------- Arduino entry points
void setup() {
    Serial.begin(115200);
#if ARDUINO_USB_CDC_ON_BOOT
    Serial.setTxTimeoutMs(0);   // never block the control loop when no terminal is attached
    for (uint32_t t0 = millis(); !Serial && millis() - t0 < 2000;) delay(10);
#endif
    delay(200);
    Serial.println();
    Serial.printf("charcoal-smoker fw %s, reset reason %d\n", FW_VERSION, static_cast<int>(esp_reset_reason()));
    Serial.println(CSV_HEADER);

    g_settings = g_store.load();

    // Blower first, forced OFF (4-wire: fan supply cut), before anything else can fail.
    if (!g_blower.begin(pins::BLOWER_PWM, g_settings.blower_pwm_freq_hz, cfg::BLOWER_PWM_RES_BITS,
                        g_settings.blower_min_start_pct, cfg::BLOWER_MIN_RUN_DUTY_PCT, g_settings.blower_max_pct, cfg::BLOWER_KICK_MS,
                        cfg::BLOWER_LONG_PWM_PERIOD_S, cfg::BLOWER_LONG_PWM_MIN_ON_S, cfg::BLOWER_4WIRE,
                        cfg::BLOWER_4WIRE ? pins::FAN_SUPPLY_EN : -1)) {
        log_e("LEDC attach failed on GPIO%d", pins::BLOWER_PWM);
    }
    g_blower.off();

    // Without a tach every cook stops on BLOWER_STALL, which is the safe outcome for a fan that cannot be watched.
    if (cfg::BLOWER_4WIRE && !g_tach.begin(pins::FAN_TACH, cfg::FAN_TACH_GLITCH_NS)) {
        log_e("fan tach counter failed to start on GPIO%d", pins::FAN_TACH);
    }

    g_pit.setOffset(g_settings.probe_offset_c);
    if (!g_pit.begin(cfg::PIT_SENSOR_MIN_VALID_C, cfg::PIT_SENSOR_MAX_VALID_C)) {
        log_e("%s not detected on CS GPIO%d", hal::PitSensor::backendName(), pins::TC_CS);
        Serial.println("# no pit sensor: use the console command 't 25' to run on a simulated probe");
    }

    g_cmd_queue = xQueueCreate(8, sizeof(Cmd));

    ble::Handlers h;
    h.on_setpoint = [](float c) { post(Cmd{CmdType::SETPOINT, c, 0}); };
    h.on_mode = [](uint8_t m) { post(Cmd{CmdType::MODE, 0.0f, m}); };
    h.on_manual_duty = [](uint8_t d) { post(Cmd{CmdType::MANUAL_DUTY, 0.0f, d}); };
    h.on_timer = [](uint16_t min) { post(Cmd{CmdType::TIMER, static_cast<float>(min), 0}); };
    h.on_log_pop = popEvent;
    h.on_console = [](const char* line) { g_console.inject(line); };

    char info[64];
    snprintf(info, sizeof(info), "smoker-ctrl fw=%s hw=%s tc=%s", FW_VERSION, pins::HW_NAME, hal::PitSensor::backendName());
    if (!g_ble.begin(cfg::BLE_DEVICE_NAME, cfg::BLE_PASSKEY, h, info)) log_e("BLE init failed");
    // Phase 1 bench: no pairing button yet, so open the pairing window at boot.
    g_ble.openPairingWindow(cfg::BLE_PAIRING_WINDOW_MS);
    g_ble.publishSetpoint(g_settings.setpoint_c);

    logEvent(evlog::EventType::BOOT, 0, 0, 0.0f);

    // Control task on core 1 (Arduino loop core), above default priority.
    xTaskCreatePinnedToCore(controlTask, "control", 8192, nullptr, 3, nullptr, 1);
}

void loop() {
    // Everything runs in controlTask. Keep loop() idle.
    vTaskDelay(pdMS_TO_TICKS(1000));
}
