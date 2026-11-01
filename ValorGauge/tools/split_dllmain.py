#!/usr/bin/env python3
"""One-shot splitter: src/dllmain.cpp (8925 lines, single TU) -> per-module .cpp files.

Rules:
- Function/global bodies are moved VERBATIM by line range; no logic edits.
- Each segment records the guard it lived under in the original:
    ''  = depth 0 (both builds); segment must be guard-balanced as a whole
    'm' = inside #ifndef VALOR_PROBE (mod only)
    'p' = inside #ifdef VALOR_PROBE (probe only)
- emit != orig only where the segment provably references no build-specific
  symbols (spirit, util Format*).
- Post edits (linkage only, no behavior change): strip the default argument
  from the SetSpiritLevel definition (it moves to spirit.h); un-static
  functions/globals that are now referenced across translation units.
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
# input = the ORIGINAL single-TU dllmain.cpp (git 6c630fc). Restore it with:
#   git show 6c630fc:ValorGauge/src/dllmain.cpp > ValorGauge/src/_dllmain_orig.cpp
ORIG = ROOT / "src" / "_dllmain_orig.cpp"
if not ORIG.exists():
    sys.exit("input missing: restore it with\n"
             "  git show 6c630fc:ValorGauge/src/dllmain.cpp"
             " > ValorGauge/src/_dllmain_orig.cpp")
SRC = ORIG
OUT = ROOT / "src"

LINES = SRC.read_text(encoding="utf-8").splitlines()

def guard_stack():
    # entries are (directive, flipped); effective kind after #else flips:
    # "#ifdef X" / not flipped -> probe; "#ifdef X" / flipped -> mod; etc.
    stack = []
    depths = []
    for raw in LINES:
        s = raw.strip()
        if s.startswith("#ifndef") or s.startswith("#ifdef"):
            stack.append([s, False])
        elif s.startswith("#else"):
            if stack:
                stack[-1][1] = not stack[-1][1]
        elif s.startswith("#endif"):
            if stack:
                stack.pop()
        depths.append(tuple(
            "m" if (d.startswith("#ifndef") != flipped) else "p"
            for (d, flipped) in stack))
    return depths

DEPTHS = guard_stack()

# (file, start, end, orig_guard, emit_guard, expect-substring-of-first-line)
S = [
    # log
    ("log", 64, 64, "", "", "std::mutex g_logMutex"),
    ("log", 490, 510, "", "", "void Log(const char* message)"),
    # util
    ("util", 380, 380, "", "", "g_moduleBase{0}"),
    ("util", 512, 528, "", "", "std::optional<std::uintptr_t> ParseAddress"),
    ("util", 559, 583, "", "", "bool IsReadable(const void* address"),
    ("util", 868, 894, "", "", "std::uint8_t* FindPattern"),
    ("util", 1040, 1047, "", "", "std::uintptr_t ModuleBaseAddr()"),
    ("util", 2043, 2061, "", "", "bool ParseIdList"),
    # memorymap (HookId enum lives in memorymap.h; HookSig/kHookSigs stay here)
    ("memorymap", 623, 633, "", "", "Quest-state singleton slot"),
    ("memorymap", 896, 902, "", "", "Update-resilient address map"),
    ("memorymap", 911, 1036, "", "", "struct HookSig"),
    ("memorymap", 1038, 1038, "", "", "g_addrOverrides"),
    ("memorymap", 1049, 1092, "", "", "void LoadAddressOverrides()"),
    ("memorymap", 1094, 1147, "", "", "Resolve one hook target"),
    ("memorymap", 1149, 1192, "", "", "PROPTRACE"),
    ("memorymap", 1194, 1235, "", "", "One-time sanity report"),
    # player
    ("player", 65, 68, "", "", "g_actionMutex"),
    ("player", 115, 117, "", "", "Configured weapon type"),
    ("player", 386, 404, "", "", "bool IsNormalModeChargingAction"),
    ("player", 406, 488, "", "", "Local player identification"),
    ("player", 585, 600, "", "", "std::int32_t* ResolveLongSwordLevel"),
    ("player", 1238, 1259, "", "", "bool IsLongSwordController"),
    ("player", 2029, 2041, "", "", "bool ActionIsForesightSlash()"),
    ("player", 1262, 1342, "m", "m", "Mod weapon gate"),
    ("player", 1494, 1509, "m", "m", "Local-player test for pre-lock"),
    ("player", 2924, 2930, "m", "m", "bool LockedPlayerActionChargesValor"),
    ("player", 4038, 4055, "m", "m", "void* ResolveActionInstance"),
    # env (quest/stage/combat zone)
    ("env", 635, 705, "", "", "Quest singleton read"),
    # art (ArtChargeRow lives in art.h)
    ("art", 76, 91, "", "", "art gauge (2026-10-26"),
    ("art", 97, 103, "", "", "std::atomic<float> g_artGauge"),
    ("art", 105, 110, "", "", "Damage-based art charging"),
    ("art", 707, 753, "", "", "DrawGauge(Present)-thread only"),
    ("art", 1721, 1766, "", "", "helpers"),
    ("art", 6152, 6258, "m", "m", "config ---"),
    # valor
    ("valor", 70, 75, "", "", "g_valorCharge"),
    ("valor", 118, 136, "", "", "Gauge-lock experiment (2026-09-24)"),
    ("valor", 602, 621, "", "", "void ActivateValorMode"),
    ("valor", 755, 866, "", "", "void PollValorLifecycle()"),
    ("valor", 2161, 2197, "m", "m", "gauge-lock.txt"),
    ("valor", 8304, 8308, "", "", "Gauge-lock experiment: runs on"),
    ("valor", 8310, 8374, "m", "m", "void PollGaugeLock()"),
    # spirit (emitted unguarded: leaf utils, no build-specific references)
    ("spirit", 104, 104, "", "", "g_lastObservedSpiritLevel"),
    ("spirit", 2636, 2697, "m", "", "timer / timerhit rules"),
    ("spirit", 2699, 2753, "m", "", "void ExtendLevelDropTimer"),
    # gauge (GaugeRule lives in gauge.h)
    ("gauge", 137, 144, "", "", "Gauge-event logger"),
    ("gauge", 180, 244, "", "", "Per-action gauge rules"),
    ("gauge", 266, 296, "", "", "std::vector<GaugeRule> g_gaugeRules;"),
    ("gauge", 2199, 2211, "m", "m", "void LoadGaugeLogConfig()"),
    ("gauge", 2213, 2390, "m", "m", "FUN_142122410 choke point"),
    ("gauge", 2392, 2596, "m", "m", "void LoadGaugeRulesConfig()"),
    ("gauge", 2598, 2625, "m", "m", "Shared by the counter window"),
    ("gauge", 2627, 2634, "m", "m", "timer rules accept id 0"),
    ("gauge", 2755, 2851, "m", "m", "Shared preamble for both rule paths"),
    ("gauge", 4534, 4629, "m", "m", "Begin-time gauge rules"),
    ("gauge", 4748, 4797, "m", "m", "GAUGEWATCH (2026-10-04)"),
    ("gauge", 4799, 4839, "m", "m", "void PollGaugeBeginRules"),
    ("gauge", 4841, 4879, "m", "m", "TRAIL rule per-tick"),
    # sheath
    ("sheath", 1344, 1428, "m", "m", "Sheathed-state draw rewrite"),
    ("sheath", 1432, 1492, "m", "m", "void LoadSheathRewriteConfig()"),
    ("sheath", 1511, 1537, "m", "m", "DRAWGANG1 config"),
    ("sheath", 1539, 1673, "m", "m", "void ApplySheathRewrite"),
    ("sheath", 1680, 1719, "", "", "Arms the 刚1 gauge scope"),
    ("sheath", 4631, 4746, "m", "m", "刚1 gauge accounting state machine"),
    ("sheath", 5070, 5166, "m", "m", "DRAWGANG1 per-frame consumer"),
    ("sheath", 8376, 8459, "m", "m", "SHEATHWATCH (2026-10-18)"),
    # actions
    ("actions", 1768, 2018, "", "", "bool __fastcall DoActionHook"),
    ("actions", 7406, 7441, "", "", "bool InstallActionHook()"),
    # damage (fn-ptr aliases + original fn-ptr globals live in damage.h)
    ("damage", 2103, 2119, "", "", "刚1 clang detector"),
    ("damage", 2121, 2126, "", "", "DmgCalcFn g_originalDmgCalc"),
    ("damage", 2134, 2159, "m", "m", "Hit-based valor charging"),
    ("damage", 2932, 3042, "m", "m", "Chargehit resolution"),
    ("damage", 3046, 3047, "m", "m", "FUN_141cc5f80 = HunterPie"),
    ("damage", 3050, 3061, "m", "m", "DealDamageFn g_originalDealDamage"),
    ("damage", 3062, 3066, "m", "m", "MOTIONTRACE"),
    ("damage", 3068, 3142, "m", "m", "HITFRAME"),
    ("damage", 3143, 3148, "m", "m", "long long __fastcall DmgCalcHook"),
    ("damage", 3150, 3154, "m", "m", "Attack-field hunt round 3"),
    ("damage", 3157, 3177, "m", "m", "DmgFormulaFn g_originalDmgFormula"),
    ("damage", 3179, 3448, "m", "m", "Every gauge charge lives here"),
    # trigger
    ("trigger", 298, 301, "", "", "Force-action: F6"),
    ("trigger", 303, 309, "", "", "std::uint32_t g_forceSet"),
    ("trigger", 367, 367, "", "", "g_counterWindowSec"),
    ("trigger", 369, 379, "", "", "The mined clang latch"),
    ("trigger", 381, 381, "", "", "g_clangLatch"),
    ("trigger", 2028, 2028, "", "", "g_vseekLogCount"),
    ("trigger", 3517, 3602, "m", "m", "見切り回避成功後の攻撃ヒット condition"),
    ("trigger", 4299, 4365, "m", "m", "Game-thread consumer of an armed force"),
    ("trigger", 6314, 6344, "m", "m", "void OpenCounterWindow"),
    ("trigger", 6531, 6711, "m", "m", "Mod per-frame trigger poll"),
    # fx
    ("fx", 145, 179, "", "", "Player-emitter notify logger"),
    ("fx", 2853, 2922, "m", "m", "FUN_140ADD670 choke point"),
    ("fx", 4367, 4435, "m", "m", "void LoadFxTestConfig()"),
    # motion (MotionBind/ClkRegion live in motion.h)
    ("motion", 310, 325, "", "", "Frame seek: jump the action-instance"),
    ("motion", 326, 329, "", "", "Clock miner v2"),
    ("motion", 339, 342, "", "", "ClkRegion g_clkRegions[7];"),
    ("motion", 344, 366, "", "", "void ClkResetRegions()"),
    ("motion", 3605, 3611, "", "", "MOTIONCOND (mod)"),
    ("motion", 3620, 3621, "", "", "std::vector<MotionBind> g_motionBinds;"),
    ("motion", 3623, 3740, "m", "m", "SHEATHEMOTION"),
    ("motion", 4028, 4036, "m", "m", "Frame seek / frame watch"),
    ("motion", 4057, 4075, "m", "m", "void ArmFrameSeekDiagnostics()"),
    ("motion", 4077, 4297, "m", "m", "Runs on the property-getter hook"),
    ("motion", 4438, 4531, "m", "m", "MOTIONBIND per-tick"),
    # prop (getter aliases + original fn-ptr globals live in prop.h)
    ("prop", 3940, 3947, "", "", "F11 FSM property-access histogram"),
    ("prop", 3957, 3995, "", "", "bool g_f11WasDown"),
    ("prop", 3998, 4024, "p", "p", "void RecordPropCall"),
    ("prop", 5172, 5236, "m", "m", "int __fastcall PropIntHook"),
    ("prop", 5239, 5267, "", "", "PROPTRACE: 属性求值轨迹"),
    ("prop", 5447, 5648, "p", "p", "Probe variant of the int getter"),
    # rollsheathe
    ("rollsheathe", 3744, 3845, "", "", "SHEATHCLSFY 探针"),
    ("rollsheathe", 5270, 5443, "m", "m", "翻滚切勇気納刀 (ROLLSHEATHE poll"),
    ("rollsheathe", 8745, 8818, "", "", "LS set-3 请求分类器表"),
    # sound (ActionSoundRule lives in sound.h)
    ("sound", 4881, 4900, "m", "m", "ACTIONSND (2026-10-22)"),
    ("sound", 4911, 5068, "m", "m", ">> g_actionSounds;"),
    # vitals
    ("vitals", 5652, 5832, "m", "m", "Vitals slots"),
    ("vitals", 5834, 6144, "m", "m", "Stamina control"),
    # overlay
    ("overlay", 46, 46, "", "", "g_originalPresent"),
    ("overlay", 52, 59, "", "", "ID3D11Device* g_device"),
    ("overlay", 8280, 8302, "", "", "bool InitializeRenderer"),
    ("overlay", 8462, 8743, "", "", "void DrawGauge"),
    # probe (watch) — Aee050Fn/HitDispatchFn aliases live in probe.h
    ("probe", 49, 51, "", "", "Aee050Fn g_originalAee050"),
    ("probe", 60, 63, "", "", "g_stateLoggerEnabled"),
    ("probe", 69, 69, "", "", "g_actionDiagnosticCount"),
    ("probe", 368, 368, "", "", "g_clangProbeLogCount"),
    ("probe", 530, 557, "", "", "void LoadStateAddress()"),
    ("probe", 2020, 2027, "", "", "damage-layer defense query"),
    ("probe", 2064, 2100, "p", "p", "float __fastcall Aee050Hook"),
    ("probe", 2128, 2132, "p", "p", "long long __fastcall DmgCalcHook"),
    ("probe", 3453, 3513, "p", "p", "HitDispatchFn g_originalHitDispatch"),
    ("probe", 6348, 6527, "p", "p", "RXN watcher: reaction"),
    ("probe", 6715, 6814, "p", "p", "POOLDIFF: per-frame"),
    ("probe", 6818, 6995, "p", "p", "void PollReactionWatch()"),
    ("probe", 7444, 7469, "p", "p", "void PollStateLogger()"),
    # probe_tools
    ("probe_tools", 6999, 7403, "p", "p", "prop-watch.txt: property"),
    ("probe_tools", 7471, 8277, "p", "p", "F12 ATK-row scan"),
    # dllmain (orchestration)
    ("dllmain", 6146, 6151, "m", "m", "force-action.txt"),
    ("dllmain", 6261, 6312, "m", "m", "void LoadForceActionConfig()"),
    ("dllmain", 3848, 3938, "", "", "bool InstallPoolQueryHook()"),
    ("dllmain", 8820, 8913, "", "", "DWORD WINAPI InitializePlugin"),
    ("dllmain", 8916, 8925, "", "", "BOOL APIENTRY DllMain"),
]

EDITS = {
    "probe": [
        # kMaxWatchIds moved to probe.h (probe_tools.cpp uses it too)
        ("constexpr int kMaxWatchIds = 6;\n", ""),
    ],
    "spirit": [
        ("float timerValue = 1.0f) {", "float timerValue) {"),
    ],
    "damage": [
        ("static void MotionTraceTick(const void* entity) {",
         "void MotionTraceTick(const void* entity) {"),
    ],
    "probe_tools": [
        ("static void AtkScanArm() {", "void AtkScanArm() {"),
        ("static void PollAtkTrace() {", "void PollAtkTrace() {"),
        ("static bool g_atkKeyWasDown = false;", "bool g_atkKeyWasDown = false;"),
    ],
    "motion": [
        ("static void ApplySheatheMotionPatch() {", "void ApplySheatheMotionPatch() {"),
    ],
}

GUARD_OPEN = {"m": "#ifndef VALOR_PROBE", "p": "#ifdef VALOR_PROBE"}
GUARD_CLOSE = {"m": "#endif // !VALOR_PROBE", "p": "#endif // VALOR_PROBE"}
WANT = {"": (), "m": ("m",), "p": ("p",)}

def first_nonblank(a, b):
    for i in range(a - 1, b):
        if LINES[i].strip():
            return LINES[i]
    return "<blank>"

errors = []
for (name, a, b, orig, emit, expect) in S:
    fb = first_nonblank(a, b)
    if expect and expect not in fb:
        errors.append(f"{name} [{a},{b}]: first line mismatch: {fb!r}")
    before = DEPTHS[a - 2] if a >= 2 else ()
    after = DEPTHS[b - 1]
    want = WANT[orig]
    if want:
        if not before or not all(k == want[0] and k in ("m", "p") for k in before):
            errors.append(f"{name} [{a},{b}]: guard before segment is {before}, want {want}")
        if not after or not all(k == want[0] and k in ("m", "p") for k in after):
            errors.append(f"{name} [{a},{b}]: guard after segment is {after}, want {want}")
    else:
        if before or after:
            errors.append(f"{name} [{a},{b}]: expected depth 0, got before={before} after={after}")

# coverage: every content line accounted for exactly once
seen = {}
for (name, a, b, orig, emit, expect) in S:
    for i in range(a, b + 1):
        if i in seen:
            errors.append(f"line {i} claimed twice: {seen[i]} and {name}")
        seen[i] = name
unclaimed = [i for i in range(38, 8915) if i not in seen]
if unclaimed:
    # informational: fwd decls / aliases / blanks intentionally dropped
    print(f"note: {len(unclaimed)} unclaimed lines in [38,8914]")
    for i in unclaimed:
        t = LINES[i - 1].strip()
        if t and not t.startswith(("//", "*")):
            print(f"  UNCLAIMED L{i}: {t[:100]}")

if errors:
    print("SEGMENT ERRORS:")
    for e in errors:
        print("  " + e)
    sys.exit(1)

per_file = {}
order = []
for (name, a, b, orig, emit, expect) in S:
    if name not in per_file:
        per_file[name] = []
        order.append(name)
    per_file[name].append((a, b, emit))

git_hash = "6c630fc"
for name in order:
    segs = per_file[name]
    parts = []
    ranges = ", ".join(f"L{a}-L{b}" for (a, b, _) in segs)
    parts.append(f"// Split out of src/dllmain.cpp (git {git_hash}) by tools/split_dllmain.py.")
    parts.append(f"// Original segments: {ranges}. Bodies are verbatim; see that file's history")
    parts.append("// for the per-feature commentary and docs/ for the topic manuals.")
    parts.append("")
    parts.append('#include "prelude.h"')
    parts.append('#include "all.h"')
    parts.append("")
    if name == "actions":
        parts.append("DoActionFn g_originalDoAction = nullptr;")
        parts.append("std::atomic_bool g_insideDoAction{false};")
        parts.append("")
    if name == "prop":
        # original L3954-3956 (cut with the alias block, aliases live in prop.h)
        parts.append("PropGetIntFn g_originalPropInt = nullptr;")
        parts.append("PropGetBoolFn g_originalPropBool = nullptr;")
        parts.append("PropGetFloatFn g_originalPropFloat = nullptr;")
        parts.append("")
    cur = None
    for (a, b, emit) in segs:
        if emit != cur:
            if cur:
                parts.append(GUARD_CLOSE[cur])
            if emit:
                parts.append(GUARD_OPEN[emit])
            cur = emit
        parts.extend(LINES[a - 1:b])
        parts.append("")
    if cur:
        parts.append(GUARD_CLOSE[cur])
    text = "\n".join(parts).rstrip() + "\n"
    for old, new in EDITS.get(name, []):
        if old not in text:
            print(f"EDIT MISS in {name}: {old!r}")
            sys.exit(1)
        text = text.replace(old, new)
    (OUT / f"{name}.cpp").write_text(text, encoding="utf-8", newline="\n")
    opens = len(re.findall(r"^\s*#(if|ifdef|ifndef)", text, re.M))
    closes = len(re.findall(r"^\s*#endif", text, re.M))
    lines_out = text.count("\n")
    print(f"{name}.cpp: {lines_out} lines, #if={opens} #endif={closes} "
          f"{'OK' if opens == closes else '*** UNBALANCED ***'}")

print("done")
