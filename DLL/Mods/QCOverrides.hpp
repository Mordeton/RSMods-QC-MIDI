#pragma once
#include "../Lib/Json/json.hpp"
#include <string>
#include <map>
#include <functional>
#include <cctype>

namespace QCAutomation::Overrides {
inline const char* Roles[] = {"Automatic", "Clean", "Overdrive", "Distortion", "Mod", "Solo"};
inline std::string NormalizeId(const std::string& value) {
    std::string result;
    for (unsigned char c : value) {
        if (c == '-' || c == '{' || c == '}' || std::isspace(c)) continue;
        if (!std::isxdigit(c)) return {};
        result += char(std::toupper(c));
    }
    return result.size() == 32 ? result : std::string{};
}
inline bool ValidRole(const std::string& role) {
    for (int i = 1; i <= 5; ++i) if (role == Roles[i]) return true;
    return false;
}
using ToneMap = std::map<std::string, std::string>;
struct Key {
    std::string song, arrangement;
    bool operator<(const Key& rhs) const { return std::tie(song, arrangement) < std::tie(rhs.song, rhs.arrangement); }
};
using Entries = std::map<Key, ToneMap>;
inline Entries Parse(const std::string& text) {
    auto json = nlohmann::json::parse(text);
    if (!json.is_object() || !json.at("version").is_number_integer() || json.at("version") != 1 || !json.at("entries").is_array())
        throw std::runtime_error("Unsupported QC override file");
    Entries result;
    for (const auto& entry : json.at("entries")) {
        Key key{entry.at("songKey").get<std::string>(), NormalizeId(entry.at("arrangementId").get<std::string>())};
        if (key.song.empty() || key.arrangement.empty() || result.count(key) || !entry.at("tones").is_object())
            throw std::runtime_error("Invalid or duplicate override identity");
        ToneMap tones;
        for (auto it = entry.at("tones").begin(); it != entry.at("tones").end(); ++it) {
            auto role = it.value().get<std::string>();
            if (it.key().empty() || !ValidRole(role)) throw std::runtime_error("Invalid tone role");
            tones.emplace(it.key(), role);
        }
        result.emplace(key, std::move(tones));
    }
    return result;
}
inline std::string Serialize(const Entries& entries) {
    nlohmann::json json = {{"version", 1}, {"entries", nlohmann::json::array()}};
    for (const auto& entry : entries)
        json["entries"].push_back({{"songKey", entry.first.song}, {"arrangementId", entry.first.arrangement}, {"tones", entry.second}});
    return json.dump(2) + "\n";
}
// The runtime owns this object. Publication follows successful durable replacement only.
struct Store {
    Entries entries;
    bool writable = true;
    std::string error;
    bool Reload(const std::string& text) {
        try { auto next = Parse(text); entries = std::move(next); writable = true; error.clear(); return true; }
        catch (const std::exception& ex) { writable = false; error = ex.what(); return false; }
    }
    std::string Resolve(const Key& key, const std::string& tone) const {
        auto found = entries.find(key);
        if (found == entries.end()) return {};
        auto mapping = found->second.find(tone);
        return mapping == found->second.end() ? std::string{} : mapping->second;
    }
    bool SaveTone(const Key& key, const std::string& tone, const std::string& role,
                  uint64_t expected, uint64_t current, const std::function<bool(const std::string&)>& write) {
        if (tone.empty() || (role != "Automatic" && !ValidRole(role))) { error = "Invalid tone role"; return false; }
        ToneMap next;
        auto found = entries.find(key);
        if (found != entries.end()) next = found->second;
        if (role == "Automatic") next.erase(tone); else next[tone] = role;
        return Save(key, next, expected, current, write);
    }
    bool Save(const Key& key, const ToneMap& tones, uint64_t expected, uint64_t current,
              const std::function<bool(const std::string&)>& write) {
        if (!writable || expected != current || key.song.empty() || NormalizeId(key.arrangement).empty()) {
            error = "Save rejected: unavailable or changed song context / override file"; return false;
        }
        for (const auto& tone : tones) if (tone.first.empty() || !ValidRole(tone.second)) {
            error = "Invalid tone role"; return false;
        }
        auto next = entries;
        if (tones.empty()) next.erase(key); else next[key] = tones;
        if (!write(Serialize(next))) { error = "Unable to save QC overrides; mappings unchanged"; return false; }
        entries = std::move(next); error.clear(); return true;
    }
};
}
