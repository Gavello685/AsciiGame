#include "game/save.h"
#include "game/world.h"
#include "platform/paths.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>

namespace fs = std::filesystem;

static fs::path save_dir() {
    fs::path dir = platform::user_data_dir() / "saves";
    std::error_code ec;
    fs::create_directories(dir, ec);
    return dir;
}

static fs::path save_path(int slot) {
    return save_dir() / ("slot_" + std::to_string(slot) + ".json");
}

// ── JSON writer ─────────────────────────────────────────────────────
//
// Separators are emitted by the writer rather than by each call site, so it
// cannot produce the trailing commas that made earlier saves invalid JSON.

namespace {

std::string json_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:   out += c; break;
        }
    }
    return out;
}

class JsonWriter {
public:
    explicit JsonWriter(std::ostringstream& out) : out_(out) {}

    void begin_object() { separate(); out_ << '{'; push(false); }
    void end_object() { pop(); out_ << '}'; }

    void begin_array() { separate(); out_ << '['; push(true); }
    void end_array() { pop(); out_ << ']'; }

    void key(const std::string& k) {
        separate();
        out_ << '"' << json_escape(k) << "\": ";
        suppress_next_separator_ = true;
    }

    void begin_object(const std::string& k) { key(k); begin_object(); }
    void begin_array(const std::string& k) { key(k); begin_array(); }

    void value(int v) { separate(); out_ << v; }
    void value(unsigned v) { separate(); out_ << v; }
    void value(uint64_t v) { separate(); out_ << v; }
    void value(bool v) { separate(); out_ << (v ? "true" : "false"); }
    void value(const std::string& v) { separate(); out_ << '"' << json_escape(v) << '"'; }
    void value(const char* v) { value(std::string(v)); }

    template <typename T>
    void field(const std::string& k, const T& v) { key(k); value(v); }

private:
    void push(bool is_array) {
        scopes_.push_back({is_array, true});
        suppress_next_separator_ = false;
    }

    void pop() {
        if (!scopes_.empty()) scopes_.pop_back();
        suppress_next_separator_ = false;
    }

    // Emits ", " between siblings, but never after a key or before the first
    // element of a scope.
    void separate() {
        if (suppress_next_separator_) {
            suppress_next_separator_ = false;
            return;
        }
        if (scopes_.empty()) return;
        Scope& scope = scopes_.back();
        if (scope.first) {
            scope.first = false;
        } else {
            out_ << ", ";
        }
    }

    struct Scope { bool is_array; bool first; };

    std::ostringstream& out_;
    std::vector<Scope> scopes_;
    bool suppress_next_separator_ = false;
};

std::string hex_string(const std::vector<bool>& bits) {
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

std::vector<bool> parse_hex(const std::string& h, int expected_bits) {
    std::vector<bool> bits;
    bits.reserve(static_cast<size_t>(expected_bits));
    for (char c : h) {
        int nibble = 0;
        if (c >= '0' && c <= '9') nibble = c - '0';
        else if (c >= 'a' && c <= 'f') nibble = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') nibble = c - 'A' + 10;
        else continue;
        for (int i = 0; i < 4 && static_cast<int>(bits.size()) < expected_bits; ++i) {
            bits.push_back(((nibble >> i) & 1) != 0);
        }
    }
    while (static_cast<int>(bits.size()) < expected_bits) bits.push_back(false);
    return bits;
}

// ── JSON reader ─────────────────────────────────────────────────────
//
// A tolerant scanner rather than a full parser. Lookups match the first
// occurrence of a key, so callers narrow to the enclosing object or array
// with extract_object/extract_array before reading scalar fields.

std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    size_t end = s.find_last_not_of(" \t\r\n");
    return (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
}

std::string extract_value(const std::string& json, const std::string& key) {
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
    }
    size_t end = pos;
    while (end < json.size() && json[end] != ',' && json[end] != '}' && json[end] != ']') end++;
    return trim(json.substr(pos, end - pos));
}

int extract_int(const std::string& json, const std::string& key, int def = 0) {
    std::string val = extract_value(json, key);
    if (val.empty()) return def;
    try { return std::stoi(val); } catch (...) { return def; }
}

uint64_t extract_u64(const std::string& json, const std::string& key, uint64_t def = 0) {
    std::string val = extract_value(json, key);
    if (val.empty()) return def;
    try { return std::stoull(val); } catch (...) { return def; }
}

std::string extract_string(const std::string& json, const std::string& key,
                           const std::string& def = "") {
    std::string val = extract_value(json, key);
    return val.empty() ? def : val;
}

// Raw text of a bracketed section, brace/bracket balanced.
std::string extract_section(const std::string& json, const std::string& key,
                            char open, char close) {
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return "";
    pos = json.find(open, pos);
    if (pos == std::string::npos) return "";
    int depth = 0;
    size_t start = pos;
    for (; pos < json.size(); ++pos) {
        if (json[pos] == open) depth++;
        else if (json[pos] == close) {
            depth--;
            if (depth == 0) return json.substr(start, pos - start + 1);
        }
    }
    return "";
}

std::string extract_array(const std::string& json, const std::string& key) {
    return extract_section(json, key, '[', ']');
}

std::string extract_object(const std::string& json, const std::string& key) {
    return extract_section(json, key, '{', '}');
}

// Split a JSON array of objects into individual object strings.
std::vector<std::string> split_array_objects(const std::string& arr) {
    std::vector<std::string> objects;
    int depth = 0;
    size_t start = 0;
    bool in_obj = false;
    for (size_t i = 0; i < arr.size(); ++i) {
        if (arr[i] == '{') {
            if (depth == 0) { start = i; in_obj = true; }
            depth++;
        } else if (arr[i] == '}') {
            depth--;
            if (depth == 0 && in_obj) {
                objects.push_back(arr.substr(start, i - start + 1));
                in_obj = false;
            }
        }
    }
    return objects;
}

// ── Per-entity writers ──────────────────────────────────────────────

void write_item_stacks(JsonWriter& w, const std::vector<std::pair<Item, int>>& stacks) {
    w.begin_array();
    for (const auto& [item, qty] : stacks) {
        w.begin_object();
        w.field("name", item.name());
        w.field("qty", qty);
        w.end_object();
    }
    w.end_array();
}

void read_item_stacks(const std::string& arr, std::vector<std::pair<Item, int>>& out) {
    out.clear();
    for (const auto& obj : split_array_objects(arr)) {
        const Item* item = find_item(extract_string(obj, "name"));
        if (item) out.emplace_back(*item, extract_int(obj, "qty", 1));
    }
}

void write_npcs(JsonWriter& w, const std::vector<GameState::NpcSave>& npcs) {
    w.begin_array("npcs");
    for (const auto& npc : npcs) {
        w.begin_object();
        w.field("x", npc.x);
        w.field("y", npc.y);
        w.field("name", npc.name);
        w.field("affinity", npc.affinity);
        w.key("shop");
        write_item_stacks(w, npc.shop_inventory);
        w.end_object();
    }
    w.end_array();
}

void read_npcs(const std::string& parent, std::vector<GameState::NpcSave>& out) {
    out.clear();
    for (const auto& obj : split_array_objects(extract_array(parent, "npcs"))) {
        GameState::NpcSave npc;
        npc.x = extract_int(obj, "x");
        npc.y = extract_int(obj, "y");
        npc.name = extract_string(obj, "name");
        npc.affinity = extract_int(obj, "affinity", 30);
        read_item_stacks(extract_array(obj, "shop"), npc.shop_inventory);
        out.push_back(std::move(npc));
    }
}

void write_enemies(JsonWriter& w, const std::vector<GameState::EnemySave>& enemies) {
    w.begin_array("enemies");
    for (const auto& e : enemies) {
        w.begin_object();
        w.field("x", e.x);
        w.field("y", e.y);
        w.field("name", e.name);
        w.field("hp", e.hp);
        w.end_object();
    }
    w.end_array();
}

void read_enemies(const std::string& parent, std::vector<GameState::EnemySave>& out) {
    out.clear();
    for (const auto& obj : split_array_objects(extract_array(parent, "enemies"))) {
        GameState::EnemySave e;
        e.x = extract_int(obj, "x");
        e.y = extract_int(obj, "y");
        e.name = extract_string(obj, "name");
        e.hp = extract_int(obj, "hp", 1);
        out.push_back(std::move(e));
    }
}

void write_world_items(JsonWriter& w, const std::vector<GameState::WorldItemSave>& items) {
    w.begin_array("world_items");
    for (const auto& item : items) {
        w.begin_object();
        w.field("x", item.x);
        w.field("y", item.y);
        w.field("item_name", item.item_name);
        w.end_object();
    }
    w.end_array();
}

void read_world_items(const std::string& parent,
                      std::vector<GameState::WorldItemSave>& out) {
    out.clear();
    for (const auto& obj : split_array_objects(extract_array(parent, "world_items"))) {
        GameState::WorldItemSave item;
        item.x = extract_int(obj, "x");
        item.y = extract_int(obj, "y");
        item.item_name = extract_string(obj, "item_name");
        out.push_back(std::move(item));
    }
}

}

// ── Save ────────────────────────────────────────────────────────────

std::string serialize_state(const GameState& state) {
    std::ostringstream ss;
    JsonWriter w(ss);

    w.begin_object();
    w.field("version", SAVE_VERSION);

    w.begin_object("meta");
    w.field("slot", state.meta.slot);
    w.field("character_name", state.meta.character_name);
    w.field("play_time_seconds", state.meta.play_time_seconds);
    w.field("turn_of_day", state.turn_of_day);
    w.field("day", state.day);
    w.field("timestamp", state.meta.timestamp);
    w.end_object();

    w.begin_object("player");
    w.field("name", state.player_name);
    w.field("x", state.player_x);
    w.field("y", state.player_y);
    w.field("gold", state.gold);
    w.field("hp", state.hp);
    w.field("xp", state.xp);
    w.field("level", state.level);

    w.begin_object("stats");
    w.field("str", state.stats.str);
    w.field("dex", state.stats.dex);
    w.field("con", state.stats.con);
    w.field("intel", state.stats.intel);
    w.field("wis", state.stats.wis);
    w.field("cha", state.stats.cha);
    w.end_object();

    w.key("inventory");
    write_item_stacks(w, state.inventory);

    w.begin_array("equipment");
    for (const auto& [slot, item] : state.equipment) {
        w.begin_object();
        w.field("slot", static_cast<int>(slot));
        w.field("name", item.name());
        w.end_object();
    }
    w.end_array();
    w.end_object(); // player

    w.begin_object("world");
    w.field("seed", static_cast<unsigned>(state.map_seed));
    w.field("rng_state", state.rng_state);
    w.end_object();

    w.begin_array("zones");
    for (const auto& z : state.zones) {
        w.begin_object();
        w.field("x0", z.x0);
        w.field("y0", z.y0);
        w.field("x1", z.x1);
        w.field("y1", z.y1);
        w.field("type", static_cast<int>(z.type));
        w.end_object();
    }
    w.end_array();

    w.begin_array("chunks");
    for (const auto& chunk : state.chunks) {
        w.begin_object();
        w.field("cx", chunk.cx);
        w.field("cy", chunk.cy);
        w.field("explored_hex", chunk.explored_hex);

        w.begin_array("mods");
        for (const auto& mod : chunk.modifications) {
            w.begin_object();
            w.field("lx", mod.local_x);
            w.field("ly", mod.local_y);
            w.field("type", static_cast<int>(mod.type));
            w.end_object();
        }
        w.end_array();

        w.begin_array("placed");
        for (const auto& obj : chunk.placed_objects) {
            w.begin_object();
            w.field("lx", obj.local_x);
            w.field("ly", obj.local_y);
            w.field("glyph", static_cast<unsigned>(obj.glyph));
            w.field("name", obj.name);
            w.field("fg_r", static_cast<int>(obj.fg_r));
            w.field("fg_g", static_cast<int>(obj.fg_g));
            w.field("fg_b", static_cast<int>(obj.fg_b));
            w.field("is_light", obj.is_light);
            w.field("light_radius", obj.light_radius);
            w.end_object();
        }
        w.end_array();

        write_npcs(w, chunk.npcs);
        write_enemies(w, chunk.enemies);
        write_world_items(w, chunk.world_items);
        w.end_object();
    }
    w.end_array();

    w.end_object();
    ss << "\n";
    return ss.str();
}

bool save_game(int slot, const GameState& state) {
    if (slot < 0 || slot >= MAX_SAVE_SLOTS) return false;

    GameState stamped = state;
    stamped.meta.slot = slot;

    std::ofstream f(save_path(slot), std::ios::binary | std::ios::trunc);
    if (!f.is_open()) return false;
    f << serialize_state(stamped);
    f.flush();
    return f.good();
}

// ── Load ────────────────────────────────────────────────────────────

bool deserialize_state(const std::string& json, GameState& state) {
    if (extract_int(json, "version") < 1) return false;

    // Narrow to each section before reading scalars: key lookups match the
    // first occurrence, so unscoped reads would pick up same-named fields
    // from elsewhere in the document.
    const std::string meta = extract_object(json, "meta");
    const std::string player = extract_object(json, "player");
    const std::string world = extract_object(json, "world");

    state.player_name = extract_string(player, "name", "Player");
    state.player_x = extract_int(player, "x");
    state.player_y = extract_int(player, "y");
    state.gold = extract_int(player, "gold", 50);
    state.hp = extract_int(player, "hp", 20);
    state.xp = extract_int(player, "xp");
    state.level = extract_int(player, "level", 1);

    const std::string stats = extract_object(player, "stats");
    state.stats.str = extract_int(stats, "str", 10);
    state.stats.dex = extract_int(stats, "dex", 10);
    state.stats.con = extract_int(stats, "con", 10);
    state.stats.intel = extract_int(stats, "intel", 10);
    state.stats.wis = extract_int(stats, "wis", 10);
    state.stats.cha = extract_int(stats, "cha", 10);

    read_item_stacks(extract_array(player, "inventory"), state.inventory);

    state.equipment.clear();
    for (const auto& obj : split_array_objects(extract_array(player, "equipment"))) {
        EquipSlot slot = static_cast<EquipSlot>(extract_int(obj, "slot", 0));
        const Item* item = find_item(extract_string(obj, "name"));
        if (item && slot != EquipSlot::None) state.equipment[slot] = *item;
    }

    state.map_seed = static_cast<uint32_t>(extract_u64(world, "seed"));
    state.rng_state = extract_u64(world, "rng_state");

    state.meta.slot = extract_int(meta, "slot", -1);
    state.meta.character_name = extract_string(meta, "character_name");
    state.meta.play_time_seconds = extract_int(meta, "play_time_seconds");
    state.meta.timestamp = extract_string(meta, "timestamp");
    state.turn_of_day = extract_int(meta, "turn_of_day", 150);
    state.day = extract_int(meta, "day", 1);

    state.zones.clear();
    for (const auto& obj : split_array_objects(extract_array(json, "zones"))) {
        Zone z;
        z.x0 = extract_int(obj, "x0");
        z.y0 = extract_int(obj, "y0");
        z.x1 = extract_int(obj, "x1");
        z.y1 = extract_int(obj, "y1");
        z.type = static_cast<ZoneType>(extract_int(obj, "type"));
        state.zones.push_back(z);
    }

    state.chunks.clear();
    for (const auto& obj : split_array_objects(extract_array(json, "chunks"))) {
        GameState::ChunkSave chunk;
        chunk.cx = extract_int(obj, "cx");
        chunk.cy = extract_int(obj, "cy");
        chunk.explored_hex = extract_string(obj, "explored_hex");

        for (const auto& mod_obj : split_array_objects(extract_array(obj, "mods"))) {
            TileMod mod;
            mod.local_x = extract_int(mod_obj, "lx");
            mod.local_y = extract_int(mod_obj, "ly");
            mod.type = static_cast<TileType>(extract_int(mod_obj, "type"));
            chunk.modifications.push_back(mod);
        }

        for (const auto& placed_obj : split_array_objects(extract_array(obj, "placed"))) {
            PlacedObject obj_data;
            obj_data.local_x = extract_int(placed_obj, "lx");
            obj_data.local_y = extract_int(placed_obj, "ly");
            obj_data.glyph = static_cast<uint32_t>(extract_int(placed_obj, "glyph", ' '));
            obj_data.name = extract_string(placed_obj, "name");
            obj_data.fg_r = static_cast<uint8_t>(extract_int(placed_obj, "fg_r", 200));
            obj_data.fg_g = static_cast<uint8_t>(extract_int(placed_obj, "fg_g", 200));
            obj_data.fg_b = static_cast<uint8_t>(extract_int(placed_obj, "fg_b", 200));
            obj_data.is_light = extract_string(placed_obj, "is_light") == "true";
            obj_data.light_radius = extract_int(placed_obj, "light_radius", 0);
            obj_data.player_placed = true; // only player objects are persisted
            chunk.placed_objects.push_back(std::move(obj_data));
        }

        read_npcs(obj, chunk.npcs);
        read_enemies(obj, chunk.enemies);
        read_world_items(obj, chunk.world_items);

        state.chunks.push_back(std::move(chunk));
    }

    return true;
}

bool load_game(int slot, GameState& state) {
    if (slot < 0 || slot >= MAX_SAVE_SLOTS) return false;

    std::ifstream f(save_path(slot), std::ios::binary);
    if (!f.is_open()) return false;

    std::string json((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    return deserialize_state(json, state);
}

// ── Delete / List ───────────────────────────────────────────────────

bool delete_save(int slot) {
    if (slot < 0 || slot >= MAX_SAVE_SLOTS) return false;
    fs::path p = save_path(slot);
    std::error_code ec;
    if (fs::exists(p, ec)) return fs::remove(p, ec);
    return true;
}

std::vector<SaveMeta> list_saves() {
    std::vector<SaveMeta> saves;
    for (int i = 0; i < MAX_SAVE_SLOTS; ++i) {
        std::ifstream f(save_path(i), std::ios::binary);
        if (!f.is_open()) continue;

        std::string json((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        const std::string meta = extract_object(json, "meta");
        const std::string player = extract_object(json, "player");

        SaveMeta entry;
        entry.slot = i;
        entry.character_name = extract_string(meta, "character_name", "Player");
        entry.play_time_seconds = extract_int(meta, "play_time_seconds");
        entry.timestamp = extract_string(meta, "timestamp");
        entry.level = extract_int(player, "level", 1);
        entry.day = extract_int(meta, "day", 1);
        saves.push_back(std::move(entry));
    }
    return saves;
}

// ── Capture ─────────────────────────────────────────────────────────

GameState capture_state(const Player& player, const World& world,
                        uint32_t map_seed,
                        int play_time_seconds,
                        const TimeSystem& time_system,
                        uint64_t rng_state) {
    GameState state;

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

    state.map_seed = map_seed;
    state.rng_state = rng_state;
    state.zones = world.zones();

    // Terrain deltas and exploration, for loaded and archived chunks alike.
    for (const auto& cd : world.gather_save_data()) {
        GameState::ChunkSave chunk;
        chunk.cx = cd.cx;
        chunk.cy = cd.cy;
        chunk.explored_hex = hex_string(cd.explored);
        chunk.modifications = cd.modifications;
        chunk.placed_objects = cd.placed_objects;
        state.chunks.push_back(std::move(chunk));
    }

    // Entities, filed into their chunk's entry.
    for (const auto& ed : world.gather_entity_save_data()) {
        GameState::ChunkSave* chunk = nullptr;
        for (auto& candidate : state.chunks) {
            if (candidate.cx == ed.cx && candidate.cy == ed.cy) { chunk = &candidate; break; }
        }
        if (!chunk) {
            GameState::ChunkSave created;
            created.cx = ed.cx;
            created.cy = ed.cy;
            state.chunks.push_back(std::move(created));
            chunk = &state.chunks.back();
        }

        for (const auto& npc : ed.npcs) {
            GameState::NpcSave saved;
            saved.x = npc.x();
            saved.y = npc.y();
            saved.name = npc.name();
            saved.affinity = npc.affinity();
            saved.shop_inventory = npc.shop_inventory();
            chunk->npcs.push_back(std::move(saved));
        }
        for (const auto& enemy : ed.enemies) {
            GameState::EnemySave saved;
            saved.x = enemy.x();
            saved.y = enemy.y();
            saved.name = enemy.name();
            saved.hp = enemy.hp();
            chunk->enemies.push_back(std::move(saved));
        }
        for (const auto& item : ed.items) {
            if (item.in_inventory()) continue;
            GameState::WorldItemSave saved;
            saved.x = item.x();
            saved.y = item.y();
            saved.item_name = item.name();
            chunk->world_items.push_back(std::move(saved));
        }
    }

    state.meta.character_name = state.player_name;
    state.meta.play_time_seconds = play_time_seconds;
    state.meta.timestamp = platform::local_timestamp();
    state.turn_of_day = time_system.turn_of_day();
    state.day = time_system.day();

    return state;
}

// ── Apply to world ──────────────────────────────────────────────────

void apply_chunk_data(World& world, const std::vector<GameState::ChunkSave>& chunks) {
    for (const auto& chunk : chunks) {
        World::ChunkSaveData terrain;
        terrain.cx = chunk.cx;
        terrain.cy = chunk.cy;
        terrain.explored = parse_hex(chunk.explored_hex, CHUNK_SIZE * CHUNK_SIZE);
        terrain.modifications = chunk.modifications;
        terrain.placed_objects = chunk.placed_objects;
        world.apply_save_data(terrain);

        World::ChunkEntitySaveData entities;
        entities.cx = chunk.cx;
        entities.cy = chunk.cy;

        // Rebuild from the archetype databases so stats, colours, dialogue and
        // loot tables always match the current definitions.
        for (const auto& saved : chunk.npcs) {
            Npc npc = make_npc(saved.name, saved.x, saved.y);
            npc.set_affinity(saved.affinity);
            npc.set_shop_inventory(saved.shop_inventory);
            entities.npcs.push_back(std::move(npc));
        }
        for (const auto& saved : chunk.enemies) {
            Enemy enemy = make_enemy(saved.name, saved.x, saved.y);
            enemy.set_hp(saved.hp);
            entities.enemies.push_back(std::move(enemy));
        }
        for (const auto& saved : chunk.world_items) {
            const Item* item = find_item(saved.item_name);
            if (!item) continue;
            Entity dropped(saved.x, saved.y, item->glyph(), item->name(),
                           item->fg_r(), item->fg_g(), item->fg_b());
            dropped.set_is_item(true);
            entities.items.push_back(std::move(dropped));
        }

        world.apply_entity_save_data(entities);
    }
}
