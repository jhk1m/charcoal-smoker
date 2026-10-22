// Closed-loop simulation plant: blower -> duct (+ optional backdraft flap) -> charcoal bed -> drum chamber -> probe.
// Every parameter below is an ASSUMPTION chosen to match the planning numbers in design doc 06
// (0.3 to 0.6 kg/h charcoal at 107 to 121 C, 2.5 to 4 CFM of air at steady state, 10 CFM blower).
// None of it is measured on the real drum. The sim tests the controller LOGIC against a plausible plant;
// it does not produce valid PID gains for the real smoker. Replace parameters with step-response data.
#pragma once

#include <algorithm>
#include <cmath>
#include <random>

namespace sim {

constexpr double CFM_TO_M3S = 0.000471947;

struct PlantParams {
    // Ambient
    double t_amb_c = 20.0;

    // Blower (brushed DC, supply-chopped). Flow is proportional to speed; speed follows duty with a lag.
    double fan_cfm_max = 15.3;          // Delta BFB0712HH-AWPH datasheet free-air CFM (was 10, generic guess)
    double fan_stall_pct = 7.0;         // MEASURED 2026-10-22 bench: runs at 10, dies at 5 (was 22)
    double fan_start_pct = 10.0;        // MEASURED 2026-10-10 bench: starts at 10 with the kick, 4 of 4 (was 28)
    double fan_tau_s = 0.6;             // motor spin-up time constant
    double duct_efficiency = 0.8;       // fraction of free-air flow delivered through the duct + flap

    // Air paths other than the fan
    double seam_leak_cfm = 0.3;         // lid and door seams, always present
    bool   flap_installed = true;       // gravity backdraft flap at the duct inlet
    double draft_leak_cfm_at_90c = 2.5; // stack-effect draft through an open duct when the fan is off (no flap)

    // Charcoal bed
    double fuel_total_kg = 4.0;         // basket load at start
    double lit_initial_kg = 0.25;       // about 12 briquettes lit on top
    double kg_c_per_m3_air = 0.075;     // 1 kg carbon per ~9.5 m3 air stoichiometric, excess air factor 1.4
    double lhv_j_per_kg = 29e6;         // charcoal lower heating value
    double heat_to_chamber = 0.45;      // rest leaves with the exhaust / through the wall near the basket
    double lit_burn_cap_per_s = 1.0 / 1800.0;  // lit coal burns out in 30 min at unlimited air
    double smoulder_per_s = 2.0e-5;     // air-starved ember consumption per kg lit
    double spread_ratio = 1.2;          // kg of fresh coal ignited per kg burned (forge effect)
    double bed_tau_s = 90.0;            // coal bed glow-up / die-down time constant

    // Drum chamber (lumped)
    double chamber_heat_cap_j_per_k = 20e3;  // thin stainless drum + grates + air
    double ua_w_per_k = 12.0;                // wall loss
    double meat_kg = 4.0;
    double meat_cp = 3500.0;
    double meat_ha_w_per_k = 8.0;
    double meat_t0_c = 4.0;

    // Probe (K-type in a sheath at the side port)
    double probe_tau_s = 8.0;
    double probe_noise_c = 0.25;
};

// Disturbances applied from the scenario timeline.
struct Disturbance {
    bool lid_open = false;       // chamber air exchange + extra air reaching the coals
    double wind_factor = 1.0;    // multiplies wall loss and the draft leak
};

class Plant {
public:
    explicit Plant(const PlantParams& p, uint32_t seed = 1) : p_(p), rng_(seed), noise_(0.0, p.probe_noise_c) {
        t_ch_ = p_.t_amb_c + 5.0;  // lit fire, lid closed, a minute in
        t_probe_ = t_ch_;
        t_meat_ = p_.meat_t0_c;
        lit_ = p_.lit_initial_kg;
        unlit_ = p_.fuel_total_kg - p_.lit_initial_kg;
        burn_ = lit_ * p_.smoulder_per_s * 5.0;
    }

    // Applied PWM duty (0..100) as the MOSFET sees it, and whether this is a start kick.
    void step(double dt, double applied_pct, const Disturbance& d) {
        // --- blower motor
        double target_speed = 0.0;
        const bool running = speed_ > 0.02;
        const double thresh = running ? p_.fan_stall_pct : p_.fan_start_pct;
        if (applied_pct >= thresh) target_speed = applied_pct / 100.0;
        speed_ += (target_speed - speed_) * std::min(1.0, dt / p_.fan_tau_s);
        if (target_speed == 0.0 && speed_ < 0.02) speed_ = 0.0;

        // --- air to the coals
        const double dT = std::max(0.0, t_ch_ - p_.t_amb_c);
        double fan_cfm = speed_ * p_.fan_cfm_max * p_.duct_efficiency;
        double draft_cfm = 0.0;
        if (!p_.flap_installed) {
            draft_cfm = p_.draft_leak_cfm_at_90c * std::sqrt(dT / 90.0) * d.wind_factor;
        }
        double lid_cfm = d.lid_open ? 8.0 : 0.0;
        air_cfm_ = fan_cfm + draft_cfm + p_.seam_leak_cfm + lid_cfm;
        const double air_m3s = air_cfm_ * CFM_TO_M3S;

        // --- charcoal bed
        const double air_limited = air_m3s * p_.kg_c_per_m3_air;
        const double lit_limited = lit_ * p_.lit_burn_cap_per_s;
        double target_burn = std::min(air_limited, lit_limited);
        target_burn = std::max(target_burn, lit_ * p_.smoulder_per_s);
        burn_ += (target_burn - burn_) * std::min(1.0, dt / p_.bed_tau_s);
        const double burned = std::min(burn_ * dt, lit_);
        lit_ -= burned;
        const double ignite = std::min(unlit_, burned * p_.spread_ratio);
        unlit_ -= ignite;
        lit_ += ignite;
        fuel_used_ += burned;

        // --- chamber energy balance
        const double q_fire = burn_ * p_.lhv_j_per_kg * p_.heat_to_chamber;
        const double q_wall = p_.ua_w_per_k * d.wind_factor * (t_ch_ - p_.t_amb_c);
        const double exhaust_kg_s = air_m3s * 1.2;
        const double q_exhaust = exhaust_kg_s * 1005.0 * (t_ch_ - p_.t_amb_c);
        const double q_lid = d.lid_open ? 180.0 * (t_ch_ - p_.t_amb_c) : 0.0;
        const double q_meat = p_.meat_ha_w_per_k * (t_ch_ - t_meat_);
        t_ch_ += dt * (q_fire - q_wall - q_exhaust - q_lid - q_meat) / p_.chamber_heat_cap_j_per_k;
        t_meat_ += dt * q_meat / (p_.meat_kg * p_.meat_cp);

        // --- probe
        t_probe_ += (t_ch_ - t_probe_) * std::min(1.0, dt / p_.probe_tau_s);
    }

    double probeReading() { return t_probe_ + noise_(rng_); }

    double chamberC() const { return t_ch_; }
    double meatC() const { return t_meat_; }
    double fanSpeed() const { return speed_; }
    double airCfm() const { return air_cfm_; }
    double litKg() const { return lit_; }
    double fuelLeftKg() const { return lit_ + unlit_; }
    double fuelUsedKg() const { return fuel_used_; }
    double burnKgPerH() const { return burn_ * 3600.0; }

private:
    PlantParams p_;
    std::mt19937 rng_;
    std::normal_distribution<double> noise_;
    double t_ch_, t_probe_, t_meat_;
    double lit_, unlit_, burn_;
    double speed_ = 0.0;
    double air_cfm_ = 0.0;
    double fuel_used_ = 0.0;
};

}  // namespace sim
