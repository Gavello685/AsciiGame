#include "game/time_system.h"
#include <cmath>

void TimeSystem::advance() {
    turn_++;
    if (turn_ >= CYCLE_LENGTH) {
        turn_ = 0;
        day_++;
    }
}

TimeOfDay TimeSystem::time_of_day() const {
    if (turn_ < 60) return TimeOfDay::Dawn;
    if (turn_ < 360) return TimeOfDay::Day;
    if (turn_ < 420) return TimeOfDay::Dusk;
    return TimeOfDay::Night;
}

uint8_t TimeSystem::ambient_light() const {
    if (turn_ < 60) {
        // Dawn: 4 → 15
        return static_cast<uint8_t>(4 + static_cast<int>(11.0 * turn_ / 59.0));
    } else if (turn_ < 360) {
        // Day: 15
        return 15;
    } else if (turn_ < 420) {
        // Dusk: 15 → 4
        int elapsed = turn_ - 360;
        return static_cast<uint8_t>(15 - static_cast<int>(11.0 * elapsed / 59.0));
    } else {
        // Night: 2
        return 2;
    }
}

std::string TimeSystem::time_string() const {
    // Map 600 turns to 24-hour clock (1 turn = 2.4 minutes)
    int total_minutes = turn_ * 24 * 60 / CYCLE_LENGTH;
    int hours = total_minutes / 60;
    int minutes = total_minutes % 60;
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d", hours, minutes);
    return std::string(buf);
}

std::string TimeSystem::time_period_string() const {
    switch (time_of_day()) {
        case TimeOfDay::Dawn:  return "Dawn";
        case TimeOfDay::Day:   return "Day";
        case TimeOfDay::Dusk:  return "Dusk";
        case TimeOfDay::Night: return "Night";
    }
    return "Day";
}
