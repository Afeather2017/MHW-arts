// Split out of src/dllmain.cpp (git 6c630fc) by tools/split_dllmain.py.
// Original segments: L65-L68, L115-L117, L386-L404, L406-L488, L585-L600, L1238-L1259, L2029-L2041, L1262-L1342, L1494-L1509, L2924-L2930, L4038-L4055. Bodies are verbatim; see that file's history
// for the per-feature commentary and docs/ for the topic manuals.

#include "prelude.h"
#include "all.h"

std::mutex g_actionMutex;
void* g_lastLongSwordController = nullptr;
void* g_lockedPlayerController = nullptr;
std::optional<ActionInfo> g_lastLongSwordAction;

// Configured weapon type (1..14); Long Sword = 3. Semantics documented before
// ModWeaponActive(); defined here because PollValorLifecycle gates on it.
std::atomic<std::uint32_t> g_modWeaponType{3};

bool IsNormalModeChargingAction(const ActionInfo& action) {
    if (action.actionSet != 3) {
        return false;
    }
    switch (action.actionId) {
    case 7:  // Step Slash from sheath
    case 85: // Step Slash after draw
    case 87: // Overhead Slash after Step Slash
    case 79: // Thrust
    case 80: // Rising Slash
    case 11: // Spirit Blade I from sheath
    case 16: // Spirit Blade I after draw
    case 67: // Spirit Blade II
    case 68: // Spirit Blade III
        return true;
    default:
        return false;
    }
}

// --- Local player identification (multiplayer correctness) --------------------
// The original ValorLS resolved the local player through its build's quest
bool IsReadable(const void* address, std::size_t size);
std::uintptr_t ModuleBaseAddr();
std::uintptr_t OverrideOrZero(const char* name);
void Log(const char* message);

// Player-manager global, per game build - the same global HunterPie's MHW
// maps call WEAPON_ADDRESS: build 421810 (current) = 0x50139A0,
// build 421471 = 0x5073ED0. The old dumped-build VA 0x145073ED0 slid with a
// game update and the chain silently NEVER validated again afterwards
// ("confirmed by manager chain" = 0 lines in the whole field log), so every
// observer stayed on the first-set-3-action lock heuristic without anyone
// noticing - keep-white was the first feature that actually needed the chain.
// Override via ValorGauge.addresses.txt: localPlayerMgr=0xRVA. The winning
// candidate is cached (Present-thread callers only) but re-validated on every
// call through the weapon-type check, so a stale cache degrades to re-probing.
void* ResolveLocalPlayerViaManager() {
    constexpr std::uintptr_t kMgrToPlayer = 0x50;
    constexpr std::uintptr_t kPlayerToWeaponSlot = 0x76B0;
    static std::uintptr_t cachedRva = 0;
    static bool announced = false;

    std::uintptr_t rvas[4] = {};
    int rvaCount = 0;
    if (cachedRva) {
        rvas[rvaCount++] = cachedRva;
    }
    if (const std::uintptr_t overrideRva = OverrideOrZero("localPlayerMgr")) {
        rvas[rvaCount++] = overrideRva;
    }
    rvas[rvaCount++] = 0x50139A0;  // build 421810 (current, HunterPie map)
    rvas[rvaCount++] = 0x5073ED0;  // build 421471 (legacy dump)

    const auto base = ModuleBaseAddr();
    for (int i = 0; i < rvaCount; ++i) {
        const auto* globalSlot =
            reinterpret_cast<void* const*>(base + rvas[i]);
        if (!IsReadable(globalSlot, sizeof(void*))) {
            continue;
        }
        auto* mgr = *globalSlot;
        if (!mgr) {
            continue;
        }
        const auto* playerSlot = reinterpret_cast<void* const*>(
            static_cast<std::uint8_t*>(mgr) + kMgrToPlayer);
        if (!IsReadable(playerSlot, sizeof(void*))) {
            continue;
        }
        auto* player = *playerSlot;
        if (!player) {
            continue;
        }
        const auto* weaponSlot = reinterpret_cast<void* const*>(
            static_cast<std::uint8_t*>(player) + kPlayerToWeaponSlot);
        if (!IsReadable(weaponSlot, sizeof(void*))) {
            continue;
        }
        auto* weapon = *weaponSlot;
        if (!weapon || !IsReadable(static_cast<std::uint8_t*>(weapon) + 0x9F8,
                                   sizeof(std::uint32_t))) {
            continue;
        }
        const auto weaponType =
            *reinterpret_cast<const volatile std::uint32_t*>(
                static_cast<std::uint8_t*>(weapon) + 0x9F8);
        if (weaponType < 1 || weaponType > 14) {
            continue;
        }
        cachedRva = rvas[i];
        if (!announced) {
            announced = true;
            std::ostringstream m;
            m << "MEMORYMAP localPlayerMgr RVA=0x" << std::hex << rvas[i]
              << " player=0x" << reinterpret_cast<std::uintptr_t>(player)
              << " weaponType=" << std::dec << weaponType;
            Log(m.str().c_str());
        }
        return player;
    }
    return nullptr;
}

std::int32_t* ResolveLongSwordLevel(void* controller) {
    if (!controller) {
        return nullptr;
    }
    auto* player = static_cast<std::uint8_t*>(controller) - 0x61C8;
    auto* weaponAddress = player + 0x76B0;
    if (!IsReadable(weaponAddress, sizeof(void*))) {
        return nullptr;
    }
    auto* weapon = *reinterpret_cast<std::uint8_t**>(weaponAddress);
    if (!weapon) {
        return nullptr;
    }
    auto* level = reinterpret_cast<std::int32_t*>(weapon + 0x2370);
    return IsReadable(level, sizeof(*level)) && IsWritable(level, sizeof(*level)) ? level : nullptr;
}

bool IsLongSwordController(void* controller) {
    // Player controllers are inline at player + 0x61C8. A player also owns a human controller
    // pointer at +0x12608. Only validated reads are used here; no player-only game function is
    // called on arbitrary entity callbacks.
    const auto ownerAddress = static_cast<std::uint8_t*>(controller) + 0x100;
    if (!IsReadable(ownerAddress, sizeof(void*))) {
        return false;
    }
    void* owner = *reinterpret_cast<void**>(ownerAddress);
    if (!owner) {
        return false;
    }
    if (static_cast<std::uint8_t*>(owner) + 0x61C8 != controller) {
        return false;
    }
    const auto humanControllerAddress = static_cast<std::uint8_t*>(owner) + 0x12608;
    if (!IsReadable(humanControllerAddress, sizeof(void*))) {
        return false;
    }
    void* humanController = *reinterpret_cast<void**>(humanControllerAddress);
    return humanController && IsReadable(humanController, sizeof(void*));
}

bool ActionIsForesightSlash() {
    std::scoped_lock lock(g_actionMutex);
    auto& a = g_lastLongSwordAction;
    return a && a->actionSet == 3 && a->actionId == 86;
}

// Drawn-state dodge roll (user-identified: action 3:18). Research-only: the
// probe watches its i-frames; the mod never triggers on it.
bool ActionIsRoll() {
    std::scoped_lock lock(g_actionMutex);
    auto& a = g_lastLongSwordAction;
    return a && a->actionSet == 3 && a->actionId == 18;
}

bool ModWeaponActive() {
    void* controller = g_lockedPlayerController;
    if (!controller) {
        return false;
    }
    auto* weaponSlot = static_cast<std::uint8_t*>(controller) - 0x61C8 + 0x76B0;
    if (!IsReadable(weaponSlot, sizeof(void*))) {
        return false;
    }
    auto* weapon = *reinterpret_cast<std::uint8_t**>(weaponSlot);
    if (!weapon || !IsReadable(weapon + 0x9F8, sizeof(std::uint32_t))) {
        return false;
    }
    const auto type = *reinterpret_cast<const volatile std::uint32_t*>(weapon + 0x9F8);
    const bool active = type == g_modWeaponType.load(std::memory_order_relaxed);
    static int gateLogs = 0;
    if (gateLogs < 6) {
        ++gateLogs;
        std::ostringstream m;
        m << "WEAPON GATE " << (active ? "OPEN" : "CLOSED") << ": weapon+0x9F8="
          << type << " expected=" << g_modWeaponType.load(std::memory_order_relaxed);
        Log(m.str());
    }
    return active;
}


#ifndef VALOR_PROBE
// --- Mod weapon gate ---------------------------------------------------------
// The gameplay mod (gauge / clang counter / force) only drives the Long Sword.
// Weapon type = *(int32*)(weapon + 0x9F8) - the same value func_0x141F627F0
// returns and FUN_14118bda0 switches on; **Long Sword = 3** (its case holds
// the LS-only condition names 練気ゲージ判定/居合斬り/見切り回避成功後の攻撃ヒット;
// matches wp03 data folder, w03 FSM files and action set 3). 0xFF = no weapon.
// Override via nativePC/plugins/ValorGauge.weapon-type.txt if a patch
// renumbers the enum.

void LoadModWeaponTypeConfig() {
    std::ifstream input("nativePC/plugins/ValorGauge.weapon-type.txt");
    int type = 0;
    if (input >> type && type >= 0 && type <= 0xFE) {
        g_modWeaponType.store(static_cast<std::uint32_t>(type), std::memory_order_relaxed);
    }
}

// Present-thread weapon resolve for the overlay mode line (workaround 1 for
// the stale HUD gauge color): the locked action controller when one exists,
// else the manager-chain local player (hub / fresh quest, no lock yet) - the
// same order PollValorLifecycle uses. Returns the weapon only when it reads
// the modded weapon type; nullptr = nothing worth displaying.
std::uint8_t* DisplayWeapon() {
    const auto modType = g_modWeaponType.load(std::memory_order_relaxed);
    const auto typeIs = [&](std::uint8_t* weapon) {
        return weapon && IsReadable(weapon + 0x9F8, sizeof(std::uint32_t)) &&
               *reinterpret_cast<const volatile std::uint32_t*>(weapon + 0x9F8) ==
                   modType;
    };
    auto* chainPlayer =
        static_cast<std::uint8_t*>(ResolveLocalPlayerViaManager());
    void* controller = g_lockedPlayerController;
    if (controller && chainPlayer &&
        static_cast<std::uint8_t*>(controller) - 0x61C8 != chainPlayer) {
        controller = nullptr;  // stale lock from a previous scene
    }
    if (controller) {
        auto* player = static_cast<std::uint8_t*>(controller) - 0x61C8;
        if (IsReadable(player + 0x76B0, sizeof(void*))) {
            auto* weapon =
                *reinterpret_cast<std::uint8_t**>(player + 0x76B0);
            if (typeIs(weapon)) {
                return weapon;
            }
        }
    }
    if (chainPlayer && IsReadable(chainPlayer + 0x76B0, sizeof(void*))) {
        auto* weapon =
            *reinterpret_cast<std::uint8_t**>(chainPlayer + 0x76B0);
        if (typeIs(weapon)) {
            return weapon;
        }
    }
    return nullptr;
}

// Local-player test for pre-lock requests: the session lock is authoritative;
// before the first set-3 action establishes it, accept the same manager-chain
// verdict the lock itself uses, so the very first sheathed R2 already rewrites.
bool IsLocalPlayerActionController(void* controller) {
    if (!controller) {
        return false;
    }
    if (controller == g_lockedPlayerController) {
        return true;
    }
    if (g_lockedPlayerController) {
        return false;
    }
    void* chainPlayer = ResolveLocalPlayerViaManager();
    return chainPlayer && chainPlayer == static_cast<std::uint8_t*>(controller) - 0x61C8;
}

bool LockedPlayerActionChargesValor() {
    std::scoped_lock lock(g_actionMutex);
    if (!g_lastLongSwordAction) {
        return false;
    }
    return IsNormalModeChargingAction(*g_lastLongSwordAction);
}

#endif // !VALOR_PROBE

void* ResolveActionInstance(void* ctrl, std::int32_t set, std::int32_t id) {
    if (set < 0 || set >= 4 || id < 0) {
        return nullptr;
    }
    auto* bytes = static_cast<std::uint8_t*>(ctrl);
    void* instArray =
        *reinterpret_cast<void* const*>(bytes + 0x68 + static_cast<std::uintptr_t>(set) * 0x10);
    if (!instArray ||
        !IsReadable(instArray, (static_cast<std::size_t>(id) + 1) * sizeof(void*))) {
        return nullptr;
    }
    void* inst = *reinterpret_cast<void* const*>(
        static_cast<std::uint8_t*>(instArray) + static_cast<std::size_t>(id) * sizeof(void*));
    if (!inst || !IsReadable(inst, 0x20)) {
        return nullptr;
    }
    return inst;
}

