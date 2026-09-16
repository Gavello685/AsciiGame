#pragma once

#include <cstdint>

// Small deterministic PRNG (xorshift64*).
//
// Replaces std::rand() so that a run is reproducible from its seed: the world
// seed drives terrain, and this drives everything else (combat rolls, AI
// wander, spawn jitter). The state is a single integer, so it round-trips
// through the save file and a reloaded game continues the same sequence.
class Rng {
public:
    Rng() = default;
    explicit Rng(uint64_t seed) { this->seed(seed); }

    void seed(uint64_t s) {
        // Zero is a fixed point for xorshift, so fold it away.
        state_ = s ? s : 0x9E3779B97F4A7C15ull;
    }

    uint64_t state() const { return state_; }
    void set_state(uint64_t s) { seed(s); }

    uint64_t next_u64() {
        state_ ^= state_ >> 12;
        state_ ^= state_ << 25;
        state_ ^= state_ >> 27;
        return state_ * 0x2545F4914F6CDD1Dull;
    }

    // Uniform in [0, bound). Returns 0 for a non-positive bound.
    int below(int bound) {
        if (bound <= 0) return 0;
        return static_cast<int>(next_u64() % static_cast<uint64_t>(bound));
    }

    // Uniform in [lo, hi] inclusive.
    int range(int lo, int hi) {
        if (hi < lo) return lo;
        return lo + below(hi - lo + 1);
    }

    // Uniform in [-spread, +spread].
    int variance(int spread) {
        if (spread <= 0) return 0;
        return range(-spread, spread);
    }

    bool percent(int chance) { return below(100) < chance; }

private:
    uint64_t state_ = 0x9E3779B97F4A7C15ull;
};
