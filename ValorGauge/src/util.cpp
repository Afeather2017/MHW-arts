// Split out of src/dllmain.cpp (git 6c630fc) by tools/split_dllmain.py.
// Original segments: L380-L380, L512-L528, L559-L583, L868-L894, L1040-L1047, L2043-L2061. Bodies are verbatim; see that file's history
// for the per-feature commentary and docs/ for the topic manuals.

#include "prelude.h"
#include "all.h"

std::atomic<std::uintptr_t> g_moduleBase{0};

std::optional<std::uintptr_t> ParseAddress(std::string value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return std::nullopt;
    }
    value = value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1);
    if (value.starts_with("0x") || value.starts_with("0X")) {
        value.erase(0, 2);
    }

    std::uintptr_t address = 0;
    const auto result = std::from_chars(value.data(), value.data() + value.size(), address, 16);
    if (result.ec != std::errc{} || result.ptr != value.data() + value.size()) {
        return std::nullopt;
    }
    return address;
}

bool IsReadable(const void* address, std::size_t size) {
    MEMORY_BASIC_INFORMATION info{};
    if (!VirtualQuery(address, &info, sizeof(info)) || info.State != MEM_COMMIT ||
        (info.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0) {
        return false;
    }
    const auto start = reinterpret_cast<std::uintptr_t>(address);
    const auto end = start + size;
    const auto regionEnd = reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize;
    return end >= start && end <= regionEnd;
}

bool IsWritable(const void* address, std::size_t size) {
    MEMORY_BASIC_INFORMATION info{};
    if (!VirtualQuery(address, &info, sizeof(info)) || info.State != MEM_COMMIT ||
        (info.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0) {
        return false;
    }
    const DWORD writable = PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE |
                           PAGE_EXECUTE_WRITECOPY;
    const auto start = reinterpret_cast<std::uintptr_t>(address);
    const auto end = start + size;
    const auto regionEnd = reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize;
    return (info.Protect & writable) != 0 && end >= start && end <= regionEnd;
}

std::uint8_t* FindPattern(const std::uint8_t* pattern, const char* mask, std::size_t length) {
    const HMODULE game = GetModuleHandleW(nullptr);
    MODULEINFO module{};
    if (!game || !GetModuleInformation(GetCurrentProcess(), game, &module, sizeof(module))) {
        return nullptr;
    }

    const auto start = static_cast<const std::uint8_t*>(module.lpBaseOfDll);
    const auto imageSize = static_cast<std::size_t>(module.SizeOfImage);
    if (imageSize < length) {
        return nullptr;
    }

    for (std::size_t i = 0; i <= imageSize - length; ++i) {
        bool matches = true;
        for (std::size_t j = 0; j < length; ++j) {
            if (mask[j] == 'x' && start[i + j] != pattern[j]) {
                matches = false;
                break;
            }
        }
        if (matches) {
            return const_cast<std::uint8_t*>(start + i);
        }
    }
    return nullptr;
}

std::uintptr_t ModuleBaseAddr() {
    const HMODULE game = GetModuleHandleW(nullptr);
    MODULEINFO module{};
    if (!game || !GetModuleInformation(GetCurrentProcess(), game, &module, sizeof(module))) {
        return 0;
    }
    return reinterpret_cast<std::uintptr_t>(module.lpBaseOfDll);
}

bool ParseIdList(const std::string& text, std::vector<std::uint32_t>& out) {
    out.clear();
    std::size_t i = 0;
    while (i <= text.size()) {
        std::size_t j = text.find(',', i);
        if (j == std::string::npos) j = text.size();
        if (j > i) {
            try {
                out.push_back(
                    static_cast<std::uint32_t>(std::stoul(text.substr(i, j - i))));
            } catch (...) {
                return false;
            }
        }
        i = j + 1;
        if (j == text.size()) break;
    }
    return !out.empty();
}
