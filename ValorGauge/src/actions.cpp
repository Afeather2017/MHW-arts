// Split out of src/dllmain.cpp (git 6c630fc) by tools/split_dllmain.py.
// Original segments: L1768-L2018, L7406-L7441. Bodies are verbatim; see that file's history
// for the per-feature commentary and docs/ for the topic manuals.

#include "prelude.h"
#include "all.h"

DoActionFn g_originalDoAction = nullptr;
std::atomic_bool g_insideDoAction{false};

bool __fastcall DoActionHook(void* controller, ActionInfo* action) {
    const bool validAction = controller && action && IsReadable(action, sizeof(ActionInfo));
    g_insideDoAction.store(true, std::memory_order_release);
    if (validAction) {
#ifdef VALOR_PROBE
        const auto sample = g_actionDiagnosticCount.fetch_add(1);
        if (sample < 32) {
            void* owner = nullptr;
            void* humanController = nullptr;
            const auto ownerAddress = static_cast<std::uint8_t*>(controller) + 0x100;
            if (IsReadable(ownerAddress, sizeof(void*))) {
                owner = *reinterpret_cast<void**>(ownerAddress);
            }
            if (owner) {
                const auto humanControllerAddress = static_cast<std::uint8_t*>(owner) + 0x12608;
                if (IsReadable(humanControllerAddress, sizeof(void*))) {
                    humanController = *reinterpret_cast<void**>(humanControllerAddress);
                }
            }
            std::ostringstream diagnostic;
            diagnostic << "Action hook sample " << (sample + 1) << ": set=" << action->actionSet
                       << " id=" << action->actionId << " controller=0x" << std::hex << std::uppercase
                       << reinterpret_cast<std::uintptr_t>(controller) << " owner=0x"
                       << reinterpret_cast<std::uintptr_t>(owner) << " humanController=0x"
                       << reinterpret_cast<std::uintptr_t>(humanController);
            Log(diagnostic.str());

            if (false && action->actionSet == 3 && action->actionId >= 98 && action->actionId <= 102) {
                auto* player = static_cast<std::uint8_t*>(controller) - 0x61C8;
                std::ostringstream special;
                special << "SPECIAL action 3:" << action->actionId
                        << " controller=0x" << std::hex << std::uppercase
                        << reinterpret_cast<std::uintptr_t>(controller)
                        << " player=0x" << reinterpret_cast<std::uintptr_t>(player);
                auto read32 = [&](std::size_t offset, const char* name) {
                    const auto* address = player + offset;
                    if (IsReadable(address, sizeof(std::int32_t))) {
                        special << " " << name << "=" << std::dec
                                << *reinterpret_cast<const std::int32_t*>(address)
                                << "(0x" << std::hex << *reinterpret_cast<const std::int32_t*>(address) << ")";
                    } else {
                        special << " " << name << "=<unreadable>";
                    }
                };
                read32(0x6274, "fsmTarget");
                read32(0x6278, "fsmId");
                read32(0x6284, "deriveSet");
                read32(0x6288, "deriveId");
                Log(special.str().c_str());
            }
        } else if (sample == 32) {
            Log("Action hook diagnostic sample limit reached");
        }
#endif
    }

#ifdef VALOR_PROBE
    if (validAction && action->actionSet == 3 && action->actionId >= 98 && action->actionId <= 102) {
        auto* player = static_cast<std::uint8_t*>(controller) - 0x61C8;
        std::ostringstream special;
        special << "SPECIAL action 3:" << action->actionId << " controller=0x" << std::hex << std::uppercase
                << reinterpret_cast<std::uintptr_t>(controller) << " player=0x" << reinterpret_cast<std::uintptr_t>(player);
        auto read32 = [&](std::size_t offset, const char* name) {
            const auto* address = player + offset;
            if (IsReadable(address, sizeof(std::int32_t))) {
                const auto value = *reinterpret_cast<const std::int32_t*>(address);
                special << " " << name << "=" << std::dec << value << "(0x" << std::hex << value << ")";
            }
        };
        read32(0x6274, "fsmTarget"); read32(0x6278, "fsmId");
        read32(0x6284, "deriveSet"); read32(0x6288, "deriveId");
        Log(special.str().c_str());
    }
#endif

#ifndef VALOR_PROBE
    // ARTGATE (战技槽): below a full gauge the art combo request is swallowed
    // BEFORE the tracker and the original DoAction see it - it never becomes
    // current, never lands in the buffer, and no node on the request path can
    // enter. At 100% the request passes; the accept path drains the gauge.
    bool artPass = false;
    if (validAction && g_artEnabled.load(std::memory_order_relaxed) &&
        ArtGateMatches(action->actionSet, action->actionId) &&
        IsLocalPlayerActionController(controller) && ArtWeaponGate(controller)) {
        if (g_artGauge.load(std::memory_order_relaxed) >= 100.0f) {
            artPass = true;
        } else {
            static std::atomic_uint32_t artBlockLogs{0};
            const auto n = artBlockLogs.fetch_add(1, std::memory_order_relaxed);
            if (n < 8 || (n % 600) == 0) {
                std::ostringstream m;
                m << "ARTGATE swallow 3:" << action->actionId << " gauge="
                  << static_cast<int>(g_artGauge.load(std::memory_order_relaxed) +
                                      0.5f)
                  << "% (n=" << n << ")";
                Log(m.str());
            }
            g_insideDoAction.store(false, std::memory_order_release);
            return false;
        }
    }
#endif

#ifndef VALOR_PROBE
    // 收刀拔刀刚1: vanilla routes sheathed R2 to {3,11} in code; rewrite the
    // request BEFORE the tracker copies it so latch/rules/logs see the new id.
    if (validAction && action->actionSet == 3 && IsLongSwordController(controller)) {
        ApplySheathRewrite(controller, action);
    }
#endif

    if (validAction && IsLongSwordController(controller)) {
        const ActionInfo current = *action;
        bool changed = false;
        bool isLockedPlayer = false;
        {
            std::scoped_lock lock(g_actionMutex);
            // Weapon actions use set 3. Once observed, keep this controller for the session so
            // monster/secondary controllers that happen to pass the structural test are ignored.
            // Multiplayer: another hunter's weapon action must never win the lock. The manager
            // chain (when it validates) is authoritative; without it we only lock when no
            // chain-based objection exists, and log the refusal a few times.
            if (!g_lockedPlayerController && current.actionSet == 3) {
                void* chainPlayer = ResolveLocalPlayerViaManager();
                const auto candidatePlayer = static_cast<std::uint8_t*>(controller) - 0x61C8;
                if (chainPlayer && chainPlayer != candidatePlayer) {
                    static std::atomic_uint32_t foreignActionLogs{0};
                    if (foreignActionLogs.load(std::memory_order_relaxed) < 8) {
                        foreignActionLogs.fetch_add(1, std::memory_order_relaxed);
                        std::ostringstream refused;
                        refused << "FOREIGN set-3 action: controller 0x" << std::hex
                                << std::uppercase
                                << reinterpret_cast<std::uintptr_t>(controller)
                                << " is not the local player (manager chain player 0x"
                                << reinterpret_cast<std::uintptr_t>(chainPlayer)
                                << ") - not locking";
                        Log(refused.str());
                    }
                } else {
                g_lockedPlayerController = controller;
                if (chainPlayer) {
                    Log("Local player controller lock confirmed by manager chain");
                }
                std::ostringstream locked;
                locked << "Player action controller locked at 0x" << std::hex << std::uppercase
                       << reinterpret_cast<std::uintptr_t>(controller);
                Log(locked.str());
                const auto* player = static_cast<std::uint8_t*>(controller) - 0x61C8;
                std::ostringstream addrLog;
                addrLog << "ADDR player=0x" << std::hex << std::uppercase
                        << reinterpret_cast<std::uintptr_t>(player);
                const auto* weaponSlot = player + 0x76B0;
                if (IsReadable(weaponSlot, sizeof(void*))) {
                    const void* weapon = *reinterpret_cast<void* const*>(weaponSlot);
                    addrLog << " weaponSlot=0x"
                            << reinterpret_cast<std::uintptr_t>(weaponSlot)
                            << " weapon=0x" << reinterpret_cast<std::uintptr_t>(weapon);
                    if (weapon && IsReadable(weapon, 0x2400)) {
                        addrLog << " spiritLevel="
                                << std::dec
                                << *reinterpret_cast<const std::int32_t*>(
                                       static_cast<const std::uint8_t*>(weapon) + 0x2370)
                                << " invulnTimer="
                                << *reinterpret_cast<const float*>(
                                       static_cast<const std::uint8_t*>(weapon) + 0x2388);
                    }
                } else {
                    addrLog << " weaponSlot=<unreadable>";
                }
                Log(addrLog.str());
                }
            }
            isLockedPlayer = controller == g_lockedPlayerController;
            changed = isLockedPlayer &&
                      (controller != g_lastLongSwordController || !g_lastLongSwordAction ||
                       g_lastLongSwordAction->actionSet != current.actionSet ||
                       g_lastLongSwordAction->actionId != current.actionId);
            if (changed) {
                g_lastLongSwordController = controller;
                g_lastLongSwordAction = current;
#ifndef VALOR_PROBE
                // Re-arm the clang latch only when a NEW foresight slash starts;
                // resetting on every action change caused a force loop (the
                // injected window kept producing rejections during the counter).
                if (current.actionSet == 3 && (current.actionId == 86 ||
                    current.actionId == 101 || current.actionId == 102)) {
                    g_clangLatch = false;
                }

                const bool normalAction = IsNormalModeChargingAction(current) &&
                                          ModWeaponActive();
                if (g_valorMode.load() && normalAction) {
                    g_valorMode = false;
                    g_valorCharge = 0;
                    Log("Valor mode exited via normal-mode action; charging re-enabled");
                }
                // Legacy charging: +10 on action START (fires even on a whiff).
                // Only active when hit-charge is set to 0 in hit-charge.txt;
                // the default path charges in DmgCalcHook on CONNECTED hits.
                if (normalAction && g_hitChargePercent.load(std::memory_order_relaxed) == 0) {
                    const auto oldCharge = g_valorCharge.load();
                    const auto newCharge = std::min<std::uint32_t>(100, oldCharge + 10);
                    g_valorCharge = newCharge;
                    std::ostringstream chargeMessage;
                    chargeMessage << "Valor charge: " << oldCharge << "% -> " << newCharge
                                  << "% from action 3:" << current.actionId;
                    if (newCharge == 100 && oldCharge < 100) {
                        chargeMessage << " (READY)";
                        ActivateValorMode(controller);
                    }
                    Log(chargeMessage.str());
                }
#endif
            }
        }
        if (changed && isLockedPlayer) {
            std::ostringstream message;
            message << "Long Sword action changed: set=" << current.actionSet
                    << " id=" << current.actionId << " controller=0x"
                    << std::hex << std::uppercase << reinterpret_cast<std::uintptr_t>(controller);
            Log(message.str());
        }
    }
    const bool result = g_originalDoAction(controller, action) != 0;
#ifndef VALOR_PROBE
    if (result && artPass && g_artConsumeOnPass.load(std::memory_order_relaxed)) {
        // The released art request was accepted: spend the gauge.
        const float spent = g_artGauge.exchange(0.0f);
        std::ostringstream m;
        m << "ART released: 3:" << action->actionId << " accepted, gauge "
          << static_cast<int>(spent + 0.5f) << "% -> 0";
        Log(m.str());
    }
    if (result && validAction && controller == g_lockedPlayerController &&
        action->actionSet == 3) {
        // Stamina control: one-shot cost (见切) + reset the empty-exit latch.
        StaminaOnActionAccepted(
            static_cast<std::uint32_t>(action->actionId));
        ApplyActionGaugeRules(action->actionSet, action->actionId);
        // 刚1 gauge scope: arm for EVERY local-player 112 accept (rewritten
        // sheathed draws and any natural 112 - the 刚1 move, unconditional
        // 0.15 net cost). Snapshot is pre-cost: the updater's begin
        // (start-cost handler) runs after this.
        if (action->actionId == 112) {
            ArmGang1GaugeScope(controller);
        }
    }
#endif
    g_insideDoAction.store(false, std::memory_order_release);
    return result;
}

bool InstallActionHook() {

    constexpr std::uint8_t pattern[] = {
        0x48, 0x8D, 0x41, 0x07, 0x48, 0xC1, 0xE0, 0x04, 0x46, 0x3B, 0x04, 0x08
    };
    constexpr char mask[] = "xxxxxxxxxxxx";
    auto* match = FindPattern(pattern, mask, sizeof(pattern));
    if (!match) {
        Log("Action hook failed: ActionController::DoAction signature not found");
        return false;
    }

    void* target = match - 10; // SharpPluginLoader AddressRecords.json offset.
    g_addrDoAction = reinterpret_cast<std::uintptr_t>(target);
    const auto createStatus = MH_CreateHook(target, reinterpret_cast<void*>(DoActionHook),
                                            reinterpret_cast<void**>(&g_originalDoAction));
    if (createStatus != MH_OK) {
        std::ostringstream message;
        message << "Action hook failed: MH_CreateHook status=" << createStatus;
        Log(message.str());
        return false;
    }
    const auto enableStatus = MH_EnableHook(target);
    if (enableStatus != MH_OK) {
        std::ostringstream message;
        message << "Action hook failed: MH_EnableHook status=" << enableStatus;
        Log(message.str());
        return false;
    }

    std::ostringstream message;
    message << "Long Sword action hook installed at 0x" << std::hex << std::uppercase
            << reinterpret_cast<std::uintptr_t>(target);
    Log(message.str());
    return true;
}
