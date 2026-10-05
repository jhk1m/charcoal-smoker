// Fan tach input: counts the open-collector FG pulses of the 4-wire fan on a PCNT unit (ESP-IDF 5.x
// pulse counter driver, shipped with Arduino-ESP32 3.x).
// VERIFIED 2026-10-04 in framework-arduinoespressif32-libs (3.3.12), esp_driver_pcnt/include/driver/pulse_cnt.h:
//   pcnt_new_unit, pcnt_unit_set_glitch_filter, pcnt_new_channel (edge GPIO: input, pull-up enabled),
//   pcnt_channel_set_edge_action, pcnt_unit_enable, pcnt_unit_start, pcnt_unit_get_count, pcnt_unit_clear_count.
// The hardware glitch filter rejects the edges the 25 kHz PWM lead couples onto the tach lead.
#pragma once

#include <cstdint>

struct pcnt_unit_t;
struct pcnt_chan_t;

namespace hal {

class FanTach {
public:
    // Rising edges on `pin` are counted. Pulses narrower than glitch_ns are ignored.
    bool begin(int pin, uint32_t glitch_ns);

    // Pulses since the previous call. False if the counter is not running or could not be read;
    // `pulses` is then 0, which the fan monitor treats as a silent tach.
    bool take(uint32_t& pulses);

    bool running() const { return unit_ != nullptr; }

private:
    pcnt_unit_t* unit_ = nullptr;   // owned for the life of the firmware once begin() succeeds
    pcnt_chan_t* chan_ = nullptr;
};

}  // namespace hal
