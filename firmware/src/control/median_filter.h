// Fixed-window median filter. Pure C++, no Arduino dependency.
#pragma once

#include <algorithm>
#include <array>
#include <cstddef>

namespace control {

template <size_t N>
class MedianFilter {
    static_assert(N % 2 == 1, "window must be odd");

public:
    void push(float v) {
        buf_[head_] = v;
        head_ = (head_ + 1) % N;
        if (count_ < N) ++count_;
    }

    bool ready() const { return count_ == N; }

    // Median of the samples present. With fewer than N samples, median of what is there.
    float value() const {
        std::array<float, N> tmp{};
        for (size_t i = 0; i < count_; ++i) tmp[i] = buf_[i];
        std::sort(tmp.begin(), tmp.begin() + count_);
        return count_ ? tmp[count_ / 2] : 0.0f;
    }

    void reset() { head_ = 0; count_ = 0; }

private:
    std::array<float, N> buf_{};
    size_t head_ = 0;
    size_t count_ = 0;
};

}  // namespace control
