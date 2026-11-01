#pragma once
#include <atomic>
#include <cstdint>

// FSM condition-tree property getters (FUN_141922190/1f0/2240). The mod build
// hooks int+bool (force path + PROPTRACE); the probe adds float + the F11
// (entity, id, index) call histogram.
using PropGetIntFn = int (__fastcall*)(void* entity, void* id, std::uint32_t index, int defaultValue);
using PropGetBoolFn = bool (__fastcall*)(void* entity, void* id, std::uint32_t index);
using PropGetFloatFn = float (__fastcall*)(void* entity, void* id, std::uint32_t index, std::uint64_t defaultBits);
using PropRegistryLookupFn = void* (__fastcall*)(void* table, std::int32_t key);
using PropNameFn = const char* (__fastcall*)(std::int32_t id);

extern PropGetIntFn g_originalPropInt;
extern PropGetBoolFn g_originalPropBool;
extern PropGetFloatFn g_originalPropFloat;

// PROPTRACE: 属性求值轨迹 (诊断翻滚切納刀的居合斬り属性源)。addresses.txt
// `propTrace=1` 启用; 只记本地玩家、结果非零、0→非0 跳变, 上限 120 行。
void PropTraceRecord(const void* entity, std::uint32_t id, std::uint32_t index,
                     std::uint32_t kind, std::int32_t ret);
// Probe-only histogram recorder (no-op stub in the mod build).
void RecordPropCall(const void* entity, std::uint32_t id, std::uint32_t index,
                    std::uint32_t kind, const void* caller, std::int32_t ret);

int __fastcall PropIntHook(void* entity, void* id, std::uint32_t index, int defaultValue);
bool __fastcall PropBoolHookMod(void* entity, void* id, std::uint32_t index);
bool __fastcall PropBoolHook(void* entity, void* id, std::uint32_t index);
float __fastcall PropFloatHook(void* entity, void* id, std::uint32_t index, std::uint64_t defaultBits);

bool InstallPropBoolHookMod();
bool InstallForceHook();
bool InstallPropHistogramHooks();
void PollPropHistogram();
