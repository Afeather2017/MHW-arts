#pragma once
#include <atomic>
#include <memory>
#include <string>
#include <vector>

// ACTIONSND (2026-10-22): play a custom WAV (winmm PlaySound) at a configured
// TIML frame of a configured action, or on "enter" - the moment the action
// becomes current (phase 2), hitstop-immune (defense-frame clang chain).
struct ActionSoundRule {
    std::int32_t set = 0;
    std::int32_t id = 0;
    float frame = 0.0f;      // TIML frames (60fps), frame mode only
    std::string path;
    bool onEnter = false;    // fire when the action starts, not at a frame
    bool armed = false;      // armed for the current occurrence of the action
    bool fired = false;
    float armMot = 0.0f;     // motion-clock snapshot taken at arm time
};
extern std::atomic<std::shared_ptr<std::vector<ActionSoundRule>>> g_actionSounds;

void LoadActionSoundConfig();
void PollActionSounds(const void* entity);
