# Closed-loop blower simulation

Runs the real firmware control core (`src/control`: state machine, PID, long-PWM, median filter) against a
simulated blower, duct, charcoal bed and drum. The HAL blower logic (long-PWM plus the 300 ms kick) is
re-implemented without Arduino in `sim_main.cpp`; thresholds are built from `include/config.h` exactly as
`main.cpp` builds them.

The plant (`plant.h`) is a set of ASSUMPTIONS sized to the planning numbers in design doc 06 (0.3 to 0.6 kg/h
charcoal at 107 to 121 C, about 3 CFM of air at steady state, 10 CFM blower). Nothing in it is measured on
the drum. Use it to test controller logic and compare design choices. Do not copy PID gains from it into
`config.h`; derive those from the step-response cook.

## Build and run (Linux, macOS, WSL2, or MSYS2 on Windows; needs g++ 9+ and Python 3 with matplotlib)

```
cd firmware/sim
mkdir -p out
g++ -std=gnu++17 -O2 -Wall -Wextra -Wno-array-bounds -I../include -I../src sim_main.cpp ../src/control/pid.cpp ../src/control/state_machine.cpp -o sim
./sim            # scenarios -> out/*.csv, out/metrics.json
./sim --sweep    # PID gain grid on the nominal plant -> out/sweep.json
python3 plot.py  # -> out/*.png
```

`-Wno-array-bounds` silences a GCC 13 false positive inside `std::sort` in `median_filter.h`; the xtensa
toolchain used by `pio run` does not emit it.

## Scenarios (12 h, 110 C unless noted; lid 90 s at 3 h and 20 s at 6 h; wind x2 from 5 h for 20 min; 4 kg basket)

| Name | What differs |
|---|---|
| nominal | firmware defaults, 10 CFM blower, flap installed |
| no_flap | stack draft leaks through the duct when the fan is off |
| no_flap_93c | no flap at a 93 C (200 F) setpoint |
| flap_93c | flap at 93 C, for comparison |
| caps_removed | Phase 1 defaults: IGNITE 90 percent, no startup cap, max duty 100 |
| fan_20cfm | 20 CFM blower (FireBoard class), current caps and gains |

## Plant parameters to replace with measurements

`fan_cfm_max`, `fan_stall_pct`, `fan_start_pct` (bench step 2), `draft_leak_cfm_at_90c` (fan-off draft test),
`ua_w_per_k`, `chamber_heat_cap_j_per_k`, `bed_tau_s`, `heat_to_chamber` (step-response cook), `kg_c_per_m3_air`
and burn rate (weigh the basket before and after a cook).
