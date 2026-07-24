#include "game/settings.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cstdlib>
#include <string>

namespace fs = std::filesystem;

static fs::path settings_path() {
    fs::path dir = fs::path(std::getenv("APPDATA")) / "AsciiGame";
    fs::create_directories(dir);
    return dir / "settings.json";
}

static int extract_int(const std::string& json, const std::string& key, int def) {
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return def;
    pos = json.find(':', pos + search.size());
    if (pos == std::string::npos) return def;
    pos++;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
    size_t end = pos;
    while (end < json.size() && (isdigit(json[end]) || json[end] == '-')) end++;
    if (end == pos) return def;
    try { return std::stoi(json.substr(pos, end - pos)); } catch (...) { return def; }
}

bool load_settings(Settings& s) {
    std::ifstream f(settings_path());
    if (!f.is_open()) return false;
    std::string json((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

    s.load_radius   = extract_int(json, "load_radius", s.load_radius);
    s.render_radius = extract_int(json, "render_radius", s.render_radius);
    s.sim_radius    = extract_int(json, "sim_radius", s.sim_radius);
    return true;
}

bool save_settings(const Settings& s) {
    std::ofstream f(settings_path());
    if (!f.is_open()) return false;
    f << "{\n";
    f << "  \"load_radius\": "   << s.load_radius   << ",\n";
    f << "  \"render_radius\": " << s.render_radius << ",\n";
    f << "  \"sim_radius\": "    << s.sim_radius    << "\n";
    f << "}\n";
    return f.good();
}
