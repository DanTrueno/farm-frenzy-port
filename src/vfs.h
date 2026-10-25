// Virtual file system over the original game's .pack files (plain zip archives).
// Paths are case-insensitive and accept both '/' and '\' like on Windows.
#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

class Vfs {
public:
    // Packs mounted later take priority (locale pack over data pack).
    bool mount(const std::string& packPath);
    // Plain directory with loose files (also case-insensitive lookup).
    bool exists(const std::string& path) const;
    bool read(const std::string& path, std::vector<uint8_t>& out) const;
    static std::string normalize(const std::string& path);
    std::vector<std::string> list(const std::string& prefix) const;

private:
    struct Entry {
        int pack;
        uint32_t localOffset, compSize, size;
        uint16_t method;
    };
    std::vector<std::vector<uint8_t>> packs_;
    std::unordered_map<std::string, Entry> entries_;
};

extern Vfs g_vfs;
