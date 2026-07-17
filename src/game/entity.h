#pragma once

#include <cstdint>
#include <string>

class Entity {
public:
    Entity() = default;
    Entity(int x, int y, uint32_t glyph, const std::string& name,
           uint8_t fg_r, uint8_t fg_g, uint8_t fg_b, bool blocking = false);

    int x() const { return x_; }
    int y() const { return y_; }
    void set_pos(int x, int y) { x_ = x; y_ = y; }

    uint32_t glyph() const { return glyph_; }
    const std::string& name() const { return name_; }

    uint8_t fg_r() const { return fg_r_; }
    uint8_t fg_g() const { return fg_g_; }
    uint8_t fg_b() const { return fg_b_; }

    bool blocking() const { return blocking_; }
    void set_blocking(bool b) { blocking_ = b; }

    bool visible() const { return visible_; }
    void set_visible(bool v) { visible_ = v; }

    bool in_inventory() const { return in_inventory_; }
    void set_in_inventory(bool v) { in_inventory_ = v; }

    bool is_item() const { return is_item_; }
    void set_is_item(bool v) { is_item_ = v; }

private:
    int x_ = 0;
    int y_ = 0;
    uint32_t glyph_ = ' ';
    std::string name_;
    uint8_t fg_r_ = 255, fg_g_ = 255, fg_b_ = 255;
    bool blocking_ = false;
    bool visible_ = true;
    bool in_inventory_ = false;
    bool is_item_ = false;
};
