// Native unit tests for the pure-C++ control core. Run: pio test -e native
#include <unity.h>

#include <algorithm>

#include "control/fan_monitor.h"
#include "control/long_pwm.h"
#include "control/median_filter.h"
#include "control/pid.h"
#include "control/state_machine.h"

using namespace control;

static Thresholds bench_thresholds() {
    Thresholds t{};
    t.ignite_exit_margin_c = 15.0f;
    t.ignite_duty_pct = 50;
    t.ignite_timeout_s = 600;
    t.ignite_min_rise_c = 10.0f;
    t.startup_max_duty_pct = 50;
    t.max_duty_pct = 100;
    t.hold_band_c = 5.0f;
    t.hold_settle_s = 10;
    t.recovery_max_s = 600;
    t.lid_open_drop_c_per_s = 0.3f;
    t.lid_open_total_drop_c = 0.0f;
    t.lid_rate_window_s = 10;
    t.lid_open_min_s = 30;
    t.lid_open_timeout_s = 240;
    t.lid_open_duty_pct = 0;
    t.lid_open_recover_band_c = 0.0f;
    t.lid_exit_rise_above_low_c = 11.0f;
    t.lid_exit_plateau_c_per_s = 0.06f;
    t.lid_guard_out_ema_pct = 90;
    t.lid_rising_rate_c_per_s = 0.3f;
    t.lid_rising_window_s = 20;
    t.lid_rising_duty_pct = 10;
    t.out_ema_alpha = 2.0f / 241.0f;
    t.probe_ema_alpha = 2.0f / 61.0f;
    t.fuel_low_fall_c = 5.0f;
    t.fuel_low_window_s = 600;
    t.fuel_low_out_pct = 90;
    t.fuel_low_clear_s = 300;
    t.fire_out_below_sp_c = 40.0f;
    t.fire_out_s = 900;
    t.pit_alarm_dev_c = 14.0f;
    t.pit_alarm_hyst_c = 1.0f;
    t.creep_above_sp_c = 14.0f;
    t.creep_s = 900;
    t.pit_over_margin_c = 40.0f;
    t.pit_soft_limit_c = 240.0f;
    return t;
}

static Sample ok(float c, float dt = 1.0f) { return Sample{c, true, Fault::NONE, dt}; }

static void expectState(const Command& c, State s) {
    TEST_ASSERT_EQUAL(static_cast<int>(s), static_cast<int>(c.state));
}

// Drive a machine into HOLD at `sp` with the pit sitting at `sp`. Returns the last command.
static Command settleInHold(StateMachine& sm, float sp) {
    sm.startCook(sp);
    sm.tick(ok(sp - 20.0f));   // IGNITE
    sm.tick(ok(sp - 10.0f));   // -> RAMP
    Command c{};
    for (int i = 0; i < 15; ++i) c = sm.tick(ok(sp));  // crosses setpoint, settles -> HOLD
    return c;
}

// ---------------------------------------------------------------- median filter
void test_median_odd_window() {
    MedianFilter<5> f;
    for (float v : {10.f, 500.f, 11.f, 9.f, 12.f}) f.push(v);
    TEST_ASSERT_TRUE(f.ready());
    TEST_ASSERT_EQUAL_FLOAT(11.0f, f.value());
}

// ---------------------------------------------------------------- PID
void test_pid_proportional_only() {
    Pid pid({2.0f, 0.0f, 0.0f}, 0.0f, 100.0f);
    pid.reset(100.0f);
    TEST_ASSERT_EQUAL_FLOAT(20.0f, pid.update(110.0f, 100.0f, 1.0f));
}

void test_pid_output_clamped() {
    Pid pid({10.0f, 0.0f, 0.0f}, 0.0f, 100.0f);
    pid.reset(0.0f);
    TEST_ASSERT_EQUAL_FLOAT(100.0f, pid.update(100.0f, 0.0f, 1.0f));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, pid.update(0.0f, 100.0f, 1.0f));
}

void test_pid_integral_no_windup_when_saturated() {
    Pid pid({10.0f, 1.0f, 0.0f}, 0.0f, 100.0f);
    pid.reset(0.0f);
    for (int i = 0; i < 100; ++i) pid.update(100.0f, 0.0f, 1.0f);  // hard saturated high
    TEST_ASSERT_TRUE(pid.integral() <= 100.0f);
    // Once measurement passes setpoint, output must be able to come down within a few steps.
    float out = 100.0f;
    for (int i = 0; i < 5; ++i) out = pid.update(100.0f, 110.0f, 1.0f);
    TEST_ASSERT_TRUE(out < 100.0f);
}

void test_pid_conditional_integration_stops_at_imax() {
    // Once the output is pinned at imax in the error's direction, the integral must not keep growing.
    Pid pid({0.0f, 5.0f, 0.0f}, 0.0f, 100.0f);
    pid.setIntegralMax(50.0f);
    pid.reset(0.0f);
    for (int i = 0; i < 50; ++i) pid.update(100.0f, 0.0f, 1.0f);
    TEST_ASSERT_EQUAL_FLOAT(50.0f, pid.integral());
    TEST_ASSERT_EQUAL_FLOAT(50.0f, pid.lastOutput());
    // Raising imax lets it grow again.
    pid.setIntegralMax(100.0f);
    pid.update(100.0f, 0.0f, 1.0f);
    TEST_ASSERT_TRUE(pid.integral() > 50.0f);
}

void test_pid_frozen_integral() {
    Pid pid({0.0f, 1.0f, 0.0f}, 0.0f, 100.0f);
    pid.reset(0.0f);
    pid.update(10.0f, 0.0f, 1.0f);
    const float i1 = pid.integral();
    pid.setIntegralFrozen(true);
    pid.update(10.0f, 0.0f, 1.0f);
    TEST_ASSERT_EQUAL_FLOAT(i1, pid.integral());
}

void test_pid_halve_integral() {
    Pid pid({0.0f, 1.0f, 0.0f}, 0.0f, 100.0f);
    pid.reset(0.0f);
    for (int i = 0; i < 10; ++i) pid.update(10.0f, 0.0f, 1.0f);  // integral 100 clamped... ki*err*dt = 10 per step
    const float before = pid.integral();
    pid.halveIntegral();
    TEST_ASSERT_EQUAL_FLOAT(before * 0.5f, pid.integral());
}

void test_pid_derivative_on_ema_opposes_rise() {
    // At setpoint with a sudden rise above the slow average, D must pull the output down.
    Pid pid({0.0f, 0.0f, 2.0f}, -100.0f, 100.0f);
    pid.reset(100.0f);
    for (int i = 0; i < 5; ++i) pid.update(100.0f, 100.0f, 1.0f);
    const float out = pid.update(100.0f, 110.0f, 1.0f);  // T jumps 10 C above a 100 C average
    TEST_ASSERT_TRUE(out < -15.0f);                       // -2 * (110 - ~100)
    // The average follows slowly, so the D term decays rather than vanishing next tick.
    const float out2 = pid.update(100.0f, 110.0f, 1.0f);
    TEST_ASSERT_TRUE(out2 < 0.0f && out2 > out);
}

// ---------------------------------------------------------------- long PWM
void test_long_pwm_passthrough_at_or_above_min() {
    LongPwm lp(30, 10);
    TEST_ASSERT_EQUAL_UINT8(30, lp.step(30, 1.0f));
    TEST_ASSERT_EQUAL_UINT8(75, lp.step(75, 1.0f));
    TEST_ASSERT_EQUAL_UINT8(0, lp.step(0, 1.0f));
}

void test_long_pwm_below_min_duty_accounting() {
    // 15 percent requested with min 30 over a 10 s period: on at 30 for 5 s, off for 5 s.
    LongPwm lp(30, 10);
    int on = 0;
    for (int i = 0; i < 20; ++i) if (lp.step(15, 1.0f) == 30) ++on;
    TEST_ASSERT_EQUAL_INT(10, on);
    // 3 percent -> on for 1 s of each 10.
    LongPwm lp2(30, 10);
    on = 0;
    for (int i = 0; i < 20; ++i) {
        const uint8_t v = lp2.step(3, 1.0f);
        TEST_ASSERT_TRUE(v == 0 || v == 30);
        if (v == 30) ++on;
    }
    TEST_ASSERT_EQUAL_INT(2, on);
}

void test_long_pwm_min_run_hysteresis() {
    // Delta data (doc 09 M5): starts at 15, keeps running down to 10, dies at 5.
    LongPwm lp(15, 10, 10);
    // From rest a 12 percent request is below min-start: long PWM at 15 for 8 s of 10.
    TEST_ASSERT_EQUAL_UINT8(15, lp.step(12, 1.0f));
    // Now spinning: 12 is above min-run, so it passes through instead of stop-starting.
    TEST_ASSERT_EQUAL_UINT8(12, lp.step(12, 1.0f));
    TEST_ASSERT_EQUAL_UINT8(10, lp.step(10, 1.0f));
    // Below min-run: back to long PWM (on-time 7/15 of 10 s = 4.67 s at 15, then 0).
    TEST_ASSERT_EQUAL_UINT8(15, lp.step(7, 1.0f));
    // An off tick of the long-PWM period re-arms the min-start threshold.
    LongPwm lp2(15, 10, 10);
    TEST_ASSERT_EQUAL_UINT8(15, lp2.step(3, 1.0f));   // 3/15 of 10 s = 2 s on at 15
    TEST_ASSERT_EQUAL_UINT8(15, lp2.step(3, 1.0f));
    TEST_ASSERT_EQUAL_UINT8(0, lp2.step(3, 1.0f));    // off part of the period: fan stops
    TEST_ASSERT_EQUAL_UINT8(15, lp2.step(12, 1.0f));  // from rest again: min-start, not pass-through
    TEST_ASSERT_EQUAL_UINT8(12, lp2.step(12, 1.0f));
    // min_run 0 or above min_start = no hysteresis.
    LongPwm lp3(15, 10, 0);
    TEST_ASSERT_EQUAL_UINT8(15, lp3.minRunPct());
    LongPwm lp4(15, 10, 30);
    TEST_ASSERT_EQUAL_UINT8(15, lp4.minRunPct());
    // reset() forgets the spinning state.
    LongPwm lp5(15, 10, 10);
    lp5.step(20, 1.0f);
    lp5.reset();
    TEST_ASSERT_EQUAL_UINT8(15, lp5.step(12, 1.0f));
}

void test_long_pwm_min_on_stretches_period() {
    // 1 percent requested, min 15, period 10, min-on 5: on 5 s then off 70 s (average still 1 percent).
    LongPwm lp(15, 10, 10, 5);
    int on = 0, total = 0;
    uint8_t first_zero_at = 0;
    for (int i = 0; i < 150; ++i) {
        const uint8_t v = lp.step(1, 1.0f);
        ++total;
        if (v == 15) ++on;
        else if (first_zero_at == 0) first_zero_at = static_cast<uint8_t>(i);
    }
    TEST_ASSERT_EQUAL_UINT8(5, first_zero_at);   // 5 on-ticks before the first off tick
    TEST_ASSERT_EQUAL_INT(10, on);               // 2 periods of 75 s in 150 s, 5 s on each
    // 12 percent requested: on 8 s of 10 already exceeds min-on, so nothing changes (then pass-through).
    LongPwm lp2(15, 10, 0, 5);
    on = 0;
    for (int i = 0; i < 20; ++i) if (lp2.step(12, 1.0f) == 15) ++on;
    TEST_ASSERT_EQUAL_INT(16, on);
}

// ---------------------------------------------------------------- state machine
void test_sm_starts_idle_blower_off() {
    StateMachine sm(bench_thresholds(), {4.0f, 0.02f, 0.0f});
    Command c = sm.tick(ok(25.0f));
    expectState(c, State::IDLE);
    TEST_ASSERT_EQUAL_UINT8(0, c.duty_pct);
    TEST_ASSERT_EQUAL_UINT8(0, c.warnings);
}

void test_sm_ignite_to_ramp_to_hold() {
    StateMachine sm(bench_thresholds(), {4.0f, 0.0f, 0.0f});
    sm.startCook(110.0f);
    Command c = sm.tick(ok(30.0f));
    expectState(c, State::IGNITE);
    TEST_ASSERT_EQUAL_UINT8(50, c.duty_pct);

    c = sm.tick(ok(96.0f));  // >= 110 - 15
    expectState(c, State::RAMP);

    for (int i = 0; i < 12; ++i) c = sm.tick(ok(108.0f));  // within 5 C band for > 10 s
    expectState(c, State::HOLD);
}

void test_sm_startup_cap_then_full_authority_after_crossing() {
    // Big P so the PID wants 100 percent; the startup cap must hold it at 50 until the pit crosses the setpoint.
    StateMachine sm(bench_thresholds(), {20.0f, 0.0f, 0.0f});
    sm.startCook(110.0f);
    sm.tick(ok(30.0f));
    Command c = sm.tick(ok(96.0f));  // -> RAMP, err 14 C -> P = 280 -> capped
    expectState(c, State::RAMP);
    TEST_ASSERT_EQUAL_UINT8(50, c.duty_pct);
    TEST_ASSERT_FALSE(sm.setpointReached());

    c = sm.tick(ok(111.0f));  // crossed
    TEST_ASSERT_TRUE(sm.setpointReached());
    c = sm.tick(ok(100.0f));  // now 10 C low with full authority: 200 -> clamped to max 100
    TEST_ASSERT_EQUAL_UINT8(100, c.duty_pct);
}

void test_sm_halves_integral_at_first_crossing() {
    StateMachine sm(bench_thresholds(), {0.0f, 2.0f, 0.0f});
    sm.startCook(110.0f);
    sm.tick(ok(30.0f));
    sm.tick(ok(96.0f));                     // -> RAMP, integral starts at 0
    for (int i = 0; i < 10; ++i) sm.tick(ok(100.0f));  // integral grows toward the 50 cap
    const float before = sm.pid().integral();
    TEST_ASSERT_TRUE(before > 10.0f);
    sm.tick(ok(110.5f));                    // first crossing: halve, then one update with err ~ -0.5
    TEST_ASSERT_TRUE(sm.pid().integral() < before * 0.6f);
}

void test_sm_sensor_fault_forces_off() {
    StateMachine sm(bench_thresholds(), {4.0f, 0.0f, 0.0f});
    sm.startCook(110.0f);
    sm.tick(ok(30.0f));
    Command c = sm.tick(Sample{0.0f, false, Fault::SENSOR_OPEN, 1.0f});
    expectState(c, State::FAULT);
    TEST_ASSERT_EQUAL(static_cast<int>(Fault::SENSOR_OPEN), static_cast<int>(c.fault));
    TEST_ASSERT_EQUAL_UINT8(0, c.duty_pct);
    sm.startCook(110.0f);
    TEST_ASSERT_EQUAL(static_cast<int>(State::FAULT), static_cast<int>(sm.state()));
    sm.clearFault();
    TEST_ASSERT_EQUAL(static_cast<int>(State::IDLE), static_cast<int>(sm.state()));
}

void test_sm_over_limit_faults() {
    // Dynamic limit: setpoint + margin (110 + 40 = 150), capped at the absolute limit (240).
    StateMachine sm(bench_thresholds(), {4.0f, 0.0f, 0.0f});
    sm.startCook(110.0f);
    sm.tick(ok(100.0f));
    Command c = sm.tick(ok(149.0f));
    TEST_ASSERT_TRUE(c.state != State::FAULT);
    c = sm.tick(ok(150.0f));
    expectState(c, State::FAULT);
    TEST_ASSERT_EQUAL(static_cast<int>(Fault::PIT_OVER_LIMIT), static_cast<int>(c.fault));

    StateMachine hot(bench_thresholds(), {4.0f, 0.0f, 0.0f});
    hot.startCook(220.0f);   // above SETPOINT_MAX, but the cap must still hold
    hot.tick(ok(100.0f));
    c = hot.tick(ok(239.0f));
    TEST_ASSERT_TRUE(c.state != State::FAULT);
    c = hot.tick(ok(240.0f));
    expectState(c, State::FAULT);
}

void test_sm_timer_warns_at_zero_and_clears_on_stop() {
    StateMachine sm(bench_thresholds(), {4.0f, 0.0f, 0.0f});
    sm.setTimer(3);
    Command c = sm.tick(ok(100.0f));   // IDLE: timer still runs
    TEST_ASSERT_EQUAL_UINT32(2, sm.timerS());
    TEST_ASSERT_EQUAL_UINT8(0, c.warnings & Warning::TIMER_DONE);
    sm.startCook(110.0f);
    sm.tick(ok(100.0f));
    c = sm.tick(ok(100.0f));
    TEST_ASSERT_EQUAL_UINT32(0, sm.timerS());
    TEST_ASSERT_TRUE((c.warnings & Warning::TIMER_DONE) != 0);
    expectState(c, State::IDLE);       // timer end stops the cook, fan off (2026-10-25)
    TEST_ASSERT_EQUAL_UINT8(0, c.duty_pct);
    c = sm.tick(ok(100.0f));           // latched
    TEST_ASSERT_TRUE((c.warnings & Warning::TIMER_DONE) != 0);
    sm.setTimer(0);
    c = sm.tick(ok(100.0f));
    TEST_ASSERT_EQUAL_UINT8(0, c.warnings & Warning::TIMER_DONE);
    sm.setTimer(1);
    sm.tick(ok(100.0f));
    TEST_ASSERT_TRUE((sm.warnings() & Warning::TIMER_DONE) != 0);
    sm.stop();
    sm.tick(ok(100.0f));
    TEST_ASSERT_EQUAL_UINT8(0, sm.warnings() & Warning::TIMER_DONE);
    // Fault, then clear: TIMER_DONE stayed lit into the next cook on the bench (2026-10-24)
    sm.startCook(110.0f);
    sm.setTimer(2);
    sm.tick(ok(100.0f));
    sm.raiseFault(Fault::BLOWER_STALL);
    sm.tick(ok(100.0f));               // timer ends inside FAULT
    TEST_ASSERT_TRUE((sm.warnings() & Warning::TIMER_DONE) != 0);
    sm.clearFault();
    sm.tick(ok(100.0f));
    TEST_ASSERT_EQUAL_UINT8(0, sm.warnings() & Warning::TIMER_DONE);
    // A new cook drops the last cook's alert and runs until a timer ends it
    sm.startCook(110.0f);
    sm.setTimer(1);
    sm.tick(ok(100.0f));
    TEST_ASSERT_TRUE((sm.warnings() & Warning::TIMER_DONE) != 0);
    sm.startCook(110.0f);
    c = sm.tick(ok(100.0f));
    TEST_ASSERT_EQUAL_UINT8(0, c.warnings & Warning::TIMER_DONE);
    expectState(c, State::RAMP);
}

void test_sm_ignite_timeout() {
    Thresholds t = bench_thresholds();
    t.ignite_timeout_s = 5;
    StateMachine sm(t, {4.0f, 0.0f, 0.0f});
    sm.startCook(110.0f);
    Command c{};
    for (int i = 0; i < 7; ++i) c = sm.tick(ok(30.0f));  // no rise
    expectState(c, State::FAULT);
    TEST_ASSERT_EQUAL(static_cast<int>(Fault::IGNITE_TIMEOUT), static_cast<int>(c.fault));
}

void test_sm_lid_rate_trigger_plateau_exit_recovery_hold() {
    StateMachine sm(bench_thresholds(), {4.0f, 0.0f, 0.0f});
    Command c = settleInHold(sm, 110.0f);
    expectState(c, State::HOLD);

    // Lid opens: 0.5 C/s drop, above the 0.3 C/s trigger at 110 C. Rate window is 10 s, needs >= 5 s of data.
    float pit = 110.0f;
    int ticks = 0;
    while (sm.state() == State::HOLD && ticks < 30) {
        pit -= 0.5f;
        c = sm.tick(ok(pit));
        ++ticks;
    }
    expectState(c, State::LID_OPEN);
    TEST_ASSERT_TRUE(ticks <= 12);   // LidTrack: triggers within seconds, before the pit is 10 C low
    TEST_ASSERT_EQUAL_UINT8(0, c.duty_pct);
    TEST_ASSERT_EQUAL_UINT8(0, c.warnings & (Warning::PIT_LOW | Warning::PIT_HIGH));  // alarms muted

    // Keeps falling 20 s, then lid closes and the pit climbs 1 C/s for 12 s (stays below setpoint), then
    // plateaus at +0.02 C/s. Exit must come from the plateau rule, not from reaching the setpoint.
    for (int i = 0; i < 20; ++i) c = sm.tick(ok(pit -= 0.5f));
    expectState(c, State::LID_OPEN);
    const float low = pit;
    for (int i = 0; i < 12; ++i) c = sm.tick(ok(pit += 1.0f));   // still climbing fast: no exit
    expectState(c, State::LID_OPEN);
    TEST_ASSERT_TRUE(pit >= low + 11.0f);
    TEST_ASSERT_TRUE(pit < 110.0f);
    int plateau_ticks = 0;
    while (sm.state() == State::LID_OPEN && plateau_ticks < 40) {
        c = sm.tick(ok(pit += 0.02f));
        ++plateau_ticks;
    }
    expectState(c, State::RECOVERY);
    TEST_ASSERT_TRUE(plateau_ticks >= 5 && plateau_ticks <= 20);  // rate window is 10 s
    TEST_ASSERT_TRUE(pit < 110.0f);
    TEST_ASSERT_TRUE(c.duty_pct <= 50);                            // startup cap applies in RECOVERY

    // Back inside the band for the settle time -> HOLD.
    for (int i = 0; i < 12; ++i) c = sm.tick(ok(109.0f));
    expectState(c, State::HOLD);
}

void test_sm_lid_min_time_then_setpoint_exit() {
    StateMachine sm(bench_thresholds(), {4.0f, 0.0f, 0.0f});
    settleInHold(sm, 110.0f);
    float pit = 110.0f;
    Command c{};
    int ticks = 0;
    while (sm.state() == State::HOLD && ticks < 30) { c = sm.tick(ok(pit -= 1.0f)); ++ticks; }
    expectState(c, State::LID_OPEN);
    // Pit snaps back to the setpoint at once, but the 30 s minimum must hold first.
    int in_lid = 0;
    while (sm.state() == State::LID_OPEN && in_lid < 60) { c = sm.tick(ok(111.0f)); ++in_lid; }
    expectState(c, State::RECOVERY);
    TEST_ASSERT_TRUE(in_lid >= 29 && in_lid <= 31);
}

void test_sm_lid_blocked_when_output_pegged_fuel_out_guard() {
    // A fire that is dying: output EMA is near 100 percent, pit falls fast. Must NOT be read as a lid.
    Thresholds t = bench_thresholds();
    t.out_ema_alpha = 1.0f;  // make the average track instantly for the test
    StateMachine sm(t, {50.0f, 0.0f, 0.0f});  // huge P pins the output
    Command c = settleInHold(sm, 110.0f);
    expectState(c, State::HOLD);
    float pit = 110.0f;
    for (int i = 0; i < 20; ++i) c = sm.tick(ok(pit -= 1.0f));  // 1 C/s fall with duty 100
    TEST_ASSERT_TRUE(c.state == State::HOLD);
    TEST_ASSERT_EQUAL_UINT8(100, c.duty_pct);
}

void test_sm_rising_lid_on_drum() {
    StateMachine sm(bench_thresholds(), {4.0f, 0.0f, 0.0f});
    Command c = settleInHold(sm, 110.0f);
    // Lid cracked open on a drum: pit rises 0.5 C/s with the blower at 0 (PID output negative -> 0).
    float pit = 110.0f;
    for (int i = 0; i < 30; ++i) c = sm.tick(ok(pit += 0.5f));
    expectState(c, State::LID_OPEN);
    TEST_ASSERT_TRUE((c.warnings & Warning::LID_RISING) != 0);
    // Rise stops after the minimum time -> RECOVERY, still duty 0 while above setpoint.
    for (int i = 0; i < 31; ++i) c = sm.tick(ok(pit));
    for (int i = 0; i < 12; ++i) c = sm.tick(ok(pit -= 0.1f));
    expectState(c, State::RECOVERY);
    TEST_ASSERT_EQUAL_UINT8(0, c.duty_pct);
}

void test_sm_fire_out_fault() {
    Thresholds t = bench_thresholds();
    t.fire_out_s = 20;
    StateMachine sm(t, {50.0f, 0.0f, 0.0f});
    Command c = settleInHold(sm, 110.0f);
    // Fire dies: pit slides to 60 C (50 below) with the blower pegged. Slow slide so lid mode never triggers.
    float pit = 110.0f;
    for (int i = 0; i < 300 && sm.state() == State::HOLD; ++i) c = sm.tick(ok(pit = pit > 60.0f ? pit - 0.2f : 60.0f));
    TEST_ASSERT_TRUE(c.state == State::FAULT);
    TEST_ASSERT_EQUAL(static_cast<int>(Fault::FIRE_OUT), static_cast<int>(c.fault));
    TEST_ASSERT_EQUAL_UINT8(0, c.duty_pct);
}

void test_sm_fuel_low_warning() {
    Thresholds t = bench_thresholds();
    t.out_ema_alpha = 1.0f;
    t.fuel_low_window_s = 60;   // 2 decimated samples back
    StateMachine sm(t, {50.0f, 0.0f, 0.0f});
    Command c = settleInHold(sm, 110.0f);
    for (int i = 0; i < 60; ++i) c = sm.tick(ok(110.0f));  // HOLD for at least one look-back window
    // Slow slide, blower pinned: 0.1 C/s means -6 C per minute, above the 5 C threshold.
    float pit = 110.0f;
    for (int i = 0; i < 120; ++i) c = sm.tick(ok(pit -= 0.1f));
    TEST_ASSERT_TRUE(c.state == State::HOLD);
    TEST_ASSERT_TRUE((c.warnings & Warning::FUEL_LOW) != 0);
}

void test_sm_fuel_low_latches_then_clears_after_delay() {
    Thresholds t = bench_thresholds();
    t.out_ema_alpha = 1.0f;
    t.fuel_low_window_s = 60;
    t.fuel_low_clear_s = 120;
    StateMachine sm(t, {50.0f, 0.0f, 0.0f});
    Command c = settleInHold(sm, 110.0f);
    for (int i = 0; i < 60; ++i) c = sm.tick(ok(110.0f));
    float pit = 110.0f;
    for (int i = 0; i < 120; ++i) c = sm.tick(ok(pit -= 0.1f));
    TEST_ASSERT_TRUE((c.warnings & Warning::FUEL_LOW) != 0);
    // Fire recovers (refuelled): pit climbs back. The warning must not drop on the first good tick.
    for (int i = 0; i < 60; ++i) c = sm.tick(ok(pit += 0.2f));
    TEST_ASSERT_TRUE((c.warnings & Warning::FUEL_LOW) != 0);
    for (int i = 0; i < 200; ++i) c = sm.tick(ok(110.0f));
    TEST_ASSERT_EQUAL_UINT8(0, c.warnings & Warning::FUEL_LOW);
}

void test_sm_no_fuel_low_during_recovery_after_lid() {
    // Regression from the closed-loop sim: with the caps removed, the blower pegs in RECOVERY while the
    // 10 min look-back still sees the pre-lid temperature. That must not raise FUEL_LOW.
    Thresholds t = bench_thresholds();
    t.out_ema_alpha = 1.0f;        // EMA = current duty, so "pegged" is immediate
    t.fuel_low_window_s = 300;     // look-back spans the whole lid event
    t.startup_max_duty_pct = 100;  // caps removed
    StateMachine sm(t, {4.0f, 0.0f, 0.0f});
    Command c = settleInHold(sm, 110.0f);
    for (int i = 0; i < 400; ++i) c = sm.tick(ok(110.0f));
    float pit = 110.0f;
    int guard = 0;
    while (sm.state() != State::LID_OPEN && guard++ < 30) c = sm.tick(ok(pit -= 1.0f));
    expectState(c, State::LID_OPEN);
    while (sm.state() == State::LID_OPEN && guard++ < 400) c = sm.tick(ok(pit = std::max(pit - 1.0f, 80.0f)));
    expectState(c, State::RECOVERY);
    bool saw_pegged = false;
    for (int i = 0; i < 300 && sm.state() == State::RECOVERY; ++i) {
        c = sm.tick(ok(pit += 0.02f));   // slow climb 30 C below setpoint with the blower pegged
        if (c.duty_pct == 100) saw_pegged = true;
        TEST_ASSERT_EQUAL_UINT8(0, c.warnings & Warning::FUEL_LOW);
    }
    TEST_ASSERT_TRUE(saw_pegged);
}

void test_sm_pit_deviation_alarms_with_hysteresis() {
    StateMachine sm(bench_thresholds(), {4.0f, 0.0f, 0.0f});
    Command c = settleInHold(sm, 110.0f);
    c = sm.tick(ok(124.5f));
    TEST_ASSERT_TRUE((c.warnings & Warning::PIT_HIGH) != 0);
    c = sm.tick(ok(123.5f));   // inside hysteresis: still high
    TEST_ASSERT_TRUE((c.warnings & Warning::PIT_HIGH) != 0);
    c = sm.tick(ok(122.5f));   // cleared
    TEST_ASSERT_EQUAL_UINT8(0, c.warnings & Warning::PIT_HIGH);
}

void test_sm_creep_warning() {
    Thresholds t = bench_thresholds();
    t.creep_s = 30;
    StateMachine sm(t, {4.0f, 0.0f, 0.0f});
    Command c = settleInHold(sm, 110.0f);
    for (int i = 0; i < 40; ++i) c = sm.tick(ok(126.0f));  // 16 C over with the blower at 0, steady (no rising-lid trigger)
    TEST_ASSERT_EQUAL_UINT8(0, c.duty_pct);
    TEST_ASSERT_TRUE((c.warnings & Warning::CREEP) != 0);
}

void test_sm_setpoint_change_reenters_ramp_with_cap() {
    StateMachine sm(bench_thresholds(), {20.0f, 0.0f, 0.0f});
    Command c = settleInHold(sm, 110.0f);
    expectState(c, State::HOLD);
    sm.setSetpoint(130.0f);
    c = sm.tick(ok(110.0f));
    expectState(c, State::RAMP);
    TEST_ASSERT_FALSE(sm.setpointReached());
    TEST_ASSERT_EQUAL_UINT8(50, c.duty_pct);  // 20 C low wants 400, capped at the startup 50
}

void test_sm_manual_passes_duty() {
    StateMachine sm(bench_thresholds(), {4.0f, 0.0f, 0.0f});
    sm.enterManual(40);
    Command c = sm.tick(ok(25.0f));
    expectState(c, State::MANUAL);
    TEST_ASSERT_EQUAL_UINT8(40, c.duty_pct);
    sm.setManualDuty(200);  // clamps to max
    c = sm.tick(ok(25.0f));
    TEST_ASSERT_EQUAL_UINT8(100, c.duty_pct);
}

void test_sm_raise_fault_latches_first_fault_and_stops_blower() {
    StateMachine sm(bench_thresholds(), {4.0f, 0.0f, 0.0f});
    settleInHold(sm, 110.0f);
    sm.raiseFault(Fault::BLOWER_RUNAWAY);
    sm.raiseFault(Fault::BLOWER_STALL);  // ignored: the first fault stays latched
    Command c = sm.tick(ok(100.0f));
    expectState(c, State::FAULT);
    TEST_ASSERT_EQUAL(static_cast<int>(Fault::BLOWER_RUNAWAY), static_cast<int>(c.fault));
    TEST_ASSERT_EQUAL_UINT8(0, c.duty_pct);
    sm.startCook(110.0f);  // refused until cleared
    expectState(sm.tick(ok(100.0f)), State::FAULT);
    sm.clearFault();
    c = sm.tick(ok(100.0f));
    expectState(c, State::IDLE);
    TEST_ASSERT_EQUAL(static_cast<int>(Fault::NONE), static_cast<int>(c.fault));
}

void test_sm_raise_fault_ignored_in_idle_and_for_none() {
    StateMachine sm(bench_thresholds(), {4.0f, 0.0f, 0.0f});
    sm.raiseFault(Fault::BLOWER_STALL);
    expectState(sm.tick(ok(25.0f)), State::IDLE);
    sm.enterManual(40);
    sm.raiseFault(Fault::NONE);
    expectState(sm.tick(ok(25.0f)), State::MANUAL);
}

// ---------------------------------------------------------------- fan monitor (4-wire tach)
// 4400 RPM rated, runaway above commanded share + 25 percent of rated for 5 s, stall after 3 driven seconds.
// At 2 pulses per revolution and 1 s ticks, N pulses = 30 * N RPM.
static FanMonitor bench_fan() { return FanMonitor(FanLimits{4400, 25, 3, 5}, 2); }

static void expectFault(Fault expected, Fault actual) {
    TEST_ASSERT_EQUAL(static_cast<int>(expected), static_cast<int>(actual));
}

void test_fan_rpm_from_pulses() {
    FanMonitor f = bench_fan();
    f.update(true, 50, 74, 1.0f);
    TEST_ASSERT_EQUAL_FLOAT(2220.0f, f.rpm());
    f.setPulsesPerRev(4);
    f.update(true, 50, 74, 1.0f);
    TEST_ASSERT_EQUAL_FLOAT(1110.0f, f.rpm());
    f.setPulsesPerRev(0);  // guarded: treated as 1
    TEST_ASSERT_EQUAL_UINT8(1, f.pulsesPerRev());
}

void test_fan_running_as_commanded_no_fault() {
    FanMonitor f = bench_fan();
    for (int i = 0; i < 60; ++i) expectFault(Fault::NONE, f.update(true, 30, 44, 1.0f));    // 1320 RPM at 30 percent
    for (int i = 0; i < 60; ++i) expectFault(Fault::NONE, f.update(true, 0, 0, 1.0f));      // stopped at 0 percent
    for (int i = 0; i < 60; ++i) expectFault(Fault::NONE, f.update(true, 100, 161, 1.0f));  // +10 percent fan flat out
}

void test_fan_stall_after_three_driven_seconds() {
    FanMonitor f = bench_fan();
    expectFault(Fault::NONE, f.update(true, 30, 0, 1.0f));
    expectFault(Fault::NONE, f.update(true, 30, 0, 1.0f));
    expectFault(Fault::BLOWER_STALL, f.update(true, 30, 0, 1.0f));
}

void test_fan_stall_counts_across_long_pwm_gaps_and_clears_on_pulses() {
    FanMonitor f = bench_fan();
    // Long PWM: driven one second in ten. A fan that never starts is caught on the third driven second.
    for (int cycle = 0; cycle < 2; ++cycle) {
        expectFault(Fault::NONE, f.update(true, 30, 0, 1.0f));
        for (int i = 0; i < 9; ++i) expectFault(Fault::NONE, f.update(true, 0, 0, 1.0f));
    }
    expectFault(Fault::BLOWER_STALL, f.update(true, 30, 0, 1.0f));

    FanMonitor g = bench_fan();
    expectFault(Fault::NONE, g.update(true, 30, 0, 1.0f));
    expectFault(Fault::NONE, g.update(true, 30, 0, 1.0f));
    expectFault(Fault::NONE, g.update(true, 30, 44, 1.0f));  // it started: cleared
    expectFault(Fault::NONE, g.update(true, 30, 0, 1.0f));
    expectFault(Fault::NONE, g.update(true, 30, 0, 1.0f));
}

void test_fan_runaway_at_zero_command() {
    FanMonitor f = bench_fan();
    // Broken PWM lead: full speed while commanded off.
    for (int i = 0; i < 4; ++i) expectFault(Fault::NONE, f.update(true, 0, 147, 1.0f));
    expectFault(Fault::BLOWER_RUNAWAY, f.update(true, 0, 147, 1.0f));
}

void test_fan_runaway_at_max_duty_cap_even_on_a_slow_fan() {
    FanMonitor f = bench_fan();
    // 60 percent commanded allows 2640 + 1100 = 3740 RPM. A fan at the low end of tolerance (3960) still trips.
    for (int i = 0; i < 4; ++i) expectFault(Fault::NONE, f.update(true, 60, 132, 1.0f));
    expectFault(Fault::BLOWER_RUNAWAY, f.update(true, 60, 132, 1.0f));
    FanMonitor g = bench_fan();
    for (int i = 0; i < 60; ++i) expectFault(Fault::NONE, g.update(true, 60, 100, 1.0f));  // 3000 RPM: inside the margin
}

void test_fan_coast_down_is_not_runaway() {
    FanMonitor f = bench_fan();
    for (int i = 0; i < 10; ++i) expectFault(Fault::NONE, f.update(true, 60, 88, 1.0f));
    for (uint32_t pulses : {80u, 60u, 45u, 38u, 20u, 5u, 0u, 0u}) expectFault(Fault::NONE, f.update(true, 0, pulses, 1.0f));
}

void test_fan_unpowered_or_disabled_reports_nothing() {
    FanMonitor f = bench_fan();
    for (int i = 0; i < 10; ++i) expectFault(Fault::NONE, f.update(false, 0, 147, 1.0f));  // supply cut: nothing to judge
    // Accumulators do not carry over a supply cut.
    for (int i = 0; i < 4; ++i) expectFault(Fault::NONE, f.update(true, 0, 147, 1.0f));
    expectFault(Fault::NONE, f.update(false, 0, 147, 1.0f));
    for (int i = 0; i < 4; ++i) expectFault(Fault::NONE, f.update(true, 0, 147, 1.0f));

    FanMonitor g = bench_fan();
    g.setEnabled(false);
    for (int i = 0; i < 10; ++i) expectFault(Fault::NONE, g.update(true, 30, 0, 1.0f));
    for (int i = 0; i < 10; ++i) expectFault(Fault::NONE, g.update(true, 0, 147, 1.0f));
    TEST_ASSERT_EQUAL_FLOAT(4410.0f, g.rpm());  // still measured for the dump and the CSV
    g.setEnabled(true);
    expectFault(Fault::NONE, g.update(true, 30, 0, 1.0f));  // starts counting from zero when re-enabled
}

void setUp() {}
void tearDown() {}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_median_odd_window);
    RUN_TEST(test_pid_proportional_only);
    RUN_TEST(test_pid_output_clamped);
    RUN_TEST(test_pid_integral_no_windup_when_saturated);
    RUN_TEST(test_pid_conditional_integration_stops_at_imax);
    RUN_TEST(test_pid_frozen_integral);
    RUN_TEST(test_pid_halve_integral);
    RUN_TEST(test_pid_derivative_on_ema_opposes_rise);
    RUN_TEST(test_long_pwm_passthrough_at_or_above_min);
    RUN_TEST(test_long_pwm_below_min_duty_accounting);
    RUN_TEST(test_long_pwm_min_run_hysteresis);
    RUN_TEST(test_long_pwm_min_on_stretches_period);
    RUN_TEST(test_sm_starts_idle_blower_off);
    RUN_TEST(test_sm_ignite_to_ramp_to_hold);
    RUN_TEST(test_sm_startup_cap_then_full_authority_after_crossing);
    RUN_TEST(test_sm_halves_integral_at_first_crossing);
    RUN_TEST(test_sm_sensor_fault_forces_off);
    RUN_TEST(test_sm_over_limit_faults);
    RUN_TEST(test_sm_ignite_timeout);
    RUN_TEST(test_sm_lid_rate_trigger_plateau_exit_recovery_hold);
    RUN_TEST(test_sm_lid_min_time_then_setpoint_exit);
    RUN_TEST(test_sm_lid_blocked_when_output_pegged_fuel_out_guard);
    RUN_TEST(test_sm_rising_lid_on_drum);
    RUN_TEST(test_sm_fire_out_fault);
    RUN_TEST(test_sm_fuel_low_warning);
    RUN_TEST(test_sm_fuel_low_latches_then_clears_after_delay);
    RUN_TEST(test_sm_no_fuel_low_during_recovery_after_lid);
    RUN_TEST(test_sm_pit_deviation_alarms_with_hysteresis);
    RUN_TEST(test_sm_creep_warning);
    RUN_TEST(test_sm_setpoint_change_reenters_ramp_with_cap);
    RUN_TEST(test_sm_manual_passes_duty);
    RUN_TEST(test_sm_raise_fault_latches_first_fault_and_stops_blower);
    RUN_TEST(test_sm_raise_fault_ignored_in_idle_and_for_none);
    RUN_TEST(test_fan_rpm_from_pulses);
    RUN_TEST(test_fan_running_as_commanded_no_fault);
    RUN_TEST(test_fan_stall_after_three_driven_seconds);
    RUN_TEST(test_fan_stall_counts_across_long_pwm_gaps_and_clears_on_pulses);
    RUN_TEST(test_fan_runaway_at_zero_command);
    RUN_TEST(test_fan_runaway_at_max_duty_cap_even_on_a_slow_fan);
    RUN_TEST(test_fan_coast_down_is_not_runaway);
    RUN_TEST(test_fan_unpowered_or_disabled_reports_nothing);
    RUN_TEST(test_sm_timer_warns_at_zero_and_clears_on_stop);
    return UNITY_END();
}
