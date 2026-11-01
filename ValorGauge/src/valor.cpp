// Split out of src/dllmain.cpp (git 6c630fc) by tools/split_dllmain.py.
// Original segments: L70-L75, L118-L136, L602-L621, L755-L866, L2161-L2197, L8304-L8308, L8310-L8374. Bodies are verbatim; see that file's history
// for the per-feature commentary and docs/ for the topic manuals.

#include "prelude.h"
#include "all.h"

std::atomic_uint32_t g_valorCharge = 0;
std::atomic_bool g_valorMode = false;
// Hit-based valor charging (mod build). >0: percent per CONNECTED hit
// (DmgCalcHook); 0: legacy charge-on-action-start; <0: charging disabled.
// Configured via nativePC/plugins/ValorGauge.hit-charge.txt.
std::atomic_int g_hitChargePercent{10};

// Gauge-lock experiment (2026-09-24): per-frame overwrite of an LS gauge
// float. Round 1 (target 0x2374): game drains it 0.000556/frame but the
// visible bar did NOT follow -> 0x2374 is the level-duration timer. Round 2
// target: weapon+0x2368 (second HUD getter FUN_140616DD0 reads it x100; the
// per-move charge gains and the cost compare FUN_140B980C0 use it; the iai
// stance writes int 0/1/2 over the same dword = gauge-clear-on-stance).
// Config nativePC/plugins/ValorGauge.gauge-lock.txt = "[<target>] <value 0..1>"
//   target 1 = weapon+0x2374, 2 = weapon+0x2368; bare "<value>" = target 1.
// F5 toggles at runtime. BOTH fields are always watched: drift = the game
// wrote underneath, rate-limited logged as GAUGELOCK.
std::atomic<int> g_gaugeLockTarget{1};
std::atomic<float> g_gaugeLockValue{0.0f};
std::atomic_bool g_gaugeLockEnabled{false};
bool g_gaugeLockKeyWasDown = false;
// DrawGauge(Present)-thread only state. Index 0 = +0x2374, 1 = +0x2368.
float g_gaugeLockLastWritten[2] = {-1.0f, -1.0f};
float g_gaugeWatchDrift[2] = {0.0f, 0.0f};
std::uint32_t g_gaugeWatchFrames[2] = {0, 0};
std::chrono::steady_clock::time_point g_gaugeLockNextReport{};

void ActivateValorMode(void* controller) {
    if (g_valorMode.load()) {
        return;
    }
    auto* level = ResolveLongSwordLevel(controller);
    if (!level) {
        Log("Valor activation failed: Long Sword level pointer chain did not resolve");
        return;
    }

    const auto previous = *level;
    *level = 3; // Red aura: strongest Long Sword level and confirmed writable by the Lua mod.
    g_lastObservedSpiritLevel = 3;
    g_valorMode = true;

    std::ostringstream message;
    message << "Valor mode activated: Long Sword level " << previous << " -> 3 at 0x"
            << std::hex << std::uppercase << reinterpret_cast<std::uintptr_t>(level);
    Log(message.str());
}

void PollValorLifecycle() {
    void* controller = nullptr;
    {
        std::scoped_lock lock(g_actionMutex);
        controller = g_lockedPlayerController;
    }
    // The chain player tracks the LIVE entity across scene swaps; the action
    // lock survives them (hub -> quest creates a new controller), so a stale
    // lock must not shadow the chain.
    std::uint8_t* chainPlayer =
        static_cast<std::uint8_t*>(ResolveLocalPlayerViaManager());
    if (chainPlayer && controller &&
        static_cast<std::uint8_t*>(controller) - 0x61C8 != chainPlayer) {
        controller = nullptr;
    }
    std::uint8_t* player = nullptr;
    std::uint8_t* weapon = nullptr;
    const std::int32_t* level = nullptr;
    if (controller) {
        player = static_cast<std::uint8_t*>(controller) - 0x61C8;
        level = ResolveLongSwordLevel(controller);
        if (level && IsReadable(player + 0x76B0, sizeof(void*))) {
            weapon = *reinterpret_cast<std::uint8_t**>(player + 0x76B0);
        }
    }
    if (!level) {
        // No action lock yet (main menu / hub / a fresh quest before the
        // first LS action). Watch through the manager chain instead so the
        // keep-white pre-arm below fires BEFORE any action happens - the
        // HUD gauge color only repaints from the value already present at
        // hub/quest entry, so the white blade must exist by then.
        player = chainPlayer;
        if (!player || !IsReadable(player + 0x76B0, sizeof(void*))) {
            return;
        }
        weapon = *reinterpret_cast<std::uint8_t**>(player + 0x76B0);
        if (!weapon || !IsReadable(weapon + 0x2370, sizeof(std::int32_t))) {
            return;
        }
        // The 0x2370 slot means something else on foreign weapons - only
        // follow the modded weapon through this path.
        if (!IsReadable(weapon + 0x9F8, sizeof(std::uint32_t)) ||
            *reinterpret_cast<const volatile std::uint32_t*>(weapon + 0x9F8) !=
                g_modWeaponType.load(std::memory_order_relaxed)) {
            return;
        }
        level = reinterpret_cast<const std::int32_t*>(weapon + 0x2370);
    }

    const auto current = *level;
    const auto previous = g_lastObservedSpiritLevel.exchange(current);
    if (previous != current) {
        std::ostringstream message;
        message << "Long Sword spirit level changed: " << previous << " -> " << current;
        Log(message.str());
        // Keep-white (2026-10-25, user workaround for the stale HUD gauge
        // color): mid-quest level writes drive the FSM and the blade aura
        // but never repaint the HUD color, so an LS never rests below white.
        // Any observation of level 0 (hub spawn, quest start, natural white
        // decay) is promoted straight back with the usual near-empty charge
        // bar - the gauge is already white before every quest loads (the
        // FLiNG "set it before the quest" behavior). Only level 0 folds
        // here; the yellow/red edges below are untouched.
        if (current == 0 && weapon &&
            IsReadable(weapon + 0x2370, sizeof(float) * 2) &&
            IsReadable(weapon + 0x9F8, sizeof(std::uint32_t)) &&
            *reinterpret_cast<const volatile std::uint32_t*>(
                weapon + 0x9F8) ==
                g_modWeaponType.load(std::memory_order_relaxed)) {
            SetSpiritLevel(player, weapon, 1, "keep-white", 0.05f);
            g_lastObservedSpiritLevel.store(1, std::memory_order_relaxed);
        }
        // Valor recharge loop (user design 2026-10-24): EVERY entry into
        // white starts the charge bar (the level timer) near empty. The
        // native yellow/red -> white drop refills the timer, which would
        // leave the bar "born full" and unable to cross into a promotion;
        // the chargehit rule's level-0 conversion covers the other entry.
        if (current == 1 && previous >= 2) {
            if (weapon && IsReadable(weapon + 0x2374, sizeof(float)) &&
                IsReadable(weapon + 0x9F8, sizeof(std::uint32_t)) &&
                *reinterpret_cast<const volatile std::uint32_t*>(
                    weapon + 0x9F8) ==
                    g_modWeaponType.load(std::memory_order_relaxed)) {
                auto* timer =
                    reinterpret_cast<volatile float*>(weapon + 0x2374);
                const float prev = *timer;
                *timer = 0.05f;
                std::ostringstream m;
                m << "GAUGERULE valor-recharge: dropped to white, bar "
                  << prev << " -> 0.05";
                Log(m.str());
            }
        }
        // Valor exit (2026-10-25 plan B): RED is valor mode. The native red
        // expiry (and level-consuming moves like helm breaker/iai) lands on
        // yellow with the timer refilled - yellow has no role in this model,
        // so fold the drop straight back into the white charge phase with the
        // usual near-empty bar. The tracker is pinned to 1 so the white-entry
        // edge above does not log a second recharge on the next frame.
        if (current == 2 && previous == 3) {
            if (weapon && IsReadable(weapon + 0x2370, sizeof(float) * 2) &&
                IsReadable(weapon + 0x9F8, sizeof(std::uint32_t)) &&
                *reinterpret_cast<const volatile std::uint32_t*>(
                    weapon + 0x9F8) ==
                    g_modWeaponType.load(std::memory_order_relaxed)) {
                SetSpiritLevel(player, weapon, 1, "valor-exit", 0.05f);
                g_lastObservedSpiritLevel.store(1, std::memory_order_relaxed);
            }
        }
    }

}

#ifndef VALOR_PROBE
// nativePC/plugins/ValorGauge.gauge-lock.txt: "[<target>] <value 0..1>"
//   target 1 = weapon+0x2374 (level-duration timer), 2 = weapon+0x2368 (the
//   visible spirit gauge candidate); bare "<value>" = target 1.
//   F5 toggles the lock at runtime. Both fields are watched either way.
void LoadGaugeLockConfig() {
    std::ifstream input("nativePC/plugins/ValorGauge.gauge-lock.txt");
    float first = 0.0f;
    float second = 0.0f;
    int target = 1;
    float value = 0.0f;
    if (input >> first >> second) {
        target = static_cast<int>(first);
        value = second;
    } else {
        input.clear();
        input.seekg(0);
        if (input >> first) {
            target = 1;
            value = first;
        }
    }
    if (target != 1 && target != 2) {
        target = 1;
    }
    g_gaugeLockTarget.store(target, std::memory_order_relaxed);
    g_gaugeLockValue.store(value, std::memory_order_relaxed);
    g_gaugeLockEnabled.store(value > 0.0f && value <= 1.0f,
                             std::memory_order_relaxed);
    std::ostringstream m;
    m << "Gauge-lock config: target=weapon+0x"
      << (target == 2 ? "2368" : "2374") << " value="
      << g_gaugeLockValue.load(std::memory_order_relaxed) << ", "
      << (g_gaugeLockEnabled.load(std::memory_order_relaxed)
              ? "ON (F5 toggles)"
              : "off");
    Log(m.str());
}

#endif // !VALOR_PROBE
// Gauge-lock experiment: runs on the Present/render thread AFTER the game's
// frame update, so values read here are whatever the game left in the fields
// this frame. Drift from our last write = the game wrote underneath (its own
// drain, a level-change refill, ...). Both fields are always watched; only
// the configured target is written. Reported every 2s.

#ifndef VALOR_PROBE
void PollGaugeLock() {
    const float value = g_gaugeLockValue.load(std::memory_order_relaxed);
    const bool enabled = g_gaugeLockEnabled.load(std::memory_order_relaxed);
    if (value <= 0.0f || value > 1.0f) {
        return;
    }
    void* controller = nullptr;
    {
        std::scoped_lock lock(g_actionMutex);
        controller = g_lockedPlayerController;
    }
    if (!controller) {
        return;
    }
    if (!ModWeaponActive()) {
        g_gaugeLockLastWritten[0] = -1.0f;
        g_gaugeLockLastWritten[1] = -1.0f;
        return;
    }
    auto* player = static_cast<std::uint8_t*>(controller) - 0x61C8;
    if (!IsReadable(player + 0x76B0, sizeof(void*))) {
        return;
    }
    auto* weapon = *reinterpret_cast<std::uint8_t**>(player + 0x76B0);
    if (!weapon || !IsReadable(weapon + 0x2374, sizeof(float))) {
        return;
    }
    constexpr std::uintptr_t kGaugeFields[2] = {0x2374, 0x2368};
    constexpr float kEpsilon = 1.0e-6f;
    for (int i = 0; i < 2; ++i) {
        auto* slot =
            reinterpret_cast<volatile float*>(weapon + kGaugeFields[i]);
        const float current = *slot;
        if (g_gaugeLockLastWritten[i] >= 0.0f &&
            (current > g_gaugeLockLastWritten[i] + kEpsilon ||
             current < g_gaugeLockLastWritten[i] - kEpsilon)) {
            ++g_gaugeWatchFrames[i];
            g_gaugeWatchDrift[i] = current;
        }
        if (enabled && g_gaugeLockTarget.load(std::memory_order_relaxed) ==
                           i + 1) {
            *slot = value;
            g_gaugeLockLastWritten[i] = value;
        } else {
            g_gaugeLockLastWritten[i] = current;
        }
    }

    const auto now = std::chrono::steady_clock::now();
    if (now >= g_gaugeLockNextReport) {
        g_gaugeLockNextReport = now + std::chrono::seconds(2);
        std::ostringstream m;
        m << "GAUGELOCK: lock=0x"
          << (g_gaugeLockTarget.load(std::memory_order_relaxed) == 2 ? "2368"
                                                                     : "2374")
          << "@" << value << (enabled ? " ON" : " OFF")
          << " | 0x2374 raw=" << g_gaugeWatchDrift[0]
          << " drift=" << g_gaugeWatchFrames[0] << "/2s"
          << " | 0x2368 raw=" << g_gaugeWatchDrift[1]
          << " drift=" << g_gaugeWatchFrames[1] << "/2s";
        Log(m.str());
        g_gaugeWatchFrames[0] = 0;
        g_gaugeWatchFrames[1] = 0;
    }
}

#endif // !VALOR_PROBE
