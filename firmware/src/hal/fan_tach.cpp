#include "fan_tach.h"

#include <driver/pulse_cnt.h>

namespace hal {

bool FanTach::begin(int pin, uint32_t glitch_ns) {
    if (pin < 0 || unit_ != nullptr) return false;

    pcnt_unit_config_t ucfg = {};
    ucfg.low_limit = -1;       // never counts down; the driver only requires a negative limit
    ucfg.high_limit = 32767;   // 16-bit counter; read and cleared every control tick, far below this
    pcnt_unit_handle_t unit = nullptr;
    if (pcnt_new_unit(&ucfg, &unit) != ESP_OK) return false;

    pcnt_glitch_filter_config_t fcfg = {};
    fcfg.max_glitch_ns = glitch_ns;
    pcnt_chan_config_t ccfg = {};
    ccfg.edge_gpio_num = pin;
    ccfg.level_gpio_num = -1;
    pcnt_channel_handle_t chan = nullptr;
    bool enabled = false;

    bool ok = pcnt_unit_set_glitch_filter(unit, &fcfg) == ESP_OK;
    ok = ok && pcnt_new_channel(unit, &ccfg, &chan) == ESP_OK;
    ok = ok && pcnt_channel_set_edge_action(chan, PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_HOLD) == ESP_OK;
    if (ok) {
        enabled = pcnt_unit_enable(unit) == ESP_OK;
        ok = enabled;
    }
    ok = ok && pcnt_unit_clear_count(unit) == ESP_OK;
    ok = ok && pcnt_unit_start(unit) == ESP_OK;

    if (!ok) {
        // Release in reverse order. Nothing useful can be done if a release itself fails at boot.
        if (enabled) (void)pcnt_unit_disable(unit);
        if (chan != nullptr) (void)pcnt_del_channel(chan);
        (void)pcnt_del_unit(unit);
        return false;
    }
    unit_ = unit;
    chan_ = chan;
    return true;
}

bool FanTach::take(uint32_t& pulses) {
    pulses = 0;
    if (unit_ == nullptr) return false;
    int count = 0;
    if (pcnt_unit_get_count(unit_, &count) != ESP_OK) return false;
    if (pcnt_unit_clear_count(unit_) != ESP_OK) return false;
    if (count > 0) pulses = static_cast<uint32_t>(count);
    return true;
}

}  // namespace hal
