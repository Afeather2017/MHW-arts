#pragma once
#include <atomic>
#include <cstdint>
#include <mutex>
#include <optional>

#include "game_types.h"

// Action tracker + session lock. g_lockedPlayerController is authoritative
// once the first set-3 action locks it for the session; the tracker survives
// scene swaps only via the manager-chain cross-check.
extern std::mutex g_actionMutex;
extern void* g_lastLongSwordController;
extern void* g_lockedPlayerController;
extern std::optional<ActionInfo> g_lastLongSwordAction;

// Configured weapon type (1..14); Long Sword = 3. The hard gate for every
// gameplay write (iron rule 11).
extern std::atomic<std::uint32_t> g_modWeaponType;

void* ResolveLocalPlayerViaManager();
std::int32_t* ResolveLongSwordLevel(void* controller);
bool IsLongSwordController(void* controller);
bool IsLocalPlayerActionController(void* controller);
bool ModWeaponActive();
void LoadModWeaponTypeConfig();
std::uint8_t* DisplayWeapon();
bool IsNormalModeChargingAction(const ActionInfo& action);
bool ActionIsForesightSlash();
bool ActionIsRoll();
bool LockedPlayerActionChargesValor();
void* ResolveActionInstance(void* ctrl, std::int32_t set, std::int32_t id);
