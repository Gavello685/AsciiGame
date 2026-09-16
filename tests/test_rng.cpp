#include "test_framework.h"

#include "game/rng.h"

#include <vector>

// The world claims to be reproducible from its seed. That only holds if every
// random draw comes from a seeded generator whose state can be saved, which is
// why std::rand() was replaced.

TEST(same_seed_produces_the_same_sequence) {
    Rng a(12345);
    Rng b(12345);
    for (int i = 0; i < 200; ++i) {
        CHECK_EQ(a.next_u64(), b.next_u64());
    }
}

TEST(different_seeds_diverge) {
    Rng a(1);
    Rng b(2);
    bool differs = false;
    for (int i = 0; i < 20 && !differs; ++i) {
        if (a.next_u64() != b.next_u64()) differs = true;
    }
    CHECK_EQ(differs, true);
}

TEST(state_round_trip_resumes_the_sequence) {
    Rng source(99);
    for (int i = 0; i < 50; ++i) source.next_u64();

    Rng resumed;
    resumed.set_state(source.state());

    for (int i = 0; i < 50; ++i) {
        CHECK_EQ(resumed.next_u64(), source.next_u64());
    }
}

TEST(zero_seed_does_not_lock_up) {
    // Zero is a fixed point for xorshift; seeding must fold it away.
    Rng rng(0);
    CHECK_NE(rng.state(), uint64_t{0});
    uint64_t first = rng.next_u64();
    uint64_t second = rng.next_u64();
    CHECK_NE(first, second);
}

TEST(below_stays_in_range) {
    Rng rng(4242);
    for (int bound : {1, 2, 4, 6, 10, 100}) {
        for (int i = 0; i < 500; ++i) {
            int value = rng.below(bound);
            CHECK(value >= 0);
            CHECK(value < bound);
        }
    }
    CHECK_EQ(rng.below(0), 0);
    CHECK_EQ(rng.below(-5), 0);
}

TEST(range_is_inclusive_on_both_ends) {
    Rng rng(777);
    bool saw_low = false, saw_high = false;
    for (int i = 0; i < 2000; ++i) {
        int value = rng.range(-2, 2);
        CHECK(value >= -2);
        CHECK(value <= 2);
        if (value == -2) saw_low = true;
        if (value == 2) saw_high = true;
    }
    CHECK_EQ(saw_low, true);
    CHECK_EQ(saw_high, true);
    // Inverted bounds fall back to the low value rather than misbehaving.
    CHECK_EQ(rng.range(5, 1), 5);
}

TEST(variance_is_symmetric_and_bounded) {
    Rng rng(31337);
    int sum = 0;
    const int samples = 4000;
    for (int i = 0; i < samples; ++i) {
        int value = rng.variance(2);
        CHECK(value >= -2);
        CHECK(value <= 2);
        sum += value;
    }
    // A symmetric draw should not drift far from zero over many samples.
    CHECK(std::abs(sum) < samples / 4);
    CHECK_EQ(rng.variance(0), 0);
    CHECK_EQ(rng.variance(-1), 0);
}

TEST(percent_respects_its_bounds) {
    Rng rng(24680);
    for (int i = 0; i < 200; ++i) {
        CHECK_EQ(rng.percent(0), false);
        CHECK_EQ(rng.percent(100), true);
    }

    int hits = 0;
    const int samples = 4000;
    for (int i = 0; i < samples; ++i) {
        if (rng.percent(25)) hits++;
    }
    // Loose bounds: this asserts the draw is roughly calibrated, not exact.
    CHECK(hits > samples / 8);
    CHECK(hits < samples / 2);
}

TEST(draws_cover_their_range) {
    // A generator that always returned the same value would satisfy the range
    // checks above, so confirm the distribution actually spreads out.
    Rng rng(13579);
    std::vector<int> buckets(6, 0);
    for (int i = 0; i < 3000; ++i) buckets[static_cast<size_t>(rng.below(6))]++;
    for (int count : buckets) CHECK(count > 0);
}
