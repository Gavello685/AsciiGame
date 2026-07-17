#include "game/entity.h"

Entity::Entity(int x, int y, uint32_t glyph, const std::string& name,
               uint8_t fg_r, uint8_t fg_g, uint8_t fg_b, bool blocking)
    : x_(x), y_(y), glyph_(glyph), name_(name),
      fg_r_(fg_r), fg_g_(fg_g), fg_b_(fg_b), blocking_(blocking) {
}
