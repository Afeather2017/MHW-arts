// Split out of src/dllmain.cpp (git 6c630fc) by tools/split_dllmain.py.
// Original segments: L635-L705. Bodies are verbatim; see that file's history
// for the per-feature commentary and docs/ for the topic manuals.

#include "prelude.h"
#include "all.h"

// Quest singleton read: *(base+g_addrQuestStateMgr) -> +0x4C id / +0x54 state.
// Returns false on any unreadable link (title screen, post-update RVA drift);
// callers must treat that as "no quest info".
bool ReadQuestInfo(std::int32_t* questId, std::int32_t* state) {
    if (!g_addrQuestStateMgr ||
        !IsReadable(reinterpret_cast<void*>(g_addrQuestStateMgr), sizeof(void*))) {
        return false;
    }
    const void* inst =
        *reinterpret_cast<void* const*>(g_addrQuestStateMgr);
    if (!inst || !IsReadable(static_cast<const std::uint8_t*>(inst) + 0x4C,
                             sizeof(std::int32_t) * 3)) {
        return false;
    }
    const auto* base =
        static_cast<const volatile std::int32_t*>(inst);
    *questId = base[0x4C / sizeof(std::int32_t)];
    *state = base[0x54 / sizeof(std::int32_t)];
    return true;
}

// True only while the local session is inside a quest (state 2). Explorations
// and the training area may ALSO read state 2 (empty quest id) - state alone
// never distinguishes them from a real quest; the id does.
bool InQuestState() {
    std::int32_t questId = 0;
    std::int32_t state = 0;
    return ReadQuestInfo(&questId, &state) && state == 2;
}

// Stage ids you can draw a weapon and fight in (HunterPie Stage enum):
// 101..109 hunting fields incl. Guiding Lands (quest maps AND 自由探索 share
// the same ids - the stage alone cannot tell them apart either), 201..203
// arenas, 401..417 story specials, 504 the training area - which is NOT a
// quest at all, yet everything there works (user 2026-10-27: ART must show
// there). Explicitly excluded: main menu 0, peace zones 301..306 and player
// rooms 501..503/505/506.
bool PlayableStageId(const std::int32_t stage) {
    if (stage >= 101 && stage <= 109) return true;
    if (stage >= 201 && stage <= 203) return true;
    if (stage >= 401 && stage <= 417) return true;
    return stage == 504;
}

// Zone singleton read: *(base+g_addrZoneMgr) -> +0xAED0 stage id.
// Returns -1 when any link is unreadable (callers must not edge-detect on -1).
std::int32_t ReadStageId() {
    if (!g_addrZoneMgr ||
        !IsReadable(reinterpret_cast<void*>(g_addrZoneMgr), sizeof(void*))) {
        return -1;
    }
    const void* inst = *reinterpret_cast<void* const*>(g_addrZoneMgr);
    if (!inst || !IsReadable(static_cast<const std::uint8_t*>(inst) + 0xAED0,
                             sizeof(std::int32_t))) {
        return -1;
    }
    return *reinterpret_cast<const volatile std::int32_t*>(
        static_cast<const std::uint8_t*>(inst) + 0xAED0);
}

// The ART gate's real predicate: quests cover the common case, and the zone
// whitelist catches the combat areas whose quest-state bit is not InQuest
// (training area, possibly free explorations). Either path showing true keeps
// the gauge visible/charged; both false (hub, rooms, main menu, garbage reads)
// hides it and wipes.
bool InCombatZone() {
    if (InQuestState()) {
        return true;
    }
    return PlayableStageId(ReadStageId());
}
