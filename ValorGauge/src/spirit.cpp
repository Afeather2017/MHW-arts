// Split out of src/dllmain.cpp (git 6c630fc) by tools/split_dllmain.py.
// Original segments: L104-L104, L2636-L2697, L2699-L2753. Bodies are verbatim; see that file's history
// for the per-feature commentary and docs/ for the topic manuals.

#include "prelude.h"
#include "all.h"

std::atomic_int32_t g_lastObservedSpiritLevel = -1;

// timer / timerhit rules: extend the level-drop countdown (weapon+0x2374,
// 0..1 fraction of the current level's Gauge_Duration) - the same primitive
// as the vanilla 见切-success extension (FUN_142123510:
// +0x2374 += playerParam+0x888). Seconds convert with the live w03p duration
// (playerParam+0x858/85C/860 by level) so "1.0" buys one countdown second.
// Mirror of the vanilla spirit-level change recipe (FUN_14211c5f0 branches):
// write the level, refresh the drop countdown, arm the no-decay grace
// (w03p Gauge_No_Decay_Time_after_Spirit_Gauge_Upgrade at playerParam+0x884)
// and fire the blade-aura transition FX + steady color latch (weapon+0xad4)
// with the same ids the game uses for the same transition. Used by the
// chargehit path (level 0 -> white with a near-empty bar, full white ->
// yellow = valor entry); vanilla upgrade moves still own yellow->red.
// timerValue: the drop-countdown value to leave the new level with. Full
// (default) for yellow/red entry; near-empty for the white conversion so the
// bar must be charged up by hits before it can cross into a promotion.
void SetSpiritLevel(std::uint8_t* player, std::uint8_t* weapon,
                    std::int32_t newLevel, const char* why,
                    float timerValue) {
    if (!player || !weapon || newLevel < 1 || newLevel > 3 ||
        !IsReadable(weapon + 0x2370, sizeof(float) * 2)) {
        return;
    }
    auto* levelAddr =
        reinterpret_cast<volatile std::int32_t*>(weapon + 0x2370);
    auto* timerAddr = reinterpret_cast<volatile float*>(weapon + 0x2374);
    const auto prev = *levelAddr;
    *levelAddr = newLevel;
    *timerAddr = timerValue;
    float grace = 0.0f;
    if (IsReadable(player + 0x7810, sizeof(void*))) {
        auto* paramTable = *reinterpret_cast<std::uint8_t**>(player + 0x7810);
        if (paramTable && IsReadable(paramTable + 0x884, sizeof(float))) {
            grace = *reinterpret_cast<const volatile float*>(paramTable + 0x884);
        }
    }
    if (grace > 0.0f && IsWritable(weapon + 0x2390, sizeof(float))) {
        *reinterpret_cast<volatile float*>(weapon + 0x2390) = grace;
    }
    std::uint32_t fxId = 0;
    std::uint32_t steady = 0;
    switch (newLevel) {
        case 1: fxId = 100; steady = 100; break;   // white
        case 2: fxId = 0x65; steady = 0x66; break;  // yellow
        case 3: fxId = 0x68; steady = 0x69; break;  // red
    }
    bool auraOk = false;
    if (fxId && g_auraSet &&
        IsReadable(weapon + 0xAD4, sizeof(std::uint32_t))) {
        g_auraSet(weapon, fxId, 0, 0, 0);
        *reinterpret_cast<volatile std::uint32_t*>(weapon + 0xAD4) = steady;
        auraOk = true;
    }
    static std::atomic_uint32_t levelLogs{0};
    if (levelLogs.load(std::memory_order_relaxed) < 64) {
        levelLogs.fetch_add(1, std::memory_order_relaxed);
        std::ostringstream m;
        m << "GAUGERULE spirit-level " << why << ": " << prev << " -> "
          << newLevel << " timer=" << timerValue << " grace=" << grace
          << "s aura=" << (auraOk ? "fired" : "skipped");
        Log(m.str());
    }
}

void ExtendLevelDropTimer(std::uint8_t* player, float seconds,
                          std::int32_t actionSet, std::int32_t actionId,
                          bool fromHit) {
    if (seconds <= 0.0f || !player) {
        return;
    }
    if (!IsReadable(player + 0x76B0, sizeof(void*))) {
        return;
    }
    auto* weapon = *reinterpret_cast<std::uint8_t**>(player + 0x76B0);
    if (!weapon || !IsReadable(weapon + 0x2374, sizeof(float))) {
        return;
    }
    // Defense in depth: the level timer / duration params only exist on the
    // Long Sword weapon struct - never touch a foreign weapon.
    if (!IsReadable(weapon + 0x9F8, sizeof(std::uint32_t)) ||
        *reinterpret_cast<const volatile std::uint32_t*>(weapon + 0x9F8) !=
            g_modWeaponType.load(std::memory_order_relaxed)) {
        return;
    }
    const auto level =
        *reinterpret_cast<const volatile std::int32_t*>(weapon + 0x2370);
    if (level < 1 || level > 3) {
        return;  // no aura level to sustain
    }
    if (!IsReadable(player + 0x7810, sizeof(void*))) {
        return;
    }
    auto* paramTable = *reinterpret_cast<std::uint8_t**>(player + 0x7810);
    if (!paramTable) {
        return;
    }
    const float duration = *reinterpret_cast<const volatile float*>(
        paramTable + 0x858 + (level - 1) * 4);
    if (!(duration > 0.0f)) {
        return;
    }
    auto* timer = reinterpret_cast<volatile float*>(weapon + 0x2374);
    const float cur = *timer;
    const float next = std::min(1.0f, cur + seconds / duration);
    if (next == cur) {
        return;
    }
    *timer = next;
    static std::atomic_uint32_t timerRuleLogs{0};
    if (timerRuleLogs.load(std::memory_order_relaxed) < 128) {
        timerRuleLogs.fetch_add(1, std::memory_order_relaxed);
        std::ostringstream m;
        m << "GAUGERULE timer" << (fromHit ? "-hit" : "") << " act="
          << actionSet << ':' << actionId << " +" << seconds << "s lv=" << level
          << " dur=" << duration << " countdown=" << cur * duration << "s -> "
          << next * duration << "s";
        Log(m.str());
    }
}
