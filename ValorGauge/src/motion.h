#pragma once
#include <atomic>
#include <cstdint>
#include <vector>

// MOTIONCOND (mod): conditional motion swap for one action. While the watched
// action is fresh (frame < 0.2 s) the instance motion id (+0x30) is rewritten
// to entry|0xC000 and the motion restarts via player vtable+0x288.
struct MotionBind {
    std::uint32_t actionId = 0; // the action always plays this LMT entry
    std::uint32_t entry = 0;
    bool swapped = false;
    std::uint32_t lastActId = 0;
    std::uint32_t missTicks = 0; // transient id-read glitches must not re-arm
    float prevFrame = 0.0f;      // backwards jump = the action re-began
};
extern std::vector<MotionBind> g_motionBinds;

// Clock miner v2: while a forced action runs, scan candidate regions for
// floats whose per-tick delta is a CONSTANT step.
struct ClkRegion {
    const char* name;
    std::uint32_t size;
    std::vector<float> prev;
    std::vector<float> step;
    std::vector<std::uint8_t> valid;
    std::vector<std::uint8_t> hits;
    const void* lastBase = nullptr;
};
extern ClkRegion g_clkRegions[7];
extern std::int32_t g_clkTicks;
extern bool g_clkReported;
extern float g_clkLastFrame;

// Frame seek: jump the action-instance frame counter (instance+8) and the
// motion clock (anim+0x10C) to a chosen start frame right after a forced
// action begins. <0 = off; the 4th token of force-action.txt.
extern std::atomic_bool g_frameWatchArmed;
extern std::atomic_bool g_seekArmed;
extern std::int32_t g_frameWatchSet;
extern std::int32_t g_frameWatchId;
extern std::int32_t g_frameWatchTicksLeft;
extern std::int32_t g_frameTickLogsLeft;
extern bool g_frameWatchSeen;
extern float g_frameLastLogged;
extern float g_frameLastDt;
extern bool g_seekFired;
extern std::int32_t g_seekWrites;
extern float g_seekFrame;

void ClkResetRegions();
void ApplySheatheMotionPatch();
void ArmFrameSeekDiagnostics();
void MineClocks(ClkRegion& r, const void* base);
void TryFrameSeek(const void* entity);
void PollMotionBind(const void* entity);
