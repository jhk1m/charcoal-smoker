// PID in the HeaterMeter form (grillpid.cpp, verified 2026-09-21):
//   P = kp * (setpoint - T)
//   I += ki * (setpoint - T) * dt, conditional integration (only while the output is not saturated in the
//       direction of the error), clamped to [0, imax]. imax is the startup cap until the setpoint is first
//       reached, then the full output max. halveIntegral() is called at that first crossing.
//   D = -kd * (T - T_avg), derivative on measurement against a slow EMA of the measurement (alpha 2/61 at
//       1 Hz, about a 60 s window), not against the previous sample. kd unit: percent per C of deviation.
// Output unit: percent duty, clamped to [out_min, out_max]. Pure C++.
#pragma once

namespace control {

struct PidGains {
    float kp;  // %/C
    float ki;  // %/(C*s)
    float kd;  // % per C of (T - EMA(T))
};

class Pid {
public:
    Pid(PidGains g, float out_min, float out_max);

    void setGains(PidGains g) { g_ = g; }
    PidGains gains() const { return g_; }
    void setOutputLimits(float out_min, float out_max);

    // Upper clamp for the integral term alone (HeaterMeter IMax). Defaults to out_max.
    void setIntegralMax(float imax);
    float integralMax() const { return imax_; }

    // EMA smoothing factor for the derivative reference (default 2/61).
    void setMeasurementAvgAlpha(float alpha) { avg_alpha_ = alpha; }

    // Reset integrator and derivative memory. Call when entering closed loop from open loop.
    void reset(float measurement);

    // Freeze/unfreeze the integrator (LID_OPEN).
    void setIntegralFrozen(bool frozen) { frozen_ = frozen; }

    // Called once when the pit first reaches the setpoint after startup (HeaterMeter: prevents bouncing
    // above the setpoint on the first approach).
    void halveIntegral() { integral_ *= 0.5f; }

    // One control step. dt_s > 0. Returns clamped output.
    float update(float setpoint, float measurement, float dt_s);

    float integral() const { return integral_; }
    float lastOutput() const { return last_out_; }
    float measurementAvg() const { return meas_avg_; }

private:
    PidGains g_;
    float out_min_, out_max_;
    float imax_;
    float integral_ = 0.0f;
    float meas_avg_ = 0.0f;
    float avg_alpha_ = 2.0f / 61.0f;
    bool have_avg_ = false;
    bool frozen_ = false;
    float last_out_ = 0.0f;
};

}  // namespace control
