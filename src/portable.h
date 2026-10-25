// Small portability helpers shared by the engine and tools.
#pragma once
#include <filesystem>
#include <fstream>
#include <string>

constexpr float kPi = 3.14159265358979f;

// All paths in the code are UTF-8 (SDL returns UTF-8, game folders are often Cyrillic).
// On Windows a narrow std::string path is interpreted in the ANSI code page, so convert explicitly.
inline std::filesystem::path u8path(const std::string& s) {
#if defined(__cpp_char8_t)
    return std::filesystem::path(std::u8string(s.begin(), s.end()));
#else
    return std::filesystem::u8path(s);
#endif
}

inline bool fileExists(const std::string& p) {
    std::error_code ec;
    return std::filesystem::exists(u8path(p), ec);
}

inline void removeFile(const std::string& p) {
    std::error_code ec;
    std::filesystem::remove(u8path(p), ec);
}
