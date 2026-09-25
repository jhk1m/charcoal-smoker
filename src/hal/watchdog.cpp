#include "watchdog.h"

#include <esp_system.h>
#include <esp_task_wdt.h>

namespace hal {

bool Watchdog::begin(uint32_t timeout_ms) {
    esp_task_wdt_config_t cfg = {};
    cfg.timeout_ms = timeout_ms;
    cfg.idle_core_mask = 0;  // do not watch idle tasks; only our subscribed control task
    cfg.trigger_panic = true;

    // Arduino-ESP32 3.x already initialises the TWDT at boot on the classic ESP32, so reconfigure first;
    // fall back to init only if it is not running (reconfigure returns ESP_ERR_INVALID_STATE in that case).
    // This order avoids the "TWDT already initialized" error log from esp_task_wdt_init.
    esp_err_t err = esp_task_wdt_reconfigure(&cfg);
    if (err == ESP_ERR_INVALID_STATE) err = esp_task_wdt_init(&cfg);
    if (err != ESP_OK) return false;
    return esp_task_wdt_add(nullptr) == ESP_OK;  // nullptr = current task
}

void Watchdog::feed() { esp_task_wdt_reset(); }

bool Watchdog::lastResetWasWatchdog() {
    const esp_reset_reason_t r = esp_reset_reason();
    return r == ESP_RST_TASK_WDT || r == ESP_RST_INT_WDT || r == ESP_RST_WDT;
}

}  // namespace hal
