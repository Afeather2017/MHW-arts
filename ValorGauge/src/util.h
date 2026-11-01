#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// Module base of the running game exe (set once by InstallPoolQueryHook /
// InstallPropHistogramHooks, read everywhere for RVA math).
extern std::atomic<std::uintptr_t> g_moduleBase;

bool IsReadable(const void* address, std::size_t size);
bool IsWritable(const void* address, std::size_t size);
std::uint8_t* FindPattern(const std::uint8_t* pattern, const char* mask,
                          std::size_t length);
std::uintptr_t ModuleBaseAddr();
std::optional<std::uintptr_t> ParseAddress(std::string value);
bool ParseIdList(const std::string& text, std::vector<std::uint32_t>& out);
