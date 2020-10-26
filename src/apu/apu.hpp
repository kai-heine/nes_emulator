#ifndef NES_APU_APU_HPP
#define NES_APU_APU_HPP

#include "dsp.hpp"
#include "types.hpp"
#include <algorithm>
#include <cassert>
#include <span>

namespace nes {

namespace utility {

constexpr void set_upper_byte(u16& destination, u8 value) noexcept {
    destination = (destination & 0x00ff) | (value << 8);
}
constexpr void set_lower_byte(u16& destination, u8 value) noexcept {
    destination = (destination & 0xff00) | value;
}

} // namespace utility

// divider and timer are the same thing
template <std::size_t Bits>
class timer {
    static_assert(Bits <= 64);

  public:
    using unterlying_type = std::conditional_t<
        Bits <= 8, u8,
        std::conditional_t<Bits <= 16, u16, std::conditional_t<Bits <= 32, u32, u64>>>;

    unterlying_type reload_value{}; // not a bit field so that its address can be taken

    [[nodiscard]] constexpr bool clock() const noexcept { return counter_ == 0; }

    constexpr void reload() noexcept { counter_ = reload_value; }

    constexpr void step() noexcept {
        if (counter_ == 0) {
            reload();
        } else {
            --counter_;
        }
    }

  private:
    unterlying_type counter_ : Bits{};
};

class frame_counter {
  public:
    constexpr void handle_register_write(u8 value) noexcept {
        sequencer_mode_ = ((value & 0x80) == 0) ? mode::four_step : mode::five_step;
        interrupt_inhibit_ = (value & 0x40) != 0;
        clear_frame_interrupt();
    }

    constexpr void step() noexcept {
        ++cycle_count_;
        if (cycle_count_ >= (sequencer_mode_ == mode::four_step ? 29830 : 37282)) {
            cycle_count_ = 0;
        }

        if (!interrupt_inhibit_ && (sequencer_mode_ == mode::four_step) &&
            (cycle_count_ == 29828)) {
            frame_interrupt_ = true;
        } else if (cycle_count_ == 1) {
            frame_interrupt_ = false;
        }
    }

    // every second cpu cycle
    constexpr bool apu_clock() const noexcept { return (cycle_count_ % 2) == 1; }

    constexpr bool half_frame_clock() const noexcept {
        return (cycle_count_ == 14913) ||
               ((sequencer_mode_ == mode::four_step) && (cycle_count_ == 29829)) ||
               ((sequencer_mode_ == mode::five_step) && (cycle_count_ == 37281));
    }

    constexpr bool quarter_frame_clock() const noexcept {
        return half_frame_clock() || (cycle_count_ == 7457) || (cycle_count_ == 22371);
    }

    constexpr bool frame_interrupt() const noexcept { return frame_interrupt_; }
    constexpr void clear_frame_interrupt() noexcept { frame_interrupt_ = false; }

  private:
    enum class mode : bool {
        four_step,
        five_step,
    } sequencer_mode_{};
    bool interrupt_inhibit_{};
    bool frame_interrupt_{};
    std::size_t cycle_count_{};
};

class envelope_generator {
    // clocked every quarter frame
    // generates sawtooth or constant volume
    // output: value 0..15 (4 bits)
  public:
    constexpr void handle_register_write(u8 value) noexcept {
        // --LC.VVVV
        loop_ = (value & 0x20) != 0;
        constant_volume_ = (value & 0x10) != 0;
        max_volume_ = value & 0x0f;

        decay_timer_.reload_value = value & 0x0f;
    }

    // every quarter frame
    constexpr void step() noexcept {
        if (start_) {
            start_ = false;
            decay_level_ = 15;
            decay_timer_.reload();
            return;
        }

        if (decay_timer_.clock()) {
            if (decay_level_ == 0) {
                if (loop_) {
                    decay_level_ = 15;
                }
            } else {
                --decay_level_;
            }
        }

        decay_timer_.step();
    }

    constexpr void restart() noexcept { start_ = true; }

    constexpr u8 volume() const noexcept { return constant_volume_ ? max_volume_ : decay_level_; }

  private:
    u8 max_volume_ : 4 {}; // "envelope parameter"
    bool constant_volume_ : 1 {};
    bool loop_ : 1 {};

    timer<4> decay_timer_{}; // "divider"
    bool start_{};
    u8 decay_level_ : 4 {15};
};

class sweep_generator {
  public:
    constexpr void handle_register_write(u8 value) noexcept {
        shift_count_ = value & 0x07;
        negate_ = (value & 0x08) != 0;
        enabled_ = (value & 0x80) != 0;

        reload_ = true; // side effect
        sweep_timer_.reload_value = (value >> 4) & 0x07;
    }

    // every half frame
    constexpr u16 step(u16 current_timer_period) noexcept {
        if (sweep_timer_.clock() || reload_) {
            sweep_timer_.reload();
            reload_ = false;
        } else {
            sweep_timer_.step();
        }

        // TODO ones complement on pulse1, twos complement on pulse2
        auto const change_amount = (current_timer_period >> shift_count_) * (negate_ ? -1 : 1);
        target_period_ = static_cast<u16>(current_timer_period + change_amount);

        if (enabled_ && sweep_timer_.clock() && !mute()) {
            return target_period_;
        }
        return current_timer_period;
    }

    [[nodiscard]] constexpr bool mute() const noexcept {
        // TODO current period less than 8: check in here or in pulse channel?
        return target_period_ > 0x7ff;
    }

  private:
    u8 shift_count_ : 3 {};
    bool negate_ : 1 {};
    u8 padding : 3 {};
    bool enabled_ : 1 {};

    bool reload_{};
    timer<3> sweep_timer_{}; // "divider"
    u16 target_period_{};
};

template <typename T, std::size_t Steps>
class sequencer {
  public:
    using range = std::span<T const, Steps>;
    // has a sequence and an index into the sequence
    // pulse: 4 different 8-step-sequences of 1 and 0
    // triangle: 32-step-sequence 0..15
    explicit constexpr sequencer(range sequence) noexcept : sequence_{sequence} {}
    constexpr void set_sequence(range new_sequence) noexcept { sequence_ = new_sequence; }
    constexpr void restart() noexcept { current_position_ = 0; }
    constexpr void step() noexcept { current_position_ = (current_position_ + 1) % Steps; }
    constexpr T output() const noexcept { return sequence_[current_position_]; }

  private:
    range sequence_;
    std::size_t current_position_{};
};

class length_counter {
  public:
    constexpr void step() noexcept {
        if ((length_ != 0) && !halt_) {
            length_--;
        }
    }
    constexpr void set_length(u8 table_index) noexcept {
        if (enabled_) {
            length_ = length_table_[table_index % 32];
        }
    }
    constexpr void enable() noexcept { enabled_ = true; }
    constexpr void disable() noexcept {
        enabled_ = false;
        length_ = 0;
    }
    constexpr void halt() noexcept { halt_ = true; }
    // my wayward son
    constexpr void carry_on() noexcept { halt_ = false; }

    [[nodiscard]] constexpr auto length() const noexcept { return length_; }

  private:
    static constexpr array<u8, 32> length_table_{{
        10, 254, 20, 2,  40, 4,  80, 6,  160, 8,  60, 10, 14, 12, 26, 14,
        12, 16,  24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30,
    }};
    bool enabled_{};
    bool halt_{};
    u8 length_{};
};

class pulse_channel {
  public:
    enum class registers : u8 { duty_envelope, sweep, timer_low, timer_high };

    constexpr void handle_register_write(registers register_select, u8 value) noexcept {
        switch (register_select) {
        case registers::duty_envelope: {
            // DDlc.vvvv Duty cycle, length counter halt, constant volume, env divider period
            std::size_t const duty_cycle = (value >> 6) & 0x03;
            sequencer_.set_sequence(sequences_[duty_cycle]);
            envelope_.handle_register_write(value & 0x3f);
            if ((value & 0x20) != 0) {
                length_counter_.halt();
            } else {
                length_counter_.carry_on();
            }
        } break;
        case registers::sweep: {
            sweep_.handle_register_write(value);
        } break;
        case registers::timer_low: {
            utility::set_lower_byte(sequence_timer_.reload_value, value);
        } break;
        case registers::timer_high: {
            utility::set_upper_byte(sequence_timer_.reload_value, value & 0x07);
            envelope_.restart();
            length_counter_.set_length(value >> 3);
            sequencer_.restart();
        } break;
        default: assert(false);
        }
    }

    // every second cpu cycle
    constexpr void step() noexcept {
        sequence_timer_.step();
        if (sequence_timer_.clock()) {
            sequencer_.step();
        }
    }
    constexpr void half_frame_step() noexcept {
        sequence_timer_.reload_value = sweep_.step(sequence_timer_.reload_value);
        length_counter_.step();
    }
    constexpr void quarter_frame_step() noexcept { envelope_.step(); }

    constexpr u8 output() const noexcept {
        if (sweep_.mute() || !sequencer_.output() || (sequence_timer_.reload_value < 8) ||
            (length_counter_.length() == 0)) {
            return 0;
        }
        return envelope_.volume();
    }

    constexpr void enable() noexcept { length_counter_.enable(); }
    constexpr void disable() noexcept { length_counter_.disable(); }
    [[nodiscard]] constexpr bool enabled() const noexcept { return length_counter_.length() > 0; }

  private:
    static constexpr array<array<bool, 8>, 4> sequences_{{
        {1, 0, 0, 0, 0, 0, 0, 0},
        {1, 1, 0, 0, 0, 0, 0, 0},
        {1, 1, 1, 1, 0, 0, 0, 0},
        {1, 1, 1, 1, 1, 1, 0, 0},
    }};

    envelope_generator envelope_{}; // controls volume
    sweep_generator sweep_{};       // controls timer
    timer<11> sequence_timer_{};    // controls sequencer
    sequencer<bool, 8> sequencer_{sequences_[0]};
    length_counter length_counter_{};
};

class triangle_channel {
  public:
    enum class registers : u8 { linear_counter_setup, timer_low = 2, timer_high };

    constexpr void handle_register_write(registers register_select, u8 value) noexcept {
        switch (register_select) {
        case registers::linear_counter_setup: {
            linear_counter_reload_value_ = value & 0x7f;
            control_ = (value & 0x80) != 0;
            if (control_) {
                length_counter_.halt();
            } else {
                length_counter_.carry_on();
            }
        } break;

        case registers::timer_low: {
            utility::set_lower_byte(sequence_timer_.reload_value, value);
        } break;
        case registers::timer_high: {
            utility::set_upper_byte(sequence_timer_.reload_value, value & 0x07);
            length_counter_.set_length(value >> 3);
            linear_counter_reload_ = true;
        } break;
        default: assert(false);
        }
    }

    // every cpu cycle
    constexpr void step() noexcept {
        sequence_timer_.step();
        if (sequence_timer_.clock() && (length_counter_.length() != 0) && (linear_counter_ != 0)) {
            sequencer_.step();
        }
    }

    constexpr void quarter_frame_step() noexcept {
        if (linear_counter_reload_) {
            linear_counter_ = linear_counter_reload_value_;
        } else if (linear_counter_ != 0) {
            --linear_counter_;
        }

        if (!control_) {
            linear_counter_reload_ = false;
        }
    }

    constexpr void half_frame_step() noexcept { length_counter_.step(); }

    constexpr u8 output() const noexcept { return sequencer_.output(); }

    constexpr void enable() noexcept { length_counter_.enable(); }
    constexpr void disable() noexcept { length_counter_.disable(); }
    [[nodiscard]] constexpr bool enabled() const noexcept { return length_counter_.length() > 0; }

  private:
    static constexpr array<u8, 32> sequence_{{
        15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5,  4,  3,  2,  1,  0,
        0,  1,  2,  3,  4,  5,  6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
    }};

    timer<11> sequence_timer_{};
    sequencer<u8, 32> sequencer_{sequence_};
    length_counter length_counter_{};
    u8 linear_counter_reload_value_ : 7 {};
    u8 linear_counter_ : 7 {};
    bool linear_counter_reload_{};
    bool control_{};
};

class noise_channel {};            // TODO
class delta_modulation_channel {}; // TODO

constexpr float mix(u8 pulse1, u8 pulse2, u8 triangle, u8 noise, u8 dmc) noexcept {
    auto const pulse_out = 95.88 / ((8128.0 / (pulse1 + pulse2)) + 100.0);
    auto const tnd_out =
        159.79 / ((1.0 / ((triangle / 8227.0) + (noise / 12241.0) + (dmc / 22638.0))) + 100.0);
    return static_cast<float>(pulse_out + tnd_out);
}

class audio_processing_unit {
  public:
    static constexpr std::size_t sample_rate = 44100;

    constexpr u8 read(u16 address) noexcept {
        assert(address >= 0x4000);
        assert(address < 0x4018);
        assert(address != 0x4014);
        // 0x4015 status only
        if (address != 0x4015) {
            return 0;
        }

        auto const frame_interrupt = frame_counter_.frame_interrupt();
        frame_counter_.clear_frame_interrupt();

        // TODO: triangle, noise, dmc, dmc interrupt
        return ((pulse1_.enabled() << 0) & 0x01) | ((pulse2_.enabled() << 1) & 0x02) |
               ((triangle_.enabled() << 2) & 0x04) | ((frame_interrupt << 7) & 0x40);
    }
    constexpr void write(u16 address, u8 value) noexcept {
        assert(address >= 0x4000);
        assert(address < 0x4018);
        assert(address != 0x4014);

        address %= 0x4000;

        if (address < 0x08) {
            auto const pulse_register = static_cast<pulse_channel::registers>(address % 4);
            auto& pulse = ((address < 0x04) ? pulse1_ : pulse2_);
            pulse.handle_register_write(pulse_register, value);
        } else if (address < 0x0c) {
            auto const triangle_register = static_cast<triangle_channel::registers>(address % 4);
            triangle_.handle_register_write(triangle_register, value);
        } else if (address < 0x10) {
            // noise
        } else if (address < 0x14) {
            // dmc
        } else if (address == 0x15) {
            // status
            // TODO: dmc, noise, triangle
            // TODO: cleaner with enable(bool) ?
            if ((value & 0x01) != 0) {
                pulse1_.enable();
            } else {
                pulse1_.disable();
            }
            if ((value & 0x02) != 0) {
                pulse2_.enable();
            } else {
                pulse2_.disable();
            }
            if ((value & 0x04) != 0) {
                triangle_.enable();
            } else {
                triangle_.disable();
            }
        } else if (address == 0x17) {
            frame_counter_.handle_register_write(value);
        }
    }

    [[nodiscard]] constexpr bool interrupt() const noexcept {
        return frame_counter_.frame_interrupt(); // TODO dmc?
    }

    // every cpu cycle
    constexpr void step() noexcept {
        frame_counter_.step();
        triangle_.step();

        if (frame_counter_.apu_clock()) {
            pulse1_.step();
            pulse2_.step();
        }

        if (frame_counter_.quarter_frame_clock()) {
            pulse1_.quarter_frame_step();
            pulse2_.quarter_frame_step();
            triangle_.quarter_frame_step();
        }

        if (frame_counter_.half_frame_clock()) {
            pulse1_.half_frame_step();
            pulse2_.half_frame_step();
            triangle_.half_frame_step();
        }

        // 2x oversampling
        constexpr std::size_t cycles_per_sample = 1789773 / (sample_rate * 2);
        cpu_cycle_count_++;
        if (cpu_cycle_count_ > cycles_per_sample) {
            cpu_cycle_count_ = 0; // rounding errors?

            // TODO: stereo panning of channels would be cool

            lpf.push_back(
                hpf.process(mix(pulse1_.output(), pulse2_.output(), triangle_.output(), 0, 0)));

            // downsample by writing every second sample
            if (write_sample) {
                *write_pointer_++ = lpf.calculate_filtered_sample();
            }
            write_sample = !write_sample;

            if (write_pointer_ == sample_buffer_.end()) {
                write_pointer_ = sample_buffer_.begin();
            }
        }
    }

    // copies the requested number of samples into the destination buffer (for audio callback)
    // if there are not enough samples, the remaining samples are filled with silence
    void read_samples(std::span<float> destination) noexcept {
        auto begin = destination.begin();
        while (begin != destination.end()) {
            if (read_pointer_ == write_pointer_) {
                // not enough samples: fill with silence
                *begin++ = 0.0f;
            } else {
                // copy sample
                *begin++ = *read_pointer_++;
            }
            if (read_pointer_ == sample_buffer_.end()) {
                read_pointer_ = sample_buffer_.begin();
            }
        }
    }

    // returns span of all samples written since last call (for audio queue)
    std::span<float const> get_sample_buffer() noexcept {
        // not using read pointer with this approach
        std::size_t const length = std::distance(sample_buffer_.begin(), write_pointer_);
        write_pointer_ = sample_buffer_.begin();
        return {sample_buffer_.data(), length};
    }

  private:
    frame_counter frame_counter_{};
    pulse_channel pulse1_{};
    pulse_channel pulse2_{};
    triangle_channel triangle_{};
    noise_channel noise_{};
    delta_modulation_channel dmc_{};

    // sampling
    std::size_t cpu_cycle_count_{};
    vector<float> sample_buffer_ = vector<float>(44100); // TODO: ring buffer type
    vector<float>::iterator write_pointer_{sample_buffer_.begin()};
    vector<float>::iterator read_pointer_{sample_buffer_.begin()};
    first_order_highpass_filter<sample_rate, 37> hpf;
    antialiasing_filter lpf;
    bool write_sample = false;
};

} // namespace nes

#endif
