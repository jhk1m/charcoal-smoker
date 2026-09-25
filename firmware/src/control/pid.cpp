#include "pid.h"

#include <algorithm>

namespace control {

Pid::Pid(PidGains g, float out_min, float out_max)
    : g_(g), out_min_(out_min), out_max_(out_max), imax_(out_max) {}

void Pid::setOutputLimits(float out_min, float out_max) {
    out_min_ = out_min;
    out_max_ = out_max;
    if (imax_ > out_max_) imax_ = out_max_;
    integral_ = std::clamp(integral_, 0.0f, imax_);
}

void Pid::setIntegralMax(float imax) {
    imax_ = std::clamp(imax, 0.0f, out_max_);
    integral_ = std::clamp(integral_, 0.0f, imax_);
}

void Pid::reset(float measurement) {
    integral_ = 0.0f;
    meas_avg_ = measurement;
    have_avg_ = true;
    last_out_ = 0.0f;
}

float Pid::update(float setpoint, float measurement, float dt_s) {
    if (dt_s <= 0.0f) return last_out_;

    if (!have_avg_) {
        meas_avg_ = measurement;
        have_avg_ = true;
    }

    const float err = setpoint - measurement;
    const float p = g_.kp * err;

    // Conditional integration (HeaterMeter): accumulate only when the previous output was not already
    // pinned in the direction the error is pushing.
    if (!frozen_) {
        const bool push_up_ok = err > 0.0f && last_out_ < imax_;
        const bool push_down_ok = err < 0.0f && last_out_ > out_min_;
        if (push_up_ok || push_down_ok) integral_ += g_.ki * err * dt_s;
        integral_ = std::clamp(integral_, 0.0f, imax_);
    }

    // Derivative on measurement against the slow average, computed before the average is updated with
    // this sample (HeaterMeter order).
    const float d = -g_.kd * (measurement - meas_avg_);
    meas_avg_ += avg_alpha_ * (measurement - meas_avg_);

    last_out_ = std::clamp(p + integral_ + d, out_min_, out_max_);
    return last_out_;
}

}  // namespace control
