// Fixed-capacity RAM ring buffer of events. Flash persistence (LittleFS/NVS blob) is Phase 2.
// Packed record so it can be pushed over BLE as-is.
#pragma once

#include <cstddef>
#include <cstdint>

namespace evlog {

enum class EventType : uint8_t {
    BOOT = 0,
    STATE_CHANGE = 1,
    FAULT = 2,
    SETPOINT_CHANGE = 3,
    MODE_CHANGE = 4,
    BLE_CONNECT = 5,
    BLE_DISCONNECT = 6,
    SAMPLE = 7,  // periodic telemetry sample (pit temp, duty)
    POWER_RESUME = 8,  // boot found cook_active set: cook resumed automatically
    WARNING = 9,       // warning bitmask changed (a = new mask)
};

struct __attribute__((packed)) Event {
    uint32_t uptime_s;
    EventType type;
    uint8_t a;      // state / fault / duty depending on type
    uint8_t b;      // secondary
    uint8_t _pad;
    float value;    // temperature or setpoint
};
static_assert(sizeof(Event) == 12, "Event must stay 12 bytes for the BLE record format");

template <size_t CAP>
class EventLog {
public:
    void push(const Event& e) {
        buf_[head_] = e;
        head_ = (head_ + 1) % CAP;
        if (count_ < CAP) ++count_; else tail_ = (tail_ + 1) % CAP;
        ++seq_;
    }

    size_t size() const { return count_; }
    uint32_t sequence() const { return seq_; }  // total pushed since boot

    // Oldest-first access. idx in [0, size()).
    const Event& at(size_t idx) const { return buf_[(tail_ + idx) % CAP]; }

    // Pop oldest into out; false if empty. Used by the BLE chunked reader.
    bool pop(Event& out) {
        if (count_ == 0) return false;
        out = buf_[tail_];
        tail_ = (tail_ + 1) % CAP;
        --count_;
        return true;
    }

    void clear() { head_ = tail_ = count_ = 0; }

private:
    Event buf_[CAP]{};
    size_t head_ = 0, tail_ = 0, count_ = 0;
    uint32_t seq_ = 0;
};

}  // namespace evlog
