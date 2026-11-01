#pragma once
// Shared prelude: every split translation unit compiles with the exact include
// environment the old single-TU dllmain.cpp had (WIN32_LEAN_AND_MEAN and
// NOMINMAX come from the CMake target definitions, /utf-8 from CMakeLists).
#include <Windows.h>
#include <mmsystem.h>
#include <d3d11.h>
#include <dxgi.h>
#include <Psapi.h>

#include <atomic>
#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <intrin.h>
#include <mutex>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "imgui.h"
#include "backends/imgui_impl_dx11.h"
#include "kiero.h"
#include "MinHook.h"
