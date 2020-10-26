#include "dsp.hpp"

namespace nes {

void antialiasing_filter::push_back(float input) noexcept {
    history_[last_index_++] = input;
    if (last_index_ == tap_num) {
        last_index_ = 0;
    }
}

float antialiasing_filter::calculate_filtered_sample() noexcept {
    static constexpr array<double, tap_num> filter_taps{
        {-0.000165371425938316,  -0.0010142366677726668, -0.0031915882103072985,
         -0.006473383207434769,  -0.00858335618521196,   -0.005688368443273637,
         0.004178793334531979,   0.016591628711275536,   0.02030909322310278,
         0.004928485816587725,   -0.02658527078058321,   -0.05151827373029294,
         -0.03730493935172431,   0.034555098175678936,   0.14772597039690868,
         0.2528709265676202,     0.2957421307452675,     0.2528709265676202,
         0.14772597039690868,    0.034555098175678936,   -0.03730493935172431,
         -0.05151827373029294,   -0.02658527078058321,   0.004928485816587725,
         0.02030909322310278,    0.016591628711275536,   0.004178793334531979,
         -0.005688368443273637,  -0.00858335618521196,   -0.006473383207434769,
         -0.0031915882103072985, -0.0010142366677726668, -0.000165371425938316}};

    // TODO: write an iterable ring buffer and use std::accumulate
    double acc = 0;
    std::size_t index = last_index_;
    for (int i = 0; i < tap_num; ++i) {
        index = (index != 0) ? (index - 1) : (tap_num - 1);
        acc += history_[index] * filter_taps[i];
    };

    return static_cast<float>(acc);
}

} // namespace nes
