#include "ble_service.h"

#include <Arduino.h>

#include "config.h"

namespace ble {

// ---------------------------------------------------------------- server callbacks
class ServerCb : public NimBLEServerCallbacks {
public:
    explicit ServerCb(Service& s) : s_(s) {}

    void onConnect(NimBLEServer* /*pServer*/, NimBLEConnInfo& connInfo) override {
        s_.bonds_at_connect_ = NimBLEDevice::getNumBonds();
        log_i("BLE connect %s (bonds stored: %d)", connInfo.getAddress().toString().c_str(), s_.bonds_at_connect_);
    }

    void onDisconnect(NimBLEServer* /*pServer*/, NimBLEConnInfo& connInfo, int reason) override {
        log_i("BLE disconnect %s reason %d", connInfo.getAddress().toString().c_str(), reason);
        // advertiseOnDisconnect(true) restarts advertising automatically.
    }

    // Static passkey shown/entered on the phone. MITM + bonding + secure connections.
    // Outside the pairing window a random passkey is presented, so a late attempt fails at PIN entry before any
    // keys are stored. Deleting the bond afterwards (below) left the phone holding half a bond, and iOS then
    // refused every later connect with "peer removed pairing information" (2026-10-23).
    uint32_t onPassKeyDisplay() override {
        if (s_.pairing_open_) return cfg::BLE_PASSKEY;
        log_w("BLE passkey requested outside the pairing window: presenting a random one");
        return esp_random() % 1000000u;
    }

    void onAuthenticationComplete(NimBLEConnInfo& connInfo) override {
        if (!connInfo.isEncrypted()) {
            log_w("BLE auth failed, dropping %s", connInfo.getAddress().toString().c_str());
            s_.server_->disconnect(connInfo);
            return;
        }
        // A NEW bond (bond count grew during this connection) while the pairing window is closed is
        // rejected: delete it and drop the link. Bonds that existed before the connection pass, so
        // previously paired phones reconnect freely after a reboot.
        const NimBLEAddress id = connInfo.getIdAddress();
        const bool new_bond = NimBLEDevice::getNumBonds() > s_.bonds_at_connect_;
        if (new_bond && !s_.pairing_open_) {
            log_w("BLE bond attempt outside pairing window from %s; rejecting", id.toString().c_str());
            NimBLEDevice::deleteBond(id);
            s_.server_->disconnect(connInfo);
            return;
        }
        log_i("BLE authenticated %s bonded=%d new_bond=%d", id.toString().c_str(), connInfo.isBonded(), new_bond);
    }

private:
    Service& s_;
};

// ---------------------------------------------------------------- characteristic callbacks
class CharCb : public NimBLECharacteristicCallbacks {
public:
    explicit CharCb(Service& s) : s_(s) {}

    void onWrite(NimBLECharacteristic* ch, NimBLEConnInfo& connInfo) override {
        if (!connInfo.isEncrypted()) return;  // belt and braces; WRITE_ENC already enforces this
        const NimBLEAttValue& v = ch->getValue();

        if (ch == s_.ch_setpoint_) {
            if (v.size() < sizeof(float)) return;
            float c;
            memcpy(&c, v.data(), sizeof(c));
            if (c < cfg::SETPOINT_MIN_C || c > cfg::SETPOINT_MAX_C) {
                log_w("BLE setpoint %.1f out of range, ignored", c);
                return;
            }
            if (s_.h_.on_setpoint) s_.h_.on_setpoint(c);
        } else if (ch == s_.ch_mode_) {
            if (v.size() < 1) return;
            if (s_.h_.on_mode) s_.h_.on_mode(v.data()[0]);
        } else if (ch == s_.ch_manual_) {
            if (v.size() < 1) return;
            if (s_.h_.on_manual_duty) s_.h_.on_manual_duty(v.data()[0] > 100 ? 100 : v.data()[0]);
        } else if (ch == s_.ch_timer_) {
            if (v.size() < sizeof(uint16_t)) return;
            uint16_t min;
            memcpy(&min, v.data(), sizeof(min));
            if (s_.h_.on_timer) s_.h_.on_timer(min);
        } else if (ch == s_.ch_console_) {
            char line[96];
            const size_t n = v.size() < sizeof(line) - 1 ? v.size() : sizeof(line) - 1;
            memcpy(line, v.data(), n);
            line[n] = '\0';
            if (s_.h_.on_console) s_.h_.on_console(line);
        }
    }

    // Chunked log read: each read pops the oldest event. Empty log -> zero-length value.
    void onRead(NimBLECharacteristic* ch, NimBLEConnInfo& /*connInfo*/) override {
        if (ch != s_.ch_log_) return;
        evlog::Event e{};
        if (s_.h_.on_log_pop && s_.h_.on_log_pop(e)) {
            ch->setValue(reinterpret_cast<const uint8_t*>(&e), sizeof(e));
        } else {
            ch->setValue(reinterpret_cast<const uint8_t*>(&e), 0);
        }
    }

private:
    Service& s_;
};

// ---------------------------------------------------------------- Service
bool Service::begin(const char* device_name, uint32_t passkey, const Handlers& h, const char* device_info) {
    h_ = h;

    if (!NimBLEDevice::init(device_name)) return false;
    NimBLEDevice::setSecurityAuth(/*bonding=*/true, /*mitm=*/true, /*sc=*/true);
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_ONLY);  // we "display" the static passkey; phone enters it
    NimBLEDevice::setSecurityPasskey(passkey);

    server_ = NimBLEDevice::createServer();
    if (!server_) return false;
    server_->setCallbacks(new ServerCb(*this), /*deleteCallbacks=*/true);
    server_->advertiseOnDisconnect(true);

    NimBLEService* svc = server_->createService(cfg::UUID_SERVICE);
    auto* cb = new CharCb(*this);

    // NimBLE grants the ATT write/read permission only with the plain WRITE/READ flag; WRITE_ENC and WRITE_AUTHEN
    // are modifiers on top of it (ble_gatts.c ble_gatts_chr_properties). Without P::WRITE every write came back
    // Write Not Permitted (found 2026-10-22, first phone-side write test).
    using P = NIMBLE_PROPERTY;
    ch_pit_ = svc->createCharacteristic(cfg::UUID_PIT_TEMP, P::READ | P::NOTIFY, sizeof(float));
    ch_setpoint_ = svc->createCharacteristic(cfg::UUID_SETPOINT, P::READ | P::WRITE | P::WRITE_ENC | P::WRITE_AUTHEN | P::NOTIFY, sizeof(float));
    ch_duty_ = svc->createCharacteristic(cfg::UUID_BLOWER_DUTY, P::READ | P::NOTIFY, 1);
    ch_state_ = svc->createCharacteristic(cfg::UUID_STATE, P::READ | P::NOTIFY, 1);
    ch_fault_ = svc->createCharacteristic(cfg::UUID_FAULT, P::READ | P::NOTIFY, 1);
    ch_mode_ = svc->createCharacteristic(cfg::UUID_MODE, P::WRITE | P::WRITE_ENC | P::WRITE_AUTHEN, 1);
    ch_manual_ = svc->createCharacteristic(cfg::UUID_MANUAL_DUTY, P::WRITE | P::WRITE_ENC | P::WRITE_AUTHEN, 1);
    ch_log_ = svc->createCharacteristic(cfg::UUID_EVENT_LOG, P::READ | P::READ_ENC, sizeof(evlog::Event));
    ch_info_ = svc->createCharacteristic(cfg::UUID_DEVICE_INFO, P::READ, 64);
    ch_warn_ = svc->createCharacteristic(cfg::UUID_WARNINGS, P::READ | P::NOTIFY, 1);
    ch_timer_ = svc->createCharacteristic(cfg::UUID_TIMER, P::READ | P::WRITE | P::WRITE_ENC | P::WRITE_AUTHEN | P::NOTIFY, sizeof(uint16_t));
    ch_console_ = svc->createCharacteristic(cfg::UUID_CONSOLE, P::WRITE | P::WRITE_ENC | P::WRITE_AUTHEN | P::NOTIFY, 96);
    ch_telem_ = svc->createCharacteristic(cfg::UUID_TELEMETRY, P::NOTIFY, 160);

    ch_setpoint_->setCallbacks(cb);
    ch_mode_->setCallbacks(cb);
    ch_manual_->setCallbacks(cb);
    ch_log_->setCallbacks(cb);
    ch_console_->setCallbacks(cb);
    ch_info_->setValue(device_info);

    const float zero_f = 0.0f;
    const uint8_t zero_u8 = 0;
    ch_pit_->setValue(zero_f);
    ch_setpoint_->setValue(zero_f);
    ch_duty_->setValue(zero_u8);
    ch_state_->setValue(zero_u8);
    ch_fault_->setValue(zero_u8);
    ch_warn_->setValue(zero_u8);
    // NimBLE 2.x: services start with the server; NimBLEService::start() is deprecated and a no-op.

    // Legacy advertising payload is 31 bytes: flags (3) + 128-bit service UUID (18) fit; the name does not fit
    // alongside them, so it goes in the scan response (VERIFIED NimBLE-Arduino 2.5.1 NimBLEAdvertising.h /
    // NimBLEAdvertisementData.h: setAdvertisementData, setScanResponseData, setFlags, addServiceUUID, setName).
    NimBLEAdvertisementData adv_data;
    adv_data.setFlags(BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP);
    adv_data.addServiceUUID(cfg::UUID_SERVICE);
    NimBLEAdvertisementData scan_data;
    scan_data.setName(device_name);

    NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
    adv->setAdvertisementData(adv_data);
    adv->setScanResponseData(scan_data);
    return NimBLEDevice::startAdvertising();
}

void Service::publishPit(float c) {
    if (!ch_pit_) return;
    ch_pit_->setValue(c);
    if (connected()) ch_pit_->notify();
}

void Service::publishDuty(uint8_t pct) {
    if (!ch_duty_) return;
    ch_duty_->setValue(pct);
    if (connected()) ch_duty_->notify();
}

void Service::publishState(control::State s) {
    if (!ch_state_) return;
    const uint8_t v = static_cast<uint8_t>(s);
    ch_state_->setValue(v);
    if (connected()) ch_state_->notify();
}

void Service::publishFault(control::Fault f) {
    if (!ch_fault_) return;
    const uint8_t v = static_cast<uint8_t>(f);
    ch_fault_->setValue(v);
    if (connected()) ch_fault_->notify();
}

void Service::publishSetpoint(float c) {
    if (!ch_setpoint_) return;
    ch_setpoint_->setValue(c);
    if (connected()) ch_setpoint_->notify();
}

void Service::publishWarnings(uint8_t mask) {
    if (!ch_warn_) return;
    ch_warn_->setValue(mask);
    if (connected()) ch_warn_->notify();
}

// Chunked at the peer's ATT MTU minus 3, because the NimBLE host truncates longer notifications silently.
// ponytail: first peer's MTU only; one phone or PC at a time is the use case.
void Service::notifyText(NimBLECharacteristic* ch, const char* text, size_t len) {
    if (!ch || !connected() || len == 0) return;
    const uint16_t mtu = server_->getPeerInfo(0).getMTU();
    const size_t chunk = mtu > 3 ? mtu - 3 : 20;
    for (size_t off = 0; off < len; off += chunk) {
        const size_t n = len - off < chunk ? len - off : chunk;
        ch->notify(reinterpret_cast<const uint8_t*>(text + off), n);
    }
}

void Service::publishTelemetry(const char* csv_line) { notifyText(ch_telem_, csv_line, strlen(csv_line)); }
void Service::publishConsole(const char* text, size_t len) { notifyText(ch_console_, text, len); }

void Service::publishTimer(uint16_t minutes_left) {
    if (!ch_timer_) return;
    ch_timer_->setValue(minutes_left);
    if (connected()) ch_timer_->notify();
}

bool Service::connected() const { return server_ && server_->getConnectedCount() > 0; }

void Service::openPairingWindow(uint32_t window_ms) {
    pairing_open_ = true;
    pairing_end_ms_ = millis() + window_ms;
    log_i("BLE pairing window open for %u ms", static_cast<unsigned>(window_ms));
}

void Service::service(uint32_t now_ms) {
    if (pairing_open_ && static_cast<int32_t>(now_ms - pairing_end_ms_) >= 0) {
        pairing_open_ = false;
        log_i("BLE pairing window closed");
    }
}

}  // namespace ble
