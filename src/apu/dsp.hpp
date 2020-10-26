#ifndef NES_APU_DSP_HPP
#define NES_APU_DSP_HPP

#include "../types.hpp"
#include <numbers>

namespace nes {

// first order iir highpass filter stolen from wikipedia
template <std::size_t SampleRateHz, std::size_t CutoffFrequencyHz>
class first_order_highpass_filter {
  public:
    constexpr float process(float x) noexcept {
        float const y = alpha_ * (last_y_ + x - last_x_);
        last_y_ = y;
        last_x_ = x;
        return y;
    }

  private:
    static constexpr float alpha_ = static_cast<float>(
        1.0 / (2 * std::numbers::pi * (1.0 / SampleRateHz) * CutoffFrequencyHz + 1.0));

    float last_y_ = 0.0f;
    float last_x_ = 0.0f;
};

// fir lowpass filter generated with t-filter.engineeringjs.com
// samplerate 88.2kHz, passband <=10kHz, stopband >=20kHz with >100dB attenuation
class antialiasing_filter {
  public:
    void push_back(float input) noexcept;
    float calculate_filtered_sample() noexcept;

  private:
    static constexpr std::size_t tap_num = 33;

    array<double, tap_num> history_{};
    std::size_t last_index_{};
};

} // namespace nes

#endif
