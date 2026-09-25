// Pin map, selected by the IDF target of the active PlatformIO env.
// Source of truth: project doc 04-phase1-wiring.md. Confirm against the board silkscreen before first flash.
#pragma once

#include <sdkconfig.h>

namespace pins {

// Blower: GPIO -> 200 ohm -> IRLZ44N gate, 10k gate pull-down (blower OFF while MCU is in reset).
// GPIO4 on the S3 and WROOM-32 (no strapping duty); GPIO2 on the Freenove WROVER (GPIO4 is a camera line).
#if defined(BOARD_FREENOVE_WROVER)
constexpr int BLOWER_PWM = 2;
#else
constexpr int BLOWER_PWM = 4;
#endif

// Delta BFB0712HH-AWPH (4-wire) additions, doc 04 "Fan control pins by board". -1 = not fitted.
// FAN_TACH: open-collector FG output, 10k pull-up to 3.3 V, pulse counted in firmware.
// FAN_SUPPLY_EN: IRLZ44N gate, cuts the fan supply on fault or runaway. NOT a PWM pin.
#if defined(CONFIG_IDF_TARGET_ESP32S3)
constexpr int FAN_TACH = 5;
constexpr int FAN_SUPPLY_EN = 6;
#elif defined(BOARD_FREENOVE_WROVER)
constexpr int FAN_TACH = 15;       // camera fitted; with the camera removed use 39 (doc 04 map A2)
constexpr int FAN_SUPPLY_EN = 12;  // safe as an output only because of the 10k gate pull-down
#else
constexpr int FAN_TACH = 34;       // input-only pin, fine for an open-collector signal
constexpr int FAN_SUPPLY_EN = 13;
#endif

#if defined(CONFIG_IDF_TARGET_ESP32S3)
// ESP32-S3-DevKitC-1 (YEJMKJ N8R8 clone). Arduino-ESP32 FSPI defaults. Avoid strapping pins 0, 3, 45, 46.
constexpr int TC_SCK  = 12;
constexpr int TC_MOSI = 11;  // MAX31856 SDI (unused by MAX31855)
constexpr int TC_MISO = 13;  // MAX31856 SDO / MAX31855 SO
constexpr int TC_CS   = 10;
constexpr int TC_DRDY = 9;   // optional, not used in Phase 1 polling
constexpr int TC_FLT  = 8;   // optional, fault bits are read over SPI instead
constexpr const char* HW_NAME = "s3-n8r8";
#elif defined(CONFIG_IDF_TARGET_ESP32) && defined(BOARD_FREENOVE_WROVER)
// Freenove ESP32-WROVER (camera) board on the Freenove GPIO extension board.
// The OV2640 camera is hard-wired (no PWDN) to 4, 5, 18, 19, 21, 22, 23, 25, 26, 27, 34, 35, 36, 39
// (VERIFIED: Freenove_ESP32_WROVER_Board camera_pins.h, CAMERA_MODEL_WROVER_KIT). PSRAM uses 16, 17.
// Strapping pins 0, 2, 12, 15: GPIO2 is used for PWM and is held low at boot by the 10k gate pull-down.
constexpr int TC_SCK  = 14;
constexpr int TC_MOSI = 13;  // unused by MAX31855, needed by MAX31856 SDI
constexpr int TC_MISO = 33;  // MAX31855 SO / MAX31856 SDO
constexpr int TC_CS   = 32;
constexpr int TC_DRDY = -1;
constexpr int TC_FLT  = -1;
constexpr const char* HW_NAME = "freenove-wrover";
#elif defined(CONFIG_IDF_TARGET_ESP32)
// ESP32-WROOM-32 dev board (no camera). Arduino-ESP32 VSPI defaults (VERIFIED variants/esp32/pins_arduino.h:
// SS 5, MOSI 23, MISO 19, SCK 18). Strapping pins 0, 2, 5, 12, 15: GPIO5 is CS, which is driven
// only by the MCU after boot, so it is acceptable; move CS to 17 if boot mode ever misbehaves.
constexpr int TC_SCK  = 18;
constexpr int TC_MOSI = 23;  // MAX31856 SDI (unused by MAX31855)
constexpr int TC_MISO = 19;  // MAX31856 SDO / MAX31855 SO
constexpr int TC_CS   = 5;
constexpr int TC_DRDY = -1;  // not wired
constexpr int TC_FLT  = -1;  // not wired
constexpr const char* HW_NAME = "wroom32";
#else
#error "Unsupported IDF target: add a pin map in include/pins.h"
#endif

// Pairing button (deferred; enclosure phase). -1 = not fitted.
constexpr int PAIR_BUTTON = -1;

}  // namespace pins
