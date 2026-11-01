#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>

// Vitals slots on the player object. USER-VERIFIED 2026-10-05: HP box IS
// *(player+0x7630): +0x60=max, +0x64=cur, stamina cur/max at +0x12C/+0x130.
extern std::atomic<std::size_t> g_vitalsHpSlot;
extern std::atomic<std::size_t> g_vitalsStamSlot;

std::uint8_t* VitalsBox(std::uint8_t* player, std::size_t slot);
std::uint8_t* WeaponOfPlayer(std::uint8_t* player);
void LoadVitalsConfig();
void LoadHpLockConfig();
void PollHpLock();
void PollHpWatch();
void StaminaOnActionAccepted(std::uint32_t actionId);
void PollStaminaControl();
void LoadStaminaConfig();
