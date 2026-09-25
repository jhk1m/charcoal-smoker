// BLE GATT server: one custom service, characteristics per 02-phase1-architecture.md section 7.
// NimBLE-Arduino 2.5.1. VERIFIED against headers at tag 2.5.1 (src/NimBLEDevice.h, NimBLEServer.h,
// NimBLECharacteristic.h, NimBLEAdvertising.h, NimBLEConnInfo.h, NimBLELocalValueAttribute.h):
//   NimBLEDevice::init(const std::string&); setSecurityAuth(bool bonding, bool mitm, bool sc);
//   setSecurityIOCap(uint8_t); setSecurityPasskey(uint32_t); createServer(); getAdvertising(); startAdvertising(uint32_t)
//   NimBLEServer::createService(const char*); setCallbacks(NimBLEServerCallbacks*, bool); advertiseOnDisconnect(bool); getConnectedCount()
//   NimBLEService::createCharacteristic(const char*, uint32_t props, uint16_t maxLen); start()
//   NimBLECharacteristic::setValue(const T&); notify(); setCallbacks(); getValue<T>()
//   NimBLECharacteristicCallbacks::onWrite(NimBLECharacteristic*, NimBLEConnInfo&); onRead(...); onSubscribe(...)
//   NimBLEServerCallbacks::onConnect(NimBLEServer*, NimBLEConnInfo&); onDisconnect(NimBLEServer*, NimBLEConnInfo&, int);
//   onAuthenticationComplete(NimBLEConnInfo&); uint32_t onPassKeyDisplay()
//   NimBLEConnInfo::isBonded(); isEncrypted(); isAuthenticated()
//   NIMBLE_PROPERTY::{READ, WRITE, NOTIFY, READ_ENC, WRITE_ENC, WRITE_AUTHEN}
#pragma once

#include <NimBLEDevice.h>

#include <cstdint>
#include <functional>

#include "control/state_machine.h"
#include "log/event_log.h"

namespace ble {

// Callbacks into the application (control task). Invoked from the NimBLE host task; keep them short
// and hand off via the command queue in main.cpp.
struct Handlers {
    std::function<void(float)> on_setpoint;             // validated setpoint in C
    std::function<void(uint8_t)> on_mode;               // 0 = auto (start/continue cook), 1 = manual, 2 = stop/idle, 3 = clear fault
    std::function<void(uint8_t)> on_manual_duty;        // percent
    std::function<bool(evlog::Event&)> on_log_pop;      // pop oldest event for chunked read; false if empty
};

class Service {
public:
    bool begin(const char* device_name, uint32_t passkey, const Handlers& h, const char* device_info);

    // Publish telemetry. Safe to call from the control task; NimBLE serialises internally.
    void publishPit(float c);
    void publishDuty(uint8_t pct);
    void publishState(control::State s);
    void publishFault(control::Fault f);
    void publishSetpoint(float c);
    void publishWarnings(uint8_t mask);

    bool connected() const;
    // Opens a pairing window: allows new bonds for window_ms. Outside it, only bonded peers may write.
    void openPairingWindow(uint32_t window_ms);
    void service(uint32_t now_ms);  // closes the window on expiry
    bool pairingOpen() const { return pairing_open_; }

private:
    friend class ServerCb;
    friend class CharCb;

    Handlers h_;
    NimBLEServer* server_ = nullptr;
    NimBLECharacteristic* ch_pit_ = nullptr;
    NimBLECharacteristic* ch_setpoint_ = nullptr;
    NimBLECharacteristic* ch_duty_ = nullptr;
    NimBLECharacteristic* ch_state_ = nullptr;
    NimBLECharacteristic* ch_fault_ = nullptr;
    NimBLECharacteristic* ch_mode_ = nullptr;
    NimBLECharacteristic* ch_manual_ = nullptr;
    NimBLECharacteristic* ch_log_ = nullptr;
    NimBLECharacteristic* ch_info_ = nullptr;
    NimBLECharacteristic* ch_warn_ = nullptr;
    bool pairing_open_ = false;
    uint32_t pairing_end_ms_ = 0;
    int bonds_at_connect_ = 0;
};

}  // namespace ble
