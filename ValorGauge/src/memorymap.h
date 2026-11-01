#pragma once
#include <atomic>
#include <cstdint>
#include <map>
#include <string>

// --- Update-resilient address map --------------------------------------------
// Hook targets resolve by byte signature first, then by the recorded RVA of
// the last known build, then by a manual override file
// nativePC/plugins/ValorGauge.addresses.txt ("name=0xRVA" lines). Signatures
// are generated and verified offline against the unpacked dump exe:
//   python Reverse/sig_check.py --emit && python Reverse/sig_check.py --verify
enum HookId {
    HkCondEval, HkPoolQuery, HkDmgCalc, HkHitDispatch,
    HkPropInt, HkPropBool, HkPropFloat,
    HkGaugeAdd, HkFxNotify,
    HkDealDamage, HkDmgFormula, HkAtkCtxBuilder,
    HkSheathClsfy, HkSheathCap, HkCount
};

extern std::uintptr_t g_hookAddr[HkCount];
extern std::uintptr_t g_addrAudioSingleton;  // audio manager singleton slot
extern std::uintptr_t g_addrPropRegistry;    // FSM property registry (count +0xB8, rows +0xC8)
extern std::uintptr_t g_addrPropLookup;      // registry row lookup FUN_140462700
extern std::uintptr_t g_addrPropName;        // property id -> name
extern std::uintptr_t g_addrDoAction;        // set by InstallActionHook
// Quest-state singleton slot: *(base+RVA) -> int32 id at +0x4C, state at +0x54
// (2 = InQuest). Zone singleton slot: *(base+RVA) -> int32 stage id at +0xAED0.
extern std::uintptr_t g_addrQuestStateMgr;
extern std::uintptr_t g_addrZoneMgr;

extern std::map<std::string, std::uintptr_t> g_addrOverrides;

// PROPTRACE switch (addresses.txt `propTrace=1`), consumed by PropTraceRecord.
extern std::atomic<bool> g_propTrace;
extern std::atomic<int> g_propTraceLines;

void LoadAddressOverrides();
std::uintptr_t OverrideOrZero(const char* name);
std::uintptr_t ResolveHookTarget(HookId id);
void InitMemoryMap();
void ValidateMemoryMap();
