// Split out of src/dllmain.cpp (git 6c630fc) by tools/split_dllmain.py.
// Original segments: L6146-L6151, L6261-L6312, L3848-L3938, L8820-L8913, L8916-L8925. Bodies are verbatim; see that file's history
// for the per-feature commentary and docs/ for the topic manuals.

#include "prelude.h"
#include "all.h"

#ifndef VALOR_PROBE
// Optional nativePC\plugins\ValorGauge.force-action.txt:
// "<set> <id> [invulnSec] [startFrame]" (invulnSec: no-hit window injected at
// the clang; startFrame >= 0: play the forced action from that action-clock
// time instead of 0 - the field counts SECONDS at 30Hz, so TIML frame n
// (60/s) = n/60 seconds. Skipped frames drop their TIML events, e.g. 见切's
// f0-f12 parry window). Shift+F6 reloads this file in-game.

void LoadForceActionConfig() {
    std::ifstream input("nativePC\\plugins\\ValorGauge.force-action.txt");
    std::int32_t set = 0;
    std::int32_t id = 0;
    float invulnSec = 0.0f;
    if (input >> set >> id && set >= 0 && id >= 0) {
        g_forceSet = static_cast<std::uint32_t>(set);
        g_forceId = static_cast<std::uint32_t>(id);
        g_forceCfgSet.store(g_forceSet, std::memory_order_relaxed);
        g_forceCfgId.store(g_forceId, std::memory_order_relaxed);
    }
    if (input >> invulnSec && invulnSec >= 0.0f && invulnSec <= 3.0f) {
        g_forceInvulnSec = invulnSec;
    }
    float startFrame = -1.0f;
    if (input >> startFrame && startFrame >= 0.0f && startFrame <= 1000.0f) {
        g_seekFrame = startFrame;
    }
    std::ifstream autoCfg("nativePC/plugins/ValorGauge.counter-auto.txt");
    int autoForce = 1;
    if (autoCfg >> autoForce) {
        g_counterAutoForce = (autoForce != 0);
    }
    std::ostringstream msg;
    msg << "Force action configured: F6/clang queues set=" << g_forceSet
        << " id=" << g_forceId << ", counter no-hit injection "
        << g_forceInvulnSec << "s, auto-force="
        << (g_counterAutoForce ? "on" : "off (Ctrl inside window)")
        << ", startFrame=" << (g_seekFrame >= 0.0f ? "on" : "off");
    if (g_seekFrame >= 0.0f) {
        msg << " (" << g_seekFrame << "s)";
    }
    msg << ", Shift+F6 reloads";
    Log(msg.str());
    LoadModWeaponTypeConfig();
    LoadHitChargeConfig();
    LoadGaugeLockConfig();
    LoadGaugeLogConfig();
    LoadGaugeRulesConfig();
    LoadSheathRewriteConfig();
    LoadDrawGang1Config();
    LoadArtConfig();
    LoadActionSoundConfig();
    LoadVitalsConfig();
    LoadHpLockConfig();
    LoadStaminaConfig();
    std::ostringstream wt;
    wt << "Mod weapon gate: only weapon type "
       << g_modWeaponType.load(std::memory_order_relaxed)
       << " (Long Sword) activates the gauge/counter (ValorGauge.weapon-type.txt)";
    Log(wt.str());
}

#endif // !VALOR_PROBE
bool InstallPoolQueryHook() {
    const HMODULE game = GetModuleHandleW(nullptr);
    MODULEINFO module{};
    if (!game || !GetModuleInformation(GetCurrentProcess(), game, &module, sizeof(module))) {
        Log("Hook install failed: module info unavailable");
        return false;
    }
    g_moduleBase = reinterpret_cast<std::uintptr_t>(module.lpBaseOfDll);
    InitMemoryMap();
#ifdef VALOR_PROBE
    void* target = reinterpret_cast<void*>(g_hookAddr[HkPoolQuery]);
    if (MH_CreateHook(target, reinterpret_cast<void*>(&Aee050Hook),
                      reinterpret_cast<void**>(&g_originalAee050)) != MH_OK) {
        Log("Pool query hook failed: MH_CreateHook");
        return false;
    }
    if (MH_EnableHook(target) != MH_OK) {
        Log("Pool query hook failed: MH_EnableHook");
        return false;
    }
    if (MH_CreateHook(reinterpret_cast<void*>(g_hookAddr[HkDmgCalc]),
                      reinterpret_cast<void*>(&DmgCalcHook),
                      reinterpret_cast<void**>(&g_originalDmgCalc)) != MH_OK ||
        MH_EnableHook(reinterpret_cast<void*>(g_hookAddr[HkDmgCalc])) != MH_OK) {
        Log("Damage calc hook failed");
    } else {
        Log("Damage calc hook installed");
    }
    if (MH_CreateHook(reinterpret_cast<void*>(g_hookAddr[HkHitDispatch]),
                      reinterpret_cast<void*>(&HitDispatchHook),
                      reinterpret_cast<void**>(&g_originalHitDispatch)) != MH_OK ||
        MH_EnableHook(reinterpret_cast<void*>(g_hookAddr[HkHitDispatch])) != MH_OK) {
        Log("Hit dispatcher hook failed");
    } else {
        Log("Hit dispatcher hook installed");
    }
    std::ostringstream m;
    m << "Pool query hook installed at 0x" << std::hex << std::uppercase
      << reinterpret_cast<std::uintptr_t>(target);
    Log(m.str());
#else
    InstallCondEvalHook();
    if (MH_CreateHook(reinterpret_cast<void*>(g_hookAddr[HkDmgCalc]),
                      reinterpret_cast<void*>(&DmgCalcHook),
                      reinterpret_cast<void**>(&g_originalDmgCalc)) != MH_OK ||
        MH_EnableHook(reinterpret_cast<void*>(g_hookAddr[HkDmgCalc])) != MH_OK) {
        Log("Damage calc hook failed (hit charging inactive)");
    } else {
        Log("Damage calc hook installed (hit-based valor charging)");
    }
    if (MH_CreateHook(reinterpret_cast<void*>(g_hookAddr[HkDealDamage]),
                      reinterpret_cast<void*>(&DealDamageHook),
                      reinterpret_cast<void**>(&g_originalDealDamage)) != MH_OK ||
        MH_EnableHook(reinterpret_cast<void*>(g_hookAddr[HkDealDamage])) != MH_OK) {
        Log("Deal damage hook failed (art gauge charging inactive)");
    } else {
        Log("Deal damage hook installed (art gauge on applied damage)");
    }
    if (MH_CreateHook(reinterpret_cast<void*>(g_hookAddr[HkDmgFormula]),
                      reinterpret_cast<void*>(&DmgFormulaHook),
                      reinterpret_cast<void**>(&g_originalDmgFormula)) != MH_OK ||
        MH_EnableHook(reinterpret_cast<void*>(g_hookAddr[HkDmgFormula])) != MH_OK) {
        Log("Damage formula hook failed (atkMV latch inactive)");
    } else {
        Log("Damage formula hook installed (ctx latch)");
    }
    if (MH_CreateHook(reinterpret_cast<void*>(g_hookAddr[HkGaugeAdd]),
                      reinterpret_cast<void*>(&GaugeAddHook),
                      reinterpret_cast<void**>(&g_originalGaugeAdd)) != MH_OK ||
        MH_EnableHook(reinterpret_cast<void*>(g_hookAddr[HkGaugeAdd])) != MH_OK) {
        Log("Gauge add hook failed");
    } else {
        Log("Gauge add hook installed (GAUGEEVT logging)");
    }
    if (MH_CreateHook(reinterpret_cast<void*>(g_hookAddr[HkFxNotify]),
                      reinterpret_cast<void*>(&FxNotifyHook),
                      reinterpret_cast<void**>(&g_originalFxNotify)) != MH_OK ||
        MH_EnableHook(reinterpret_cast<void*>(g_hookAddr[HkFxNotify])) != MH_OK) {
        Log("FX notify hook failed");
    } else {
        Log("FX notify hook installed (FXNOTIFY logging)");
    }
    InstallSheathClsfyProbeHook();
    InstallSheathEvalProbe();
    InstallPropBoolHookMod();
#endif
#ifndef VALOR_PROBE
    ApplySheatheMotionPatch();
#endif
    ValidateMemoryMap();
    return true;
}

DWORD WINAPI InitializePlugin(void*) {
    // Never co-install: both builds hook the same game functions from their
    // own MinHook instances - the second hook on already-patched bytes would
    // corrupt the trampoline. Stand down if the sibling DLL is present.
    if (GetModuleHandleW(
#ifdef VALOR_PROBE
            L"ValorGauge.dll"
#else
            L"ValorProbe.dll"
#endif
        ) != nullptr) {
        Log("Sibling Valor DLL detected - standing down (only one build may be installed)");
        return 0;
    }
#ifdef VALOR_PROBE
    Log("ValorProbe loading");
#else
    Log("ValorGauge (mod) loading");
#endif
    LoadAddressOverrides();
    PatchRollSheatheRows();
    LoadStateAddress();

    for (int attempt = 0; attempt < 120; ++attempt) {
        const auto status = kiero::init(kiero::RenderType::D3D11);
        if (status == kiero::Status::Success) {
            const auto bindStatus = kiero::bind(8, reinterpret_cast<void**>(&g_originalPresent),
                                                reinterpret_cast<void*>(PresentHook));
            if (bindStatus == kiero::Status::Success) {
                Log("DX11 Present hook installed");
                InstallActionHook();
                InstallPoolQueryHook();
#ifdef VALOR_PROBE
                InstallPropHistogramHooks();
                LoadPropWriteConfig();
                LoadPropWatchConfig();
                LoadChipGuardConfig();
                InstallChipGuardHook();
#else
                InstallForceHook();
                LoadForceActionConfig();
                {
                    const std::uintptr_t base = ModuleBaseAddr();
                    const std::uintptr_t rvaA =
                        OverrideOrZero("fxNotifyA") != 0
                            ? OverrideOrZero("fxNotifyA")
                            : 0xADD670;
                    const std::uintptr_t rvaB =
                        OverrideOrZero("fxNotifyB") != 0
                            ? OverrideOrZero("fxNotifyB")
                            : 0xAE0D70;
                    const std::uintptr_t rvaGlow =
                        OverrideOrZero("glowEmit") != 0
                            ? OverrideOrZero("glowEmit")
                            : 0x21224D0;
                    const std::uintptr_t rvaAura =
                        OverrideOrZero("auraSet") != 0
                            ? OverrideOrZero("auraSet")
                            : 0x2110A90;
                    g_fxNotifyA = reinterpret_cast<void* (__fastcall*)(
                        void*, std::uint32_t, std::uint32_t)>(base + rvaA);
                    g_fxNotifyB = reinterpret_cast<void* (__fastcall*)(
                        void*, std::uint32_t, std::uint32_t)>(base + rvaB);
                    g_glowEmit = reinterpret_cast<void (__fastcall*)(void*)>(
                        base + rvaGlow);
                    g_auraSet = reinterpret_cast<void* (__fastcall*)(
                        void*, std::uint32_t, std::uint32_t, std::uint32_t,
                        std::uint32_t)>(base + rvaAura);
                    g_timlGainCallerRva =
                        OverrideOrZero("gaugeTimlGain") != 0
                            ? OverrideOrZero("gaugeTimlGain")
                            : 0x2124F13;
                    std::ostringstream fxm;
                    fxm << "FX test notify resolved: ADD670="
                        << (g_fxNotifyA ? "ok" : "null") << " AE0D70="
                        << (g_fxNotifyB ? "ok" : "null") << " glowEmit="
                        << (g_glowEmit ? "ok" : "null") << " auraSet="
                        << (g_auraSet ? "ok" : "null") << " timlGainRva=0x"
                        << std::hex << g_timlGainCallerRva;
                    Log(fxm.str());
                }
                LoadFxTestConfig();
                InstallSheathCapHook();
                LoadRollSheatheConfig();
                LoadChipGuardConfig();
                InstallChipGuardHook();
#endif
                return 0;
            }
            Log("DX11 Present bind failed");
            return 1;
        }
        Sleep(1000);
    }

    Log("DX11 initialization timed out");
    return 1;
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        const HANDLE thread = CreateThread(nullptr, 0, InitializePlugin, nullptr, 0, nullptr);
        if (thread) {
            CloseHandle(thread);
        }
    }
    return TRUE;
}
