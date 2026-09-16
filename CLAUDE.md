# MHW-arts: Hunter Arts Mod for Monster Hunter World

## Project Overview

**Goal:** Introduce Monster Hunter XX (MHXX) Hunter Arts system into Monster Hunter World (MHW) as a mod.

**Approach:** Start with a minimal milestone: introduce a simple damaging motion on Long Sword, then scale to full Hunter Arts system with meter, multiple arts, and eventual MHXX motion import.

**Current Phase:** Native Valor mode prototype — normal Long Sword actions charge a live blue gauge, full charge forces red spirit level and successfully enables the Valor FSM branches.

**Valor attack model:** Normal mode uses Step Slash, Thrust, and Spirit Slash I/II/III. Valor mode changes the Spirit input chain to Valor Spirit I, Valor Spirit II, Lai Slash, and Spirit Iaido Slash. Valor Spirit I is entered through the Foresight Slash input and carries the FSM-provided guard-point behavior; this is separate from the numeric Long Sword aura/spirit level.

---

## Game Architecture & Data Location

### MHW File Structure
- **Game Root:** `E:\SteamLibrary\steamapps\common\Monster Hunter World\`
- **Chunk Archives:** `chunk\chunkG*.bin` (13 files: chunkG0.bin through chunkG11.bin + chunkG60.bin)
- **Override Folder:** `nativePC\` — where mod files are placed to override packed data

### Chunk Layering System
- Higher-numbered chunks override lower-numbered chunks when the same file path exists
- `chunkG0.bin` = base game data
- `chunkG1–chunkG11` = Iceborne updates
- `chunkG60.bin` = Hi-Res texture DLC (ignore for weapon data)

### Long Sword Data Location
**Path:** `chunkG0/wp/mus/` (weapon ID: `wg10` = Long Sword)

**Key Files:**
- **Motion files:** `mot/wg10mus_00.lmt`, `wg10mus_01.lmt`, `wg10mus_10.lmt`
- **State machines:** `mot/wg10mus_00_fs.lmt`, `wg10mus_01_fs.lmt`, `wg10mus_10_fs.lmt`
- **Chain/combo data:** `common/chain/on_deco_chain*.ctc`, `off_deco_chain*.ctc`
- **Model/visual:** `mus001/mod/mus001.mod3`, `.mrl3`, `.evwp`, `.tex`

**Weapon Folder Structure:**
- `wp/two/` = Great Sword (two-handed)
- `wp/swo/` = Sword & Shield
- `wp/mus/` = Long Sword (wg10)
- `wp/one/` = Dual Blades (one-handed)
- `wp/saxe/` = Switch Axe
- `wp/caxe/` = Charge Blade
- `wp/ham/` = Hammer
- `wp/sou/` = Hunting Horn (sou = sound)
- `wp/lan/` = Lance
- `wp/slg/` = Gunlance
- `wp/rod/` = Insect Glaive (rod)
- `wp/bow/` = Bow
- `wp/lbg/` = Light Bowgun
- `wp/hbg/` = Heavy Bowgun

---

## Tools & Workflow

### Required Tools
1. **Stracker's Loader** ([Nexus #1982](https://www.nexusmods.com/monsterhunterworld/mods/1982)) — Re-enables `nativePC/` loading. Already installed in your game directory.
2. **MHWNoChunk** ([Nexus #411](https://www.nexusmods.com/monsterhunterworld/mods/411)) — Extracts/decompresses chunk archives. Recommended over WorldChunkTool.
3. **WorldChunkTool** ([GitHub](https://github.com/mhvuze/WorldChunkTool)) — Alternative chunk extractor (older).
4. **MHW Editor** — Opens `.pak`, `.lmt`, `.ctc`, and other game files for editing.
5. **Ezekial711/MonsterHunterWorldModding** ([GitHub Wiki](https://github.com/Ezekial711/MonsterHunterWorldModding/wiki)) — Weapon IDs, Action IDs, FSM editor, moveset editing reference.
6. **Visual Studio 2022 C++ toolchain:** Installed at `E:\MSVC2022`; used to build native x64 Stracker plugins.

### Extraction Workflow
```bash
# Extract base game chunk (contains Long Sword data)
MHWNoChunk.exe "E:\SteamLibrary\steamapps\common\Monster Hunter World\chunk\chunkG0.bin" "E:\MHW-arts\chunk-extracted\G0"

# Extract Iceborne chunks (if needed for updated weapon data)
MHWNoChunk.exe "E:\SteamLibrary\steamapps\common\Monster Hunter World\chunk\chunkG5.bin" "E:\MHW-arts\chunk-extracted\G5"
# ... repeat for G6-G11 as needed
```

### Mod Installation Workflow
1. Edit extracted files in MHW Editor
2. Save modified files to `nativePC/<same internal path as extracted>`
3. Launch game with Stracker's Loader
4. Game loads modded files instead of packed versions

---

## Current Progress

### ✅ Completed
- **Task #1:** Extracted Long Sword data from chunkG0.bin and identified file structure
- Located Long Sword data at `chunkG0/wp/mus/` with motion files (`.lmt`), state machines (`_fs.lmt`), chain data (`.ctc`), and model files
- Installed and tested the MHW Valor nativePC assets; the FSM/motion changes work without the original DLL.
- Confirmed the original `ValorLS.dll` is incompatible with the current game build and fails Stracker initialization.
- Built `ValorGauge.dll`, a native C++ Stracker plugin with no SharpPluginLoader dependency.
- Hooked the DirectX 11 swap-chain Present call using Kiero and MinHook.
- Rendered a blue ImGui Valor gauge inside the game frame and verified it in-game.
- Added `F8` gauge visibility toggle and `nativePC/plugins/ValorGauge.log` diagnostics.
- Signature-scanned and hooked `ActionController::DoAction`, identified the player action controller, and added live `set:id` logging/UI.
- Mapped the normal-mode ground attacks used to charge Valor in `mhw-valor-normal-mode.txt`.
- Replaced the fixed 65% prototype with a live 0–100% gauge; mapped normal attacks add 10% per transition.
- Located the working Long Sword spirit-level field through `player + 0x76B0 -> weapon + 0x2370`.
- Confirmed in game that spirit level 2 (yellow) or 3 (red) enables the modified Valor FSM branches; the prototype writes level 3 at full charge and enters Valor mode successfully.

### 🔄 In Progress
- Validate FSM-owned Valor-mode depletion and return to normal mode.
- Refine per-move charge gains and prevent charging while already in Valor mode.

### ⏳ Pending
- Detect the FSM's Valor-to-normal transition without treating aura level as mode state.
- Reimplement skill selection and activation previously provided by `ValorLS.dll`.
- Match the original Valor gauge placement and behavior.

---

## Technical Discoveries

### File Extensions
- **`.lmt`** — Motion files (animations) and state machines
- **`.ctc`** — Chain/combo data files
- **`.col`** — Collision/hitbox data
- **`.mod3`** — 3D models
- **`.mrl3`** — Material/resource list files
- **`.evwp`** — Equipment parameter/visual files
- **`.epv3`** — Equipment parameter/visual files
- **`.mhla`** — Motion files (found in player data)
- **`.tex`** — Texture files

### Data Organization
- Player appearance data (equipment, faces, hair) is in `pl/` folders
- Weapon animation/moveset data is in `wp/` folders
- Different chunks contain different types of data — not all chunks have `pl/` or `wp/` folders
- Damage/attack data location is still being mapped (likely in `.lmt` or `.ctc` files, or separate parameter files)

---

## Hunter Arts Design (Future Phases)

### System Design
- **Charge Meter:** Fills by dealing/taking damage, displayed on UI
- **Art Activation:** Trigger button/key when meter is full
- **Per-Weapon Arts:** Each weapon gets unique arts that play to its fantasy
- **Universal Arts:** Utility arts like Absolute Evasion, Healing, etc.

### Long Sword Candidate Arts
- **Sakura Slash:** Multi-hit dash that fills Spirit gauge
- **Spirit Blade:** Enhanced spirit attacks
- **Critical Juncture:** Precision damage boost

### Technical Challenges
- **New Animations:** Importing MHXX motions requires skeleton retargeting and `.mot` format conversion
- **Custom Meter UI:** Adding visible gauge requires HUD modifications or overlay plugin
- **Action Registration:** New actions may need registration in game's action table
- **Multiplayer Sync:** Custom moves may not sync in multiplayer (acceptable for initial prototype)

---

## Development Notes

### Installation Status
- **Stracker's Loader:** ✅ Installed (`loader.dll`, `dinput8.dll`, `loader-config.json` present)
- **CRC Bypass:** ✅ Installed as `nativePC/plugins/!CRCBypass.dll`
- **MSVC 2022:** ✅ Installed at `E:\MSVC2022`
- **ValorGauge:** ✅ Built, installed, and verified in-game under DirectX 11
- **MHWNoChunk:** ✅ Available for extraction
- **MHW Editor:** ✅ Available under `tools/MHWEditor`
- **Chunk Extraction:** ✅ G0 fully extracted, other chunks partially extracted

### Native Gauge Plugin
- **Source:** `ValorGauge/`
- **Output:** `ValorGauge/build/Release/ValorGauge.dll`
- **Install path:** `nativePC/plugins/ValorGauge.dll`
- **Runtime log:** `nativePC/plugins/ValorGauge.log`
- **Renderer:** DirectX 11 (`EnableDX12=OFF` in the active game configuration)
- **Libraries:** Dear ImGui, Kiero, and MinHook
- **Current behavior:** Logs and shows the live action `set:id`, gains 10% from mapped normal attacks, writes Long Sword spirit level 3 at full charge to enter Valor, then detects the first normal-mode action after the FSM exits Valor and re-arms charging. Native Valor and spirit gauges remain responsible for their own display and depletion. `F8` toggles visibility.

### Live Valor/FSM Discoveries
- `ActionController::DoAction` signature: `48 8D 41 07 48 C1 E0 04 46 3B 04 08`, function start at match `- 10`.
- Player action controller is inline at `player + 0x61C8`.
- Live FSM fields are available at `player + 0x6274` (target) and `player + 0x6278` (id).
- Current weapon pointer is stored at `player + 0x76B0`.
- Long Sword spirit level is `int32` at `weapon + 0x2370`: 0 none, 1 white, 2 yellow, 3 red.
- Aura/spirit level is an entry condition, not the Valor attack state; do not use its transitions as the mode exit signal.
- The FSM owns Valor-to-normal transition and the Valor Spirit guard point. The DLL only supplies normal-to-Valor entry by writing level 3.
- The Valor FSM does not use a custom `南风焓` runtime property; that text is an author label on nodes/links.
- Exported `wp03_action.xml` showed that the relevant alternate branches test the standard `オーラレベル白以上` condition, but runtime testing established that yellow/red activates the intended Valor behavior while white alone did not.

### LMT/TIML mapping rules (2026-09-13; avoids a known detour)

- **FSM action id ≠ LMT entry index.** The id→entry mapping table is `E:\MHW-arts\action-list.md` (见切 86→140, 突刺 79→105, 気刃突刺 87→170, 特殊纳刀 99→306...). Never `dump` an LMT by the FSM id directly.
- Working LMT: `nativePC/hm/wp/wp03/mot/w03_00/w03_00.lmt` (only one; 464 slots, most empty). Tool: `python patch_timl_defense.py dump|move|setval <lmt> <entry> ...` (expects checked edits, auto .bak).
- Vanilla revert point: `w03_00-origin.lmt`. The `.bak` is from an old session, NOT vanilla. Files restored from the game carry the read-only attribute — clear with `attrib -R` before editing.
- TIML frames tick at ~60fps (12f = 0.2s, user-measured).
- Defense recipe (empirically closed): i-frames = PlMotionCommon `mFlag` b26 (0x04000000) window + ONE `mTrigger` 2048 key anywhere inside it (gate, position-free); super armor = b24 (0x01000000) window, no trigger; both on → no-hit wins. 叮+蓝光 deflect presentation is FSM-property-driven, not TIML-switchable.
- 见切 entry 140 now runs a 0.2s parry window: `mFlag` f0=0x80400000 → f12=0x80000000; `mTrigger` f10=2048 (moved from f45 by value-swap so keys stay sorted). In-game verification pending.
- ValorGauge input-layer Ctrl auto-repeat: every Ctrl press pulses Ctrl for N seconds (config `nativePC/plugins/ValorGauge.counter-repeat.txt` = "<seconds> <pulseMs>", default 5.0/50) so any post-clang success window arms `見切り回避成功後の攻撃ヒット` without extra input. Pure SendInput/GetAsyncKeyState — no game memory layout involved. Replaces three failed memory-side negated-hit detectors (stager init-only; leaves 南风-only; negated hits never reach the 0x11B8880 arbitration latch).
- 2026-09-14 route decision: `見切り回避成功後の攻撃ヒット` is HIT-gated ("the follow-up attack landed after a successful foresight"), not a dodge-success flag, and no vanilla move offers dodge-success-without-hit (guard weapons react but chip). R-simulation stays installed as a convenience/test tool but is NOT the 刚1 mechanism. Fallback if the RXN watcher dies: hook the TIML no-hit collision rejection upstream of FUN_141f66dd0 (RVA 0x1F66DD0) — also the last gap in the iframe mechanism.
- 2026-09-14 decompilation round (Reverse/MonsterHunterWorld_dump.exe.c — use it, the on-disk exe is packed): **+0x7C0 is DEAD as a clang detector.** FUN_142045350 (RVA 0x2045350) clears `mgr+0x7C0` then sets it to 1 whenever prop `0xDE idx1 >= 1 && player->0x7626 == 0` — i.e. it arms at 见切 move start, unrelated to negation. `player+0x7626` is a global-config copy (`_DAT_1451c42b8+0xD2EA`), not a clang latch. Property chain fully reverse-engineered from FUN_141922190/1419221f0 + FUN_1412f80f0 + FUN_140462700: `component = *(player+0x126D8)`; `key = *(int32*)(component + 0x84 + id*24)` (resolver FUN_1412f80f0: `lea rdx,[rax+rax*2]` = 3*id then `[rcx+rdx*8+0x84]` = 0x18 stride — a hand-decode first read this as *16 and resolved garbage key=0; capstone + the *0x18 binding code confirm 24); `registry = base+0x5012048`, count `+0xB8`, `row = *(rowTable=*(registry+0xC8))[key]`; counter = `*(int16*)(row+0x24+index*2)`. 0xDE block: idx0 = follow-up latch (刚1 target), idx1 = foresight-family flag, idx2 = success counter (scales clang presentation duration in FUN_1411c26e0). No static write site for runtime counters (goes through virtual/cached-pointer paths) — irrelevant, we read AND write the chain ourselves.
- ValorGauge RXN watcher (deployed 2026-09-14): per-frame reads of mgr+0x7C0, pool+0xE4 no-hit remaining, and the 0xDE idx0/1/2 counters via the verified chain; logs edges only (`RXN f=<frame> act=<set:id> 7C0= nohit= DE=[i0,i1,i2]`), draws a live `DE[..] 7C0=` line top-left (cyan when 7C0 armed). Whiff-vs-clang run decides whether idx0 or idx2 is the clang signal. **F6 = property write test** through the same chain, config `nativePC/plugins/ValorGauge.prop-write.txt` = "<id> <index> <value>" (default `222 0 1`); logs key/row/before/readback. If idx0 isn't the 刚1 latch, repoint the config to the F11-identified id without rebuilding.
- 2026-09-14 first live run: with the wrong stride the watcher resolved key=0 (static read-only row, `PROPWRITE: unresolved or not writable`). Fixed build also logs `gate=` (player+0x7626) and falls back to VirtualProtect when the counter row is write-protected. Watch: does DE idx1 go 1 during 见切 3:86, does idx2/idx0 jump on the clang?

### 2026-09-15 Original ValorLS.dll mechanism (Reverse/ValorLS.dll.c — use it)

- User's counter-evidence killed the "unbound = no clang" theory: vanilla iai/见切 still clang with the vanilla FSM. Conclusion: 0xDE/7C0 is ONE success candidate (likely tied to the extended-iframe iai mechanic), not THE clang presentation; other candidate evaluators (FUN_1411c2780/27c0/2800 family reading prop 0x35) exist.
- The original mod NEVER touched property R. `Reverse/ValorLS.dll.c` (native C++ layer, MinHook) shows its real mechanism:
  - Hooked 0x140310030 (frame tick) and 0x140AE9EA0 = the **action request processor**; param_2 = request struct: `+0x00` lifecycle (1 idle / 3 executing, old build), `+0x10` actionSet, `+0x14` actionId, `-0x88` current action id. **Our DoActionHook's ActionInfo* == req+0x10.**
  - Key press (configurable VK): if request idle (`*req==1`) → write `req+0x10=3, req+0x14=<art action id>` (0x5e/0x6f/0x6a/0x68 for its four arts) → game queues that action. **Force-action bypasses all FSM conditions** - no property write needed for 刚1.
  - Gauge: player-manager chain from DAT_145073ED0; spirit level lock via `weapon+0x2370` (same field we use); weapon+0x2388 (invuln timer) checked before one art. One hard-coded old-build call `0x142134B70` - the version-locked part that broke.
  - Action-id whitelist (10 ints) = moves that do NOT reset valor charge, checked at every action change (`req-0x88`).
- ValorGauge now has the same force path: DoActionHook captures ActionInfo* (logs a one-time REQ layout check: state + id@-0x98), **F6** queues {set,id} when idle (refuses when busy), **Shift+F6** = old property-write test. Config `nativePC/plugins/ValorGauge.force-action.txt` = "<set> <id>" (default `3 104`; set 刚1's real id).
- Next: validate force mechanism with F6, then wire clang detector + R-window to auto-queue 刚1.

### 2026-09-16 刚1 build session (mid-action interrupt works; clang discriminator still open)

- **Mid-action interrupt SOLVED**: calling `g_originalDoAction(controller, {set,id})` directly from the property-getter hook (game thread, every frame) interrupts ANY current move immediately. F6 arms a one-shot force; config `nativePC/plugins/ValorGauge.force-action.txt` = "<set> <id> [invulnSec]" (default 3 104 0.5). Shift+F6 = property write test. The ActionInfo* that DoAction receives is a STACK TEMPORARY - never store it (captured pointers decayed into float/resolution garbage).
- **Crash lesson**: MinHook on an already-hooked target from inside its own detour = crash (MinHook disassembles the patched bytes for the trampoline). Happened because vtable+0x150 of the audio singleton IS the sound wrapper itself. Also: request-slot scans are dead ends - controller memory contains many {3,0} copies and the one "persistent" hit was a passive echo struct that never gets consumed.
- **Wwise route (dead for detection, tooling kept)**: AK::SoundEngine::PostEvent id-version RVA 0x28B8DD0, string-version 0x28B8FE0, game wrapper FUN_1426d4c50 (RVA 0x26D4C50, args: entity, eventId, gameObjectId, flags), audio singleton global RVA 0x51C458 (vtable+0x150 = the wrapper itself). Event **1086158451 is NOT clang-exclusive** - it is the shared "metal blade ring" family: draw attacks (3:7) post it too. 4-whiff/7-clang diff inside 3:86 looked perfect but does not generalize. WWISEEV + CLANGSOUND logging capped and kept.
- **Hit dispatcher FUN_141f71960 (RVA 0x1F71960, 2 args)**: returns 0xD for ambient no-collision checks (per-frame flood); NON-0xD returns fire whenever the no-hit window is OPEN (negative floats drifting -136→-350 + 0x6B alternating) - **they track the i-frame window, not the clang** (user-verified: COUNTER WINDOW opens on every 见切, whiff included). Damage calc FUN_141f66dd0 (RVA 0x1F66DD0, 3 args) entries = hits that actually connected (FSDDMG log).
- **Current live design**: dispatcher rejection during 3:86 opens a 0.6s COUNTER WINDOW (screen indicator, cyan) + injects pool+0xE4 no-hit 0.5s; a Ctrl (spirit key) press inside the window fires the force. Opens on whiffs too - **known false positive, accepted for now**.
- **見切り回避成功後の攻撃ヒット identity solved**: FSM condition property names live in a .rdata string table (~VA 0x1430BA8F0); the name→member-index resolver (inlined in FUN_14118bda0) maps it to **member index 0x277=631 of cFSMPl_W03**. It is a REFLECTION member, NOT registry-backed (REG id=631 row=0x0; component slot key=1 exists but lookup(1) returns junk). Evaluator = FUN_14118bda0 (node: +8 owner instance, +0x10 index). **Tomorrow's primary lead**: extract the descriptor-table offset for index 631 and read the vanilla latch directly - that is the exact 刚1 trigger. Vanilla 见切 already uses this condition, so the latch demonstrably works.
- User's additional requirement noted: the vanilla follow-up also has a kenki gauge cost - the counter trigger should respect gauge state (window currently opens even with no gauge).
- Current milestone line: see bottom.

### 2026-09-15 ROOT CAUSE: property R (222) was UNBOUND under the vanilla FSM

- Live A/B came back ALL-ZERO: F11 groups (clang 见切 / 居合 3:102 / whiff 见切) show `222 idx0/1/2` read on PLAYER (callers 0x11BE7AC = arbitration-region, 0x11C02CD, 0x20453AC = FUN_142045350+0x5C, 0x11C2713 = FUN_1411c26e0+0x33) but **max=0x0 in every group — even the clang one**. 7C0 never armed because idx1 never reached 1: **the success-window system was dead the whole session.**
- Cause: the 09-14 revert to 原版 wp03_action.fsm removed every FSM reference to property R(222) → the property binder leaves the component key slot 0 (`key = *(int32*)(component+0x84+222*24) == 0`) → all reads fall through to static row 0 → idx1 can never arm. At the 09-12 A-capture the modded FSM still referenced R, so the whole chain lived. The clang the user "heard" was only the TIML b26 negation feedback — no blue-flash presentation.
- Evidence chain: TIML is innocent (vanilla entry 140 `PlMotionCommon mFlag` is already f0=0x80400000/f130=0x80000000 — our edit only moved keyframes f130→f12 and trigger f45→f10). 0xDE's setter lives in the event system: `func_0x141f604b0(target, bit)` = `bts [target+0x8938+(bit>>5)*4], bit` (edge array at +0x8968, id 0x9B special-cased); call sites fire it when a TIML-flags bit28 is consumed.
- Consequences: every earlier "detector failed" result (stager / 南风 leaves / arbitration latch / 7C0 / 0xDE polls) ran against the unbound property — do not trust them as negative evidence. RXN poll + F6 now treat key==0 as "unbound" and say so in the log.
- Fix path: re-add an FSM reference to R via XFsm — ideally the real 刚1 branch condition (VariableNode R / cFSMPl_W03), which both rebinds the property and implements the gating. Then reload the area/quest (binding happens at load) and rerun the whiff-vs-clang A/B with live DE values.

### 2026-09-17 刚1 SUCCESS for iai (101/102); foresight detector built, deploy pending

- **F5 paired-scenario state miner** (the tool that broke the deadlock): records
  per-frame memory deltas per evasion run over weapon/mgr/pool/FSM/player
  windows (incl. the event bit array at player+0x8938!), auto-tags phases per
  F5 press, dumps a correlation-ranked report (clang-rate minus whiff-rate).
  Always-on recording; F5 = phase marker + snapshot dump.
- **Mined + verified: iai negation = event bit 250** at player+0x8954 bit 26
  (dword 0x1C000000 vs whiff 0x18000000/0x18400000; self-clears when the
  presentation ends). Trigger wired: bit rising during 86/101/102 → inject
  pool+0xE4 no-hit 0.5s + counter window + auto-force 刚1 via DoAction.
  **User-verified working for 小居合 101 AND 大居合 102**; whiffs silent.
- **Foresight (86) detector built, not yet deployed-validated**: weapon+0x23A0
  = blue-flash intensity, jumps to 1.0 and decays only when the deflect
  presentation plays (mined 4/4 clang vs 0/5 whiff). Trigger: crossing 0.5
  upward during 86. Was blocked by the game locking the DLL mid-test.
- **Ruled out this session**: player+0xB228 (attack-connect flag - sets on ANY
  connecting attack incl. normal slashes); Wwise event 1086158451 (shared
  metal blade-ring family: draw attacks post it too); dispatcher non-0xD
  codes (track the no-hit window state, not negation; rejected type-6 hits
  never reach FUN_141f71960 at all); FUN_14118bda0 (editor/debug-only path,
  never called in gameplay); weapon+0x2374 (slow linear float timer, likely
  the red-spirit-gauge duration - useful later for gauge gating, NOT the
  clang latch; flooding it masked other probes once).
- User's mechanics input (authoritative): 见切 negation gates whether the
  follow-up (大回旋 route) is possible - success has TWO parts (i-frame hit +
  follow-up lands = condition 631); 居合 negation only drives the super-armor
  frames - that is why iai has a clean standalone negation state and 见切
  needs the presentation-side signal instead.
- Config surface: force-action.txt "<set> <id> [invulnSec]"; counter-auto.txt
  0/1 (auto-force vs Ctrl-gated inside the 0.6s window); prop-watch.txt ids;
  prop-trigger.txt id/index; prop-write.txt id/index/value; state-address.txt.
- Next: deploy the flash detector (game closed), validate 见切 clang → done.
  Optional later: kenki-gauge gating via the 練気ゲージ判定 properties (628/629).

### XFsm condition-tree authoring (2026-09-14, user-verified)

- Minimal link condition: one top-level `OperationNode` (Operator `None` = passthrough for a single child; AND=16 / OR=17 for multiple), then `Add Child` → `VariableNode`.
- `VariableNode`: Property Name = the condition property (e.g. `R`), Owner Name = `cFSMPl_W03` for the weapon FSM; leave Is Singleton Owner / Is BitNo / Is Array / Is Dynamic Index / Use Index Enum unchecked, Index = 0, Index Variable N/A.
- Conditions evaluate at FSM checkpoints; presses buffer until the checkpoint (pressing R mid-见切 applies at action end).
- XFsm crash fixes shipped (2026-09-14): root-type-change double-free in `AIConditionTreeInfo.RootNode` setter, stale cached wrapper in `XFsmConditionTreeInfo`, unknown-node-type ctor throw. Unhandled C# exceptions now log to `nativePC/plugins/CSharp/XFsm/render-error.log` instead of killing the game; empty log + instant exit = native crash (x64dbg: break on 0xC0000005/0xC0000409, or attach dnSpy).

Build command:

```powershell
& 'E:\MSVC2022\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' -S ValorGauge -B ValorGauge\build -A x64
& 'E:\MSVC2022\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build ValorGauge\build --config Release
```

### Known Issues
- **Cracked/Repacked Versions:** FitGirl repacks etc. have altered chunk files incompatible with Nexus mods. Your install appears legitimate (Stracker + nativePC mods work).
- **Hi-Res Pack Confusion:** chunkG60.bin contains Hi-Res DLC, not base game data — ignore for weapon modding.
- **File Structure Complexity:** Damage/attack data location not yet mapped — requires opening files in MHW Editor.

---

### MHRS Animation Extraction (Verified 2026-08-30)
- Rise animation files use `MOTLIST v528`; the Long Sword file is `STM/player/mot/plw_longsword_100.motlist.528`.
- `yun-wulian/REasy` adds MHRS-specific `MOTFSM2 v43` reading/editing, but it is not the MOTLIST animation decoder.
- `PredatorCZ/RevilLib` contains `src/reng/motion_list_528.hpp` and registers `REMotlist528Asset`.
- The `nightly` RevilToolset release was published **2026-05-09 12:29:14 UTC** (20:29:14 China Standard Time).
- Downloaded tool: `tools/RevilToolset-nightly/` (`RevilToolset v2.10.201`, module `reasset_to_gltf`).
- Verified command:

```powershell
& '.\\tools\\RevilToolset-nightly\\revil_toolset.exe' reasset_to_gltf `
  (Resolve-Path '.\\MHRS\\LongSwordExtract\\plw_longsword_100.motlist.528').Path
```

- Output: `MHRS/LongSwordExtract/plw_longsword_100.motlist.glb`.
- Blender 4.3.2 imported the GLB successfully and produced 1 Armature, 3 Mesh objects, and 156 Actions. Animation tracks are present; the GLB has no character skin/mesh weighting, so the armature appears unskinned until a compatible Rise player mesh is imported and bound.
- Preferred workflow is `MOTLIST v528 -> RevilToolset GLB -> Blender`; a future Blender addon should wrap this converter rather than reimplement the binary parser.
- **Path encoding limitation:** RevilToolset `reasset_to_gltf` currently aborts (`SIGABRT`, code 22) when the input path contains non-ASCII characters (for example the Chinese `勇气太刀本体` folder). Stage the `.motlist.528` file under an ASCII-only path before conversion, then copy the generated `.glb` back to the desired location. Example:

```powershell
$stage = '.\\conversion_stage'
New-Item -ItemType Directory -Force $stage | Out-Null
Copy-Item -LiteralPath '.\\valor-mhrs\\勇气太刀本体\\valor\\natives_backup\\STM\\player\\mot\\plw_LongSword_100.motlist.528' `
  -Destination (Join-Path $stage 'plw_longsword_100.motlist.528') -Force
& '.\\tools\\RevilToolset-nightly\\revil_toolset.exe' reasset_to_gltf `
  (Resolve-Path (Join-Path $stage 'plw_longsword_100.motlist.528')).Path
Copy-Item -LiteralPath (Join-Path $stage 'plw_longsword_100.motlist.glb') `
  -Destination '.\\valor-mhrs\\勇气太刀本体\\valor\\natives_backup\\STM\\player\\mot\\plw_LongSword_100.motlist.glb' -Force
```

- The staging directory and filename must contain only ASCII characters; the output can be copied to a Unicode-named destination afterward.

## Resources & References

### Modding Community
- **MHW Modding Wiki:** https://modding.wiki/en/monsterhunterworld
- **MHW Modding Discord:** Community support and latest tool updates
- **Ezekial711's FSM Editor:** For moveset/state machine editing
- **"MHWI Weapon Moveset Editing Guide" (YouTube):** FSM editing workflow

### Game Knowledge
- **MHXX Hunter Arts:** Original system with chargeable special moves per weapon + universal utility arts
- **MHW Weapon IDs:** See Ezekial711 wiki for definitive weapon type identifiers
- **MHW Action IDs:** See Ezekial711 wiki for action/move identifiers

---

## Quick Reference

### Common Paths
- **Game Root:** `E:\SteamLibrary\steamapps\common\Monster Hunter World\`
- **Chunk Archives:** `E:\SteamLibrary\steamapps\common\Monster Hunter World\chunk\`
- **Extraction Output:** `E:\MHW-arts\chunk-extracted\`
- **Mod Override:** `E:\SteamLibrary\steamapps\common\Monster Hunter World\nativePC\`
- **Long Sword Data:** `E:\MHW-arts\chunk-extracted\G0\chunkG0\wp\mus\`

### Weapon ID Mappings
- `wg10` = Long Sword
- `wg11` = Great Sword (two-handed)
- `wg01` = Sword & Shield
- `wg02` = Dual Blades
- (Full list in Ezekial711 wiki)

### Tool lists

MHWNoChunk and MHWEditor

---

*Last Updated: 2026-09-14*
*Current Milestone: 刚1 counter-parry — IAI (101/102) COMPLETE: event bit 250 (player+0x8954 bit26) = negation, wired to no-hit injection + auto-force, user-verified. Foresight (86) detector built (weapon+0x23A0 blue-flash >0.5), blocked on DLL deploy (game locked it). Next session: deploy, validate 见切, optionally kenki-gate via 628/629. Mid-action interrupt + no-hit injection all working*
