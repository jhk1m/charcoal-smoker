// Task watchdog wrapper. ESP-IDF 5.x API (Arduino-ESP32 3.x).
// VERIFIED: https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32s3/api-reference/system/wdts.html
//   esp_err_t esp_task_wdt_init(const esp_task_wdt_config_t*);  esp_err_t esp_task_wdt_reconfigure(const esp_task_wdt_config_t*);
//   esp_err_t esp_task_wdt_add(TaskHandle_t);  esp_err_t esp_task_wdt_reset(void);
//   struct esp_task_wdt_config_t { uint32_t timeout_ms; uint32_t idle_core_mask; bool trigger_panic; };
// Arduino-ESP32 may already have the TWDT initialised; init returning ESP_ERR_INVALID_STATE falls back to reconfigure.
#pragma once

#include <cstdint>

namespace hal {

class Watchdog {
public:
    // Configure TWDT and subscribe the calling task. Panic on timeout -> reset -> gate pull-down keeps blower OFF.
    static bool begin(uint32_t timeout_ms);
    static void feed();
    // True if the last reset was caused by the task watchdog (reported as a FAULT at boot).
    static bool lastResetWasWatchdog();
};

}  // namespace hal
