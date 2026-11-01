#pragma once
#include <cstdint>

// Quest/stage/zone reads (HunterPie-mapped singletons, see memorymap.h).
bool ReadQuestInfo(std::int32_t* questId, std::int32_t* state);
bool InQuestState();
bool PlayableStageId(std::int32_t stage);
std::int32_t ReadStageId();
bool InCombatZone();
