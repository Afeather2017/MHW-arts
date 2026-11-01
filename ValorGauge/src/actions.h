#pragma once
#include <atomic>

#include "game_types.h"

using DoActionFn = bool(__fastcall*)(void*, ActionInfo*);
extern DoActionFn g_originalDoAction;
// Reentrancy latch: true while DoActionHook / a trampoline DoAction call is
// running (game thread), so the per-frame polls never interrupt into it.
extern std::atomic_bool g_insideDoAction;

bool __fastcall DoActionHook(void* controller, ActionInfo* action);
bool InstallActionHook();
