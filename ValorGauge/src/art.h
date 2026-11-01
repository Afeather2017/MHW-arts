#pragma once
#include <atomic>
#include <cstdint>

// --- 战技槽 / art gauge (2026-10-26, user design) ----------------------------
// MHXX hunter-arts resource as a DLL-EXTERNAL field (no game memory holds it).
// Release gate: the R+△ combo reaches the game as the 3:87 request; below a
// full gauge the DoAction hook swallows it. FSM-side art nodes MUST enter via
// the request path (ActionNo = the gated id) - button-property link edges read
// raw keys and bypass this gate entirely (AGENTS.md iron rule).
struct ArtChargeRow {
    std::int32_t set = 3;
    std::int32_t id = 0;    // 0 = every move in the set (exact rows win)
    float percent = 0.0f;   // +percent per connected hit
};

extern std::atomic<float> g_artGauge;        // the 战技条 itself, 0..100
extern std::atomic_bool g_artEnabled;        // config file present
extern std::atomic_int g_artConsumeOnPass;   // drain on the released request
extern std::atomic<std::int32_t> g_artGateSet;
extern std::atomic<float> g_artDmgScale;     // charge-dmg scaling, 0 = off
extern std::atomic<float> g_artDmgCap;

bool ArtGateMatches(std::int32_t set, std::int32_t id);
bool ArtWeaponGate(void* controller);
float ArtChargePercentFor(std::int32_t set, std::int32_t id);
void PollArtQuestGate();
void LoadArtConfig();
