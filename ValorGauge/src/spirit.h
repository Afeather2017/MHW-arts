#pragma once
#include <atomic>
#include <cstdint>

extern std::atomic<std::int32_t> g_lastObservedSpiritLevel;

// Mirror of the vanilla spirit-level change recipe (FUN_14211c5f0 branches):
// write the level, refresh the drop countdown, arm the no-decay grace and
// fire the blade-aura transition FX + steady color latch. Used by keep-white /
// valor-exit / chargehit conversion paths. timerValue: the drop-countdown
// value to leave the new level with (full default; near-empty 0.05 for the
// white conversion so the bar must be charged up by hits).
void SetSpiritLevel(std::uint8_t* player, std::uint8_t* weapon,
                    std::int32_t newLevel, const char* why,
                    float timerValue = 1.0f);

// Extend the level-drop countdown (weapon+0x2374, 0..1 fraction of the
// current level's Gauge_Duration) - the timer/timerhit rule primitive.
void ExtendLevelDropTimer(std::uint8_t* player, float seconds,
                          std::int32_t actionSet, std::int32_t actionId,
                          bool fromHit);
