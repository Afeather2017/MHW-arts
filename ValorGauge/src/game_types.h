#pragma once
#include <cstdint>

// The game's action-request pair handed to ActionController::DoAction
// (set 3 = Long Sword weapon actions).
struct ActionInfo {
    std::int32_t actionSet;
    std::int32_t actionId;
};
