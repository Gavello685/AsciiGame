#include "game/save.h"
#include "game/world.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <ctime>
#include <algorithm>
#include <iomanip>

namespace fs = std::filesystem;

static fs::path save_dir() {
    fs::path dir = fs::path(std::getenv("APPDATA")) / "AsciiGame" / "saves";
    fs::create_directories(dir);
    return dir;
}

static fs::path save_path(int slot) {
    return save_dir() / ("slot_" + std::to_string(slot) + ".json");
}

// ── JSON writers ────────────────────────────────────────────────────

static std::string jesc(const std::string& s) {
    std::string o;
    for (char c : s) {
        switch (c) {
            case '"': o += "\\\""; break;
            case '\\': o += "\\\\"; break;
            case '\n': o += "\\n"; break;
            case '\r': o += "\\r"; break;
            case '\t': o += "\\t"; break;
            default: o += c; break;
        }
    }
    return o;
}

static std::string jstr(const std::string& k, const std::string& v, bool comma = true) {
    return "\"" + k + "\": \"" + jesc(v) + "\"" + (comma ? "," : "");
}

static std::string jint(const std::string& k, int v, bool comma = true) {
    return "\"" + k + "\": " + std::to_string(v) + (comma ? "," : "");
}

static std::string juint(const std::string& k, unsigned v, bool comma = true) {
    return "\"" + k + "\": " + std::to_string(v) + (comma ? "," : "");
}

static std::string jbool(const std::string& k, bool v, bool comma = true) {
    return "\"" + k + "\": " + (v ? "true" : "false") + (comma ? "," : "");
}

static std::string hex_string(const std::vector<bool>& bits) {
    std::string h;
    h.reserve(bits.size() / 4 + 1);
    int val = 0, shift = 0;
    for (bool b : bits) {
        if (b) val |= (1 << shift);
        shift++;
        if (shift == 4) {
            h += "0123456789abcdef"[val];
            val = 0;
            shift = 0;
        }
    }
    if (shift > 0) h += "0123456789abcdef"[val];
    return h;
}

static std::vector<bool> parse_hex(const std::string& h, int expected_bits) {
    std::vector<bool> bits;
    bits.reserve(expected_bits);
    for (char c : h) {
        int nibble = 0;
        if (c >= '0' && c <= '9') nibble = c - '0';
        else if (c >= 'a' && c <= 'f') nibble = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') nibble = c - 'A' + 10;
        else continue;
        for (int i = 0; i < 4 && static_cast<int>(bits.size()) < expected_bits; ++i) {
            bits.push_back((nibble >> i) & 1);
        }
    }
    while (static_cast<int>(bits.size()) < expected_bits) bits.push_back(false);
    return bits;
}

// ── JSON parser helpers ─────────────────────────────────────────────

static std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    size_t end = s.find_last_not_of(" \t\r\n");
    return (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
}

static std::string extract_value(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return "";
    pos = json.find(':', pos + search.size());
    if (pos == std::string::npos) return "";
    pos++;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
    if (pos >= json.size()) return "";

    if (json[pos] == '"') {
        pos++;
        size_t end = pos;
        while (end < json.size() && json[end] != '"') {
            if (json[end] == '\\') end++;
            end++;
        }
        return json.substr(pos, end - pos);
    } else {
        size_t end = pos;
        while (end < json.size() && json[end] != ',' && json[end] != '}' && json[end] != ']') end++;
        return trim(json.substr(pos, end - pos));
    }
}

static int extract_int(const std::string& json, const std::string& key, int def = 0) {
    std::string val = extract_value(json, key);
    if (val.empty()) return def;
    try { return std::stoi(val); } catch (...) { return def; }
}

static bool extract_bool(const std::string& json, const std::string& key, bool def = false) {
    std::string val = extract_value(json, key);
    if (val == "true") return true;
    if (val == "false") return false;
    return def;
}

static std::string extract_string(const std::string& json, const std::string& key, const std::string& def = "") {
    std::string val = extract_value(json, key);
    return val.empty() ? def : val;
}

// Extract a JSON array section by key name — returns the raw array text
static std::string extract_array(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return "";
    pos = json.find('[', pos);
    if (pos == std::string::npos) return "";
    int depth = 0;
    size_t start = pos;
    for (; pos < json.size(); ++pos) {
        if (json[pos] == '[') depth++;
        else if (json[pos] == ']') { depth--; if (depth == 0) return json.substr(start, pos - start + 1); }
    }
    return "";
}

// Extract a JSON object section by key name — returns the raw object text
static std::string extract_object(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return "";
    pos = json.find('{', pos);
    if (pos == std::string::npos) return "";
    int depth = 0;
    size_t start = pos;
    for (; pos < json.size(); ++pos) {
        if (json[pos] == '{') depth++;
        else if (json[pos] == '}') { depth--; if (depth == 0) return json.substr(start, pos - start + 1); }
    }
    return "";
}

// Split a JSON array of objects into individual object strings
static std::vector<std::string> split_array_objects(const std::string& arr) {
    std::vector<std::string> objects;
    int depth = 0;
    size_t start = 0;
    bool in_obj = false;
    for (size_t i = 0; i < arr.size(); ++i) {
        if (arr[i] == '{') { if (depth == 0) { start = i; in_obj = true; } depth++; }
        else if (arr[i] == '}') { depth--; if (depth == 0 && in_obj) { objects.push_back(arr.substr(start, i - start + 1)); in_obj = false; } }
    }
    return objects;
}

// ── Save ────────────────────────────────────────────────────────────

bool save_game(int slot, const GameState& state) {
    if (slot < 0 || slot >= MAX_SAVE_SLOTS) return false;

    std::ostringstream ss;
    ss << "{\n";
    ss << "  " << jint("version", SAVE_VERSION) << "\n";

    // Meta
    ss << "  \"meta\": {\n";
    ss << "    " << jint("slot", slot) << "\n";
    ss << "    " << jstr("character_name", state.player_name) << "\n";
    ss << "    " << jint("play_time_seconds", state.meta.play_time_seconds) << "\n";
    ss << "    " << jint("turn_of_day", state.turn_of_day) << "\n";
    ss << "    " << jstr("timestamp", state.meta.timestamp, false) << "\n";
    ss << "  },\n";

    // Player
    ss << "  \"player\": {\n";
    ss << "    " << jstr("name", state.player_name) << "\n";
    ss << "    " << jint("x", state.player_x) << "\n";
    ss << "    " << jint("y", state.player_y) << "\n";
    ss << "    " << jint("gold", state.gold) << "\n";
    ss << "    " << jint("hp", state.hp) << "\n";
    ss << "    " << jint("xp", state.xp) << "\n";
    ss << "    " << jint("level", state.level) << "\n";
    ss << "    \"stats\": {\n";
    ss << "      " << jint("str", state.stats.str) << "\n";
    ss << "      " << jint("dex", state.stats.dex) << "\n";
    ss << "      " << jint("con", state.stats.con) << "\n";
    ss << "      " << jint("intel", state.stats.intel) << "\n";
    ss << "      " << jint("wis", state.stats.wis) << "\n";
    ss << "      " << jint("cha", state.stats.cha, false) << "\n";
    ss << "    },\n";

    // Inventory
    ss << "    \"inventory\": [\n";
    for (int i = 0; i < static_cast<int>(state.inventory.size()); ++i) {
        const auto& [item, qty] = state.inventory[i];
        ss << "      { \"name\": \"" << jesc(item.name()) << "\", \"qty\": " << qty << " }";
        if (i < static_cast<int>(state.inventory.size()) - 1) ss << ",";
        ss << "\n";
    }
    ss << "    ],\n";

    // Equipment
    ss << "    \"equipment\": [\n";
    {
        int i = 0;
        int count = static_cast<int>(state.equipment.size());
        for (const auto& [slot, item] : state.equipment) {
            ss << "      { " << jint("slot", static_cast<int>(slot))
               << " " << jstr("name", item.name(), false) << " }";
            if (i < count - 1) ss << ",";
            ss << "\n";
            i++;
        }
    }
    ss << "    ],\n";
    ss << "  },\n";

    // World
    ss << "  \"world\": {\n";
    ss << "    " << juint("seed", state.map_seed, false) << "\n";
    ss << "  },\n";

    // NPCs
    ss << "  \"npcs\": [\n";
    for (int i = 0; i < static_cast<int>(state.npcs.size()); ++i) {
        const auto& npc = state.npcs[i];
        ss << "    { " << jint("x", npc.x) << " " << jint("y", npc.y)
           << " " << juint("glyph", npc.glyph)
           << " " << jint("fg_r", npc.fg_r) << " " << jint("fg_g", npc.fg_g) << " " << jint("fg_b", npc.fg_b)
           << " " << jstr("name", npc.name) << " " << jbool("is_merchant", npc.is_merchant)
           << " " << jint("affinity", npc.affinity) << "\n";
        ss << "      \"shop\": [";
        for (int j = 0; j < static_cast<int>(npc.shop_inventory.size()); ++j) {
            const auto& [item, qty] = npc.shop_inventory[j];
            ss << "{ \"name\": \"" << jesc(item.name()) << "\", \"qty\": " << qty << " }";
            if (j < static_cast<int>(npc.shop_inventory.size()) - 1) ss << ", ";
        }
        ss << "] }";
        if (i < static_cast<int>(state.npcs.size()) - 1) ss << ",";
        ss << "\n";
    }
    ss << "  ],\n";

    // Enemies
    ss << "  \"enemies\": [\n";
    for (int i = 0; i < static_cast<int>(state.enemies.size()); ++i) {
        const auto& e = state.enemies[i];
        ss << "    { " << jint("x", e.x) << " " << jint("y", e.y)
           << " " << juint("glyph", e.glyph)
           << " " << jint("fg_r", e.fg_r) << " " << jint("fg_g", e.fg_g) << " " << jint("fg_b", e.fg_b)
           << " " << jstr("name", e.name)
           << " " << jint("hp", e.hp) << " " << jint("max_hp", e.max_hp)
           << " " << jint("attack", e.attack) << " " << jint("defense", e.defense)
           << " " << jint("damage_variance", e.damage_variance)
           << " " << jint("xp_value", e.xp_value, false) << " }";
        if (i < static_cast<int>(state.enemies.size()) - 1) ss << ",";
        ss << "\n";
    }
    ss << "  ],\n";

    // World items
    ss << "  \"world_items\": [\n";
    for (int i = 0; i < static_cast<int>(state.world_items.size()); ++i) {
        const auto& wi = state.world_items[i];
        ss << "    { " << jint("x", wi.x) << " " << jint("y", wi.y)
           << " " << juint("glyph", wi.glyph)
           << " " << jint("fg_r", wi.fg_r) << " " << jint("fg_g", wi.fg_g) << " " << jint("fg_b", wi.fg_b)
           << " " << jstr("item_name", wi.item_name, false) << " }";
        if (i < static_cast<int>(state.world_items.size()) - 1) ss << ",";
        ss << "\n";
    }
    ss << "  ],\n";

    // Chunks (per-chunk explored + modifications)
    ss << "  \"chunks\": [\n";
    for (int ci = 0; ci < static_cast<int>(state.chunks.size()); ++ci) {
        const auto& ch = state.chunks[ci];
        ss << "    { " << jint("cx", ch.cx) << " " << jint("cy", ch.cy)
           << " " << jstr("explored_hex", ch.explored_hex);
        // Modifications
        ss << " \"mods\": [";
        for (int mi = 0; mi < static_cast<int>(ch.modifications.size()); ++mi) {
            const auto& mod = ch.modifications[mi];
            ss << "{ " << jint("lx", mod.local_x) << " " << jint("ly", mod.local_y)
               << " " << jint("type", static_cast<int>(mod.type), false) << " }";
            if (mi < static_cast<int>(ch.modifications.size()) - 1) ss << ", ";
        }
        ss << "]";
        // Placed objects
        ss << " \"placed\": [";
        for (int pi = 0; pi < static_cast<int>(ch.placed_objects.size()); ++pi) {
            const auto& po = ch.placed_objects[pi];
            ss << "{ " << jint("lx", po.local_x) << " " << jint("ly", po.local_y)
               << " " << juint("glyph", po.glyph)
               << " " << jstr("name", po.name)
               << " " << jint("fg_r", po.fg_r) << " " << jint("fg_g", po.fg_g) << " " << jint("fg_b", po.fg_b)
               << " " << jbool("is_light", po.is_light)
               << " " << jint("light_radius", po.light_radius, false) << " }";
            if (pi < static_cast<int>(ch.placed_objects.size()) - 1) ss << ", ";
        }
        ss << "]";
        // Per-chunk NPCs
        ss << " \"npcs\": [";
        for (int ni = 0; ni < static_cast<int>(ch.npcs.size()); ++ni) {
            const auto& npc = ch.npcs[ni];
            ss << "{ " << jint("x", npc.x) << " " << jint("y", npc.y)
               << " " << juint("glyph", npc.glyph)
               << " " << jint("fg_r", npc.fg_r) << " " << jint("fg_g", npc.fg_g) << " " << jint("fg_b", npc.fg_b)
               << " " << jstr("name", npc.name) << " " << jbool("is_merchant", npc.is_merchant)
               << " " << jint("affinity", npc.affinity) << "\n";
            ss << "        \"shop\": [";
            for (int sj = 0; sj < static_cast<int>(npc.shop_inventory.size()); ++sj) {
                const auto& [item, qty] = npc.shop_inventory[sj];
                ss << "{ \"name\": \"" << jesc(item.name()) << "\", \"qty\": " << qty << " }";
                if (sj < static_cast<int>(npc.shop_inventory.size()) - 1) ss << ", ";
            }
            ss << "] }";
            if (ni < static_cast<int>(ch.npcs.size()) - 1) ss << ", ";
        }
        ss << "]";
        // Per-chunk enemies
        ss << " \"enemies\": [";
        for (int ei = 0; ei < static_cast<int>(ch.enemies.size()); ++ei) {
            const auto& e = ch.enemies[ei];
            ss << "{ " << jint("x", e.x) << " " << jint("y", e.y)
               << " " << juint("glyph", e.glyph)
               << " " << jint("fg_r", e.fg_r) << " " << jint("fg_g", e.fg_g) << " " << jint("fg_b", e.fg_b)
               << " " << jstr("name", e.name)
               << " " << jint("hp", e.hp) << " " << jint("max_hp", e.max_hp)
               << " " << jint("attack", e.attack) << " " << jint("defense", e.defense)
               << " " << jint("damage_variance", e.damage_variance)
               << " " << jint("xp_value", e.xp_value, false) << " }";
            if (ei < static_cast<int>(ch.enemies.size()) - 1) ss << ", ";
        }
        ss << "]";
        // Per-chunk world items
        ss << " \"world_items\": [";
        for (int wi = 0; wi < static_cast<int>(ch.world_items.size()); ++wi) {
            const auto& w = ch.world_items[wi];
            ss << "{ " << jint("x", w.x) << " " << jint("y", w.y)
               << " " << juint("glyph", w.glyph)
               << " " << jint("fg_r", w.fg_r) << " " << jint("fg_g", w.fg_g) << " " << jint("fg_b", w.fg_b)
               << " " << jstr("item_name", w.item_name, false) << " }";
            if (wi < static_cast<int>(ch.world_items.size()) - 1) ss << ", ";
        }
        ss << "]";
        ss << " }";
        if (ci < static_cast<int>(state.chunks.size()) - 1) ss << ",";
        ss << "\n";
    }
    ss << "  ]\n";

    ss << "}\n";

    std::ofstream f(save_path(slot));
    if (!f.is_open()) return false;
    f << ss.str();
    return f.good();
}

// ── Load ────────────────────────────────────────────────────────────

bool load_game(int slot, GameState& state) {
    if (slot < 0 || slot >= MAX_SAVE_SLOTS) return false;

    std::ifstream f(save_path(slot));
    if (!f.is_open()) return false;

    std::string json((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

    int version = extract_int(json, "version");
    if (version < 1) return false;

    // Player
    state.player_name = extract_string(json, "name");
    state.player_x = extract_int(json, "x");
    state.player_y = extract_int(json, "y");
    state.gold = extract_int(json, "gold", 50);
    state.hp = extract_int(json, "hp", 20);
    state.xp = extract_int(json, "xp");
    state.level = extract_int(json, "level", 1);
    state.stats.str = extract_int(json, "str", 10);
    state.stats.dex = extract_int(json, "dex", 10);
    state.stats.con = extract_int(json, "con", 10);
    state.stats.intel = extract_int(json, "intel", 10);
    state.stats.wis = extract_int(json, "wis", 10);
    state.stats.cha = extract_int(json, "cha", 10);

    // Inventory
    state.inventory.clear();
    {
        std::string inv_arr = extract_array(json, "inventory");
        auto objs = split_array_objects(inv_arr);
        for (const auto& obj : objs) {
            std::string name = extract_string(obj, "name");
            int qty = extract_int(obj, "qty", 1);
            const Item* item = find_item(name);
            if (item) state.inventory.emplace_back(*item, qty);
        }
    }

    // Equipment
    state.equipment.clear();
    {
        std::string eq_arr = extract_array(json, "equipment");
        auto objs = split_array_objects(eq_arr);
        for (const auto& obj : objs) {
            EquipSlot slot = static_cast<EquipSlot>(extract_int(obj, "slot", 0));
            std::string name = extract_string(obj, "name");
            const Item* item = find_item(name);
            if (item && slot != EquipSlot::None) {
                state.equipment[slot] = *item;
            }
        }
    }

    // World
    state.map_seed = static_cast<uint32_t>(extract_int(json, "seed"));

    // Meta
    state.meta.character_name = extract_string(json, "character_name");
    state.meta.play_time_seconds = extract_int(json, "play_time_seconds");
    state.turn_of_day = extract_int(json, "turn_of_day", 150);

    // NPCs
    state.npcs.clear();
    {
        std::string npc_arr = extract_array(json, "npcs");
        auto objs = split_array_objects(npc_arr);
        for (const auto& obj : objs) {
            GameState::NpcSave ns;
            ns.x = extract_int(obj, "x");
            ns.y = extract_int(obj, "y");
            ns.glyph = static_cast<uint32_t>(extract_int(obj, "glyph"));
            ns.fg_r = static_cast<uint8_t>(extract_int(obj, "fg_r", 200));
            ns.fg_g = static_cast<uint8_t>(extract_int(obj, "fg_g", 200));
            ns.fg_b = static_cast<uint8_t>(extract_int(obj, "fg_b", 200));
            ns.name = extract_string(obj, "name");
            ns.is_merchant = extract_bool(obj, "is_merchant");
            ns.affinity = extract_int(obj, "affinity", 30);
            // Shop inventory
            std::string shop_arr = extract_array(obj, "shop");
            auto shop_objs = split_array_objects(shop_arr);
            for (const auto& si : shop_objs) {
                std::string iname = extract_string(si, "name");
                int qty = extract_int(si, "qty", 1);
                const Item* item = find_item(iname);
                if (item) ns.shop_inventory.emplace_back(*item, qty);
            }
            state.npcs.push_back(ns);
        }
    }

    // Enemies
    state.enemies.clear();
    {
        std::string e_arr = extract_array(json, "enemies");
        auto objs = split_array_objects(e_arr);
        for (const auto& obj : objs) {
            GameState::EnemySave es;
            es.x = extract_int(obj, "x");
            es.y = extract_int(obj, "y");
            es.glyph = static_cast<uint32_t>(extract_int(obj, "glyph"));
            es.fg_r = static_cast<uint8_t>(extract_int(obj, "fg_r", 200));
            es.fg_g = static_cast<uint8_t>(extract_int(obj, "fg_g", 200));
            es.fg_b = static_cast<uint8_t>(extract_int(obj, "fg_b", 200));
            es.name = extract_string(obj, "name");
            es.hp = extract_int(obj, "hp", 10);
            es.max_hp = extract_int(obj, "max_hp", 10);
            es.attack = extract_int(obj, "attack", 2);
            es.defense = extract_int(obj, "defense", 0);
            es.damage_variance = extract_int(obj, "damage_variance", 0);
            es.xp_value = extract_int(obj, "xp_value", 5);
            state.enemies.push_back(es);
        }
    }

    // World items
    state.world_items.clear();
    {
        std::string wi_arr = extract_array(json, "world_items");
        auto objs = split_array_objects(wi_arr);
        for (const auto& obj : objs) {
            GameState::WorldItemSave wi;
            wi.x = extract_int(obj, "x");
            wi.y = extract_int(obj, "y");
            wi.glyph = static_cast<uint32_t>(extract_int(obj, "glyph"));
            wi.fg_r = static_cast<uint8_t>(extract_int(obj, "fg_r", 200));
            wi.fg_g = static_cast<uint8_t>(extract_int(obj, "fg_g", 200));
            wi.fg_b = static_cast<uint8_t>(extract_int(obj, "fg_b", 200));
            wi.item_name = extract_string(obj, "item_name");
            state.world_items.push_back(wi);
        }
    }

    // Chunks
    state.chunks.clear();
    {
        std::string ch_arr = extract_array(json, "chunks");
        auto objs = split_array_objects(ch_arr);
        for (const auto& obj : objs) {
            GameState::ChunkSave cs;
            cs.cx = extract_int(obj, "cx");
            cs.cy = extract_int(obj, "cy");
            cs.explored_hex = extract_string(obj, "explored_hex");
            // Modifications
            std::string mods_arr = extract_array(obj, "mods");
            auto mod_objs = split_array_objects(mods_arr);
            for (const auto& mo : mod_objs) {
                TileMod mod;
                mod.local_x = extract_int(mo, "lx");
                mod.local_y = extract_int(mo, "ly");
                mod.type = static_cast<TileType>(extract_int(mo, "type"));
                cs.modifications.push_back(mod);
            }
            // Placed objects
            std::string placed_arr = extract_array(obj, "placed");
            auto placed_objs = split_array_objects(placed_arr);
            for (const auto& po : placed_objs) {
                PlacedObject pobj;
                pobj.local_x = extract_int(po, "lx");
                pobj.local_y = extract_int(po, "ly");
                pobj.glyph = static_cast<uint32_t>(extract_int(po, "glyph"));
                pobj.name = extract_string(po, "name");
                pobj.fg_r = static_cast<uint8_t>(extract_int(po, "fg_r", 200));
                pobj.fg_g = static_cast<uint8_t>(extract_int(po, "fg_g", 200));
                pobj.fg_b = static_cast<uint8_t>(extract_int(po, "fg_b", 200));
                pobj.is_light = extract_bool(po, "is_light");
                pobj.light_radius = extract_int(po, "light_radius", 0);
                cs.placed_objects.push_back(pobj);
            }
            // Per-chunk NPCs
            {
                std::string npc_arr = extract_array(obj, "npcs");
                auto npc_objs = split_array_objects(npc_arr);
                for (const auto& nobj : npc_objs) {
                    GameState::NpcSave ns;
                    ns.x = extract_int(nobj, "x");
                    ns.y = extract_int(nobj, "y");
                    ns.glyph = static_cast<uint32_t>(extract_int(nobj, "glyph"));
                    ns.fg_r = static_cast<uint8_t>(extract_int(nobj, "fg_r", 200));
                    ns.fg_g = static_cast<uint8_t>(extract_int(nobj, "fg_g", 200));
                    ns.fg_b = static_cast<uint8_t>(extract_int(nobj, "fg_b", 200));
                    ns.name = extract_string(nobj, "name");
                    ns.is_merchant = extract_bool(nobj, "is_merchant");
                    ns.affinity = extract_int(nobj, "affinity", 30);
                    std::string shop_arr = extract_array(nobj, "shop");
                    auto shop_objs = split_array_objects(shop_arr);
                    for (const auto& si : shop_objs) {
                        std::string iname = extract_string(si, "name");
                        int qty = extract_int(si, "qty", 1);
                        const Item* item = find_item(iname);
                        if (item) ns.shop_inventory.emplace_back(*item, qty);
                    }
                    cs.npcs.push_back(ns);
                }
            }
            // Per-chunk enemies
            {
                std::string e_arr = extract_array(obj, "enemies");
                auto e_objs = split_array_objects(e_arr);
                for (const auto& eobj : e_objs) {
                    GameState::EnemySave es;
                    es.x = extract_int(eobj, "x");
                    es.y = extract_int(eobj, "y");
                    es.glyph = static_cast<uint32_t>(extract_int(eobj, "glyph"));
                    es.fg_r = static_cast<uint8_t>(extract_int(eobj, "fg_r", 200));
                    es.fg_g = static_cast<uint8_t>(extract_int(eobj, "fg_g", 200));
                    es.fg_b = static_cast<uint8_t>(extract_int(eobj, "fg_b", 200));
                    es.name = extract_string(eobj, "name");
                    es.hp = extract_int(eobj, "hp", 10);
                    es.max_hp = extract_int(eobj, "max_hp", 10);
                    es.attack = extract_int(eobj, "attack", 2);
                    es.defense = extract_int(eobj, "defense", 0);
                    es.damage_variance = extract_int(eobj, "damage_variance", 0);
                    es.xp_value = extract_int(eobj, "xp_value", 5);
                    cs.enemies.push_back(es);
                }
            }
            // Per-chunk world items
            {
                std::string wi_arr = extract_array(obj, "world_items");
                auto wi_objs = split_array_objects(wi_arr);
                for (const auto& wobj : wi_objs) {
                    GameState::WorldItemSave wi;
                    wi.x = extract_int(wobj, "x");
                    wi.y = extract_int(wobj, "y");
                    wi.glyph = static_cast<uint32_t>(extract_int(wobj, "glyph"));
                    wi.fg_r = static_cast<uint8_t>(extract_int(wobj, "fg_r", 200));
                    wi.fg_g = static_cast<uint8_t>(extract_int(wobj, "fg_g", 200));
                    wi.fg_b = static_cast<uint8_t>(extract_int(wobj, "fg_b", 200));
                    wi.item_name = extract_string(wobj, "item_name");
                    cs.world_items.push_back(wi);
                }
            }
            state.chunks.push_back(std::move(cs));
        }
    }

    return true;
}

// ── Delete / List ───────────────────────────────────────────────────

bool delete_save(int slot) {
    if (slot < 0 || slot >= MAX_SAVE_SLOTS) return false;
    fs::path p = save_path(slot);
    if (fs::exists(p)) return fs::remove(p);
    return true;
}

std::vector<SaveMeta> list_saves() {
    std::vector<SaveMeta> saves;
    for (int i = 0; i < MAX_SAVE_SLOTS; ++i) {
        std::ifstream f(save_path(i));
        if (f.is_open()) {
            std::string json((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
            SaveMeta meta;
            meta.slot = i;
            meta.character_name = extract_string(json, "character_name");
            meta.play_time_seconds = extract_int(json, "play_time_seconds");
            meta.timestamp = extract_string(json, "timestamp");
            saves.push_back(meta);
        }
    }
    return saves;
}

// ── Capture ─────────────────────────────────────────────────────────

GameState capture_state(const Player& player, const World& world,
                        uint32_t map_seed,
                        int play_time_seconds,
                        const TimeSystem& time_system) {
    GameState state;

    // Player
    state.player_name = "Player";
    state.player_x = player.x();
    state.player_y = player.y();
    state.gold = player.gold();
    state.hp = player.hp();
    state.stats = player.stats();
    state.inventory = player.inventory();
    state.equipment = player.equipment();
    state.xp = player.xp();
    state.level = player.level();

    // World
    state.map_seed = map_seed;

    // Per-chunk data (explored tiles, modifications, placed objects)
    auto chunk_data = world.gather_save_data();
    for (const auto& cd : chunk_data) {
        GameState::ChunkSave cs;
        cs.cx = cd.cx;
        cs.cy = cd.cy;
        cs.explored_hex = hex_string(cd.explored);
        cs.modifications = cd.modifications;
        cs.placed_objects = cd.placed_objects;
        state.chunks.push_back(std::move(cs));
    }

    // Entities from World (per-chunk, stored in ChunkSave)
    auto entity_data = world.gather_entity_save_data();
    for (const auto& ed : entity_data) {
        // Find the matching ChunkSave entry
        GameState::ChunkSave* cs = nullptr;
        for (auto& c : state.chunks) {
            if (c.cx == ed.cx && c.cy == ed.cy) { cs = &c; break; }
        }
        if (!cs) continue;

        // NPCs
        for (const auto& npc : ed.npcs) {
            GameState::NpcSave ns;
            ns.x = npc.x();
            ns.y = npc.y();
            ns.glyph = npc.glyph();
            ns.fg_r = npc.fg_r();
            ns.fg_g = npc.fg_g();
            ns.fg_b = npc.fg_b();
            ns.name = npc.name();
            ns.is_merchant = npc.is_merchant();
            ns.affinity = npc.affinity();
            ns.shop_inventory = npc.shop_inventory();
            cs->npcs.push_back(ns);
        }
        // Enemies
        for (const auto& e : ed.enemies) {
            GameState::EnemySave es;
            es.x = e.x();
            es.y = e.y();
            es.glyph = e.glyph();
            es.fg_r = e.fg_r();
            es.fg_g = e.fg_g();
            es.fg_b = e.fg_b();
            es.name = e.name();
            es.hp = e.hp();
            es.max_hp = e.max_hp();
            es.attack = e.attack();
            es.defense = e.defense();
            es.damage_variance = e.damage_variance();
            es.xp_value = e.xp_value();
            cs->enemies.push_back(es);
        }
        // World items
        for (const auto& item : ed.items) {
            if (!item.in_inventory()) {
                GameState::WorldItemSave wis;
                wis.x = item.x();
                wis.y = item.y();
                wis.glyph = item.glyph();
                wis.fg_r = item.fg_r();
                wis.fg_g = item.fg_g();
                wis.fg_b = item.fg_b();
                wis.item_name = item.name();
                cs->world_items.push_back(wis);
            }
        }
    }

    // Meta
    state.meta.character_name = state.player_name;
    state.meta.play_time_seconds = play_time_seconds;
    state.turn_of_day = time_system.turn_of_day();

    time_t now = time(nullptr);
    char buf[64];
    struct tm time_info;
    localtime_s(&time_info, &now);
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &time_info);
    state.meta.timestamp = buf;

    return state;
}

// ── Apply chunk data to world ───────────────────────────────────────

void apply_chunk_data(World& world, const std::vector<GameState::ChunkSave>& chunks) {
    for (const auto& cs : chunks) {
        // Apply chunk terrain/explored/mods
        World::ChunkSaveData data;
        data.cx = cs.cx;
        data.cy = cs.cy;
        data.explored = parse_hex(cs.explored_hex, CHUNK_SIZE * CHUNK_SIZE);
        data.modifications = cs.modifications;
        data.placed_objects = cs.placed_objects;
        world.apply_save_data(data);

        // Apply per-chunk entities
        for (const auto& ns : cs.npcs) {
            std::vector<DialogueNode> dialogue;
            if (ns.name == "Merchant") dialogue = build_merchant_dialogue();
            else if (ns.name == "Villager") dialogue = build_villager_dialogue();
            else if (ns.name == "Old Sage") dialogue = build_sage_dialogue();
            else if (ns.name == "Child") dialogue = build_child_dialogue();
            else if (ns.name == "Wanderer") dialogue = build_wanderer_dialogue();

            world.spawn_npc(Npc(ns.x, ns.y, ns.glyph, ns.name,
                                ns.fg_r, ns.fg_g, ns.fg_b,
                                ns.is_merchant, ns.affinity,
                                dialogue, ns.shop_inventory));
        }
        for (const auto& es : cs.enemies) {
            world.spawn_enemy(Enemy(es.x, es.y, es.glyph, es.name,
                                    es.fg_r, es.fg_g, es.fg_b,
                                    es.hp, es.max_hp, es.attack, es.defense,
                                    es.damage_variance, es.xp_value));
        }
        for (const auto& wis : cs.world_items) {
            Entity e(wis.x, wis.y, wis.glyph, wis.item_name,
                     wis.fg_r, wis.fg_g, wis.fg_b);
            e.set_is_item(true);
            world.spawn_item(std::move(e));
        }
    }
}
