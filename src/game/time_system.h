#pragma once

#include <cstdint>
#include <string>

enum class TimeOfDay : uint8_t {
    Dawn,
    Day,
    Dusk,
    Night,
};

class TimeSystem {
public:
    TimeSystem() = default;

    void advance();

    int turn_of_day() const { return turn_; }
    void set_turn_of_day(int t) { turn_ = t % CYCLE_LENGTH; }

    TimeOfDay time_of_day() const;
    uint8_t ambient_light() const;
    std::string time_string() const;
    std::string time_period_string() const;

    static const int CYCLE_LENGTH = 600;

private:
    int turn_ = 150; // Start at midday (turn 150 = Day phase)
};
