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

### Native Gauge Plugin (2026-09-17: mod/probe split)
- **Source:** `ValorGauge/` — ONE source (`src/dllmain.cpp`), TWO build targets:
  - **`ValorGauge.dll` (mod, default):** Long-Sword-only gameplay mod. Weapon gate =
    `*(int32*)(weapon+0x9F8) == 3` (LS; same value func_0x141F627F0 returns and
    FUN_14118bda0 switches on — LS case holds 練気ゲージ判定/居合斬り/見切り conditions;
    consistent with wp03/w03/action-set 3. [CORRECTED 2026-09-25: the remapper
    FUN_14118c320's decompiled "case 2" IS LS — its dispatch does `dec eax` on
    weapon+0x9F8 before the jump table, so decompiled case N = weapon type N+1;
    see the 2026-09-25 section]),
    0xFF = no weapon; override via `nativePC/plugins/ValorGauge.weapon-type.txt`).
    Contains: action tracking, clang triggers (bit15 / bit250 / deflect flash /
    condEval-631) → counter window + no-hit injection + auto-force, F6 force,
    valor charge, overlay text. NO audio hooks, NO damage/hit-dispatch hooks.
    No-hit pool derived in-place: `mgr = *(player+0x7D20)`, `pool = mgr+0x28`.
  - **`ValorProbe.dll` (`VALOR_PROBE=1`):** passive research build. Wwise (3 hooks),
    damage calc, hit dispatcher, pool query (Aee050), condition-evaluator logging,
    F5 miner, F11 property histogram, F7 state logger, F10 diff capture,
    RXN/EPROBE watchers. Installs NO gameplay behavior. Logs to
    `nativePC/plugins/ValorProbe.log`. Install ONLY while researching.
- **NEVER install both DLLs at once**: separate MinHook instances double-hooking
  the same targets corrupts trampolines (crash). Both DLLs self-check at init and
  stand down if the sibling is loaded — but keep only one in `nativePC/plugins`.
- **Renderer:** DirectX 11 (`EnableDX12=OFF`); Libraries: ImGui, Kiero, MinHook.
- **Current mod behavior:** LS-only. Normal attacks charge valor 10%/hit; level-3
  write enters Valor; 见切/居合 clang auto-fires 刚1 mid-action with no-hit
  coverage. `F8` toggles overlay. **User-verified 2026-09-18**: LS triggers work
  post-split and the BGM bug is GONE — root cause confirmed as the old
  all-in-one DLL's audio hooks (sound-post wrapper arg mismatch suspected);
  the mod hooks no audio.

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

### Weapon attack-box format: wp03.col DECODED (2026-09-19; see COL_AttackBox_Research.md)

- **Weapon hitboxes are NOT in TIML and NOT mesh-bound.** TIML `EventCollision00` only opens the
  damage WINDOW; the geometry + damage live in `hm/wp/wp03/collision/wp03.col`:
  `COL\0`(names) + `CLND`(13 banks) + `CLGM`×13 (72-byte parametric volumes: Attack_Range + 20/20/20
  dims + ±90° sweep) + `ATK\0` (59×218B damage rows: Motion_Value, HitLag, Energy_Gen, Soften,
  MindsEye...) + `OAP\0`. Routing: TIML `CollisionTimelineObject` ノードID = names-table `unk4`
  → that entry's `cgp` = CLGM bank, `move` = ATK row (verified: 見切 node 63 ↔ name[24], 居合2 101 ↔ name[45]).
- **Attack_Range = hitbox volume SCALE, user-verified 2026-09-19**: the record volume is a
  player-centered ellipsoid; rng=500 (10x) felt ~3x bigger and hit monsters BEHIND the player
  (perceived size ≈ √rng). Banks are per-MOVE geometry sets, not weapon-length tiers
  (bank0=normal slashes 50, bank1=thrust 50/57, bank6=必殺 65/77, bank8=见切 75/93,
  bank9=居合2 100/124, bank10=mounted 360; 6 slot records per bank, f30 constant per slot
  across banks = 3/118/153/69/240/154; u48=8/9/10 likely ↔ EventCollision00 ReqNo A/B/C).
  Position does NOT come from the weapon mesh — and needs NO data at all: swap experiment
  (2026-09-19, thrust 3:79=LMT105 vs spirit blade 1 3:11=LMT104 boxes exchanged) proved the
  engine sweeps the volume along the ANIMATED WEAPON during the TIML window (poke anim = small
  front hit, sweep anim = 180° arc; hit TIMING shifted, window unchanged). Labels verified
  consistent (thrust node42="突き", spirit1 node56="気刃斬り１"); action-list.md now records
  11→104 alongside 79→105. **HitLag (hitstop, seconds) user-verified editable**: ATK HitLag_a/b
  set in pairs, scale 0.03 light → 0.40 max (aerial spirit thrust); HitLagSpd keep 1.0.
  Multi-hit recipe found: ATK Lingering=1 + Frame_Delay + long TIML window (3 vanilla rows use
  it; Frame_Delay units unverified). Open: s18/s20 (-1 vs valor's 8), f30 slot-type ids,
  u48↔ReqNo mapping, Frame_Delay units.
- Tool: `col_research/col_tool.py dump|bank|atk|setval` (value edits only, auto .bak; setval
  reproduced SR's one-byte range edit exactly). Structural edits (add bank/name/ATK row like the
  valor author did) need a section rebuilder — parser already splits sections, rebuilder unwritten.
- **SR author's col diff** = pure ATK tuning (MV 14→25/38→55, HitLag 0.03→0.15, Energy_Gen,
  Soften) + ONE geometry change (bank4[0] rng 50→65). **Valor author's diff** = structural: new
  name 南风焓 + new bank13 (rng=200, s18=8 — the only non-(-1) s18 in the file, possible bone
  binding) + 見切り攻撃 cgp 8→13 repoint + new ATK row. No global checksum; appends load fine.
- Blender plugin inventory (installed 4.3): MHW_Model_Editor edits `.ccl` = BONE-BOUND capsule
  collision (StartID/EndID bone ids, 64B/entry) for body/props/acc — NOT weapon hitboxes;
  FreeKinetics = LMT/TIML (windows); **no plugin parses .col** — use col_tool.py. Format spec:
  `col_research/mhw-editor-src/010 Templates/col.bt` (sparse clone of Synthlight/MHW-Editor; its
  CollisionTranslations/wp03.col.csv = move-name translations).

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
  **CORRECTED 2026-09-24: 0x2374 IS a 0..1 float gauge-family field, but round-2
  in-game A/B proved it is the LEVEL-DURATION timer (game drains 0.000556/frame
  ≈ 30 s cycle; refilled from params on level change), NOT the visible gauge —
  the 气刃量 is weapon+0x2368. See the 09-24 section.**
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

### 2026-09-17 (late) Vanilla 见切 success latch FOUND: event bit 15 at player+0x8938

- User's model confirmed by decompile: 见切/居合 share the negation machinery,
  见切 ADDS a success judgment; bit 250 is the IAI-specific negation event,
  not "hit during i-frames" in general.
- **The vanilla input remapper = FUN_14118c320** (registered per player-FSM
  init in FUN_14118dca0/14118ddc0 as an action-request callback; IB variant
  FUN_14118d3a0 chosen when player+0x7626 != 0). Signature
  `(mgrCtx, out{state,id}, req{state,id})`; player at mgrCtx+0xD90,
  weapon = *(player+0x76B0). It REMAPS buffered inputs into special actions:
  - weapon+0x2368 (special-sheathe stance): 0→100, 1→101, 2→102, 3→103,
    4→104, 0x12-0x22→105..0x7d family, 0x35/36→160/161, 0x8d→0x8e..0x95 by
    FUN_14211f040(weapon).
  - mgr+0x660/661/662 (mgr=*(player+0x7D20)) latched + req state==1:
    0/9/99/13→762/766/764/764-family, **100/747→(bit15 ? 763 : 764)**,
    7/8/101/748→763, 0x19d(413) with player+0xDDA0 action∈{97,101}→770.
  - **Event bit 15 of player+0x8938 (`u32 & 0x8000`) is the vanilla
    success/failure discriminator** for the 见切 follow-up remap (763 vs 764)
    and the IB special-sheathe remap (162 vs 163). Same TIML event-bit array
    that carries iai negation bit 250 (player+0x8954 bit26). This is the
    "见切成功" latch the user hypothesized - set by the game itself, no FSM
    edit needed.
- Property 631 static facts ( closes the earlier lead): its registration
  accessor SUB_140240720 = `xor al,al; ret` (stub) - condition 631 is NOT a
  stored bool; the "value" lives in the remapper/state logic above. Per-weapon
  condition descriptor tables at 0x143FD5590 (stride 0x130 per weapon type,
  rows {i32 key, ptr, u32} @ block+0x20/+0x28) hold only 2-16 special-case
  rows; global fallback 0x1434DA930 has 12 rows (keys 0x3E8-0x3F3). Name→index
  resolver FUN_140b2e4e0 (giant strcmp chain): 0x1430BA8F0 = 見切り回避成功後の攻撃ヒット
  → 0x277; neighbors 0x274 練気ゲージ判定(見切り 0x275 練気ゲージ判定(必殺 0x276 居合斬り.
- New tools (Reverse/): `vadump.py <va> [len]` (dump bytes at VA from the
  unpacked dump exe), `fsm_table.py` (parse condition descriptor tables),
  `scan_277.py` (immediate-scan); `disasm_rva.py` now reads the dump exe by
  default (on-disk exe .text is encrypted - capstone gets garbage).
- DLL (deployed 862208B): VSEEK watcher logs bit15 + mgr660/661/662 edges
  (`VSEEK bit15= ev0= m660= m661= m662= act=`); trigger = bit15 rising during
  3:86 → counter window + no-hit + auto-force 刚1 (same flow as iai bit250).
  Flash detector (weapon+0x23A0) still armed as a second 见切 signal; log
  lines identify which fired.
- Test matrix: whiff (no trigger expected); clang w/o follow-up hit (fires?
  → bit15 = negation-only); clang + follow-up hit (full success). iai
  101/102 regression must stay green.

### 2026-09-17 (night) 刚1 COMPLETE (all clang paths) + update-resilient address map

- **User-verified done**: whiff never triggers; clang fires immediately on
  见切 86, 小居合 101 AND 大居合 102. Mechanics confirmed: 见切 = step-back
  (whole step is i-frames; hit there = 叮) then counter slash (no i-frames,
  small super armor window when clang'd).
- Final trigger set: bit15 rising during 3:86 (foresight success latch),
  bit 250 rising during 101/102 (iai negation), weapon+0x23A0 flash and the
  CondEvalHook 631 path as extra safety nets. Any of them → counter window +
  pool+0xE4 no-hit + auto-force 刚1 via direct DoAction.
- **Update-resilience** (preps for future game patches):
  - All 10 function hooks resolve by byte signature first (kHookSigs[] in
    dllmain.cpp; wildcards auto-cover rel32 branches + rip-relative disp32),
    then recorded RVA, then `nativePC/plugins/ValorGauge.addresses.txt`
    override (`name=0xRVA`). Signatures generated/verified offline by
    `Reverse/sig_check.py --emit|--verify` (all 10 unique vs dump exe).
  - `MEMORYMAP` startup report in ValorGauge.log: per-hook ok/BAD + propRegistry
    count/rowTable + audioSingleton. Runtime semantic probes log once when
    spirit level leaves [0,3], standby >1, or no-hit < -1 (struct-shift alarm).
  - Recovery wizard: `Reverse/ADDRESS_RECOVERY.md` — step-by-step lookup guide
    (per-target anchor/steps/verify/write-back for every hook, data address and
    struct field; dump+Ghidra setup; find_xrefs.py string→xref tool; F5-miner
    fallback). Someone holding the doc can re-derive all addresses after an update.

### 2026-09-18 Roll i-frame negation: NOT detectable player-side (research closed)

- Question: can a perfect through-roll (drawn roll = 3:18) be detected like
  the 见切/居合 clang? Answer after 3 instrumented mining rounds: **NO.**
- Evidence (ValorProbe.log, 6-run session): during PERFECT through-rolls the
  hit dispatcher produced ZERO type-6 interactions (ROLLDISPATCH log empty);
  no event bits, no presentation fields moved. During the FAILED roll (i-frames
  expired, hit connected): event bit 250 (ev8954 bit26) fired + pB228=1 + HP-1.
- Conclusion: MHW implements roll i-frames as pure collision EXCLUSION - the
  player simply is not in the hit query set, so no layer records "the attack
  passed through me". 见切/居合 are detectable ONLY because they have
  deflect/negation INTERACTIONS (type-6 with special codes + presentations).
  Generic perfect-dodge detection would require computing hitbox-vs-player
  overlap geometrically from monster collision data - a separate project.
- Refinement: event bit 250 is a per-motion TIML event - same id, different
  meaning per move (iai: fired on negation/clang; roll: fired on hit-connect).
  Do not treat event bits as global semantic constants.
- Swap discipline learned the hard way: the old POST_BUILD auto-install kept
  re-adding ValorGauge.dll during research builds and the sibling check stood
  the probe down (two wasted rounds). Auto-install removed from both targets;
  deploy explicitly, one DLL at a time.
- (2026-10-04 更正) **所有带空格的文件名都是用户刻意改的** (如 `w03_00.lm t`、
  `Monster Hunter World v...+.exe`) — 不是损坏事故, 不要当异常报告, 也不要"修复"。
  (早前记录的 09-29 "ValorGauge.d ll 改名事故"经用户否认, 原文已删。)

### 2026-09-19 FSM 驱动链: how zero-inbound nodes (居合 101/102) are entered

- Tools: `dump_fsm_xml.py` (parses XFsm XML export) → `wp03_fsm_map.md` (all
  90 nodes: id/ActionNo/in/out + route sections). XFsm shows NODE NAMES only —
  node identity is ActionNo, names lie: 必殺攻撃=87 気刃突刺, 必殺駆け上がり=89,
  居合気刃=node65 act=102. Node45→55 "必殺攻撃→必殺駆け上がり" IS the 87→89 link.
- Zero-inbound nodes (64 居合斬り/101, 65 居合気刃/102, rolls 20/21/22/1005,
  jumps 1006/1007, ledges 1002/1003) are entered by ACTION REQUEST matching the
  node's ActionNo — links only route buffered combo follow-ups. Node-less ids
  (98, 99, 100, 103 have NO node at all) still play motions: action layer and
  FSM node entry are decoupled. Force-DoAction (F6/刚1 path) is the manual
  version of the same mechanism.
- Iai chain (decompile-verified): ほぼ全攻撃(1013 sys-state) --cond232-->
  居合斬り開始(node61, act=-10 marker, no outbound) hands over to the action
  layer. Special-sheathe stance = weapon+0x2368 via FUN_14211cc60(weapon,bool):
  true→=2 + FUN_140ae08a0(player+0x8808) start-notify; per-frame updater decays
  2→1, then 1→0 + FUN_140ae10f0 end-notify (gate `0x2368 != 0` = stance window
  +1 linger frame). **CORRECTION to 09-17 notes: 0x2368 is a stance flag, NOT a
  0-4 selector.**
- Button → buffered set-3 request → remapper FUN_14118c320 (registered by
  FUN_14118dca0/ddc0; player at mgrCtx+0xD90): only in-set==3 proceeds →
  switch(weapon+0x9f8) case 3 (LS) → if 0x2368!=0: REQUEST-id→action-id remap
  0→100, 1→101(小居合), 2→102(大居合), 3→103, 4→104, 0x12-0x22→0x69..0x7d/0x43,
  0x35/36→0xa0/0xa1, 0x8d→FUN_14211f040→0x8e..0x95; else passthrough. Output
  → DoAction (current build VA 0x140269CE0) → FSM enters node 64/65.
  **CORRECTION: mgr+0x660/661/662 remap (in-set==1 gate) outputs are SET 1**:
  99/13/746→{1,764}, 0→{1,762}, 9→{1,766}, 100/747→bit15?{1,763}:{1,764},
  101/7/8/748→{1,763}, 413→{1,770} (gated on player+0xDDA0 action∈{97,101} via
  func_141cb4fb0∈{0x61,0x65}).
- FUN_14211f040 is NOT an iai selector: it grades the 0x8d draw-family —
  FUN_142178100(player+0x230, player+0x7870)*DAT_142f16ec0 vs obfuscated floats
  (player+0x7810)+0xa40/+0xa44 ^ DAT_142f130d0, returns 0-3 → 0x8e/0x8f/0x90/
  0x92/0x93/0x94/0x95.
- Open: upstream producer of request-id 2 (the 大居合 trigger — test via
  capture log on counter-iai: 3:101 then 3:102?); FSM jump-by-ActionNo function
  not pinned (evidence behavioral); what FUN_140ae08a0/10f0 toggle on
  player+0x8808.

### 2026-09-19 (late) Hit-based valor charging + multiplayer-safe local player

- **Charge now requires a CONNECTED hit.** DmgCalcHook (FUN_141f66dd0, sig in
  kHookSigs, fallback RVA 0x1F66DD0) is installed in the MOD build too. a1 is
  the attacker entity (body derefs a1+0x7D20 = mgr); the session-wide hook is
  filtered: a1 == lockedPlayer-0x61C8 or it is counted (g_hitChargeSkips) and
  never charges. Charge = `ValorGauge.hit-charge.txt` percent (default 10) per
  connected hit of an IsNormalModeChargingAction move while !valorMode &&
  ModWeaponActive; reaching 100 → ActivateValorMode. percent 0 = legacy
  charge-on-action-start; negative = charging off. First 8 entries log
  `HITATT a1=.. localPlayer=.. MATCH/skip` — empirically confirms a1 is the
  ATTACKER (if received hits also MATCH, a1 is defender-side and the filter
  must be revisited).
- **Local player (multiplayer).** ValorLS's manager chain, offsets read from
  its .data (0x1800329A8 = 0x50/0x76B0/0x00): player = *( *(0x145073ED0) + 0x50 ).
  Current build has ZERO .text refs to that VA (may have slid) →
  ResolveLocalPlayerViaManager() uses it only when it validates (weapon+0x9F8
  ∈ 1..14). DoAction lock now REFUSES set-3 controllers that are not the chain
  player (`FOREIGN ... not locking` log); without a valid chain it falls back
  to first-set-3 + IsLongSwordController. Unverified: whether remote hunters'
  actions even route through this process's DoAction.
- Build ok (both targets); deployed mod md5 a8df953f4bc20baa3e1732744d57fbb8.
  Test: 1 hit on monster → HITCHARGE +10 in log; whiff → no charge; accept a
  hit → HITATT a1=monster (skip, no charge).

### 2026-09-20 真正纳刀机制: 动作 id 驱动的挂点表切换 (decompile-verified, 复用槽位定案)

- **合并 = 刀模型绑定节点从"手部挂点表"切到"鞘内挂点表"**（两模型从未合并; 纳刀态刀刚体
  跟随鞘所以抖动同步）。两层机制缺一不可: 030/047 Y-scale (1/2/3) 只做**表选择**（位置）,
  "携带态开关"（游戏认为拔刀/纳刀）由**当前动作 id 每帧决策 + 边沿触发**执行。自定义动作
  只写 (1,1) 通道 → 刀到背后但携带态仍"拔刀" → 独立抖动/分离（(1,1) 不合并的根因）。
- 链路: 每帧 FUN_1421237B0 (RVA 0x21237B0) 读当前动作 id (func_0x141BFF740(player,0)) —
  **收刀家族 {5,6,9,10,69(0x45),87(0x57)} 或 weapon+0xA50/+0x23C0 非0 → 期望纳刀,
  其余 → 期望拔刀**; 动作 98/100/101/102 锁存 weapon+0xAB8=1; weapon+0x2D0 bit0x20/0x40
  = 納刀中/拔刀中 (疑似 FSM 条件源) → 边沿触发 func_0x142150D10 (RVA 0x2150D10, 锁存
  weapon+0xC71: 0=拔刀/非0=纳刀, 仅状态翻转时放行) → 控制器 (weapon+0xAF0, vtable
  0x1435195B8) vtable+0x110 = 0x142374710 写 ctrl+0x181 (0=拔刀/1=纳刀, 极性推定待实测)
  → FUN_142373BF0/D50 重绑全部节点: `[表+0x4B0][节点id&0x1FF]`→行号, 行 =
  `[表+0x4A8]+row*0xC0` (+0x20 起 16 float 变换); 激活表 = vtable+0x158 = *(ctrl+0x170)。
- 表加载: FUN_140325130(持有者, 槽0..4, 状态值) → FUN_1403253F0/140326520 按资源 id;
  **状态值 = 030/047 的 1/2/3 去向**（采样指令→此参数未钉死, Ghidra 在调用点丢 r8）。
  武器上有**三个同款控制器** +0xAF0/+0xFA0/+0x1450 (析构 FUN_14210B710 证实) — 推测
  刀/鞘/配件。特效 FUN_142125150/125860 用同一挂点表取位 (row*0xC0+0x20)。
- **定案: 自定义纳刀动作复用收刀系槽位（优先实测过的 5/6/9/10）** — 决策/携带态/特效/
  挂点自动继承, 与"叮+蓝光复用居合槽"同一策略。LMT entry 的 030/047 翻转帧只负责视觉
  滑入对时。
- DLL 测试: `(*(void(**)(void*,int))(*(void**)(weapon+0xAF0)))(weapon+0xAF0, 1)` =
  强制合并 (传 0 = 分离); 若被更新器拉回, 在收刀系动作期间测。
- 待钉 (ValorProbe): 0x325130/0x3253F0 (槽号+状态值序列 vs 030/047), 0x2150D10/
  0x2374710 (极性+时序), +0xC71 维护方, +0xA50/+0x23C0 语义, +0xAB8 读取方,
  set≠3 分支的 0x214/0x215 (疑似 3:99 架势合并路径), 挂点表资源文件定位。
  详见 LMT_TIML_Hitbox_IFrame_SuperArmor_Sheath_Research.md 文末章节。

### 2026-09-20 (2) 为什么 FSM 里找不到 3:11 気刃斬り1 — 链接条件测按键属性,不测请求 id (XML-verified)

- 気刃斬り1 (request 3:11, LMT104) 在 wp03_action.fsm 里**没有 ActionNo=11 的节点,也永远不会有**。
  它的 FSM 状态 = **节点1: ActionNo MtEnum=1001,编辑器名"特抜(気刃斬1"**(节点名再次说谎;`ActionNo`
  在 XFS 里是单值 MtEnum,没有隐藏第二字段)。证据: 节点1 的 15 条入边 = 恰好所有能 R2 接气刃斬り1
  的招式(待機２ cond38 / 斬り上げ16 / 突き19 / 縦斬り17 / 踏込斬り18 / 回避F·R·B·L 125·132·130·131 /
  J攻撃着地133 / 段登(納刀)47 / 気刃斬り上げ71 / 移動抜刀78 / s0006 231 / s000c 286=小居合路线);
  出边 = 気刃斬り1 的真实派生(→気刃斬2 cond20, →見切り153, →必殺攻撃163, →滑歩気刃1 F/L/R 243/247/248)。
- **机制: FSM 链接条件是 cFSMPl_W03 的按键属性 VariableNode,与请求 id 无关**: cond16-20/38 = `R`
  (气刃键), cond153 = `R+○`, cond163 = `R+△`(气刃突刺输入 R2+△ ✓), 另有 `△`/`○`/`△+○`/`△+○+←`
  树。R2 按下 → 输入层发请求 {3,11} → DoAction → LMT104 播放;同一 checkpoint 上源节点的 `R` 链接
  点火 → FSM 进节点1。**动作层与 FSM 各自 authored 对齐,不靠 id 互查。**
- 因此: DoAction 日志里的 id 只对**零入边节点**(居合 101/102、回避 20/21/22 — 靠请求匹配 ActionNo
  进入)成立;链接进入的节点 ActionNo 用的是另一个编号系(1000 系列 = 1000待機/1002,3登段/1005回避F/
  1006,7跳跃/1008味方アッパー/1009スラ納刀/1012待機２/1013ほぼ全攻撃/1014スリまとめ),与请求 id 无关。
  反编译侧注: hook 的 0x140269CE0 在 Ghidra 里不是独立函数(显示为 `func_0x000140269ce0(...)`;
  0x140269c30 是动作实例池分配器),在 dump.exe.c 里 grep "FUN_140269ce0" 永远落空,按调用点
  `(x+0x61c8, &stack)` 搜 func_0x 才能找到。
- 推论(待实测): force-DoAction {3,11} 只播动作,**不会**让 FSM 进入节点1(没有 ActionNo=11 可匹配,
  链接条件 R 又未被满足) — id 11 实质上是"无节点 id"(同 98/99/100/103)。要 FSM 侧同步得满足对应
  按键属性或直接写 FSM 状态。编辑気刃斬り1 的 FSM 行为 → 改节点1(ActionNo 1001);改动作/判定 → 仍走
  request 11 (LMT104/TIML/COL)。

### FSM Studio — browser-based XFsm replacement (2026-09-20, verified)

- **`FSM-Studio/`** is its own independent git repo (nested, like
  MHW-FSM/SPL-Examples — parent-repo commits do not capture it; check
  `git -C FSM-Studio status` there). Edits MHW `.fsm` files in the browser —
  no game launch needed for editing OR binary conversion. TypeScript +
  esbuild, zero runtime deps.
  Run: `npm start` in FSM-Studio (node at `E:\MHW-arts\node-v24.21.0-win-x64`) →
  http://localhost:8123; double-clicking `index.html` also works.
- Opens `.fsm` (XFS binary) AND `.xml` (XFsm MtSerializer export); saves game-loadable
  `.fsm` directly. Deploy target unchanged (`nativePC/hm/wp/wp03/wp03_action.fsm`).
- **XFS format fully cracked + byte-verified** (see FSM-Studio/README.md and
  `research/` python scripts): `XFS\0` + defs block (15 known classes, regenerable
  byte-identically from `src/deftable.ts`) + ClassEntry tree
  (`id=defIdx*2+1`, `var=ordinal`, header u64 = instance count, null ref = `0xFFFE`).
  Two game-written samples round-trip **byte-identically**; 28 node tests (`npm test`).
- Graph editor: layered auto-layout (main chain + request-entered subgraph bands),
  pan/zoom/drag, node/link/condition-tree inspectors, undo, search.
  The 刚1-era workflow (add `VariableNode R` / `cFSMPl_W03` to a condition) is a
  built-in button flow; verified the written binary reparses with the R child.
- Limitations: canonical rewrite re-numbers var ordinals (game-written files are
  already canonical); unknown classes (quest FSMs) synthesize defs and warn;
  `mUIPos` preserved raw, not written back.

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

### 2026-09-22 R2+方向 combo 不触发根因: 请求层按武器路由 combo, FSM 条件覆盖不到 + DIRREWRITE stopgap

- **Symptom:** custom wp03 FSM link `VariableNode("R+↑")` on node 12 (気刃斬2, the REAL one: node1 --cond20--> 12 --cond21--> node2 気刃斬3) never fired, while plain-R link (tree 307) worked and its node-entry ACTION plays over the vanilla request (log: `3:67 -> 3:101` — node-entry beats the request layer). AND(`R`,`レバー前キープ`) also failed.
- **Root cause chain (all verified):**
  1. The property string was first saved as bare `↑` (byte check: `.fsm` had `e2 86 91` alone at 0x1cc70, `R+↑` 0 hits) — bare arrows are NOT registered properties; only combos (`R+↑`, `回避+↑`, `レバー前キープ`...) exist. Re-export fixed the bytes (`52 2B E2 86 91` present).
  2. With correct bytes it STILL fails: `R+↑` IS globally registered (string table `.rdata` 0x1430B9C34; reflection builder 0x140B25DC1 + name resolver 0x140B2F26B both contain it; wp05/wp08 vanilla FSMs use it) — but the **combo→request classifier is per-weapon**: LS routes only what vanilla wp03 uses (`R`, `R+△`, `R+○`, `△+○`, `△+○+←/→`; ← works on keyboard user-verified via 左移動斬り). `R+↑`/`↑`-combos are never classified for LS → the property value is never written → condition false forever. Vanilla wp03 has 0×`↑`, 0×`レバー` conditions — no LS precedent.
  3. `レバー前キープ` etc. registration accessor = 0x140240720 stub (`xor al,al; ret`) — all cFSMPlBase transition values are written per-frame by the input system, registration table has no real getters. `wp03_param.w03p` (1584B, w03p.bt template in mhw-editor-src) = numeric tuning only, NO combo routing table → routing is code-side (upstream producer of buffered set-3 requests — same open item as the backlog's 大居合 request-id-2 producer).
- **Tool gotcha:** XFsm/XML export vs `.fsm` can silently desync (xml newer than fsm with different property text). ALWAYS byte-verify the deployed fsm: `python -c "print(open(r'...wp03_action.fsm','rb').read().count('R+\u2191'.encode()))"`.
- **DIRREWRITE (shipped, mod build only):** DoActionHook stopgap — while hold key is down and PREVIOUS action == duringId, rewrite incoming {3,68}/{3,101} → {3,102} (钢2一长). Runs BEFORE the action tracker updates g_lastLongSwordAction; local-player locked only; iai-safe (rule scoped to during-action 3:67). Config `nativePC/plugins/ValorGauge.dir-rewrite.txt` = "<duringId> <holdVK> <toId> <fromId> [fromId...]" (default `67 87 102 68 101`; holdVK 0 = off; logs `DIRREWRITE: 3:x -> 3:y`). Caveat: FSM still enters the level-gated node (64/2) while 102 plays — follow-up links route from that node, not 65, until the real routing fix.
- Fixed two latent `"nativePC\p..."` single-backslash escape bugs (counter-auto.txt, prop-trigger.txt — C4129, files never loaded).

### 2026-09-24 气刃量定位: weapon+0x2374, float 0..1 (decompile+指令级 verified; mod 配置定案)

- **气刃量 = `float` at `weapon+0x2374`, 0.0..1.0**（user 在 MHW Editor 确认 0-1 float 语义;
  ValorLS.dll 早就写过它 — old-disasm 0x18000838B `movss [rax+2374h],xmm0` = accum/除数 的渐充动画）。
  HUD 条 getter `FUN_140616d60` (VA 0x140616D60): 门控 `func_141F627F0()==3` (LS),
  `return *(float*)(weapon+0x2374) * 100.0f`（×100 常量 `_DAT_142f16ec4`=100.0）。
  **更正 09-17 "ruled out" 条目: 0x2374 不是红刃持续时间计时器，就是气刃量本身；
  当时的"线性慢漂移"= 量条的自然衰减，所以看起来像 timer。**
- 等级变动时的回填（DropLevel, decompile ~4985040 行）: 等级归 0 → `0x2374=0`;
  降到白(1) → `0x2374 = *(playerParam+0x9AC)`, 降到黄(2) → `+0x9B4`;
  `playerParam = *(*(weapon+0x9B0)+0x7810)`（player+0x7810 结构; 实测两处 = 1.0, 与
  diff_analysis 的 "+0x2374 -> 1.0 @ 升级" 吻合）。居合事件回填 `FUN_142123510`:
  `0x2374 = clamp(0x2374 + *(playerParam+0x888), 0, 1.0)`（钳位常量 `_DAT_142e4fe58`=1.0,
  满量走 `FUN_142122410(weapon,1.0)`）。武器块 ctor 初始化 0x2374=900.0 — 出生即被覆盖, 忽略。
- **`weapon+0x2368` 双重语义 — 未经实测不要当气刃量写**: 它既是居合架势 int flag 0/1/2
  (指令级实锤: FUN_14211cc60 `mov dword ptr [w+2368h],2/0`; remapper 门 `0x2368 != 0`),
  又是 0..1 float 累加器: `FUN_142122410(weapon,float)` (RVA 0x2122410) 指令级实锤
  `addss/minss/movss [w+2368h]` 钳 0..1, 同时写 `+0x236C = *(playerParam+0x804)`(当前消耗);
  每招调用点 ~15 处 (wrapper `FUN_142122f90`, 增益参数表 playerParam+0x808..0x834 每招一格);
  动作校验器 `FUN_140b980c0` 在 `*(uint*)(w+0x2368) > *(uint*)(w+0x236C)` 时把
  185/278/285 重定向 → 294（正 float 的 uint 比较成立）。第二个 HUD getter
  `FUN_140616dd0` 读 `0x2368 × 100`（同一 ctor 绑两根条: widget 槽 +0x90 → 0x2368,
  +0x180 → 0x2374）。int flag 与 float 量在同 DWORD 上的生命周期交错方式 **未决** —
  直接覆盖会破坏居合架势进入 (remapper 门) 和 force-iai 流。Open。
- **第二轮实测: 气刃量 = weapon+0x2368 定案 (user-verified 锁住了)** — target=2 每帧写 1.0,
  可见蓝条钉死 → +0x2368 就是 HUD 那根气刃条。+0x2374 = 等级持续时间计时器（30s 周期）。
  09-17 的"线性慢漂移"观察因此完全恢复名誉 — 那时看到的就是 0x2374 本尊。
- **气刃双累加器 + 每招路径 (指令级实锤 2026-09-24)**:
  - `FUN_142122410` (RVA 0x2122410) = **+0x2368 气刃量唯一 float 累加器**, 3 条调用路径:
    wrapper FUN_142122F90（~15 个每招事件处理器各传一个参数表槽位）、FUN_142123510（居合回填）、
    FUN_142124B20（TIML/motion 参数缩放）。
  - `FUN_14211EF90` = **+0x236C 同款累加器**（10 个直接调用点, 与 FUN_142122410 前 44 字节相同,
    仅 call 转移与 addss 偏移 68/6C 不同; 签名靠 `E8 DF 0F 0A` 区分）。+0x236C 语义未定
    （validator FUN_140B980C0 拿它与 +0x2368 比较; FUN_142122410 每次调用还会把
    `+0x236C = *(playerParam+0x804)` 当"当前消耗"写入）。
  - **每招→槽位映射是代码硬编码**: 每招事件处理器形如 FUN_1417FDAE0: `param表 = func_0x14211C060(weapon)`,
    `FUN_142122F90(weapon, *(float*)(param表+0x82C))` — 调用点覆盖 **+0x808..0x834 共 12 个
    float 槽**（增量+消耗都在表里; w03p 文件里 MHW Editor 只给 5 个消耗起了名, 其余是 Unk）。
    w03p 文件偏移（user 截图: Blade1 Cost 0.15@1185 等）≠ 运行时偏移, 由加载器重排。
- **GAUGEEVT 钩子已实装 (mod build, HkGaugeAdd)**: detour FUN_142122410, 每次气刃变化记
  `GAUGEEVT +0x<callerRva> val=<增量> gauge=<前>-><后> cost=<0x804槽> own=<是否本地玩家> act=<set:id>`,
  40 行/s 限速。配置 `nativePC/plugins/ValorGauge.gauge-log.txt` = "<minAbs>"（默认 0.01,
  滤掉每帧 -0.000556 的 0x2374 衰减; 0 = 全记）。一局就能实测出完整"招式→增量/消耗"表。
  **后续改增量/加消耗/见切清空的实现点就在这个 detour 里**（改 value 再调 original 即可）。
  气刃锁实验保留: `ValorGauge.gauge-lock.txt` = "[target] value"（当前 `2 0` = 关,
  映射采数期间须关以免钳位污染数据; F5 游戏内切换）。
- **实测基线 (user 2026-09-24 局内确认)**: 3:79 突刺 +0.1 / 3:80 上挑 +0.1 /
  3:84,3:85 普斩 +0.2 / 3:11,3:67~3:70 气刃斩系消耗 / 3:87~3:90 兜割系。
- **气刃规则系统已实装 (mod build, HkGaugeAdd detour + DoActionHook)**:
  `nativePC/plugins/ValorGauge.gauge-rules.txt` 三种规则（仅本地玩家, set 3）:
  - `rewrite <set> <id> <delta> [origVal]` — 该动作期间的所有气刃事件改为 delta;
    带 origVal 时只改 ±0.005 内匹配的事件（多事件动作的精准改法）
  - `cost <set> <id> <amount>` — 动作开始（DoAction 接受后）扣 amount,
    给没有原生气刃事件的招式加消耗用; 走 g_originalGaugeAdd（clamp 安全）
  - `gate <set> <id> <minGauge> <nohitSec>` — 动作开始时气刃 ≥ minGauge 则
    注入 pool+0xE4 no-hit 窗口并清空气刃槽（见切配方）; 不足则什么都不发生。
  cost 先于 gate。注入助手 `InjectNoHitLocked` 已从 OpenCounterWindow 提取共用。
  文件当前全部注释（无行为变化）, 实测基线写在注释里。
  - **(2026-09-25 新增) `timer <set> <id> <seconds>` / `timerhit <set> <id> <seconds>`** —
    延长掉级倒计时 (weapon+0x2374), 与原版见切成功延长 (FUN_142123510 +0x2374 += param) 同一原语。
    timer = 动作开始生效 (空挥算); timerhit = 命中才生效 (DmgCalcHook 路径, 每跳叠加, 多段招多次加;
    不受 valor 充能门控影响)。`id 0` = 通配该 set 全部动作。秒数按当前等级的 w03p Gauge_Duration
    (playerParam+0x858/85C/860) 现场换算成 0..1 刻度, clamp 满级; 等级 0 不加。
    日志 `GAUGERULE timer[-hit] act=.. +Ns lv=.. dur=.. countdown=As -> Bs` (上限128条)。
- **rewrite 规则 user-verified 有效**: 突刺 +0.5 / 上挑改 -0.1（增益变消耗）都生效。
- **气刃 Regen 状态破解 (decompile 实锤 2026-09-24)**: 兜割后 `weapon+0x2378` = 持续秒
  （普通 10s/红刃 15s）, 小居合后 `weapon+0x2388` = 持续秒（普通 15s/掐点 30s, 经
  +0x2CED 掐点标志 → +0x2384 选档）; 每帧 updater 递减计时器并给 +0x2368 加
  `dt ÷ regenRate参数`（兜割 rate +0x840/+0x844 由色刃标志 +0x237C 选, 居合 rate
  +0x850/+0x854 由 +0x2384 选）, HUD 由 FUN_140617730 检测计时器 >0 切显示模式。
  **DLL 激活 = 直接写 +0x2378/+0x2388**。w03p regen 参数文件偏移+0x367=运行时偏移
  （file 1233→+0x838 ... file 1261→+0x854）; 同 skew 下 cost 表 file 0x4A1→+0x808..,
  decay 0.05=file 0x49D→+0x804, Unk120=file 0x499→+0x800（+0x236C 每帧衰减的分母）。
  ValorLS 当年写 +0x2388=90.0 之谜 = 激活此 regen 状态（旧"无敌帧"误判再次翻案）。
  **regen 规则已实装**: `regen <set> <id> <seconds> <variant>`（variant 0=兜割 +0x2378,
  1=居合 +0x2388）, 动作开始时只延长不缩短, updater 自动回气+切 HUD。
  **regen user-verified 有效**。回气量控制 (2026-09-24 补): `regen` 第 5 参 = rate 覆写
  （回气速度 = dt/rate参数, 下值快上值慢; 100000 = 只发亮基本不涨; 同时写该 variant 的
  两个色档参数 +0x840/+0x844 或 +0x850/+0x854, 对话期间有效）; 
  `option regen-freeze 1` = 回气期间气刃不降（GaugeAddHook 把负增量清零, 正增量保留）。
  修正: costSlot/regen rate 读取须双重解引用（player+0x7810 是指针, 之前读的是
  player+0x8014 的垃圾值）。
- 工具坑: `disasm_rva.py <rva> <len>` 第二参是**字节数**不是指令数（传 40 只出 ~10 条）。
- **气刃招式红光破解 (2026-09-24)**: user 交换 3:11(LMT104)/3:84(LMT103) 的动作+TIML 后
  红光不跟随 → 红光不在动作数据里。反编译定位: **红光特效 id 按"动作开始时的气刃等级"
  (weapon+0x2370) 选择, 代码硬编码**: 等级 0→0xC134 / 1→0xC135(白) / 2→0xC136(黄) /
  3→0xC137(红)。写入点两层: ① 每招事件处理器 FUN_1417FE1C0 往事件上下文 +0x30 写 id
  （该簇经函数指针表注册, 反编译无直接调用点）; ② 动作定义对象构造器
  （vtable 0x14337A040, 大小 0xCF8）把三连 id 写进动作对象 +0x19B/+0xCDC/+0x19C
  （基础系 {0xC132,33,34}, 另一系 {0xC135,36,37}）。**验证实验: 写 +0x2370=3 再放招,
  出红光 = 管线正常只差等级; 仍不出 = 自定义动作没接上气刃特效处理器层**。
  另: 刀身常驻光环是另一套 — 升级驱动（decompile 6760498 区）经 FUN_142110A90(weapon, id)
  写 weapon+0xAD4（100=白/0x66=黄/0x69=红; 0x67/0x6A=过渡/清除, 后者还会清 +0x2370）。
- **红光二次定位 (user 反馈: 0xC13x = 刀刃光随等级变色 ✓, 但"玩家身体红光"另有其主)**:
  身体特效候选 = **player+0x8808 通知系统**（纳刀研究同款 emitter）:
  `func_140AE0D70(emitter, 0x7D2/0x7DC, 0)`（门控: 动作 87/102 且等级≠0, 或 +0x2398）、
  `FUN_140ADD670(emitter, 0x7D7, 0)`（门控: +0x2398）。注意有等级门 — 交换实验无等级
  正好不出光。每招处理器尾部还有 FUN_141F755C0(player, yaw, param表+0x8AC, 2, 2) 待查。
  （player+0x1510 三连已排除 = 旋转四元数, 非特效 id。）
  **FX 试枪已实装 (mod build)**: `ValorGauge.fx-test.txt` = "<func> <id>"（func 0=ADD670,
  1=AE0D70; id 支持 0x 前缀; 默认 `0 0x7d2`）, **F7** 对本地玩家发射一次, **Shift+F7**
  重载配置。逐个试 0x7D2/0x7D7/0x7DC（红刃状态下）即可锁定红光特效 id。
  地址解析走 RVA + addresses.txt 覆盖（键 fxNotifyA=0xADD670 / fxNotifyB=0xAE0D70）。
- **红光一发定案 (user-verified 2026-09-24)**: `0x7D2` 经 ADD670 对玩家 emitter 发射 =
  **常规气刃斩身体红光**。`0x140AE0D70` 反汇编 = `jmp 0x140ADD670` 纯 thunk, func 参数
  无意义。试枪扫射结果: `0x7D3`/`0x7D4` 也是红光, `0x7D1`/`0x7D9` 无效。**蓝光改走记录路线:
  HkFxNotify 钩子已实装**（detour FUN_140ADD670, 第 12 个钩子; 签名 47 字节, 孪生
  0x201DCB0 靠 +36..46 的 movss/lea 尾巴区分; E8 rel32 和 rip disp32 已通配）——
  本地玩家的每次身体特效发射记 `FXNOTIFY id=0x.. arg=.. caller=+0x.. act=..`
  （同 id+arg+caller 0.5s 去重 + 30 行/s 上限; 非本地玩家 emitter 静默;
  caller > 0x4000000 = 试枪自身的垃圾 RVA, 已过滤）。
- **FXNOTIFY 全族实测 (user 2026-09-24 局内确认)**: `0x7D2`=气刃斩系通用红光
  （caller +0x2122559, FUN_142124F70 通用位; 3:11/67/68/70/87 共用）、**`0x7D3`=红光
  （user 二次确认, 非蓝光）**、`0x7D4`=居合音效+见切无敌帧受击音（FUN_142124F70 两分支,
  3:86/3:102）、`0x7D7`=**见切起手蓝光+音效**（FUN_1417FDAE0 尾部, 3:86 专属双发）、
  `0x7DC`=大居合专用（3:102）、`0x7D1/0x7D9` 无效。
  代码事实: 架势开始通知 `FUN_140AE08A0` 内部硬编码 `mov edx,0x7d3; call ADD670`,
  特效句柄存 emitter+0x1A40 保活, 架势结束 FUN_140AE10F0 检查同句柄移除 — 但 user
  实测 0x7D3 观感为红光, **特殊纳刀/架势蓝光的实际来源仍未钉死**（该次测试未产生
  任何 FXNOTIFY 行, 可能不走 ADD670）。用户裁定当前特效储备够用, 蓝光溯源停止。
  通知函数注意: `0x140AE0D70` = `jmp ADD670` 纯 thunk; 试枪发射的 caller 是我们 DLL 的
  垃圾 RVA（已在 FXNOTIFY 钩子里过滤）。
- **实验已实装 (mod build): 气刃量锁定** — 每帧写 Present 线程（游戏帧更新后）。
  配置 `nativePC/plugins/ValorGauge.gauge-lock.txt` = "[<target>] <value>"（target 1=+0x2374,
  2=+0x2368, 裸数字=target 1; 当前 `2 1.0`）, **F5** 游戏内切换。两字段恒被监视:
  `GAUGELOCK: lock=... | 0x2374 raw=... drift=N/2s | 0x2368 raw=... drift=N/2s`。
- **第一轮实测 (target=+0x2374): 条没有锁住 + 字段活的** — 游戏每帧写 0.999444（恒定,
  = 每帧 -0.000556 ≈ 1/1800, 60fps 下 30s 空周期）但我们写 1.0 后条照常掉 →
  **可见蓝条不是 +0x2374**（×100 getter 是另一根条）; +0x2374 更可能是等级持续时间
  （09-17 "线性慢漂移" 的原始观察因此恢复名誉）。主嫌疑转 **weapon+0x2368**（第二根条的
  getter、每招增益、消耗比较都在它上面）。
- **0x2368 双重语义解法 (指令级新证据)**: remapper 的架势检查是 `cmp byte ptr [rdi+0x2368],0`
  ——**按字节**。若 rdi=weapon, 0.999444(低字节 0x73) 会误判架势毁掉普通攻击 → 游戏没坏 →
  字节检查的对象不是 weapon（大概率 player+0x2368）。而架势生命周期 int 0/1/2 写在
  weapon+0x2368 dword 上 = **进架势把气刃 float 清零**（int 2 = float 2.8e-45 ≈0）, 语义自洽。
  → weapon+0x2368 就是气刃量本体, 架势 = "复用同 dword 做清零标记"。
  锁 target=2 的已知副作用: 锁定期架势每帧衰减检查 (==1/==2) 会被 1.0f 的 int 视图
  (0x3F800000) 绕过, 居合架势退出通知可能不触发（FSM 3:99 的 5s 超时应仍兜底）。

### 2026-09-25 收刀态输入路由: 纯代码+静态数据, 武器 FSM 完全不参与 (R2→3:11 定案)

- **问题**: 收刀态按 R2 打出 3:11 (拔刀气刃斩1), 这个切换能否用 FSM 编辑改道 (改打刚1)?
  **答案: 不能** — 收刀→拔刀动作路由不经过 wp03_action.fsm 的任何可编辑链路。
- 证据链 (全部 decompile/指令级):
  1. `wp03_action.fsm` 没有任何收刀态内容: 待機(1000) = 0入0出 parked 节点; 收刀移动
     1:0/1:4/1:8 **没有节点** — 收刀 locomotion 根本不在这个 FSM 里。持刀 R2 进节点1
     (ActionNo 1001) 靠的是待機２(1012)的 `R` 链接条件, 收刀态没有对应物。
  2. 重映射器 FUN_14118c320 对 id 11 (0xB) **透传**: LS case 只在居合架势 (0x2368!=0)
     重映射 raw id 0→100, 1→101, 2→102, 3→103, 4→104, 0x12-0x22→0x69..0x7d/0x43,
     0x35/36→0xa0/0xa1, 0x8d→FUN_14211f040 分档; 另 id 7 查 weapon+0x2369,
     id 0x5e(94)→0x5f(95)。**收刀态按 R2 产生的 {3,11} 在重映射器之前就已生成**。
  3. 武器类型 switch 的 dispatch: 0x14118C5B6 `mov eax,[rdi+0x9F8]; dec eax;
     cmp eax,0xA; ja passthrough; jmp [table 0x14118D098 + eax*4]` —
     **反编译 case N = 武器类型 N+1** (修正 09-17 "case 2 是别的武器" 的错误警告,
     case 2 就是 LS 的居合架势重映射)。武器类型枚举此表按 1-based 索引。
- **每武器 set-3 请求表 (分类器的数据面, "upstream producer" 的大致形态)**:
  - FSM 管理器初始化 FUN_14118dca0/ddc0: set-1 注册重映射回调 FUN_14118c320 (IB 态
    player+0x7626!=0 时换 FUN_14118d3a0); **set-3 注册 per-weapon 数据**:
    `row = FUN_142165600(weaponType)` = `type*0x130 + 0x143FD5590`,
    `FUN_14026ae00(obj, 3, *(row+0x48), *(u32)(row+0x50))` 把静态表拷进
    obj+(3+8)*0x10 = mgrCtx+0xAB0 (obj = mgrCtx+0xA00)。
  - LS 行 (0x143FD5920): +0x48 = **0x143F4D6A0** (113 条), +0x50 = 113。
    静态条目布局疑似 0x28B {u64 key, ptr 子表, u64 数量, pad×2} (keys 0-4 实测:
    0/4 共享子表 0x143EF3970×14, 1/3 共享 0x143EF3C10×18, 2 = 0x143EF3DC0×15,
    5 = 0x143EF43A0×7...), 但 FUN_14026ae00 的内存拷贝循环按 **0x20** 步长 —
    静态 0x28 vs 拷贝 0x20 矛盾未决 (可能反编译步长有误, 待指令级核对)。
  - 消费链: FUN_14026b670 (请求 {set,key} → 数组[key] 条目, 逐候选) →
    FUN_14026b830 (候选 0x18B 6×i32 解析, 特判 -0x14(-20)/-10/-1 标记;
    -10 = 居合斬り開始标记!) → FUN_14026b590 (最终 {set,id} 经
    func_0x140269ce0 = DoAction 发出)。子表候选里出现 240/291/196/13/278 等 id,
    语义未解码 — **想纯数据改写收刀 R2 行为, 从 0x143F4D6A0 这张表入手**。
- **实用结论 (收刀态打刚1 的路线)**:
  - FSM 编辑不可行 (无链路可改)。
  - **DLL 改写 — 已实装 (2026-09-25, mod build, md5 5fec7f00a4815013d2b16df6478630e5)**:
    DoActionHook 内、tracker 拷贝之前, `ApplySheathRewrite` 把收刀态进来的
    {3,fromId} 改写为 {3,toId}。门控链: 本地玩家 (锁 + 未锁时 manager chain 判定,
    首次按 R2 即生效) → fromIds → holdVK (可选按键门) → weapon+0x9F8 == 3 →
    **收刀判定 = tracker 里上一个动作是 set 1** (1:0/1:4/1:8/1:5 收刀 locomotion 族;
    持刀态 locomotion 不发请求) → **红刃 weapon+0x2370 >= reqLevel** →
    **气刃量 weapon+0x2368 float > minGauge** (user 规则: 红刃且余量 > 0.15 才允许
    拔刀刚1)。不满足则放行原版 {3,11} 拔刀气刃斩1, 拒绝原因记
    `SHEATHREWRITE reject` / `SHEATHPROBE` (各上限12条)。命中记
    `SHEATHREWRITE: 3:11 -> 3:86`。改写在 tracker 之前 → clang-latch 重臂/日志/
    gauge 规则看到的都是改写后的 id。
  - **无敌帧 (2026-09-25 二轮)**: 改写生效后动作出来了但没无敌帧 — 原生防御窗口
    走 TIML/FSM 状态链, 收刀态 FSM 停在待機(1000) 不进节点28, 窗口没人打开。
    解法: 改写命中同一帧调 `InjectNoHitLocked(nohitSec)` 注入 pool+0xE4 no-hit
    (与叮触发同一通道), 配置第5参 nohitSec 默认 0.2s (与见切 TIML 编写的窗口一致),
    <=0 关闭。**注意: 持刀态的见切叮触发链 (bit15→no-hit+auto-force) 是否会在
    收刀路径照常工作未验证 — 若 bit15 不置位, 收刀刚1 只有这 0.2s 窗口。**
  - **第一版失效复盘 (log 实证)**: 收刀按 R2 的请求确实是 {3,11} (tracker 日志多次
    `1:5/1:0 → 3:11`), 但无任何 SHEATHREWRITE 行 → 静默门控 = **weapon+0xC71 收刀态
    读到 0**。+0xC71 的极性/语义从未局内验证过 (一直标"待钉") — 已弃用, 改用
    tracker set-1 判定; SHEATHPROBE 行继续打印 c71 实测值, 供钉死其真实语义。
  - 配置 `nativePC/plugins/ValorGauge.sheath-rewrite.txt` =
    "<toId> <holdVK> <reqLevel> <minGauge> <nohitSec> <fromId> [fromId...]", 默认
    `86 0 3 0.15 0.2 11` (toId 0=关, holdVK 0=无按键门, reqLevel 0=不验等级,
    minGauge<0=不验气刃量, nohitSec<=0=不注入)。文件缺失 = 默认开启。
  - 注意: 见切(86) 动作从持刀姿势起手, 收刀冷播有姿势跳变 (刚1 为移植动作时需要
    收刀起手版); {3,86} 能否让 FSM 从待機(1000) 经 request-ActionNo 匹配进节点 28
    未实测 (不进也照播, 98/99/100/103 无节点前例, 只是派生链接断)。
  - 居合架势态 R2 的 raw id 是 1/2 (remap 成 101/102), 与收刀态的 11 不冲突,
    改写规则天然不误伤。
  - 顺手修复: counter-auto.txt / prop-trigger.txt 的 `nativePC\p...` 单反斜杠
    路径 bug 再次出现 (C4129, 配置从未加载) — 已改为正斜杠。
  - **事故复盘 (2026-09-25, user 局内确认): gauge-rules 的 `gate 3 86` 会杀死整个
    见切反击链。** 机理: gate 在动作第 0 帧注入 pool+0xE4 全量 no-hit — 该排除在
    命中查询层、比防御仲裁更上游 → 怪的攻击到不了仲裁 → 无弹反交互 → 无叮 →
    event bit15 永不置位 → mod 叮触发链 (反击窗口+追加no-hit+auto-force刚1) 全灭,
    且每次见切清空气刃槽 (log: `GAUGERULE gate act=3:86 ... gauge cleared` ×10+)。
    **定案: pool+0xE4 no-hit = "游戏层面你没被攻击过" (09-18 翻滚结论的另一面);
    帧杀保护与弹反计数互斥。叮类招式 (见切/居合) 的保护必须后置于叮 (bit15 先行,
    注入殿后 — mod 叮触发器的既有顺序)。** gate 规则只适用于无弹反呈现的自定义招。
    rules 文件示例行的 `#gate 3 86 0.1 0.5` 已换成 3:87 并加警告。

### 2026-09-25 (2) 气刃等级自动下降 + 气刃量自动衰减全链路 (decompile+w03p模板实锤, 站桩秒表未做)

- **总更新器 = FUN_1421237B0 (RVA 0x21237B0, 纳刀研究同款每帧函数)**。两套衰减独立:
  - **等级计时器 = weapon+0x2374 (float 0..1)**: 每帧 `- dt' × (1/Duration[level]) × (1.0/FUN_1411c27c0(mgr))`,
    dt' = `*(*(player+0x998)+0x68)`。Duration 按 0x2370 选 1/2/3 档 = playerParam+0x858/0x85C/0x860
    = **w03p `Gauge_Duration_White/Yellow/Red`** (用户冰原版实测 195/135/60; 文件偏移 1265/1269/1273,
    skew +871 ✓)。倍率函数 FUN_1411c27c0 = 属性 0x35 → 表 `_DAT_145011f68+0x5CC+(idx-1)*4`, 默认 1.0。
    门控: event bit0 (player+0x8938 &1, 全部气刃增减共用此门) **且 +0x2390 ≤ 0**。
    **+0x2390 = 升级后免衰减倒计时** (初值来自 w03p `Gauge_No_Decay_Time_after_Spirit_Gauge_Upgrade`,
    playerParam+0x884; 每帧在 bit0 下减 dt', 归零钳 0 → 衰减开始)。
  - **到 0 处理 (updater 内)**: level≥2 → level-1 + 计时器写 1.0 (回满) + FUN_140ae1630 通知;
    **level<2 (白) → level 直接归 0 且 +0x2370..+0x2374 一起清零 (undefined8 写)**。
  - **气刃量 (可见蓝条) = weapon+0x2368**: +0x236C 累计器以 `dt'/Spirit_Gauge_Decay(playerParam+0x800)` 速率扣,
    过 0 时蓝条一步扣 `param+0x804` (=Blade1_Cost 槽, FUN_142122410 每次气刃事件写 +0x236C=+0x804
    所以攻击会重置待扣批量) 并重置累计器 — **净速率 = 1/Spirit_Gauge_Decay 每秒, 与 +0x804 无关**。
    回气状态 (+0x2378 兜割/+0x2388 居合 >0) 时改为 `+0x2368 += dt'/rate(+0x840/844 或 +0x850/854)`, 不衰减。
- **刷新路径 (计时器重置/延长)**:
  - 升级/掉级事件都回填: 掉级事件处理器 **FUN_141822790 (DropLevel, 函数指针注册无直接调用点)**:
    level-1 后按新等级回填 — 白←playerParam+0x9AC, 黄←+0x9B4, 归0→清零; 另有兜割/居合的
    **等级消耗 handler FUN_142122E40**: `param+0x864 (w03p "Gauge_Consumption_for_Helm_Plus_Iai_Spirit_Slash",
    用户实测=1) <= level` 时扣该数值等级并写 +0x2374=1.0; f20/f50 两个变体, f50 在掐点 (+0x2CED) 时跳过。
  - **见切成功 = weapon+0x2399 锁存** (FUN_142125060: set3 且动作 86=0x56 时置位, 发 FX 0x7D4,
    同时写 +0x23A0 = playerParam+0x8A8 = w03p "Foresight_Slash_Super_Armor_Time_on_Successful_Evasion"
    — 09-17 的蓝光/闪白探测器字段就是它, 且 +0x23A0/+0x23A4 >0 期间每帧发 event bit 0x97(151)=超强呈现)。
    动作结束处理器 **FUN_142123510** 消费 (+0x239C 一次性锁存, 动作开始处理器清 +0x239C):
    +0x2399 置位 → **FUN_142122410(+1.0) 气刃量回满 + +0x2374 += playerParam+0x888 (延长, clamp 0..1)**。
    该函数还管: 动作 101(小居合)→+0x2388 回气档 (掐点←+0x2CED→+0x84C, 否则保底 +0x848);
    89-92(兜割系)→+0x2380 = 等级快照; 动作 69 且 bit0 → +0x2CED=1。
  - **居合弹反 handler FUN_142124F70**: set3 且动作 ∈{101,102} → +0x2CED=1 (掐点) + FX 0x7D4 +
    +0x23A4 = playerParam+0x8B8 (w03p "Iai_Super_Armor_on_Successful_Counter", 与 +0x8A8 恰差 0x10 ✓)。
  - 见切整条消耗: 每招 wrapper FUN_142122F90 内, 动作 86 时 `+0x2368 -= 1.0` (见切花掉全部气刃量)。
  - 大重置 FUN_142124590: 清 0x2368-0x237C/0x2388-0x23A4/0x2399, +0x238C=1.0, +0x23C4=7 (收刀/换区/重置)。
- **数值核对**: 09-24 GAUGELOCK 实测 +0x2374 每帧 -0.000556 (=1/1800) — 若当时红档且 60fps,
  则红档 30 壁秒见底 = 参数 60 的一半 (该时钟 30Hz / 参数单位≈2壁秒); 也可能另有 ×2 修正。
  **站桩秒表一测即知** (红→黄多少秒)。模板字段名→文件偏移: Blade1_Cost@1185, Gauge_Duration*@1265-1273,
  Consumption@1277, Damage_Buff*@1281-1289, NoDecay@~1309, 见切延长 Unk_125@~1313 (前四组用户截图核对 ✓)。
  **注意**: 本地 chunk-extracted/G0 与 nativePC 两份 w03p 内容相同且全文件搜不到 195/135/60/15/30/0.15
  — 编辑器里看的是冰原版数据, 改 w03p 前确认导出到 nativePC 的是编辑器那份。
- **mod 相关**: sheath-rewrite 的红刃门 (+0x2370>=3) 会被等级自动下降在 30~97s 后打断 (红→黄) —
  需要长效红刃可每帧写 +0x2374=1 或写大 +0x2390; 09-24 gauge-lock target 1 = 锁等级计时器 = 永不掉级。
- **(2026-09-25 晚) GS 崩溃取证 + 规则系统硬门控**: 持大剑 (weapon+0x9F8=0, GS 原始类型=0) 挥砍必崩,
  WER 两次崩溃同偏移 `MonsterHunterWorld.exe+0x232E245` (0xC0000005) — 游戏代码非本 DLL。
  崩溃指令 `cmp [rdx+0xB0],0` (rdx=野指针容器), 该处 = 辅助函数 0x14232E240 ("取容器第 index 个校验条目",
  0x18 步长 + +0xC&0xE 校验, 仅经 lea 函数指针引用无直接调用点); 调用方 FUN_140ADE270 (FX notify 家族,
  读玩家 +0x8938 event bit2 门控), 容器来自 FUN_140ADECC0 消息查询应答。GS 会话日志止于
  `FXNOTIFY id=0x7d0 caller=+0xae042b` (0x7D0 = GS 专属特效, LS 从未见)。GS 会话本 DLL 零写入:
  全部 gameplay 路径被 ModWeaponActive 关死 (GATE CLOSED 贯穿全程), 无 HITATT (空挥未命中),
  timerhit/timer 规则不可能执行。ADD670 经反汇编确认只读 rcx/edx/r8d 三参 — FXNOTIFY 3参转发无罪。
  结论: 非本次 timer 改动所致; 待 DLL 移除 A/B 定案 (vanilla GS 特效路径 vs 旧钩子交互)。
  **顺手修掉真地雷: ApplyActionGaugeRules 原本无武器门控** — 规则 id 与 GS 动作 id 撞号 (或通配)
  会对 GS 武器结构体跑 LS 专属 gauge 原生函数/偏移 (越界写)。现 ApplyActionGaugeRules 与
  ExtendLevelDropTimer 均有 weapon+0x9F8==3 硬检查 (ca10904, 部署 md5 c0fb69de)。
- **(2026-09-25 深夜) GS 蓄力崩溃 ROOT CAUSE + 修复 (54cc5d2, 部署 md5 3ac107ff, user-verified)**:
  user A/B 定案 (DLL 在就崩/不在就好) + 关键症状 (仅蓄力时崩, 蓄力有红光) → **FxNotifyHook 把
  FUN_140ADD670 的返回值丢了** (钩子声明为 void)。反汇编实锤 ADD670 结尾 `mov rax,rbx; ret` —
  返回创建的特效句柄; 太刀调用点 (0x7D2/0x7D3/0x7D7 一次性特效) 不消费返回值所以几周没炸,
  GS 蓄力红光 (0x7D0, caller +0xae042b) 的调用点继续使用该句柄 → 垃圾 rax → 野指针 →
  0x232E245 `cmp [rdx+0xB0],0` 崩溃。**修复: FxNotifyHook/g_originalFxNotify 改 void* 转发句柄;
  同类隐患 GaugeAddHook/g_originalGaugeAdd 改 float 转发 (FUN_142122410 在 xmm0 返回钳位值);
  g_fxNotifyA/B typedef 同步 void*。教训: 每个 detour 必须核对原函数的返回值寄存器 (rax/xmm0),
  声明成 void 丢返回值是隐形炸弹, 症状只在"调用方消费返回值"的路径上爆发。**
  **user-verified**: 修复后 GS 蓄力不再崩溃; 且 F7 试枪可直接调出大剑蓄力红光。
  **FX id 按"当前手持武器"分表解析 (user 实测更正)**: 同一 id 0x7D0 — 大剑在手 = 蓄力红光,
  太刀在手 = 普通气刃特效。特效库所有 id 都必须标注"哪个武器表下"; 跨武器借特效 (如太刀出
  大剑蓄力光) 靠改 id 做不到, 得走别的路 (其他 notify 函数/特效资源层)。0x7D0 记录:
  **FX id 按"当前手持武器"分表解析 (user 实测更正)**: 同一 id 0x7D0 — 大剑在手 = 蓄力红光,
  太刀在手 = 普通气刃特效。特效库所有 id 都必须标注"哪个武器表下"; 跨武器借特效 (如太刀出
  大剑蓄力光) 靠改 id 做不到, 得走别的路 (其他 notify 函数/特效资源层)。0x7D0 记录:
  蓄力红光(大剑表) / 普通气刃(太刀表)。

### 2026-09-26 动作帧控制全图: ActionController/动作实例 + 起始帧跳转 (decompile 实锤, FRAMETICK 局内验证待做)

- **问题: 能否让动作从第 n 帧开始播?** 结论: **没有原生"带起始帧播放"API** — begin 硬编码清零
  帧计数, 唯一路径 = begin 后直接写 `instance+8`。已实装 (mod build, 部署 md5 c9991130, 含 Shift+F6 热重载)。
- **ActionController = player+0x61C8** (已知 player+0x6274/0x6278 就是它的 +0xAC/+0xB0)。
  全字段图 (decompile: DoAction 0x140269CE0 是个小函数, 更新器 FUN_141f78600 / FUN_14026a820):
  - `+0x68+set*0x10` = instArray 指针, `+(set+7)*0x10` = 该 set 实例数; 实例按 (set,id)
    **预分配全局唯一** (池 FUN_140269c30, 注册表见下)。
  - `+0xA8` = phase (0 idle / 1 请求已接受 / 2 运行中); `+0xAC/+0xB0` 当前 set/id;
    `+0xB4/+0xB8` pending; `+0xBC/+0xC0` 缓冲请求 (DoAction 在 phase!=0 时写这里);
    `+0xC4/+0xC8` 前一动作, `+0xD0` 前一实例 core, `+0xD8` 前一动作结束帧 (只写不读);
    `+0x6A8` (=player+0x6870) = **动作自动结束阈值 float** (<0 关; frame 达到即 end+switch);
    `+0x6AC/0x6AD/0x6AF` 结束标志。
  - DoAction 全逻辑: 校验 set 范围+实例存在 → phase==0: 直写 +0xAC/+0xB0, phase=1;
    否则缓冲 +0xBC/+0xC0。真正的 begin 发生在每帧更新器里 (a820: phase==1 → 收尾旧动作
    a660/begin 新动作 a1e0, phase=2)。
  - **动作实例**: `+8` = **帧 float** (begin 清零; 每帧 `frame += dt`); `+0xC` = 当前 dt;
    `+0x10` = core 指针 (0xCD0 字节每动作运行时对象, 工厂 FUN_1417fada0 族);
    `+0x18` = finished byte; `+0x28` = player。
  - getter 族 (全 thunk, 简单字段读): 0x141bff720/740/760 = {set,id} 指针 / id / set,
    stride 0x6B0 = **控制器槽步长** (slot0=player+0x61C8, slot1=player+0x6878);
    0x14026a420/450/490 = 当前实例 / core / 帧。
  - 实例 vtable: +0x30 = **begin** (见切类 = FUN_1417FDAE0 — 就是笔记里的"见切起手处理器",
    内部: gauge wrapper → 通用 begin FUN_141783470: 存 yaw、发 event bit 0x63、vtable+0x288);
    +0x38 = phase/finish 检查; +0x40 = end (FUN_14026a290); 旧动作收尾走 +0x48 (FUN_14026a660)。
- **每武器动作类表** (FUN_141f5a410 换武器时注册): 行 = FUN_142165600(type) =
  0x143FD5590+type*0x130; LS 行 0x143FD5920: `+0x40` = **set-3 类表 0x142EF5C30** (113 项,
  与请求表 0x143F4D6A0 同一 count 字段 +0x50), `+0x70` = slot1 类表 0x142EF75C0 (8 项)。
  表项 stride 0x20: {id u32, set u32, classPtr, pad}。**每动作一个类**, 类对象在
  .data 0x144FExxxx 区; **类名 ASCII 就嵌在 .rdata 里 vtable 后面**
  ("nHmAction::Wp03:..." 家族; vtable 0x143382be0+0x20 起即名字) — 可整表枚举动作类名。
  set-1 locomotion 类表 per 武器: FUN_141bfcf80(player,1,0x142e6c640,0x138,0) 等 14 处调用。
- **帧控制实装 (mod build)**: `force-action.txt` 第 4 参 `startFrame` (>=0 启用)。force 被
  DoAction 接受后自动武装: **FRAMETICK** 诊断前 24 tick 记 `instance+8/+0xC` (每次强制动作都
  记, 不启用 seek 也先跑 F6 验证帧计数语义 — 预期 0,dt,2dt... 递增), **FRAMESEEK** 在
  控制器 phase==2 且 frame<4 (begin 刚跑完) 时一次性写 `instance+8 = startFrame`。
  走 property-getter 钩子 (游戏线程)。日志: `FRAMETICK act=.. phase=.. frame=.. dt=..` /
  `FRAMESEEK act=.. frame a -> b` / `FRAMEWATCH done|timeout`。
- **警告**: 前向跳帧会**跳过中间 TIML 事件** (事件按帧越过触发): 碰撞窗/音效/FX 若在
  0..n 之间就不会发生 — 见切 (弹反窗 f0-f12) 跳过 12 就废掉弹反; 无弹反呈现的招随便跳。
  motion 是否完全跟随写入值 = FRAMETICK/局内第一验证目标 (若骨架不动, 说明 motion 采样
  另有时间源, 需再挖 motion 对象 weapon+0x468/0x470 链)。
- **首轮 FRAMETICK 实测 (2026-09-26, user F6 两次 3:104)**: `dt = 0.03333 = 1/30` —
  **instance+8 单位是秒 (30Hz 逻辑帧)**, TIML 60fps 帧 n ↔ n/60 秒; force 后第一轮读到的
  0.166667 是该实例**上次运行的残值** (begin 还没跑), 之后 0.0333 = begin 清零后第一帧 ✓
  清零语义确认。property 钩子每游戏帧触发 ~10 次, FRAMETICK 已改"值变化才记" (否则 24 条
  配额 2 帧烧完)。`endAt` (控制器 +0x6A8 自动结束阈值) 已加进 FRAMETICK 行。
- **二轮实测 (user "无效" 定位): 单次写在 begin 前会被洗掉** — 日志: FRAMESEEK 写 0.667 →
  读回 0.667 → 下一帧 0.033 (begin 清零重开)。时序: begin 在控制器更新里, **晚于我们钩子的
  首个匹配 tick**; force 后首帧读到的 0.133/0.166/0.633 残值 = 该实例上一轮运行的遗留
  (DoAction 走缓冲路径时 current 不变、phase 也不变 — phase 不能用来判断 begin 已跑)。
  修复: 改为 "帧回落到 ≤2dt (新 begin 证据) 后写, 写后每 tick 钉住, 直到时间线超过目标
  半个 dt 才 settle"; 动作中途切换则放弃。endAt(=控制器+0x6A8) 实测 3:104 期间恒 -1
  (自动结束阈值未启用, 动作寿命由动作类自己管)。部署 md5 3d086837。
  **三轮实测: 钉住成功 (settled act=3:81 frame=0.700, 2 writes 自愈 ✓) 但画面无跳转 —
  instance+8 只是逻辑/记账时间线, 骨架动画另有时钟。已加 CLOCKMINER: 强制动作期间自动扫描
  两个区域找"每帧恰好±dt"的 float — A=动作 core (instance+0x10, 0xCD0), B=动画组件
  (*(player+0x468), 0x200; prime suspect anim+0x10C, 原生 getter 0x141bff780 读它做
  "步进未完成"判断), FRAMETICK 行新增 mot= 字段, 第 28 tick 报告 CLK A/B 命中偏移。
  部署 md5 1df8bae5。v1 空手而归 (anim 区无 ±dt 字段; instance+0x10 core 实测不可读 —
  "core" 不在 +0x10)。v2 (md5 a6357ca8): 改"任意恒定步长"检测 (motion 时钟可能是 TIML
  帧数 +2.0/tick, 不是秒), 扩区域: core/anim468(0x400)/anim470/weapon(0x2450, TIML 采样
  目的地+0xA50/+0x23C0 在 weapon 上)/playerA(0x400-0x900)/playerTail(0x1D600)/inst(0x60)。
  平表扫描: 动作id→LMT entry 的 u16/u32 平表不存在, 类对象里也无 LMT 字段 — LMT 选择逻辑
  在代码/类工厂深处。**但 mot= 字段直接命中**: **MOTION 时钟 = *(float*)(*(player+0x468)+0x10C)**,
  计 TIML 帧数 (+2.0/逻辑tick, 60/s), 动作/motion 每次 (重)启动归零, 骨架采样吃它 —
  instance+8 纯逻辑线, 写它画面不动 (v2 实测 mot=62 纹丝不动)。原生 getter 0x141bff780
  (=anim+0x10C) 的"步进未完成"判断语义吻合。FRAMESEEK v3 (md5 92343850) = 双写 instance+8
  (秒) + mot (秒×60 TIML 帧), 同套 fresh-begin+钉住+settle 逻辑。
  **USER-VERIFIED 2026-09-26: 画面跳帧成功** (3:81 目标 0.8s: `frame 0.033->0.8 mot 0->48`
  → `settled frame=0.833 mot=50`, 局内确认从中段起手)。CLK 报告: anim468 上有 5 个同步
  +2/帧 时钟: +0x10C(已写)/+0x110/+0x124/+0x128/+0x22C (疑似动画层/混合同步时钟, 只写
  +0x10C 即可视觉生效, 其余待用); inst+0x8 (step=dt) 交叉验证。v2 挖矿报告 bug 教训:
  属性钩子 ~10次/帧, "tick 28" 只等于 2-3 个逻辑帧 → 恒定步长命中数永远不够阈值 —
  已改为每个逻辑帧只挖一次 (frame 值变化才挖)。v2 挖矿报告 bug 教训:
  属性钩子 ~10次/帧, "tick 28" 只等于 2-3 个逻辑帧 → 恒定步长命中数永远不够阈值 —
  已改为每个逻辑帧只挖一次 (frame 值变化才挖)。
- **Shift+F6 = force-action.txt 热重载**
- **Shift+F6 = force-action.txt 热重载** (原本只在插件 init 读一次; Shift+F7 是 fx-test 的,
  别搞混)。热重载会连带重载 weapon-type/hit-charge/gauge-lock/gauge-log/gauge-rules/
  sheath-rewrite 子配置。startFrame 参数单位 = 秒。
- 工具坑: disasm_rva.py 第一参是 **RVA 不是 VA** (传 0x1417fada0 会 "not mapped")。
- **更新恢复文档**: 本章全套(控制器/实例布局、motion 时钟、类表、动态恢复总路线)
  已按锚点/步骤/验证/写回格式收录 `Reverse/ADDRESS_RECOVERY.md` 第 7 章
  (原第 7 章顺延为第 8 章); FRAMESEEK 不新增钩子地址, 会过期的只有结构偏移。

### 2026-09-26 (2) spiritreckoning（居合开刃）逆向: 轮询+缓冲槽写入的动作衔接 (定案: 整体无改进价值, 可借两个原语)

- **文件身份**: `Reverse/spiritreckoning.dll.c` = `.bak-20260901` 旧版的反编译
  (指纹: 0xc06d 立即数只在 bak 的轮询线程里); 交付版
  `MHW-MHRS-migrate/spirit-reckoning/nativePC/plugins/spiritreckoning.dll` 是重编新版,
  机制相同、偏移已更新 (0x2CEC→0x2CED, 新增读 0x2370 等级/0x2D24, anim 标记换 0xC132)。
  DLL 内部名 `居合开刃.dll`, 导出 `?Load@@YA_NXZ` (Stracker 插件约定)。
  `MHW-MHRS-migrate/居合开刃源码/` 工程**不是**它的源码 (无对应偏移, 是另一模板工程)。
- **机制 = 后台轮询线程, 不是 hook**: `Load()` 起两个线程 — FUN_18000c7d0 (按键监视,
  GetAsyncKeyState WASD+配置VK → DAT_180064e2c 移动计时) 和 FUN_1800134f0 (**动作链轮询**:
  先 Sleep(10s) 等加载, 然后 5ms 一圈, QPC 高精度睡眠 FUN_180009840)。
  配置在 `nativePC/plugins/Reckoning/key_config.json` (热键 VK)。
- **三个信息源**: ① 当前动作 = `player+0x6274/+0x6278` (我们已知的 controller+0xAC/+0xB0);
  ② **动作时钟 = `*(float*)(*(player+0x468)+0x10C)`** (就是我们 09-26 破译的 motion 时钟,
  TIML 帧 60fps) 跟**手调帧阈值**比 (5/60/70/78/87/110/117/134/200/206 硬编码);
  ③ 动画状态标记 = `*(int*)(anim+0xE9C4)`: 迁移招自定义标记 0xc06c/0xc06d,
  原生气刃系 0xc132/0xc138。**没有"动作完成"事件/回调, 纯帧数轮询硬衔接**。
- **衔接原语 = 写控制器缓冲请求槽**: `*(int*)(player+0x6284)=set; *(int*)(player+0x6288)=id`
  = controller+0xBC/+0xC0 — **正是 DoAction 忙时缓冲请求用的同一个槽** (09-26 控制器全图),
  游戏更新器在当前动作可派生的帧自己消费 → 后招在原生衔接帧播出, 观感如原生派生。
  旧版 104 链: anim 标记 0xc06c 且 motion≤5f (起手) 武装 pending {3,0x68};
  current==3:0x69 (105 在放) → 写 {3,0x68} 进缓冲槽 → 105 结束自动接 104。
  其余链段: 3:0x45 到 200f→写{3,1}或{3,0}; 3:0x6b 到 78f→{3,7}; 3:7 到 134f→{3,0};
  全是"到帧数就写缓冲槽"。注意 0x6284 的 qword 写 = {set,id} 一次写双字。
- **唯一一次函数调用**: 0xc06d 招 motion>70f 且 weapon+0x2CEC==1 (motion<70f 时清锁存)
  → `call [rip→槽0x630B0]`, rcx=**weapon**。槽值烧死 **0x1421211F0** (旧版) /
  交付版槽 0x5E0A0 烧死 **0x142121190** (当前版本, 反汇编显示写 weapon+0x23A8 一带,
  在太刀气刃状态簇 0x212xxxx, 邻接 FUN_142122410/FUN_142123510)。
  全局槽 (旧0x63060/新0x5E058) 烧死 **0x145011760** = 玩家链根 (`[它]+0x50`=player)。
  **两个槽都没有重定位表项** → 版本锁死, 跨更新必死, 与旧 ValorLS 硬调 0x142134B70 同款死法。
  (PE 分析工具向: bak .data 在 RVA 0x63000 起, 交付版在 0x5E000; rip-rel 引用暴力扫描
  时别忘了加 TEXT_VA 基址, 否则目标全偏 0x1000。)
- **定案 (user 判定): 整体无改进价值** — 5ms 轮询 + 手调帧阈值 + 烧死地址, 比现有
  钩子体系糙。可借用的只有两个原语: ① **写 controller+0xBC/+0xC0 缓冲槽排队后招** —
  我们没用过的原语, 比 force-DoAction 更贴原生 (游戏自己挑衔接帧/走正常取消窗口),
  刚1 链或 Valor 派生链可试用, 放游戏线程钩子里写比它的 5ms 旁路线程稳;
  ② motion 时钟 ≥ 阈值即"完成"判定 (帧÷60=秒), FRAMESEEK 场景可直接复用。

### 2026-09-27 MHGU→MHW 动画迁移链调通(mhgu-valor;坑目录见 valor-remap-pipeline/mhgu/README.md)

- 工作文件 `mhgu-valor-mhw.blend`(= MHW-Valor 副本,MHRS 155 动作保留在 MASTER 未动);
  脚本 `valor-remap-pipeline/mhgu/0~7`。数据事实:MHGU LMT 单位=厘米(÷100);重心骨=GLB `0:0`,
  其 locY ≡ 原生 106 MhBone_000 locY(0.4713/0.4714 同源铁证);地面位移在 -1 Root 轨道(LMT)/
  `reference.loc`(GLB),轴映射 obj[1]=−rawZ;w07::106 与 w03_00::106 仅重心通道同源,肢体不同。
- 局内"缩成一团"= **导出轨迹缺失**(15 条,只有 IK+根),不是 IK 数值错:FreeKinetics 按原生
  entry 轨迹布局逐轨导出,remap 直接接 entry 会被 omitted。修复=`7_merge_into_native.py`
  原位合并进原生 entry(luocha 路线);配 4-bit 帧增量 ≤15 切分 + frame0/末帧键 + 排序后切分
  (三 bug 曾叠加:未排序数组 gap 算出负数静默跳过切分)。
- 其余已钉:ARP 前必须抽走源动作的 object 曲线(否则地面位移污染重心位置);000 永不写旋转
  (0:0 旋转走腰 013,按用户 bmap 原样);0:0 loc 逐字拷贝到 000(轴恒等);object 位移直通不镜像;
  场景 frame_end 污染 ARP 批量长度 + remap 丢 freehk 元数据 → 钉范围+裁剪+拷贝;
  fill 拷 106 曲线要按目标长度裁剪;根管线 plw_ 过滤会空跑(验证必须断言处理数量);
  v67 解析器偶发丢 entry(重解析并集重试);LMT 导入 channelNames 第一元素才是真路径。
- **2026-09-27 局内验证通过(最终):entry 103(樱花 sa::151)+ 104(刚1 w07::192)=
  自烘焙 IK(米)+ 6_tag 全量打标 + 导出器自动 m→cm = 正常,脚手跟随迁移动作本身**。
  (此前"自烘焙 IK 缩成一团"的真正原因 = 打标缺失导致导出省略身体轨迹 + IK 位置
  以米冒充厘米差 100 倍;全量打标后导出器自动转 cm,自烘焙 IK 可用,
  PRESERVE_NATIVE_IK=1 仅作后备。证据链:w03_00-wrongik.lmt。)全部 25 个 remap 已全量打标+min-2-keys,可直接导出。
  待办:其余招式槽位映射(MERGE_PAIRS 批量 7_merge)+ 绝对回避定位(不在 w07_sa 8 动作内)。
- **2026-09-28 "缩成一团"复发定案(已重新部署, md5 c375f133, 待局内复测)**: 部署文件
  entry104 = 79 轨 + 缺 (-1,3)/(-1,4) 根位移轨 + IK/重心米值(249loc[0]=(-0.38,0.10,0.32))
  = wrongik 同签名。**根因(受控实验复现,逐值一致) = `LMT::w07::192_remap` 缺
  `freehk.tetherFrame` 绑定**(ARP 新建动作不设它;原生导入的 103/104 = m_mesh Armature)。
  导出器 LMTActionParser 的 tether 取自动作的 freehk.tetherFrame 而非场景: tether=None →
  `restoreRootFromObject` 不跑(Root 两轨没了) + 骨架逆变换跳过(米值 float 直写)。
  曲线级打标救不了动作级绑定。**修复 = `act.freehk.tetherFrame = m_mesh Armature` 一行**;
  6_tag 已内置绑定+verify 断言。补绑后 remap 直连导出 = 81 轨 cm ✓ 与管线产物逐值一致
  (remap 缺 MhBone_252 旋转 → 81 vs 82 轨,9-27 同配方局内已验证无影响)。
  和名字/接哪个动作无关,只认 tetherFrame。一键复跑+五文件对比: `mhgu/_rerun_and_compare.py`。
  坏文件留证: `nativePC/.../w03_00-curled-20260928-2000.lmt`。
  **下一主任务(2026-09-28 用户定): 武器位置** — 游戏内刀位 vs Blender 预览不一致
  (见 mhgu/README.md 已知问题: 030/047 挂点常量偏移/游戏挂点数学/scale-Y 插值待钉);
  逆向侧已有基础: 2026-09-20 纳刀合并机制章节(挂点表控制器 +0xAF0/+0xFA0/+0x1450、
  行表 [表+0x4A8]+row*0xC0、vtable+0x110 写 ctrl+0x181)。
  **(2026-09-28 晚续) 刚1 游戏全程左手持刀定案**: 用户原版对照(103/104/105/107/108/
  110/111 一致, 换手的 109/113 也一致)排除游戏数学偏移; 实测 192_remap 的 030.scale-Y
  恒 2 / 047.scale-Y 恒 1 → 状态对从未写入。**原版换手参照配方**: 109/113 的 030/047
  scale = bufType 5 + 同帧成对键瞬时台阶(109: 047 f69=1→f70=2→f128=1, 030 f69=2→
  f158=1); 补阶梯按此打标, 勿用 104 的 keyless bufType=1。
  **fcurve 归组**: 9_group_channels.py 批量归组(组名=骨骼名, object=Object; 两 blend
  已跑: 61378+67462 条), 7_merge 重建时自带归组不再丢。
  **(2026-09-29) 刚1 换手不生效真因闭环**: 用户写好 5 键阶梯后导出, 部署文件 entry104
  的 030/047 scale = **0 键 keyless 轨, basis 与原版 104 常量逐字节相同**(basis=阶梯首键)
  — keyed+bufType1 被导出器压回 keyless 常量, 键全丢。**修复 = scale 六条 fcurve 重打
  LIMITS min_x=5**; 6_tag 配方已改 scale 行从原版 109 读 5(否则每次重跑又打回 1)。
  语义: 030.Y=2→左插座持有 / 047.Y=2→右插座持有(109 右手段两者同 2, 047 优先), 1=闲置;
  原版 104 本身就是常量左插座(对它自己的动作正确)。**✅ 2026-09-29 局内验证通过(用户
  确认)**: 重打 bufType=5 后刚1 游戏内左→右→左完全正常; 部署实测 030/047 = bufType 5
  成对键台阶(10/11 换右, 44/45 换回, 60/60 收尾)。另: MASTER 的 LMT::w03_00::104
  已不存在(疑似被孤儿清理), entry104 节点现直连 192_remap(tetherFrame 已绑, 导出
  root✓/cm✓); 重跑 MASTER 上的 7_merge 会因缺原生动作 FAIL(WORK 文件仍有)。

### 2026-10-01 用户手搓动作接入规则化: 10_apply_rules_divide.py + 无头接线验证工具链 (mhgu README 同步)

- 用户在 MASTER (MHW-Valor.blend) 直接新增自定义动作 (divide_long 63f / divide_short 57f /
  left_up·rigit_up·right_down_before_divide 各 6f; 拼写 rigit 是动作真名), 全部左手握持
  + 带前后左右根位移, ARP 原始烘培态 (871 fcurves, tether=None/frameCount=-1/starType=None,
  000 带恒等旋转缩放, 030/047 被 ARP 烘成逐帧)。`mhgu/10_apply_rules_divide.py` 一次套完全套
  已验证规则 (坑⑬ tether + 坑⑥ frameCount/starType + object 层整形 + 030/047=106 左手常量
  + 000 清理 + 6_tag v2 打标 + min-2-keys), 5 动作 871→314 fcurves, 全绿保存 (.bak 留档)。
  **复用: `RULE_ACTIONS="动作名1,动作名2" blender -b MHW-Valor.blend --python
  mhgu/10_apply_rules_divide.py`**, 后续新动作同流程。
- 导出器级验证: LMTActionParser (导出同代码路径) valid=True ×5, 82 变换轨满额, Root 双轨;
  警告签名与局内验证过的 151/192_remap 同族 (UNEXPORTABLE 7 = object 层设计性忽略,
  INVALID_ENCODING_NONKEY ≈43/44 = 常量轨自动降级, 两边一致 = 背景噪声)。
- **divide_short→entry103 无头接线+正式导出成功** (mhgu-export-repro.lmt, 82 轨, frameCount
  57, Root 双轨, 030/047 = keyless 常量左手 — 与原版 104 常量左插座同形; **纯常量左手动作的
  scale 就该是 keyless bufType1, bufType5 只对真有换手阶梯键的动作必需**)。
- **坑⑭ (README): 无头改完 blend, 开着的 GUI 会话必须 File>Revert 重载后再接线导出** —
  旧会话拿内存里的旧动作导出, 崩 `bounds() ValueError: Vector(): sequence size is 0`
  (精确条件 = Root 轨恰好 2 键/非 Root 恰 1 键且 buffered, 正常管线产物出不了该状态);
  用户实测重启即好。entry 的 LMT Animation 输入只能留一条连线, 旧连线不删会被
  `next(iter(actionNodes))` 取到旧动作。
- 无头工具链 (mhgu/): `_preflight_tree.py` 全树 154 动作预检 (problems 必须=0) →
  `_verify_exporter_divide.py` 单动作 parser 预检 (-- 传名) → `_repro_wiring.py`
  无头接线+正式导出 (ENTRY/ACTION/OUT, 不保存 blend) → `_check_repro_entry.py`
  产物解析 (-- 传 lmt+entryId)。
- 存盘 MASTER 的树: entry103←103_mhworig, 104←104_mhworig, 140←192_remap, 101/102←原生。

### 2026-10-01 (2) 勇气纳刀三招迁入 MASTER + MHGU 增量迁移配方固化 (mhgu README 同步)

- **`LMT::w07::195`(松开收刀)/`196`(保持按下构,无位移)/`095`(受击打断) 已全链迁入
  MASTER (MHW-Valor.blend)**, 源 = 用户给的 `E:\MHW-arts\mhgu.blend` (84 个 w07/w07_sa
  导入动作, 全部 0 引用 — **该文件永不可保存, 只许作 append 源**, 一存 0 引用动作全灭)。
- **增量链固化 (mhgu/README "增量迁移配方" 节)**: `_append_sources.py` (append+补
  mhgu_source+fake_user) → 2_arp_remap `MHGU_ONLY=` 每动作一次 → 3_fill/3b (幂等) →
  **`_4_object_root_motion_new.py` (外科版, 原版 ONE-SHOT 守卫会拒绝增量)** →
  3_rebake_ik (原脚本 verify 段 `frame_set(float)` 必崩, 已修 `int(min(f,maxk))`) →
  4_grip → 6_tag → 8_min2keys → `_transfer_to_master.py` (**append 不搬运
  freehk.tetherFrame 指针, 进 MASTER 后必须重绑**, 坑⑬的 append 变体) →
  `_verify_exporter_divide.py` + `_fidelity_check.py` (保真抽查, 3 招 max 1mm)。
- 落盘形态 (MASTER 实测): 81 变换轨/Root 双轨/tether 绑/frameCount=143/169/121/
  030=106 左手常量 (scaleY=2)/047 scaleY=1/000+247-253 全长/object 位移 (195 后撤
  -1.44m, 196 静止, 095 前移 +4.38m); 警告码 7/50/2/43 = 已验证同族背景噪声。
  parser 预检 valid=True ×3。**挂点态用户确认三招全左手 → 030/047 左手常量 = 终态,
  无需精调。**
- 待办: 三招收刀系槽位接线 (候选 5/6/9/10, 09-20 定案), `_repro_wiring.py` 可直接用。

### 2026-09-30 motion 选择机制破译: motion id = LMT entry | 0xC000 (动作层硬编码, FSM 无关; 填补 "LMT 选择逻辑在类工厂深处" 的开放项)

- **动作实例 +0x30 = motion id, 打包公式 `entry & 0x1FF | 0xC000`** (cJumpSlashLanding
  begin 反汇编级实锤; 0xFFFF = 不换 motion 哨兵, 见 FUN_141783710)。begin handler
  (实例 vtable+0x30) 经 **玩家 vtable+0x288(player, motionId, ...)** 启动 motion;
  部分变体走 +0x290/+0x298 (带混合参数)。之前 anim+0xE9C4 读到的 0xc06c/0xc132
  标记就是这个空间的 id (0xC132 = 特殊納刀 LMT306 ✓ 旁证)。
- **id→LMT entry 没有平表**: 每动作类的工厂 (类 vtable+8/+0x10, 构造 0x50 小实例或
  0xCD0/0xCD8 大实例) 在函数体里**硬编码立即数**写 motion id (小实例 +0x30 直写;
  大实例写 +0xCD0, begin 时 FUN_1417fdd30 式拷到 +0x30)。提取: 类表 0x142EF5C30
  (113 项, stride 0x20, **classPtr 在 +8**) → classPtr 首.qword = 类 vtable → vtable+8/
  +0x10 工厂 → 函数体搜 0xC000-0xC7FF u32。批量扫描 17 已知对 6/17 直命中
  (79→105/81→102/83→127/86→140(+170)/87→170/92→174); 共享类 (67/77 同 classPtr)
  的 motion 由 begin/运行时决定, 工厂扫描不适用。
- **运行时 motion 重映射行表**: cJumpSlashLanding begin 遍历实例 +0x48 行数组
  (4×i32: {前一动作set, 前一动作id, 新LMT entry, 触发帧}, 数量在 +0xCC8, 运行时
  由排队方填充, 类模板不含), 命中条件 = 前一动作匹配 **且 motion 时钟
  (anim+0x10C) == 行帧** → 播 `row[2]&0x1FF|0xC000`。0x141881xxx 有一个排队
  {3,11}/{3,12} 后逐个盖章 +0xCD0=0xC09C 的编排器实例 (某架势下 11/12 改播 156)。
- **实锤案例 = LMT 156 = ジャンプ気刃２連斬り（二段目）**: action **77** (用户局内
  3:77 见到; begin 0x1417FE600 = `mov [rcx+0x30],0xC09C; jmp 通用begin`) 和 **76**
  (共用类 cJumpSlashLanding) 开场直播 156; **73/74** begin (0x1417FE5A0) 播 155
  (一段目, 其 TIML ノードID f0=48→f25=60 在 f25 自切二段命中节点); 另有两个
  per-frame helper 监视 "当前 motion==0xC09B && vtable+0x2A8()" → 衔接 0xC09C。
  **76/77/73/74 全部无 FSM 节点** (同 98/99/100/103, 纯动作层 id, 靠请求/链数据
  进入)。col 交叉验证: TIML CollisionTimelineObject ノードID → wp03.col names 表
  node 列 = 招式日文名 (node 60 = ジャンプ気刃2連斬り二段目 ✓)。
- 陷阱记录: ① 跨武器撞号 — weapon type 8 (斩斧) id 114 的工厂也写 0xC09C, 那是
  斩斧自己 LMT 的 156, motion-id 立即数必须按武器区段归属; ② 类名探测要从 vtable
  末尾对齐, 共享类 vtable 会让"vtable+0x20 起即名字"猜偏 (67/77 同 vtable);
  ③ mhgu 迁移链的 "LMT entry 104 = MHGU 192" 用的正是这套实例 +0x30 直读, 所以
  改 LMT entry 内容即可换动作, 无需动代码。
- 事故快照 (2026-09-30): `nativePC/hm/wp/wp03/mot/w03_00/` 目录整体缺失 (含部署的
  MHGU 迁移版 LMT) — 用户局内跑的是包内原版数据; 是有意清理还是误删待确认,
  部署迁移 LMT 前先补目录。

### 2026-09-30 (2) 刚1 双弹反定案: force 链三缺陷 + tracker 失步 (已修复, 部署 md5 9e45890d)

- **症状**: 刚1弹反触发后短时间内按 Ctrl → 弹反再触发一次; 且刚1 (3:77) 期间体感有无敌帧残留
  (刚1 本体应无任何无敌帧)。用户先验: FSM 写的就是"刚1后不接弹反", 故怀疑 DLL — 正确。
- **日志铁证** (ValorGauge.log ~613867): 一次叮 → `COUNTER WINDOW OPEN (deflect flash)` →
  FORCE 3:77 → **~4 帧后 `COUNTER WINDOW OPEN (negation bit 250)` → 第二次 FORCE 3:77**,
  且第二次仍打 `interrupting 3:86`; FRAMETICK 帧数归零 = 刚1从头重播。三连
  `COUNTER INPUT` (614027/38/49) = 0.6s 窗内每次 Ctrl 边沿都再武装。
- **缺陷1 (主犯) force 绕过钩子 → tracker 永远停在 3:86**: TryQueuedForceWrite 直调
  g_originalDoAction (trampoline), DoActionHook 不跑, g_lastLongSwordAction 不更新 →
  `inEvasionNow`/`ActionIsForesightSlash()` (只认 3:86) 在刚1全程为 true, 三个 negation
  检测器 (flash/bit250/bit15) 整个刚1期间仍在"见切门内"站岗。
- **缺陷2 刚1播放中 bit250 再升起 → 重新开窗+重新武装**: 3:77 自己的动作 TIML 事件键
  (per-motion 事件同 id 不同义, 09-18 翻滚教训同款) 在刚1开始 2-4 帧内置位 bit250,
  被当成"见切弹反"→ 第二次 FORCE。另: flash 和 bit250 本就对同一次叮先后各触发一次,
  arm/consume 错开 → 单次叮天然双发。
- **缺陷3 反击窗 fire 后不清零 + Ctrl 边沿无动作门控**: OpenCounterWindow 只写
  g_counterWindowUntil; 0.6s 内任何 Ctrl 上升沿都再武装。
- **修复 (dllmain.cpp, 5 处)**: ① TryQueuedForceWrite 成功后同步 tracker = {3, 刚1id} +
  置 g_clangLatch + 打 `Long Sword action changed (forced)` 日志; ② fire 成功即消费窗口
  (g_counterWindowUntil 清零); ③ 三个触发点接线 `&& !g_clangLatch` (注释
  "one force per 见切 action" 写了从未生效 — 只有 VALOR_PROBE 的 SoundPostHook 读过;
  DoActionHook 仅在进入 86/101/102 时清锁存, 恰好是想要的每见切一次性); ④ CondEvalHook
  (631 判真无条件武装, 无动作门无 autoForce 门) 补 `g_counterAutoForce && !g_clangLatch`
  (本次日志无 COND631 行, 属休眠路径); ⑤ 窗口日志带 `nohit=<秒>` 便于局内核对注入状态。
- **残留无敌判定**: 叮触发链当前**无注入** (部署 force-action.txt = `3 77 0 0`, 第三参
  invulnSec=0 → OpenCounterWindow 的 pool+0xE4 写入被门掉; 默认值 0.5 只在缺参时生效)。
  ClangSoundVirtualHook (2188) 是死代码 (InstallClangSoundVirtualHook 无调用点)。
  故体感残留最可能是**见切自身 TIML b26 窗口 (f0-f12, 0.2s) 穿过 mid-action 强制切换漏进
  刚1** (游戏侧, force 绕 FSM 的副作用) — 修完双发后局内复测: 若仍残留, 下一个杠杆是把
  force 推迟到见切弹反窗 (f12 = 0.2s) 之后触发。
- **杂项**: `ValorGauge.counter-action.txt` (内容 "86") 无代码读取 = 死配置;
  `counter-repeat.txt` (Ctrl 脉冲) 功能已不在现源码中, 配置文件同为遗留。
  上一版部署 md5 92343850 → 本版 9e45890d32f6b863ada58f76ef258df4。

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

### 2026-10-01 特殊纳刀系(98/306 + 100/312)无敌帧 + 受击切换触发链(局内验证待做)

- **TIML 已改**(游戏目录 w03_00.lmt,`.pre-iframes.bak` 快照;幂等脚本
  `apply_sheathe_iframes.py`):entry 306(动作 98 特殊纳刀)mFlag f0/f10/f15
  OR b26 全程窗口 + mTrigger f0=2048;entry 312(动作 100)mFlag f0=0x05000000
  (b26+b24 全程)+ **结构化插入** PlMotionCommon mTrigger 轨 {f0=2048}
  (文件尾 16 对齐追加 keys/transforms 数组 + 类型槽 {toff,tcount} 重指针;
  types/transforms/keys 数组先验证 entry 独占才动手)。
  **坑⑮:blend 里 306/312 的 entry 节点接了 TIMLDataNode(TIML Data 输入),
  exportNodeToFile inject = 整个 entry 替换 → 导出会把 TIML 打回原版;
  凡导出动过这两个 entry,重跑 apply_sheathe_iframes.py**。blend 侧根治
  (TIML 在 blend 是 action fcurve,u32 拆高低 16 位,见 timl_importer.py
  TIMLFlagImport)= 待办。工具坑:patch_timl_defense.py 只能 dump/move/setval,
  插键/建轨要结构化脚本;0x06400000 的十进制是 104857600(手算十进制必错,
  用 python 算)。动作链按用户定案:98(特殊纳刀)→99(入鞘状态,无独立 LMT
  映射)→100(LMT312);action-list.md 里"99→306 特殊纳刀"是旧行,以 98 为准。
- **受击切换 = sheath-counter 系统**(mod build,部署 md5 48d4f78705e1e7d0623b866c794bdc8a):
  见切/居合的 bit15/bit250 处理器按动作 id 门控(86/101/102),98/99/100 没有原生
  处理器 → "无敌帧中被打到"的信号未知(可能像翻滚一样完全静默,见 09-18)。
  `ValorGauge.sheath-counter.txt`:`mine <ids>` = 期间挖 event-bit 数组
  (player+0x8938..8 dwords)边沿,日志 `IFRMINE evN 0x.. -> 0x.. rose=[bit..] act=3:x`;
  **F5 = "被打到了"手动标记**(与 gauge-lock 共用 F5 边沿,lock 当前关不冲突);
  `rule <ids> <eventBit> <toSet> <toId>` = bit 上升沿 → counter 窗口 +
  no-hit(g_forceInvulnSec)+ force DoAction(先写 g_forceSet/g_forceId 再 arm,
  见切同款"先叮后注入"顺序)。默认文件只开 mine,rule 注释待挖矿结果。
  若挖矿全静默(翻滚同款)→ 下一步钩 TIML no-hit 碰撞拒绝点(FUN_141f66dd0 上游)。
- **(2026-10-01 下午)首局挖矿 + 分层定案**:TIML b26 = 查询层排除(与翻滚同类,
  零事件零表现,用户局内确认无敌但不叮);见切=仲裁层弹反(b22 窗口+id 门控 handler)。
  bit250 置位点全在伤害结算函数内(FUN_141217e50/14121ea10),查询层排除到不了;
  pool 查询 FUN_140aee690 type8 = no-hit 检查但调用方全是 FSM 属性 getter。
  **mod 版新装 HitDispatch 钩子(SHEATHDISPATCH,mine ids+86 对照)**:98/100 无敌帧
  中被命中若日志出现 type-6 non-0xD 无伤害 → 启用配置 `drule`(已带注释行)=
  纯无敌帧受击切换;若空 → 走超甲窗路线(b24+DmgCalcHook,代价挨伤害)。
  新部署 md5 待游戏关闭后覆盖(构建产物就绪,旧版 48d4f787 被游戏锁定)。
- **(2026-10-01 晚)SHEATHDISPATCH 零输出 = 钩子 bug 修正**:当前源码的分发器
  日志格式从未产出过(vtable+8 类型镜像永远≠6 → 入口全提前返回),旧的 133 条
  HITDISPATCH(code=0x6B)是类型镜像加入前的"[log only]"老代码产物;"clang =
  non-0xD + 无伤害"语义仍有效。修复(部署 md5 ab2d10039ba8a5344a41ed211e30ecbd):
  去掉类型门控,0xD 洪泛排除,non-0xD 记 SHEATHDISPATCH,drule 触发同步。
  判读要点:老笔记记录 non-0xD 在窗口开着时连空挥都出现 → 下局必须带空挥对照组,
  hit/whiff 的 code 值无差别则转超甲窗方案。用户 MARK 落点 3:99/3:0(链后段),
  下次尽量在 98/100 本体动作里标。
- **(2026-10-01 深夜)分发器判读完毕 + 方案定案**:修好的钩子实测——98 的分发器
  爆发确定性(每次恰好 33 行,空挥一样,负 float 计时器漂移),三次见切弹反零输出
  (老"clang=non-0xD"证据不复现)→ 分发器不可判别,路线关闭。用户定案选
  **超甲尾窗方案**(否决怪物出招触发/手动触发;另提案"见切 FSM 播不同 LMT"留后):
  306 f15 回退原值 = 无敌 f0-f14 + 霸体 f15+(apply_sheathe_iframes.py 已改幂等,
  修了 set_trigger_f0 返回值双义 bug);mod 加 `arule <ids> <toSet> <toId>`
  (DmgCalcHook: 霸体段命中→HP 钳制保底 1 + counter 窗口 + force 切换,配置已启用
  目标 3:77)。HP 链钉死:***(float*)(*(player+0x7630)+0x64)***(+0x60 疑似 MaxHP);
  DmgCalc 出参是分类 out 不可钳伤害。部署 md5 84ef445a7a1b5b2321fb8c7200a7a9e7。
  待验:钳制vs死亡判定时序、a2 防御方钉定(日志前 8 条 SHEADRULE hit)、多人误触。
- **(2026-10-01 深夜2)b31 = 防御/弹开窗位(重大)**:用户回忆突刺(105)当年加过
  防御帧有音效;仓库根 105.txt = 当年配方 dump:f0=0x87400006(b31+b26+b25+b22),
  原版突刺无 b31/b26 → **b31(0x80000000)= 防御窗,叮类机制 TIML 可写**(见切叮窗
  = b31+b22;307/308 无 b31 故叮靠 handler)。当前 105 已被导出冲回原版(配方存
  105.txt)。已应用:306 f15=0x87400006(照抄配方)= 无敌头 f0-f14 + 防御尾到末,
  arule 保留兜底(防御被拒→命中照连→arule 触发)。待局内验证:纳刀家系是否允许
  防御帧、防御音效、以及防御交互在 mod 侧的可测信号(FXNOTIFY/SHEATHDISPATCH/bit15)。
- **(2026-10-01 深夜3)用户规则: b26 罩死 b31**——无敌帧与防御帧同窗时只有无敌
  生效(b26 查询层先吃掉命中,防御交互不发生)。第一轮 105/306 都用 b31+b26 同窗
  (0x87400006)故无声。已改:105 f0 与 306 f15 → **0x82400006(b31+b25+b22,无
  b26)**,306 无敌头 f0-f14 不动与防御尾无重叠;脚本已同步。105.txt 是含 b26 的
  后-state,非当初工作态。判读:105 有音=配方成立;306 有音=纳刀允许防御(再看
  可测信号);306 无音=家系封死 → 见切 FSM 播不同 LMT 上位。
- **(2026-10-01 深夜4)★bit28 = 格挡位(定案,修正 b31 误判)★**:用户提供历史
  会话——全武器 LMT 扫描:**mFlag bit28(0x10000000)= 格挡有效**(CB/SnS/GL/GS
  有,太刀家系无;守备架势 = 单键 f0 整段 0x10000000 或 |bit31)。当年实验:突刺
  105 f0=0x12400006(bit28+b25+b22,无无敌)→ 正面格挡成立(音效/削伤/掉耐力/
  不硬直),背对照常掉血。**b31 非防御位**(深夜2/3 的推断作废,守备架势里只是
  伴随位)。引擎细节:方向=朝向防御锥;后果链=格挡成功分支统一;伤害分类写反应
  管理器 +0x769=0x100/0x101,仲裁 +0x768/769/76A → FUN_1411cac40(格挡反应装填);
  格挡参数 weapon+0x8A0/0x8A4(守备时长)+0x8A8 三组等级表(太刀缺块则 DLL 补)。
  叮+蓝光候选评估器表通用(复用居合槽位或 DLL 置属性 0x35 族即白得演出)。
  已应用:105 f0 与 **306 f15 → 0x12400006**(306=无敌头 f0-f14+格挡尾,无重叠,
  脚本同步幂等验证)。判读:105 正面格挡=配方成立;306 格挡=家系允许→钉切换信号;
  306 无 105 有=家系差异;都无=LS 缺 +0x8A0 块→DLL 补参数。
- **(2026-10-01 深夜5)★闭环: bit28 整段格挡成立 + bit250 受击信号 + 3:37 揭底★**:
  306 三键全 0x12400006(整段单值格挡)→ 格挡音效成立!此前"全程无敌"= f0/f10 的
  b26 键毒化整段(按动作生效非按键窗;b26 多键布局在 306 不可用,防御用单值
  bit28)。**3:37 = LS 格挡反应动作**(格挡成功→FUN_1411cac40 装填→动作换成 37;
  太刀原生不格挡故从未见过;"预知被打马上换动作"之谜 = 同一现象,非 DLL bug)。
  **bit250 在 3:98 上升(IFRMINE 实录)= 受击检测信号到手**。已接线:
  `rule 98,99,100 250 3 77`(sheath-counter.txt,Shift+F6 生效)——格挡被击 →
  counter 窗口 + force 77;格挡链不走 DmgCalc(arule 静默 ✓)。竞态:引擎先装
  37,rule 延迟 1-2 帧切走(37 闪 1-2 帧;介意→钩 FUN_1411cac40 空转抑制)。
  306 终态:三键 0x12400006 + mTrigger 2048@f0;无敌头+格挡尾的多键布局放弃
  (b26 毒化整段,锁存机制未挖);apply_sheathe_iframes.py 目标值待定案同步
  (重跑会洗回头尾布局,实验期勿跑)。
- **(2026-10-02) 强化射击(特殊射击) TIML: 纯表现层定案 (user-verified, TIML 路线死)**:
  签名 (vanilla) = `PlMotionCommon.mTrigger` **`f34=1024(bit10)+f36=1(bit0)` 成对点火
  通知** (332/338/341/344/347 加 f0=2048 无敌门变体); `mSlingerShellAttach 3→0@f36→3`
  弹壳分离同帧; 家族 = LMT 332~349 共 15 entry 全 134f 键位一致 (296/297 = @f9/12
  101f 变体); 306~311 的孤立 1024 语义不同勿混。类表: **id 43/44/45=cKijin1ComboShot
  Base/F/R, 46/47/48/51=Kijin2 Base/F/L/R, 49/50/98=Kijin3 Base/F/L,
  81=cActWpOnSlingerShot**。104 能接射击却无任何射击专属键 = 接受门不在 TIML。
  实验: 105 突刺塞触发对 (sharekeys.py 重指向 338) → 只有画面震动; 341 全套 TIML
  复制到 105 → 投石音效+震动, **无伤害无投射物**。定案: TIML=纯表现 (音/震/弹壳
  动画), 点火/伤害/输入接受全在 cKijinNComboShot 动作类代码侧。改造路线归一:
  ① motion-swap 托管 43~51/98 槽; ② DLL combo-force (duringIds+VK→force {3,43}族)。
  工具 `research/mhw/shot_weave/`: analyze.py (scan/diff/windows) + sharekeys.py
  (transform 头重指向他 entry 键数组, 零字节增长; 正式方案应复制数组)。
  **附带: 部署版 341 丢整个 PlMotionCommon (7轨) = blend 导出管线写残** (全表审计
  仅此一处, 141/142/143 为管线增益; 修复需 blend 侧; 导出后跑 analyze.py diff 审计;
  手改部署 LMT 会被下次导出冲掉)。

---

*Last Updated: 2026-10-01*
*Current Milestone: 2026-09-26: 动作帧控制定案 — 动作实例 player 控制器(player+0x61C8) instArray+id*8 的 +8 float 就是动作帧 (begin 清零, 每帧+=dt), 无原生"起始帧"API; FRAMESEEK 已实装 (force-action.txt 第4参 startFrame 单位=秒[30Hz], Shift+F6 热重载, phase==2 时一次性写帧, FRAMETICK 诊断随每次 force 记录[值变化才记], 部署 md5 92343850; motion 时钟 = *(player+0x468)+0x10C (TIML帧制) 已破译, v3 双写 USER-VERIFIED: 从任意帧起手 ✓). 2026-09-25: 收刀拔刀刚1 已实装 (sheath-rewrite: 收刀态{3,11}→{3,86}, 门控=红刃0x2370>=3 且 气刃量0x2368>0.15, 配置 ValorGauge.sheath-rewrite.txt, 第一版因 weapon+0xC71 收刀态读0而失效(该字段语义待钉,已弃用), 收刀判定改用 tracker set-1 前趋动作; 现版 md5 7fe2c220, 待局内验证). 收刀态输入路由定案 — 收刀按R2产生的{3,11}是代码+静态表路由, 武器FSM完全不参与 (待機1000零链路; 重映射器对id11透传; 每武器set-3请求表 0x143FD5590+type*0x130, LS 113条@0x143F4D6A0, 消费链 b670→b830→b590→DoAction; 想纯数据改收刀R2从这张表入手; DLL改写路线 = DIRREWRITE加 收刀态{3,11}→{3,86}, 门控weapon+0xC71). 修正: remapper反编译case N=武器类型N+1 (dispatch有dec eax), case 2就是LS.  气刃量定案 — weapon+0x2374 float 0..1 (HUD getter FUN_140616d60 ×100; DropLevel 从 player+0x7810 +0x9AC/+0x9B4 回填; 直接写此 float 即可配置, 锁满需每帧写). weapon+0x2368 双重语义未决 (居合架势 int 0/1/2 vs 0..1 float 累加器, FUN_140b980c0 用它+0x236C 做动作重定向) — 未经 A/B 不要当气刃量覆盖. 2026-09-20: 纳刀合并机制实锤 — 动作id驱动挂点表切换状态机 (收刀系槽位 5/6/9/10 复用定案; 详见 Sheath_Research 文末). 刚1 counter-parry COMPLETE + mod/probe SPLIT VERIFIED. 2026-09-19: FSM drive chain mapped — zero-inbound nodes (居合 101/102, rolls) are entered by request-ActionNo match; iai chain = ほぼ全攻撃(1013)→居合斬り開始(-10 marker)→特殊纳刀架势 weapon+0x2368 (FUN_14211cc60)→remapper FUN_14118c320 request-id remap (0→100,1→101,2→102...)→DoAction 0x140269CE0→node. Corrections: 0x2368 = stance flag not selector; 763/764 remaps are set-1. FSM map tool: dump_fsm_xml.py → wp03_fsm_map.md. ValorGauge.dll = LS-only mod, triggers: 见切 bit15 / 居合 bit250 / deflect flash / condEval-631 → no-hit + auto-force; BGM bug RESOLVED by the split (user-verified). ValorProbe.dll = passive research build, never auto-installed. Backlog: kenki-gate via 練気ゲージ判定 (0x274/0x275), gauge UI polish, 大居合(102) request-id-2 upstream trigger, +0x2368 float/int 交错实测, 命中充电字段钉死*
- **(2026-10-01 深夜6)★方案 B 落地: motion-swap 工厂补丁(用户验证)★**:mod 初始化
  扫描 LS set-3 类表(0x2EF5C30)对每类工厂扫 motion id 立即数并改写——配置
  `ValorGauge.motion-swap.txt`="140 141" → 见切(86)播放 entry 141 的动作,原生
  弹反/arm/FSM 全保留(用户局内确认)。bluesim 阴性定案(+0x2399/0x23A0 族全是
  aftermath);池计时器假设证伪(POOLDIFF 13×86 零痕迹);+0x768=居合开场旗。
  141(用户克隆的见切基底)TIML 已修 b31 窗并入脚本;**脚本工作流实战验证**:
  用户导出冲掉全部 blend 侧 entry → 一键重打 ✓。当前布局:306/312=见切形窗口
  (实验态)、105=bit28 格挡、141=b31 窗。设计待定:见切决策点(受击→反击/超时→
  收刀)的 decide 规则、98 终态防御、306/312 终态。部署 md5 0c4782455fede412696fbdad8d2493f3。

### 2026-10-02/03 勇气太刀攻击窗/输入窗 + 强化射击摘除定案(CTO=伤害窗实锤)

- **伤害窗 = `CollisionTimelineObject`(CTO) mFlag,终审定案**:参考包
  `nativePC -xfsm+rs`(SR 集成版,注意其 `w03_00.lmt` 即工作参考)entry 339 =
  能打的一文字,CTO 窗 [23,34] == 用户肉眼观察伤害帧 [24,34] 一帧不差;其 EC 窗
  [30,36) 根本不覆盖 [24,29] → **EC(`nWwiseTimeline::EventCollision00`)与伤害时机无关**
  (请求/音效状态机;早前"EC=伤害窗"的旧笔记作废)。`mTrigger` 1024+1 点火对 =
  投石器弹本体。332–349 强化射击族原版**没有 CTO** → 刀刃部分从无数据侧伤害窗,
  伤害全靠投射物——"换 TIML 射击消失同时伤害消失"的完整解释。
- **强化射击摘除配方**(339/340/343 实测:能打、无射击):保留家族骨架(mTrigger 轨、
  PlMotionInput、EventGroup01/02/03/06),清零:点火对(f34=1024+f36=1)、
  mSlingerShellAttach(dt=0!)、mClawGimmickState、mCameraShakeIndex、EventGroup09。
  SR 的 rs338/339/340 同配方(它保留点火对因为要留射击)。
- **CTO 移植** = 字节级克隆参考 339 的 CTO:双配置 ノードID[0]=59(気刃3二段/bank0)+
  ノードID[1]=72(仲間当たり),mFlag 窗口帧改用户值。node 63(見切り攻撃/bank8)
  疑似带反击态门控(中段空洞来源),弃用。**每个 target 的 CTO 是独立追加副本**
  (types 数组复制到 EOF + typeCount+1 + 指针重排)。
- **动作↔entry 路由定案**:动作 103–106 工厂三连 motion {338,340,339},107–109 →
  {341,343,342};类=通用 begin(三连按 player+0x82C8 三选一+行表),伤害不经类代码。
  FSM 节点 66–75 = 気刃スリ(強化射撃)状态(动作 104–112);用户 FSM 的 69/70/71
  已更名 一文字短/长/无双斩(动作 106/105/108)——与参考作者同款占位思路。
- **窗口分配(用户更正:曾弄反)**:动作 105 = entry 340(126f)= 一文字**长** [29,34];
  动作 106 = entry 339(114f)= 一文字**短** [17,22];343 无双斩 [31,39]+[58,70];
  EC 组别一律 v2(原版带伤害的 EC 窗全是 v2,v1=音效槽);140 刚1 EC[45,55]/CTO[43,58]。
- **崩游戏三连(全修,知识库级教训)**:
  1. col 被插入 3 字节 → ATK 全表错位 → 开游戏崩。鉴别:禁用 LMT 能进 = LMT 无罪。
     col 每次改完必须做长度+结构校验(MHW Editor 保存/装包拷贝都会动它)。
  2. TIML 键帧数组乱序 → 进存档崩。leftover 键清零后必须重铺帧保持严格递增。
  3. CTO 追加**必须先把 types 数组复制到 EOF 再 typeCount+1**——否则第 9 个类型项
     覆盖相邻 transform 数组(add_cto.py 有这步;srcto2 活是因为原地改已有 CTO;
     apply_valor_attack_config 首版踩坑 = 进存档崩)。
  4. 小数键帧(24.4/25.9)疑似崩溃源 → 一律整数帧(原版全整数)。
- **`apply_valor_attack_config.py` = 导出后一键配置,幂等**(断言:键帧排序/整数帧/
  捐赠窗数/CTO 存在性,失败拒绝写盘):射击摘除 + EC v2 窗 + SR 双配置 CTO +
  140 刚1 + INPUT_PLAN 输入窗。**每次 blend 导出后跑一次**;改帧改 PLAN/INPUT_PLAN 表。
- **`PlMotionInput` = 输入接受窗**(用户删 TIML 后中断全灭的机制实锤)。位域解码:
  b8=翻滚接受、b31+b17=特殊派生窗(见切→必殺攻撃)、b14/b16/b30=连段预输入、
  清零键=显式关门。见切 140 原版=限制型(f47 关门、b8 拖到 f110),气刃斩1 104=
  开放型(f46 b8、f56 全开不关)。
- **INPUT_PLAN**(140 刚1 = 104 骨架,翻滚/接招并至 f56,110/121 收势;340 一文字长
  全开 [38,...);339 一文字短 [26,...);343 无双斩 [92,...);**open-ended 模板的
  leftover 键继承末档值**,窗口延续到动作尾)。实测刚1 手感 ✓。
- **刚1 无敌帧**:TIML b26 清零无效(09-18 结论复验,FSM/动作层授予),数据侧关不掉。
- **DLL**(md5 058bc40d):HITFRAME(每次命中带 motion entry+mot 时钟)+ MOTIONTRACE
  (motion 切换追踪)诊断常开(80/150 条上限)。**F6 连发会打断动作+FRAMESEEK 归零,
  污染测试数据——测试期禁按 F6**。
- col 侧:bank8(見切り攻撃)rng 提至 100/124(居合档);move25 HitLag 0.1→0.05;
  Lingering=1 实验撤回(疑似 boot 崩且非洞因)。node 63→59 后 bank0(50/62)偏短,
  需要距离时把 bank0 抬到 100/124 或换 node 84(bank9)。

### 2026-10-03 新工程文件(MHW-Valor-new.blend)重建 + LMT 合并工具战报

- **新工程工作流(已验证可落地的形态)**:GUI 里 FreeKinetics 导入**原版**
  w03_00-origin.lmt(百试百灵)→ `overlay_converted_actions.py` 把
  MHW-Valor.blend 里的转换动作曲线拷到同名 action(103/140/141-143/306/312/
  338/341/339/340/343/357-367)→ `restore_vanilla_entries.py`(GUI 内跑)
  导入原版 + 给所有**非转换** entry 恢复原版曲线 + 全部动作 Fake user + 保存。
  转换 entry 的 TIML(攻击窗/输入窗/射击摘除)不靠 blend——导出后由
  `apply_valor_attack_config.py` 重打,blend 侧保持原版即可。
- **二进制 LMT 合并(build_clean_base_lmt.py)= 搁置**:结构上已通(区间移植、
  规范 TIML 重建、全指针审计 0 违规),但 FreeKinetics 的解析器还咬人:
  (1) types 数组**顺序读**且读前 `align16(timlOffset+40)` 跳齐——TIML 必须
  单区域连续且 types 紧跟头部(CTO 追加造成的两段式 TIML = 原始导入崩溃根因);
  (2) 骨骼关键帧读取存在**越界过量读**(12B/键 的 stride 走到文件尾)——原版文件
  尾部永远有后续 entry 兜底所以从不暴露,合并文件尾=最后一个 entry 就炸;
  (3) 64KB 零填充不够兜。结论:导入走 Blender 原生流程,二进制合并只在
  游戏部署链(指针驱动)有需求时再回来修。
- **字段偏移备忘**(fcurve 骨骼头 48B):bufferSize=i32@+12、bufferOffset=i64@+16、
  lerpOffset=i64@+40——读反了会把文件头当缓冲起点拉出几十 MB 的"跨度"。
- **trace 工具**:tools/parse_test.py(FreeKinetics 解析自测)、
  tools/trace_timl.py(TIML 逐结构追踪)、tools/find_bad_overlay.py(逐 entry
  二分)。注意 blender headless 会话的插件加载顺序有随机性,解析测试结果
  偶发不稳定,以两次一致为准。

### 2026-10-03 (2) MHW-Valor.blend 原版动作重置 + TIML 回灌(_vanilla_reset.py, 全部验收通过)

- **背景**:MASTER(MHW-Valor.blend) 里原版槽位被 MHRS/MHGU/自制动作占用
  (140=192_remap 刚1、312=095_remap、307/343=MHRS plw remap、339/340/358-363=divide 系,
  其余 152 槽动作正常)。用户指令:动作全部换原版(按 entry id 匹配),TIML 全部换成
  部署文件 #1 的(nativePC w03_00.lmt, 其 TIML 均正确)。
- **做法**(mhgu/_vanilla_reset.py, README 有完整章节):按 #2 原版 LMT 的
  fcurveOffset 分组 KBRAction 重导入(154 组,共享组同构)+ entry 重接;
  按 #1 的 timlOffset 分组 processTIMLData+TIMLStructure 重建 164 组 TIML 节点;
  旧动作全部 fake_user 保命、旧同名动作 .pre_reset 后缀、失连节点删除(1716 个);
  #2 不存在的 141/142/143(divide 系)动画保留。备份
  MHW-Valor.blend.bak-20261003-124129。
- **验收**(=_vanilla_reset_verify_final.py + _vanilla_reset_export.lmt):
  接线 164/164、自定义 3 槽保留、entry 头部与原版零差异;TIML 掩块首 u32 后 md5
  与 #1 逐 entry 一致 164/164;动作结构逐骨全等(键时间零漂移),数值残差 = 导出器
  再量化(max 0.125)+ loc basis w 1.0→0.0,均为工具链既有签名(#1 同样携带,游戏实测无影响)。
- **坑⑯**:TIML 块序列化首 u32 = 块在文件内的偏移,两个文件的 timl 直接 md5 永远
  不等,先掩前 4 字节再比。
- **副产品:坑⑮根治**——306/312 无敌帧、140 见切窗口等**文件级 TIML 编辑随 #1 回灌
  进了 blend 树**(164/164 逐字节一致),以后从 MASTER 导出不再需要
  apply_sheathe_iframes.py 重打。
- 部署文件 #1 未动。把 `_vanilla_reset_export.lmt` 部署进 nativePC = 整套退回
  "原版动作 + 现有 TIML 编辑"(刚1/樱花/divide 在游戏里会消失,动作仍在 blend 里
  可随时重接);是否部署由用户决定。

- **(续)用户澄清后取证**:刻意替换(remap/divide)不是问题源。对重置前备份整体导出
  比对:**备份动作与部署 #1 164/164 完全一致**,与原版 #2 的 151 槽差异全部是管线
  重编码签名(frameCount 全等/81-82 轨/时间网格零差/值差≤0.01),无 MHRS/MHGU 渗入;
  10 处"接别的 id 的 action"= 镜像招共享 fcurveOffset 的正常命名(以组内最小 id 命名)。
  布局:FreeKinetics 只有按列堆叠一种默认排布;`_relayout_tree.py` 恢复
  "action 左 / entry 右 / TIML 正下方"配对布局(167 行,按 entryNum 排序,全部展开),
  重排后接线复验 164/164 全绿。

### 2026-10-03 (3) 全树导出卡死存档 + game-safe 构建法(待局内验证)

- vanilla-reset 全树导出(_vanilla_reset_export.lmt)部署后进存档卡死;与最后已知可用
  bak-010916 全量对比:结构/TIML/自定义槽全一致,唯一全局差异 = 动作编码层
  (E 复用原版 bufferType 1/2/7 再编码,R 是管线重算编码 12/13)。4-bit 溢出已排除。
  **教训:FreeKinetics 全树 standalone 导出从未整文件进过游戏,必须先 A/B。**
- **game-safe 构建 = _build_firstaid_lmt.py**:V 原版动作本体字节(round-trip 字节稳定
  已证明)+ R 的 TIML + R 的 141-143 完整 ActionHeader → _vanilla_firstaid.lmt
  (md5 0d673ddd)。自校验全绿;与 R 差异仅"原版槽真原版字节 + 10 刻意槽回原版"。
  若 F 仍卡 → bak-010916 顶回 w03_00.lmt(旧全套,必能进)。
- 部署链第二次出现"扩展名前多空格"文件名损坏(`.l mt`),复制后必须核对文件名。

- **(续 2)F 也卡 + 定罪**:用户实测原版 override 能开、E/F 都卡 → 毒在 R 的 TIML。
  结构 diff:R 的 TIML 161/164 与原版同构,唯一新增 = addcto 链在 339/340/343 共享块的
  占位类型(param=0x4BD36121,transform hash=0x1,0 keyframe);其余 5174 个 transform
  关键帧全部合法(昨晚编辑干净)。**F2(=F 剥占位类型,须同步 TIML_Data.count——
  不同步会错位多读一个类型,假象是"剥离没生效") = _vanilla_firstaid_f2.lmt
  (md5 ee6e195c)**,保留全部关键帧编辑+141-143。**坑⑰:改 PyCStruct 子列表
  (timl.data[i].types)后必须同步父结构的 count 字段,否则序列化字节少了一截、
  count 还是旧值,重解析会"看起来没生效"。** **✅ F2 局内验证通过(2026-10-03):进游戏,占位结构定罪,关键帧编辑全无罪。**
  F2 非 Blender 导出 = struct 级拼装(原版动作字节+R 的 TIML+R 的 141-143),
  Blender 仅当 Python 运行时。E 路径卡死同样是占位结构所致——导出路径无罪。树侧删占位后正式导出 = F3
  (_blend_export_f3.lmt, md5 8a5e3541, 验证全绿),工程已保存,以后导出天然干净。
  **✅ F3 局内验证通过(2026-10-03 深夜):Blender 导入→改→导出→部署链路完全恢复。**
  当前定态:部署 = F3(md5 8a5e3541);blend = 干净基座(原版动作+全套 TIML 编辑+141-143);
  备份: bak-20261003-124129(reset 前)/ bak-20261003-154331(摘毒前);
  拼装脚本留作应急。教训汇总:①全树导出物部署前必须 A/B;②文件名空格损坏已两次;
  ③改 PyCStruct 子列表必须同步父 count。

- **(续 3)重导入动作补绑 Tether(用户指出漏了导入选项 "Tether to Skeleton")**:
  vanilla-reset 的 154 个新动作原本是合成路径(BoneFunction.xxx/Root/reference),
  编辑器无法预览编辑。`_tether_vanilla_imports.py` 补跑 transferTether(路径重命名
  MhBone_XXX、Root 转物体级、记 tetherFrame)→ F4 `_blend_export_f4.lmt`
  (md5 54eda879):Root 烘焙逐帧键(值恒定),与旧管线导出 BK 的 -1 形态完全同构,
  轨道结构=原版 82 轨,TIML/自定义全绿。blend 已保存(bak-20261003-160823=绑前)。
  F4 待局内终验;E/F3(未绑定导出)与 F4(绑定导出)均为合法形态,F4 是项目原生约定。

- **(收尾)新动作骨骼归组**:9_group_channels.py 对当前 blend 跑完(59057 条重归组),
  归组后导出 md5 == F4 逐字节一致 = 纯外观。vanilla-reset 全链收尾:原版动作
  (tether 绑定 + 归组 + 编辑器可预览)+ 全套 TIML 编辑 + 141-143 自定义,
  Blender 导出可部署(F4 已局内验证... 注: F4 同内容 F3 已验证, F4 待用户部署确认)。

### 2026-10-03 (4) 一文字/无双斩无伤害定案+修复: 树侧 CTO 补齐 (user-verified 有伤害)

- **症状**: 339 一文字短/340 一文字长/343 无双斩 播动作但零伤害; 同文件里 刚1(140) 正常。
- **根因**: 这三个槽位源自原版**强化射击家族(332-349)**, 原版刀身部分从没有数据伤害窗
  (伤害走弹体) → 家族 TIML 只有 EC 状态机**没有 CTO**。MHW 出伤害 = EC 窗(请求层,
  只开门)+**CTO(路由+伤害窗: ノードID→col 名字表 unk4→(bank,ATK行), mFlag=伤害窗)**
  两者齐备; 缺 CTO = 请求无处落地。刚1 有 CTO(見切 TIML 自带)所以一直正常。
  "昨天不用 col 也有伤害"与此不矛盾: col 目录坏名只是让 col **编辑**不生效,
  CTO 路由到的是原版 col 行(63/59/72 原版都在)。
- **修复(树侧, 随导出自带)**: 镜像 140 的树侧 CTO 写法给三条 entry 各加一个
  `TIML::nTimelineParam::CollisionTimelineObject` action
  (starType=TIML_Action, timelineParamRaw=`m38FF82EA`, unkn0=0) + 两条 transform:
  `FreeHKTiml.mB296CA66`[0]=**ノードID[0]=63**(原版見切り攻撃 ATK 行, 刚1 同款);
  `FreeHKTiml.m08FD20A6`(mFlag 按 16/16 拆两条) on=a/off=b+1, CONSTANT 键。
  脚本 `mhgu 根目录 _add_tree_cto_1003.py`(PLAN 在文件头, 新招复用改 PLAN 即可;
  GUI 路线=复制现有 CTO 三件套节点再改帧)。blend 备份 bak-20261003-190535-pre-tree-cto。
- **✅ user-verified 2026-10-03: 三招全部出伤害。** 部署=树导出
  `_blend_export_cto_1003.lmt`(md5 f136fb94), 备份 bak-20261003-1922-precto。
  审计 464 entry 0 乱序 0 小数帧。**F2 时代卡死的是 addcto 空壳占位
  (param 0x4BD36121/hash 0x1), 真 CTO 结构树侧无风险** —— 树侧 CTO 正名。
- **EC 静态轨道名破译**(此前裸哈希): m6C333EBF=ReqNo(On) A(=1), mF53A6F05=
  ReqNo(On) B(=2), m823D5F93=**ReqNo(On) C(=10, 目前 140 独有)**,
  mA39EF26C/m4D909340=SkipTime A/C。339/340/343 与 140 现仅差 ReqNo C 一组
  ——**若未来某招补 CTO 后仍无伤害, 下一杠杆=补 ReqNo(On)C=10+SkipTime C**;
  再往后= DLL MOTIONTRACE(实播 entry|0xC000)+HITFRAME(命中 entry+帧) 检测。
- 伤害数值现借見切り攻撃行(与刚1同源); 独立数值待 col 侧加专属行(节点id改指即可)。

### 2026-10-03 (5) entry 312 ← LMT::w07::095_remap 树侧接线 + 60帧无敌 + f100接招窗

- **接线真相**: blend 里 entry 312 一直挂的是旧 `LMT::w03_00::312`(纳刀100时代,90f);
  `LMT::w07::095_remap`(121f,tether已绑) 和整套 **`.095` 后缀 TIML actions**(10个:
  PMC/PIN/PV/EC/EG01/02/03/06/GP/EP, 095 自带数据) 都在文件里但**零接线**。
  本次: 建 LMTActionNode 接 095_remap; 旧 TIML 链接全清, `.095` TIML family
  经新建 TIMLActionNode 接入 entry 312 的 TIMLDataNode; animLength=121。
- **坑⑱: 用户手备的 `.095` TIML actions 里 PlMotionCommon/PlMotionInput/
  EventCollision00/EventGroup01/EventGroup06/GameParameter 的
  `freehk.timelineParamRaw` 全是空串** → ExportTIMLTypesNode `int(paramCode[1:],16)`
  直接炸。修复=按类名从树里其他 entry 的同族 action 自动查表回填
  (PMC=m48D6114F/PIN=m6FCCD10E/EC=m0CFD985C/EG01=m2EC7FA34/EG06=m30A36F97/
  GP=m01739779)。starType 缺失同理补 "TIML_Action"。**以后 GUI 复制/自制 TIML
  action 必查这两个属性**。
- **无敌帧(树侧)**: PMC.095 mFlag = f0=0x06400000|6 (b26=0x04000000 无敌) →
  f60=0x02400006(基线) = 窗 [0,59]; mTrigger 2048@f0 (用户 TIML 里已埋, 校验保留)。
  旧 312 TIML 的全程 b24 霸体随旧 TIML 一起断开(按"没提=没有"规则,未迁移)。
  **⚠ 深夜5 教训待验: b26 在 306 上曾"毒化整段"(按动作锁存非按键窗)——
  局内须测 f60 后能否被打; 若整段无敌, 312 不可用 TIML b26 布局, 改走 DLL
  no-hit 注入或短动作拆分**。
- **接招窗**: PIN.095 三轨 (mFlag/mFlag1/mFlag2 = m08FD20A6/m6E63FBC7/mF76AAA7D)
  重写为 [(0,0),(100, 原f70档"全开"值)] = f100 起可接下一格, 开放到动作尾
  (0xC300/0xC003000/0xC003F0, 无高位特殊取消位——原生纳刀家系集合)。
- **✅ 导出验证**: frameCount=121, mFlag f0=0x6400006/f60=0x2400006, trigger 0x800,
  PIN 三轨 f100, 全审计 0乱序0小数; 339/340/343/140 CTO 金丝雀全绿。
  部署 md5 8d0bcea48e8aa72ef57941b2b4ba6b42 (备份 .bak-20261003-1955-pre312);
  blend 备份 bak-20261003-194224-pre-312-095wire。
- **⚠ 附带发现: 306/105/141 的二进制侧 TIML 实验(格挡0x12400006/见切形窗/b31窗)
  已被 10-03 全天的四轮全量 blend 导出冲掉** —— 现部署文件里它们是基线态。
  要恢复须树侧重做(坑⑮的正解); 实验期结论未定案, 按需再说。

### 2026-10-03 (6) 312/action100 无敌帧崩溃 + 105 A/B (验证中)

- **症状**: 部署 8d0bcea4(312=095_remap+.095 TIML+b26[0,59]+trigger2048) 后, 99→100
  交接瞬间崩溃(用户观察+日志双证: ValorGauge.log 止于 3:98 IFRMINE, 无 id=100 行;
  DLL 改名停用仍崩 = 非 DLL)。action 100 = 无 FSM 节点的零入边动作(98/99/100/103 同族)。
- **数据面已排除**: .095 TIML family 全部键值逐项核对 = MHW 原生同款(1119/115/135/
  3198/28 均见原版 140/306), 无 MHGU 异物 id。
- **A/B 部署 md5 83e9ce63**: entry 105(原生突刺,131f,有FSM上下文) 加同款 b26[0,59]
  +原生trigger2048; entry 312 只摘 b26(其余 095_remap 接线/.095 TIML/f100 接招窗
  全保留, mTrigger 2048 留守=惰性)。判读: 105崩=配方不可用; 105活+312不崩=
  b26×action100 组合崩; 312仍崩=.095 TIML/动作换装崩因。
- **崩溃转储捕获**: 全局 LocalDumps DumpCount=0 吞 dump(C:\CrashDumps 空,20:13 被碰);
  HKLM 需管理员 → `enable_mhw_dumps.reg`(E:\MHW-arts) 双击一次, 崩后 dump 落
  E:\MHW-arts\crashdumps。背景: 10-01 二进制侧 b26 在同 action100 上曾"无敌不崩"
  (旧 312 TIML), 与本次矛盾 → 崩因也可能是 .095 TIML×100 组合, A/B 判读后再定。

- **✅ A/B 裁决 (user 局内)**: 105 同款 b26[0,59]+2048 = 无敌生效且不崩 → **TIML b26
  无敌配方本身有效**("不能加无敌帧"结论作废); 312 摘掉 b26 后 99→100 仍崩 →
  **崩因 = 312 新内容(.095 TIML family 或 095_remap 动作), 与无敌帧无关**。
  注意 095_remap 此前从未局内播过(10-01 接线一直是待办), 无游戏侧前科。
- **二分部署 md5 f3b47b1d**: 312 = 095_remap 动作 + 旧原生纳刀 TIML(PMC 0x01000000
  flat/PIN f40-55/EC [35,42][61,68], 无 b26 无 2048)。判读: 不崩=.095 TIML 有毒
  (目标配置改在旧 TIML 上重建: b26+f100); 仍崩=095_remap 动作本体踩引擎雷
  (导出警告全是背景噪声族, 数据面无异常)。转储捕获 .reg 已备(E:\MHW-arts\
  enable_mhw_dumps.reg, 全局 DumpCount=0 曾吞 dump; HKLM 需管理员手动导入)。
  blend 备份 bak-20261003-204046-pre-312-oldtiml。

- **✅ 二分裁决 (user 局内)**: f3b47b1d(095_remap+旧TIML) 不崩 → **.095 TIML family
  是崩因**(具体哪个 type 未继续二分, .095 族已断开留在树里无害); 095_remap 动作
  无辜。同时 105 确认 **b26 窗口边界真实生效**(f60 后被打飞, "毒化整段"不成立,
  306 教训限其特定布局)。
- **✅ 终局部署 md5 b97d9d953ec6ebd31259dc1049e83b92**: 目标配置重建在旧 TIML 上 = PMC.413 mFlag
  f0=0x05000000(b26+b24, 原生霸体保留) → f60=0x01000000(纯霸体) + mTrigger
  2048@f0(新轨) + PIN.197 全轨 [(0,0),(100,原生终值)] f100 开放接招。
  动作=095_remap(121f)。待局内终验: 无敌窗边界/接招窗/崩溃消失。
  blend 备份 bak-20261003-204858-pre-312-goal。`.095` TIML family 崩因追查
  (哪一步 type 毒) 挂起, 要查就走 dump 路线。

- **✅ 用户终验完成**: 312 终局配置局内通过(无敌窗/接招窗/不崩)。**残留一次不可复现
  崩溃**, 日志证据指向 10-01 实验态 DLL 干预层: sheath-counter 的 `rule 98,99,100,37
  250 3 77`(强切77) + `bluesim 98,100`(每帧写 +0x2399/0x2CED/0x23A0/0x23A4+FX, 对旧
  312 调的实验) —— 最后会话日志正止于 rule 强切 3:77 的 FRAMETICK/CLK。已把两条规则
  注释停用(mine 被动记录保留), Shift+F6 或重启生效; 恢复=去行首 #。
  备份 ValorGauge.sheath-counter.txt.bak-20261003。若再崩: 先导
  enable_mhw_dumps.reg(E:\MHW-arts) 再复现, dump 落 E:\MHW-arts\crashdumps。

- **✅ 312 定稿 (user 选定"维持现状")**: mFlag = f0=0x04000000(b26) → f60=0x00000000,
  无效的 b24 死位清除(TIML 与实际表现一致: 60帧无敌后正常受击; TIML 霸体本就
  不抗击飞, 与玩家SA同语义)。mTrigger 2048@f0 / PIN f100 接招窗不变。
  部署 md5 见本次部署(312final); blend 备份 *-pre-b24clean。
  不可复现崩溃维持挂起(实验层已停用), 复现路径=enable_mhw_dumps.reg。

### 2026-10-03 ValorLS(勇太4.2)逆向定案: 动作转换=纯数据, 防御=防御帧, DLL 零注入 (工具 _valorls_*.py)

- **DLL(3 钩子旧版/.c; 4.2 交付版 9 钩子, _valorls_42_hooks.py 反汇编枚举)**: 帧tick/
  动作请求处理器(双版本地址 0x140AE9EA0/0x141265770)/斩味函数(0x142134B70/0x142141A60,
  detour 只截参数, 绝回触发时重放 +10 斩味)+0x1419C5820(截 [rdx+0x20] 斩味浮点)+
  0x141CE1670(顺手充能)+0x14119FAA0(动作 98/99/69 事件: 99→转狩技)+**0x14213FA12 直接
  NOP 8 字节**。**全部游戏内存写入仅 weapon+0x2370/0x2374/0x2388/0x2390/0x239A + 请求槽
  {3,id}**; 无伤害钩子/无 HP 写/无防御写(吸血=游戏特性, user 确认)。钓鱼四 art 纯
  强制请求槽 {3,94/111/106/104}; +0x2388=90 需 motion==0xC153(entry339) 才写=回气激活。
- **保护=防御帧(TIML, 自带 LMT)**: 原版见切/居合家族 i-frame 在 PlMotionInput mFlag1
  b26(0x04001000→0x0C003000)+PlMotionCommon mTrigger 2048; mod 自定义 entry(339=art106
  的 motion, 74 帧窗)同配方; 加强签名 0xCFFC10CC/0xCFFC30CC(b26+b24+…)用于 106/107/110/162;
  entry140(见切槽改 13f GP) PlMotionCommon mFlag f0=0x07000000(b26+b25+b24) 全程覆盖;
  **140↔170 TIML 整体互换**=槽位复用。FSM 条件全原版属性(含 条件631 見切り回避成功後の攻撃ヒット)。
- **特殊纳刀保持机制(98/99→LMT306)**: ① entry306 原生 TIML loopStartPoint=129 动画不结束
  →动作不结束; ② 动作 98 触发的线程 10Hz 轮询, motion==0xC132 期间反复写
  **mgr+0x858=1(int)+mgr+0x85C=1.0f**(mgr=*(player+0x7D20)=FSM 管理器条件成员存储,
  指针链 {0x50,0x7D20,0x858}; motion id 链 {0x50,0x468,0xE9C4} 与我们研究一致) 钉死
  原版衰减; ③ b26 窗 f75 起无关闭键穿透循环; ④ 松键=FSM 条件"押してない"退链接火→动作
  切换→线程自然退出。DLL 松键分支只清边沿锁存。
- 工程结论: TIML 属性清单(138 项)无防御角度字段, b26=全向; "前180°"只能靠 FSM/代码侧。
  勇气条/狩技条是 DLL 自有浮点(getyqadd/getskadd 可查), 每招上限 420/830/580/1250。

### 2026-10-04 勇气纳刀动作构建 (valor_sheath_start/done, _valor_sheath_build.py, ALL GREEN 已存盘)

- **拼接点由 2D 姿态扫描实测**(_valor_sheath_scan2d.py, 195×196 全帧): 最佳对
  **(tA=46, tB=131)** — 手 IK 目标差 247=2.6cm/248=3.4cm, 头 2.3cm, 重心 3.8cm,
  体骨平均 ~8°; tA 诊断线 46 处 V 谷(user 目测"50帧后"≈实测 46)。**指标必须含
  247/248 手部目标位置**(体骨 FK 差被手 IK 运行时解算部分掩蔽); bone.isdigit() 对
  "MhBone_NNN" 恒 False 是第一版扫描全错的根因。tB 定 131 → 循环段=196[131..169]
  (38 帧定势); B@0/B@169 自循环有 4 根附属骨(053/056/060/063) 12-20° 不闭合。
- **产物(MASTER MHW-Valor.blend, 全部过 LMTActionParser valid=True, 81 变换轨+Root 双轨,
  与已局内验证迁移动作同签名)**:
  - `valor_sheath_start` **84f** = 195[0..38] + 渐变[38..46] + 196[131..169]
    (尾 8 帧回渐变到 B@131 → 循环回绕无缝; 接缝复检 head/wrap worst 3.5°/2.3°);
    位移通道 B 段加 A@46-B@131 偏移(rootY -0.2187 连续, 与 done 首键一致);
    247 符号翻转对齐; freehk.loopFrame=46(**仅记录, 导出生效在 entry 节点 loopFrame=46,
    LMTEntryNode.construct 写头部**)。
  - `valor_sheath_done` **97f** = 195[46..143] 前移(A 自身轨迹, 松手衔接; 供备选完成段)。
  - `TIML::nTimelineParam::PlMotionCommon`(TIML action): mFlag b26(0x04000000)@f0 常开
    + mTrigger 2048@f0, 无关闭键=穿透循环 —— 用户已验证配方(见切/312 同款, 312 局内通过);
    **TIML 无角度字段, b26=全向**, "前180°"归 FSM/代码侧。
  - 编码: TIML action 级 `freehk.timelineParamRaw="m48D6114F"`(PlMotionCommon);
    fcurve path=`FreeHKTiml.m<hash>`(mFlag=m08FD20A6, mTrigger=m99897C43), bigFlags 拆
    low16/high16 两条(array_index 0/1, 键帧必须对齐), 导出侧 ExportTIMLTypesNode 回读已验证。
  - 树 w03_00 已建 3 个未接线节点(2×LMTActionNode+1×TIMLActionNode), **接线时:
    entry 的 "LMT Animation"←动作节点, "TIML Data"←TIML 节点, entry.loopFrame=46**。
- 坑: ①object 层裸路径 "location"/"rotation_quaternion" 不含点, is_loc/is_rot 要单列;
  ②动作段截取后骨的 4 通道必须齐全(缺通道→导出器 Quaternion 空序列崩, 坑⑭家族),
  done 全曲线强制补 0/97 边界键(eval_hold 越界取端点保持, bezier 外推会漂);
  ③min2 兜底键方向: 末帧单键向前补(f0-1), 否则顶爆 frameCount; ④四元数比较必须
  2*acos(|dot|), 逐分量差值被 q/-q 双覆盖污染(虚报 38°→实为同姿态)。
  blend 备份 bak-20261004-004945。GUI 开着需 File>Revert 后再接线(坑⑭)。

- **✅ 根位移实测更正 (user 确认设计)**: 195_remap 根位移 0→-0.2185m **43 帧即完成**,
  之后全程持平(残余 <3mm); 196_remap 三轴恒 0(纯静止)。2026-10-01(2) 记的
  "195 后撤 -1.44m"是旧量测, 作废。valor_sheath_start 46 帧后根恒定 -0.2187
  (循环段零位移, 回绕静止→静止), done 残余漂移 2mm 级 —— "50 帧后玩家不移动"
  由现有产物自然满足, 无需重建。

- **✅ 尾段位移双变换 bug 修复 (user 抓出, 按其公式验证)**: user 给出精确构造式
  "196 段每个关键帧 = 接点(x,y) + 196 当前帧位移"(196 静止 → 恒等于接点位置)。
  实查 valor_sheath_start 根 Y 键 [77..84] = -0.246→-0.437: fade_to_bt 把"已变换目标"
  与"B 原值"直接 lerp 后又叠一次 bval → 位移通道偏移被加两次(每圈尾漂 0.22m + 回绕瞬移)。
  修复 = 两侧各自 bval 到输出空间后再 lerp, 不再二次叠加。新增公式验证: 接点后
  root X/Y/Z 全键 maxdrift=0.00000, ALL GREEN 重存。旧节点(动作重建后指针悬空)已清,
  树里保留 3 个未接线节点(LMT Action Node.003/.004 + TIML Action Node.010)。
  备份 bak-20261004-004945(修复前 specimen)/002031(pristine)。

- **✅ 循环段扩至整个 196 (user: "动画不够长")**: start = 195[0..38]+渐变[38..46]+
  196 全程(131..169 -> 绕回 1..130), **214 帧, 循环 169 帧(≈2.8s), loopFrame=46**。
  新发现: **196 源内部 B0->B1 有 42.9° 复位帧阶跃**(004/006/007/012 族, 全时间线唯一
  内部缝; B@169->B@0 反而闭合) → 循环窗口跳过 B@0, 回绕渐变目标改 B@1(608 模式带
  tgt 参数)。回绕 B@130->B@131 为源连续帧天然无缝(worst 1.1°)。接点公式复验:
  root X/Y/Z @>=46 全键恒定 maxdrift=0.00000; B-seam 残余 9.8°/4帧=渐变斜率非阶跃。
  悬空节点已清(动作重建会使旧 LMT/TIML ActionNode 指针置空, 每轮重建后要清一次)。
  备份 bak-20261004-115624(改前)/004945/002031(pristine)。

- **✅ entry306 防御帧定稿 = bit28 角度格挡 (user 指出此前"TIML 无角度"结论有误, 已更正)**:
  角度防御就是 **2026-09-13 研究文档的 PlMotionCommon mFlag bit28(0x10000000) 格挡有效**
  (运行时实锤: 正面=格挡咚声不掉血不硬直、背面=正常受击, 锥体随朝向; 当时的验证值
  306184198=0x12400006)。本session 早前"138 项属性无角度字段"的结论错误——w03_00 清单
  只覆盖用到的属性, bit28 语义记在研究文档里。**bit28 与 b26 互斥(b26 赢则格挡不运行)**,
  要角度格挡就必须去掉 b26+2048。
  落地(_valor_sheath_guard_patch.py 原位修补, 不重建动作-节点连线不动): 我们做的
  TIML::nTimelineParam::PlMotionCommon 改为 **mFlag 单键 f0=0x12400006 整段常开,
  mTrigger 轨删除**; 并入 user 精剪过的 entry306 TIMLDataNode(其保留 PlMotionInput.191/
  PlMotionVisual.119/EventCollision00/EventGroup01 四条, 原版带特效+自动纳刀的
  PlMotionCommon 已被 user 删除, 我们这条作为 PlMotionCommon 类目补位, 无冲突)。
  端到端导出回读: entry306 motion=214f/loopFrame=50/TIML 5 类, PlMotionCommon mFlag
  f0=306184198 ✓。user 已调好的三行为(不自动收刀/循环/可接招)不动。
  注: TIMLDataNode 的 TIML Animation 输入 link_limit=0 可挂任意多条 TIML action 并列导出。

- **✅ 新导出 LMT 开机崩溃定位 = TIML 类型头 unkn0 (类目分派 id)**: entry306 接上
  valor_sheath 后游戏无法启动(改名停用即恢复)。字节级对比全文件 249 个 PlMotionCommon
  类型头: **unkn0 恒为 11**(含全部此前开机过的自定义 entry 141/312/339/340/343/358-363,
  均继承自导入原版 TIML), PlMotionInput 恒 9, 其余类目恒 0 —— unkn0 = 加载器类目分派 id。
  我们凭空新建的 TIML action 照 EffectParameter 样例默认写了 unkn0=0 → 全文件唯一的
  PlMotionCommon=0 → 加载崩溃。修复 = freehk.unkn0=11(原位, 接线不动), 重导出五类目
  类型头与各常量一致(PlMotionCommon unkn0=11 + mFlag f0=0x12400006), 已部署待开机验证。
  **教训: 凭空新建 TIML action 时 freehk.unkn0 必须按类目写常量**(PlMotionCommon=11/
  PlMotionInput=9/多数=0), 不能照别的类目样例抄; build 脚本已同步。ENTRIES 区分:
  entry.loopFrame(=-1 之外 23 个先例) 与 TIML loopCtrl(全文件 163:1, 唯 306=1, 但 user
  已在 loopCtrl=1 下进过游戏 → 排除) 均非死因。
  备份 MHW-Valor.blend.bak-20261004-pre-unkn0fix。

- **✅ user 局内终验 (2026-10-04)**: unkn0=11 修复后正常进游戏; entry306 全动画
  **真·带角度防御实锤 —— 背后被打直接击飞**(正面=格挡)。至此 valor_sheath 链路定稿:
  306 = valor_sheath_start(214f, entry.loopFrame=50 循环整段196, 无 b26/无自动收刀/
  可接招) + 5 类 TIML(user 精剪 4 条 + 我们的 PlMotionCommon mFlag f0=0x12400006
  bit28 格挡整段)。后续通路: 正面被格挡 → 反应管理器 +0x769=0x100(研究文档格挡反应
  入口)可作代码侧格挡成功信号; 背面受击 → 正常受击管线 → fsm 100 打断路线可用。
  突刺(105)角度防御 = 同一个 bit28 单字段改动(9-13 已验证过一次), 待接"引到新动作"。

### 2026-10-04 勇气纳刀代码侧: krel 规则(松键收刀) + arule 复用(被打打断) (待局内验证)

- **需求**: 持 3:98 期间 ①被命中 → 强制 fsm 100(受击) ②松开纳刀键(鼠标侧键上) →
  强制 3:6 收刀; 其余不干预。98 以后可能换节点 → 动作 id 全部走配置。
- **实现 = sheath-counter 规则系统扩一个模式**, 两条规则纯配置:
  - `arule 98 3 100` — 现成 arule(DmgCalc 连接命中 + attacker≠player → arm 强制),
    自带致死锁血(HP 钳 1)。**修了一个真 bug: arule 单发锁原是 g_clangLatch, 而该锁只在
    86/101/102 复位 → 第一次被打打断后, 之后所有持刀被打永久失效**; 换成按动作实例的
    fired/lastActId(id 变化即重新武装), 与 bit 规则同模式。
  - `krel <ids> <vk> <toSet> <toId>`(新) — PollSheatheCounter 里 GetAsyncKeyState
    down→up 边沿 → 纯 arm(g_forceSet/Id+g_forceArmed), **故意不走 OpenCounterWindow**
    (纳刀收刀不需要 no-hit 注入), 无 latch(边沿即单发, force 被拒则下次按压重试)。
    侧键上=XBUTTON2 vk=6, 侧键下=5。
  - 强制执行走既有 TryQueuedForceWrite(游戏线程 getter hook), 配置追加于
    ValorGauge.sheath-counter.txt(旧实验规则保持注释)。
- **部署状态**: DLL 新版本 md5 10ec5fc1 在 ValorGauge/build/Release/ 待拷
  (游戏运行中 DLL 被锁, 关游戏后拷到 nativePC/plugins/), 配置规则已追加。
  顺手修了 ValorProbe 目标的既有 ifdef 失衡(ApplySheatheMotionPatch 调用点)。
- **已知边界**: arule 以 attacker≠player 判"被打", 联机时队友被打会误触发
  (日志有 SHEADRULE hit a2=... player=... 行, a2==player 确认后可收紧);
  致死钳 1 与"防秒杀"后续设计共用入口。

- **✅ 局内实测反馈 + drule 补全**: 日志证实持刀 id=3:98 正确、规则加载 2 条; 正面被打
  = dispatch REJECTED(格挡否决,无伤害连接,arule 天然不触发 ✓) **但引擎加载 {3,37}
  格挡反应把保持态踢出 98**(松键规则因此没机会触发)。补全: 配置加
  `drule 98 3 100`(rejected dispatch -> 打断进 100; drule 分支本来就在, 纯配置,
  Shift+F6 热重载即生效); 代码修 drule 的 g_clangLatch 死锁门(同 arule, 换
  fired/lastActId 按实例复位) + drule 触发时写 reassert(吞 {3,37} 1.2s, 保持态不
  经过 37 直接落 100)。新 DLL md5 4054ea24。三规则现状: arule(背面真命中)/
  drule(正面格挡否决)/krel(松键收刀), 全部围绕 3:98。

- **✅ krel 验证通过 + drule 撤换**: 日志实锤 krel 全链正常(侧键 vk=6 确认,
  down=[6] 唯一键; 松开边沿 -> force 3:6 accepted -> action changed forced id=6
  -> 收刀待机)。drule 对自定义动作 98 失效: 其 dispatch 返回恒非 0xD(环境帧亦然),
  入场第一帧即触发 = "按下即 100" → 配置撤换为 **bit250 位规则
  `rule 98 250 3 100`**(正面格挡交互时 250 升起, 无攻击持刀不升; krel 探针
  SHEATHKRELDBG 留用)。Shift+F6 热重载生效, 待局内确认三路齐全。

- **✅ 局内二轮: 规则全部真实触发, 新漏点 = 格挡反应变体 3:35**: 日志实锤 drule 触发
  → FORCE accepted → changed (forced) id=100 ✓; krel 全链 ✓(探针 down=[6] 唯一键,
  松开边沿 → force 3:6 accepted → id=6 ✓)。**"有时候看到 3:35" = 格挡反应家族变体**:
  引擎按情况在 35/36/37 里选, 我们只吞 37 → 35 漏网顶掉 100。**ValorLS 无此处理**
  (grep 证实: 请求改写点仅 art 触发; 白名单 0/301-306/501-506 为免重置表) —— 它不需要:
  其 306 用见切式窗口(b31+b22, 无 bit28), 走否定/叮系统不产生格挡反应; 我们用 bit28
  真格挡就必须自己吞。修复 = DoActionHook 吞单扩为 **35/36/37 全族, 且当前动作处于
  任意被监视动作(现在=98, 换节点自动跟随)期间无条件吞**(reassert 窗口之外也吞),
  SheathWatchedAction() helper + 前置声明。DLL md5 1aa879d9 已部署, 待局内验证。
  若正面被挡后 100 仍偶发被顶: 下一步查非格挡族反应 id(日志 action changed 序列)。

- **✅ 35 漏吞真因 = DoActionHook 内部顺序**: 追踪器更新(含 "action changed" 日志,
  行 1256)在吞单(行 ~1280)**之前**执行 → 吞单检查 watchedHold 时 g_lastLongSwordAction
  已被覆盖成 {3,35} → SheathWatchedAction(3,35)=false → 不吞。修复 = DoActionHook 顶部
  先快照 prevSheathAction, 吞单用前一个动作(=玩家所在 98)判断。DLL md5
  e8ed4216 已部署, 待局内验证。判定链备忘: 35 经 DoAction(已证实, "action changed
  id=35" 唯一输出点在 DoActionHook), 非缓冲直写。

- **✅ 35 仍漏的第二层真因 = 吞单位置在追踪器更新之后**: 上一版吞单虽触发(日志
  "swallowed 3:35" ×3), 但 DoActionHook 里**追踪器更新先于吞单** → 吞单前追踪器已被
  改成 {3,35}: ① bit250/arule 规则的 duringIds(98) 对着追踪器里的 35 永久失配 →
  强制 100 永远不来; ② 引擎重发第二次 DoAction({3,35}) 时 prevSheathAction 已是 35 →
  二次不吞 → 35 真播。修复 = **吞单整块上移到 DoActionHook 顶部(prevSheathAction
  快照之后、追踪器更新之前)**: 被吞的反应完全不污染追踪器(保持 98), 规则继续工作。
  DLL md5 d5eabefd 已部署。验证点: 日志 "swallowed 3:35" 后应紧跟 SHEATHCOUNTER
  (bit250 触发) + FORCE 3:100, 且不再出现 changed id=35。

- **✅ 被打信号换 bit222 + bit 规则哨兵重武装**: 吞掉反应动作后 bit250 不再升起
  (旧的 rose=[250] 发生在被吞前的 3:37 反应动作里)。日志实锤**每次格挡命中事件位
  222 脉冲一拍**(ev6 0x0→0x40000000, 6 打 6 中; 另有 225/147 部分命中伴生) →
  配置改 `rule 98 222 3 100`。bit 规则同样补哨兵重武装(离开监视集置
  lastActId=0xFFFFFFFF, 同 id 重入 98 也重新武装; 旧逻辑第二次持刀的第一下会被
  fired 卡住)。DLL md5 9bee73aa + 配置已部署, 待局内验证。

- **✅ 重击路径补全 = 吞单即信号**: 轻击(bit222 脉冲)走 bit 规则 ✓; **重击(引擎选
  35/36/37 反应)脉冲的位不同且反应被吞后无事件位** → 吞单本身即每击一次的精确信号。
  SheathArmHitRule(holdId, reactionId): 吞单时按 holdId 匹配规则 target 直接 arm
  (SHEATHGUARD 日志行)。**教训: DoActionHook 在 sheath 规则表声明之前, 相关逻辑一律
  helper + 前置声明**(本轮连续踩了两次声明顺序坑)。DLL md5 01da4c89 已部署。
  现役信号矩阵: 轻击=bit222 脉冲 / 重击=反应吞单 / 背面真命中=arule / 松键=krel。

- **✅ 刚1 被劫持成 100 的真因 = 强制目标单例被吞单武装覆盖**: g_forceSet/g_forceId
  既是 force-action.txt 的配置默认值(刚1=3:77)又是"待发目标", 吞单武装把默认值改写成
  3:100 后, **弹反链的四个武装点(iai bit250/见切 bit15/deflect flash/Ctrl 窗口)只 arm
  不装填**(依赖启动默认) → 之后所有刚1 都打出 100。修复 = 配置默认值与待发目标分离:
  新增 g_forceCfgSet/g_forceCfgId(LoadForceActionConfig 与 Shift+F6 重载同步), 弹反链
  四个武装点 arm 前从配置副本恢复。DLL md5 2bb3063c 已部署。刚1(3:77)与收刀回避
  (3:100)两条强制链现已互不污染。

- **✅ user 局内终验 (2026-10-04 晚)**: 四路全部通过 —— 持 98 轻击/重击/背面被打均进
  3:100(收刀回避), 松开侧键上键进 3:6(收刀), 见切弹反恢复刚1(3:77)且与持刀链交叉
  不串。勇气纳刀代码侧完结。遗留优化(未排期): 98 换专用 fsm 节点(现借 98); arule
  的 attacker≠player 判据在联机会误触发(a2=defender 字段待钉, SHEADRULE 日志行可证);
  krel 探针 SHEATHKRELDBG(8 采样/会话)留用可删。

### 2026-10-04 (2) 滑歩気刃 106/107/108: 气刃消耗 + 气刃特效 (id 探测定案, 待局内验证)

- 需求: 3:106(気刃１スリR) / 3:107(気刃２スリF) / 3:108(気刃２スリL) 加气刃特效 + 各消耗 0.25;
  "气刃不足不出招"的门由用户改好的 FSM 条件承担, DLL 只管扣除与特效。
  **定案: vanilla 每招特效/扣气刃都走按动作 id 硬编码的每招 handler, 改不了数据 → 只能走 DLL 的 id 探测 (gauge-rules)。**
- **新规则 `fx <set> <id> <fxId>`**: 动作开始时对 player+0x8808 emitter 经 ADD670
  (g_fxNotifyA, 与 F7 试枪同一调用通道, 返回句柄无消费方) 发特效; `0x7D2` = 气刃斩系通用红光
  (user-verified)。fxId 支持 0x 前缀。当前唯一已验证的身体红光 id 就是 0x7D2 —— 按等级变色的
  刀光走事件上下文另一套, ADD670 打不出来; 想换 id 直接改配置。
- **cost/fx 触发时机从"DoAction 接受"迁到"动作真正开始" (begin watcher)**:
  连招派生请求在接受时上一招还在播 (缓冲 +0xBC/+0xC0), 接受点扣气刃会在"被打吞掉缓冲请求"时白扣,
  特效会闪在上一招上。`PollGaugeBeginRules` (property 钩子每帧, 游戏线程) 监视 controller+0xAC/0xB0:
  set:id 变化 或 同 id 下实例帧 +8 回落 >0.5s (缓冲重入同招, begin 清零帧计数) = 一次 begin →
  `ApplyBeginGaugeRules` 发 cost + fx。**gate/regen/timer 仍在接受点, 行为不变**; cost 从
  ApplyActionGaugeRules 移出 (无既有规则在用, 无回归)。武器 LS 硬门控提取为共用 `GaugeRuleWeapon()`。
- 配置已部署: gauge-rules.txt 末尾 `cost 3 106/107/108 0.25` + `fx 3 106/107/108 0x7D2`
  (Shift+F6 可热重载)。DLL md5 ebdfd659 已部署 (mod)。
- 局内验证点: 滑歩气刃起手瞬间红光 (不早于前一招结束) + 蓝条 -0.25 + 日志
  `GAUGERULE cost act=3:10x -0.25` / `GAUGERULE fx act=3:10x id=0x7d2`; 被打吞连招时不应扣。

### 2026-10-04 (3) 勇气连段常量 + 刚1 阈值定案 + "刚1 变气刃斩"取证

- 用户定案: 刚1 (sheath-rewrite) 气刃门保持**默认 0.15** (`g_sheathRewriteMinGauge`,
  dllmain.cpp:896, 无配置文件时代码默认即生效)。勇气连段: **105/106=一文字, 107=无双斩,
  108=连段第三招**, 四招统一 = cost 0.25 + fx 0x7D2 (begin 红光)。
- **代码 seed 常量**: `LoadGaugeRulesConfig` 开头注入 105/106/107/108 的 cost+fx;
  gauge-rules.txt 显式行按 (type,set,id) **替换** seed (配置可覆盖), Shift+F6 重载同路径
  ——配置文件丢失也不丢行为 (sheath-rewrite.txt 缺失事故的教训)。现有 106/107/108 行降级为
  覆盖示例。日志标识 `Gauge rules: N loaded (incl. valor-chain seeds 105/106/107/108)`。
- 局内实测 (上一版部署): fx/cost 实际触发 106×7 / 108×11; 107 当次会话没出招, 105 当时无规则
  ——本版四招全覆盖待复测。
- **"待机刚1 按下后变气刃斩"取证 (日志 69.6万行全量 + 代码)**:
  - 假设 A (刚1 出招后迟到 {3,11} 顶掉): 全日志 124 次 SHEATHREWRITE, 其后**下一个**动作变化
    是 id=11 的 = **0 次** (紧邻 6 行窗口 0/124, 末会话宽窗口 0/5); `SHEATHPROBE ... last=3:86`
    全日志 **0 条** → 无证据, 暂按不成立。
  - 假设 B (**收刀过渡窗漏改写, 证据成立**): 收刀判定 = "上一个动作是 set 1", 但收刀动画本身
    是 set-3 (3:0/3:6 家族, 3:6=松开侧键收刀)。R2 按在收刀动画期间/刚收完的一瞬 → lastSet=3
    → 不改写 → 原版气刃斩1(带前摇)直接出来。日志 `SHEATHPROBE blocked last=3:0` ×67 /
    `last=3:6` ×27 (探针每会话限12条, 实际更多)。
  - 修复方案 (待确认后实现): ① sheath-rewrite 的收刀判定扩展: last ∈ set1 ∪ 收刀过渡家族
    (可配置, 初值 3:0/3:6); ② 改写命中后 ~0.8s 内吞掉同 fromId 迟到请求 (复用 35/36/37 吞单
    机制, 吞单日志兼作假设 A 的探测器)。
  - 设计问题: 收刀动画**播放中**按 R2 → 改写成刚1, 还是吞掉不出招? (收刀冷播有姿势跳变,
    09-25 章节老问题)

### 2026-10-04 (4) "走路刚1变气刃斩"破案: 收刀态行走 R2 = 动作 id 12 (user 方向正确, WASD 相关)

- 用户否掉"收刀过渡窗"解释, 提出与行走按 WASD 有关 → **全量日志迁移分析实锤**:
  `1:4(收刀走) -> 3:12` ×**187**, `1:8(跑) -> 3:12` ×6, `1:585 -> 3:12` ×15, 而
  `1:0(站) -> 3:11` ×158 —— **收刀态移动中按 R2 的请求是 3:12**(拔刀气刃斩迈步变体,
  与 09-30 编排器 {3,11}/{3,12} 兄弟对吻合), 不是 11。fromIds 只有 11 → 静默放行原版,
  且 fromIds 不匹配的分支**一行日志都不打**(探针只覆盖匹配后阶段) → 几周不可见。
- 修复: ① fromIds 默认 `{11, 12}` (g_sheathRewriteFromIds); ② 新增 `SHEATHSEEN`
  日志 (限30条/会话): 收刀态(set-1 前置)收到不在 fromIds 的 set-3 请求时打印 id,
  攀爬/游泳等剩余变体一次会话即可暴露。
- 迁移表补充 (set1 -> set3): `1:8->3:7` ×447 / `1:4->3:7` ×268 = 拔刀纵斩(△键),
  不在改写范围(用户只要 R2 系); `1:440/432 -> 3:18` = 翻滚; `3:5` = 普通拔刀(△系)。
- 收刀过渡窗 (3:0/3:6 期间按 R2) 仍是独立的二类洞 (blocked ×94), 优先级低于本修复,
  修不修等用户复测后定。
- **教训: 静默分支必须留探针** —— fromIds 不匹配纯 return, 让 187 次泄漏零痕迹。

### 2026-10-04 (5) fromIds 修复无效复盘: loader 局部默认值覆盖全局 (与 g_forceCfg 分裂同款教训)

- 上一版把 fromIds 默认改成 {11,12} 改在了**全局变量**上, 但 `LoadSheathRewriteConfig`
  有自己的局部 `fromIds{11}` 且加载时无条件覆盖全局 (init + Shift+F6 都走) → 新 DLL 日志
  依旧 `requests 3:{11}`, SHEATHSEEN 照抓 `3:12 not in fromIds (last=1:8)` —— 诊断对、修复
  改错层。修复 = 改 loader 局部默认 `{11,12}` (933 空回退同步), 全局变量注释标注
  "effective default lives in the loader"。
- **教训 (第二次): 配置 loader 拥有并覆盖默认值, 改默认必须改 loader** —— 与 1b0b2f2
  (g_forceCfg* 分裂, 吞单武装覆盖强制默认) 同类: 默认值有两份时, 生效的永远是 loader 那份。
- SHEATHSEEN 首日战果: `3:5 (last=1:0)` = **纳刀系动作本身** (收刀家族 {5,6,9,10}),
  不该改写, 行为正确无需处理; `3:12` 多次 (last=1:8/1:4) = 修复目标, 本版已收编。
- 附带实测: 3:12 原生自带 cost 0.15 (GAUGEEVT val=-0.15) + 原生 0x7D2 红光
  (FXNOTIFY caller +0x2122559) —— vanilla 迈步拔刀有自己的气刃呈现, 改写成刚1(86)后
  由 86 的呈现接管。
- 部署 md5 63a454a8。验证点: 启动行 `Sheath rewrite: ... 3:{11,12} -> 3:86`;
  收刀态走动按 R2 出刚1 + `SHEATHREWRITE: 3:12 -> 3:86`。

### 2026-10-04 (6) 剩余泄漏定案: 纳刀动作 3:9 播放中按 R2 (用户"1:8→1:4中间态"的实体)

- {11,12} 修复后的测试局: 改写全中 (12×13+11×2, set1→set3 迁移 15/15 → 86),
  SHEATHSEEN 零条 —— 请求 id 无漏网。仅剩 SHEATHPROBE `blocked: last=3:9` ×4。
- **3:9 身份 (前驱/后继实锤)**: 后继 = 1:15 ×293 / 1:8 ×258 / 1:4 ×100 (89% 回收刀态
  locomotion) → **3:9 = 纳刀动作**; 前驱 = 3:1(持刀走) ×464 / 3:65(翻滚恢复?) ×182 /
  翻滚。历史: 3:9 → 3:12 ×26 / → 3:11 ×22 = 收刀中按 R2, 请求仍是 11/12, 只被
  "前置必须 set1" 挡住 → 原版气刃斩1 在收刀动画里蹦出来。用户主观感受为"快跑→慢走
  中间态"(跑动中收刀/刚收完的窗口)。**1:8→1:4 本身无中间动作** (transition 表无其它 id)。
- 修复: 收刀判定 = last ∈ set1 ∪ {3:9, 3:6} (kSheatheActionIds 常量; 6 = 勇气持刀
  松开收刀)。**明确排除 3:0/3:1/3:2 = 持刀待机/走/跑** (3:0→3:11 ×347 = 持刀 R2 原版
  气刃斩, 正确放行; 修正此前"3:0 身份待定"的记录) —— 排除 3:5 (罕见, 语义未钉, SHEATHSEEN
  继续监视)。部署 md5 e14b8859, 待局内复测: 收刀动画中按 R2 应出刚1
  (`SHEATHREWRITE ... last=9` 类日志 + 无 blocked last=3:9)。

### 2026-10-04 (7) 刚1 气刃消耗: 快照-结算 (snapshot - 0.15), 放弃逐事件拦截

- 需求: 刚1 借见切(86)槽, 每次按下气刃被清; 目标 = 每次刚1 净消耗 0.15 (clamp ≥0)。
- **取证推翻单点拦截**: GAUGEEVT 全量直方图 (act=3:86) = -0.1 ×987 / -0.2 ×45 / +1.0 ×6,
  **全日志零条 -1.0** —— 09-24 "见切 -=1.0 清空"的记录与实测不符; 且实测序列里每招扣完后
  气刃都回到 1.0 (0.9→1.0 / 0.85→1.0 / 0.75→1.0) 而 GAUGEEVT **无对应事件** = 存在绕过
  FUN_142122410 的直接写入者 (未钉)。逐事件改值会漏, 放弃。
- **实现 = 用户方案的轮询版**: `ApplySheathRewrite` 改写命中 (toId==86) 时快照
  `+0x2368` → `g_gang1GaugeSnapshot`, 武装 scope (5s 兜底 deadline);
  `PollGang1GaugeScope` (property 钩子每帧): 当前动作离开 86 或超时 → 一次性写回
  `clamp(snapshot - 0.15, 0, 1)`, 日志 `GAUGERULE gang1 settle: gauge a -> b`。
  86 播放期间 vanilla 的花费/回填随便走, 结算时一笔归位 —— 净效果恒为 -0.15。
  只在改写路径武装, **自然见切(持刀 86)行为不变**; 后续连段招 (67/106/108...) 的
  消耗照常作用在结算后的值上。
- 待局内验证: 收刀刚1 一次 → 蓝条恰好 -0.15; 连段时后续招正常再扣。

- **(补丁) settle 竞态修复**: 首版 settle 读 controller 当前动作 (+0xAC/0xB0) 判"离开 86",
  但改写请求是**缓冲**的 (上一招还在播) → 当前动作仍是 {1,0} → 武装后第一帧即误结算
  (实测 settle 0.418→0.268 紧跟改写行, 之后 86 的 -0.1 和收尾继续扣 → 用户看到清空)。
  修复 = 改读 **tracker (请求流)**: 改写时 tracker 已是 86, 下一个请求到来才结算 —— 结算
  落在下招 begin 扣费之前, 连段消耗正确叠加在其上。部署待游戏关闭。

### 2026-10-04 (8) 刚1 气刃最终形态: 结算 + 1s 保护尾窗 (whiff 清空 = vanilla 见切设计)

- user 实测定案: **弹反成功 → x-0.15 ✓; 失败(打空) → 清空** = vanilla 见切的原版设计
  (打空丢条), 且失败清空是**结算之后**才落地的直接写入 (绕过 FUN_142122410, GAUGEEVT 无痕)。
- 实现 = 两相状态机 (`g_gang1Phase`):
  - **phase 1 (waiting)**: 改写武装 → tracker 离开 86 (5s 兜底)。vanilla 随便动, 无守卫。
  - **settle**: 写 `clamp(x - 0.15)` → `expected` 同值, 进 phase 2 (尾窗 1s)。
  - **phase 2 (guard tail)**: 账面 `expected` 记账 + 每帧对账 —— GAUGEEVT 可见的**负**事件
    (连段原生扣费) 叠上 expected (正事件 = 见切成功回条, 压掉, 维持 -0.15 净值);
    实际值偏离 expected >0.01 = 直接写入 (失败清空) → 打回 + `gang1 guard` 日志 (限10条)。
    尾窗结束再写一次 expected (吸收衰减漂移), 归 phase 0。
- **自吞坑**: 106/108 的 cost 规则走 trampoline 绕过 GaugeAddHook, 尾窗守卫会把自己扣的
  费打回去 → ApplyBeginGaugeRules 的 cost 分支补 expected 记账。
- 成功路径的净效果不变 (settle 压掉 vanilla 回填); 失败路径的清空在 settle 后 ≤1s 内被
  revert。部署 md5 7144d7df。验证: 打空刚1 → 蓝条也稳定 x-0.15 + 日志 `gang1 guard:
  reverted direct write`。

### 2026-10-04 (9) 刚1 气刃终版: 命中确认即撒手 (vanilla 回满设计入章)

- user 确认 vanilla 见切设计: 规避 → 回砍**砍到** → 气刃自己回满 (打空才清条)。守卫尾窗
  之所以"还是错的": 成功路径的回满是**命中确认时点的滞后直接写入**, 与 1s 尾窗的相对时序
  不定 —— 落窗内被压、窗外又弹回, 观感忽上忽下。
- 终版模型 = 顺着 vanilla 分叉: **`NotifyPlayerHitConnected` 里 phase==1 (刚1 窗口内) 的
  本地命中 → `g_gang1Phase=0` 整体解除** (`gang1 hit-confirm` 日志) —— 砍中 = vanilla
  自己回满, mod 全程不碰; **打空 = 无命中, scope 走完** settle x-0.15 + 尾窗 revert 清空。
- 语义: 刚1 砍中 = 免费+回满 (vanilla 奖励), 打空 = 0.15。部署 md5 ae2957a4。
  验证: 打空 → 稳定 x-0.15 + `gang1 settle`; 砍中 → 回满 + `gang1 hit-confirm`,
  全程无 settle/guard 行。

### 2026-10-04 (10) "空了"取证: 蒸发在持刀期, 刚1 本身清白 + GAUGEWATCH 每帧对账器

- ae2957a4 测试局账本: 唯一一次收刀刚1 **砍中了** (HITATT +110 → hit-confirm 解除 →
  气刃回满 1.0, 后两次 86 均从 1.0 起扣) —— 命中路径正确。**0.9 → 0.52 → 0 的蒸发无任何
  事件行** (无 67/106/108 扣费, GAUGEEVT 静默) = 持刀游玩期间 (自然见切打空/阶梯衰减) 的
  账面外直接写入; 最后收刀刚1 在 gauge=0 被 reject (minGauge=0.15 门), 出原版气刃斩1 =
  用户看到的"空了"。
- 新工具 **GAUGEWATCH** (PollGaugeWatch, property 钩子每帧): 对 weapon+0x2368 逐帧对账,
  |Δ|≥0.02 记 `GAUGEWATCH d=.. g=a->b act=set:id` (限200条/会话; 每帧衰减 ~0.0008 被阈值
  滤掉, 阶梯式衰减 -0.05/步会被抓)。一轮即可绘出全部隐形写入者的时刻/动作分布。
- 部署 md5 a46d04fc。待用户复测一轮 (含打空收刀刚1) 后读 GAUGEWATCH 定位蒸发源,
  再决定: 衰减要保留 (vanilla 经济) 还是给刚1 连段挂账面保底。

### 2026-10-04 (11) 成功信号纠错: HITATT ≠ 弹反成功, 换 bit15; 帧杀注入与回满互斥定案

- user 接受 vanilla 经济 (打空=清空是设计), 但测试打空刚1 "还是清空" —— 根因 = 上一版用
  **HITATT (回砍物理砍中) 当成功信号**: 贴着目标测试回砍必砍中 → hit-confirm 解除守卫 →
  vanilla 判定未弹反 → 清空无拦截。**物理命中 ≠ 弹反成功**。
- 修复: 解除信号换成 **event bit15 上升沿** (player+0x8938 & 0x8000, 见切成功锁存, 弹反链
  同款; arm 时快照基线防残留位)。HITATT 解除逻辑删除。
- **结构性定案 (09-25 老警告坐实)**: 收刀刚1 的 0.2s no-hit 注入把怪攻击排除在仲裁上游 →
  **叮/bit15/成功回满在收刀刚1 上根本不可能发生** ("帧杀保护与弹反计数互斥") → 当前模型:
  收刀刚1 恒 = x-0.15 (无论砍没砍中)。要真弹反 (叮+回满) 必须去掉注入试原生窗口
  (sheath-rewrite.txt nohitSec=0), 而 09-25 的 open question (FSM 停在待機1000 不进节点28,
  TIML 防御窗打不开) 大概率让纯去掉= 挨打 —— 真解法在状态侧/TIML 侧, 待排期。
- 部署 md5 82a5db9c。验证: 打空/砍中收刀刚1 都应稳定 x-0.15 (日志 gang1 settle, 无
  hit-confirm); GAUGEWATCH 继续监视条面蒸发。

### 2026-10-04 (12) 刚1 消耗语义对齐 + 恢复时机四触发 (user 定稿设计)

- user 对齐: **消耗恒 0.15, 与打中/弹反/打空无关, 唯一条件 = 用户出招** (入场门 = 红刃 +
  气刃 ≥0.15, 已有)。中间写入者众多 → 不拦截, **绕开**: 出招前快照 x, 到时机一次性恢复
  x-0.15。恢复时机 = 四者取先:
  1. **叮** (bit15 上升沿, player+0x8938 & 0x8000; arm 时存基线);
  2. **56 帧** (kGang1RestoreFrame, TIML 60fps → 0.933s; 用 86 实例 +8 帧计数, 取消即冻结
     不误触发);
  3. **controller 当前变为其它 set-3 动作** (86 结束/被打断 —— 出招即计费; 注意初段缓冲期
     controller 还是前一招 set1, 不会误触发);
  4. 5s 兜底。
- settle 后 1s 守卫尾窗保留 (负事件记账/正奖励压掉/账面外直接写入 revert) —— 兜住 56 帧
  之后才落地的 vanilla 清空。
- 日志 `GAUGERULE gang1 settle[parry(bit15)|frame56|left86|deadline]: gauge a -> b`。
- 部署 md5 3a7a7b79。验证: 打空收刀刚1 → 约 0.93s 时条 = x-0.15 并保持; 接连段照常再扣;
  HITATT 不再影响任何行为。

### 2026-10-04 (13) 武装点统一到 DoAction: 自然见切(持刀86)也纳入刚1 计费 + 测试无效局复盘

- 复盘 ae2957a4 测试局: **无任何 SHEATHREWRITE** —— 三次 86 (+68/+152/+234) 全是持刀自然
  见切, 打空 → vanilla 清空 (+70/+154/+236 GAUGEWATCH d=-1), 机制从未武装 (只覆盖收刀改写)
  → 用户测的招不在管辖内。且局内有**回满器**活动 (每次扣费后一帧内被补回精确扣减量:
  cost -0.25 → GAUGEWATCH +0.25 → 1.0) = 训练器气刃功能, 它开着任何 settle 都无法停留。
- 修复: 武装点从 ApplySheathRewrite 挪到 **DoActionHook post-accept** —— 任何本地玩家
  set-3 actionId==86 接受即 ArmGang1GaugeScope (收刀改写/持刀自然见切同一招同价 0.15;
  快照确定性先于起手扣费; phase==1 跳过/phase==2 重武装; LS 硬门控保留; probe 构建需
  #ifndef 包裹 —— gang1 全局在 probe 排除区)。ApplySheathRewrite 侧武装删除。
- 部署 md5 51cd4c4b。**测试前提: 训练器气刃功能必须关** (只留锁红刃), 否则 -0.15 无法停留。

- **✅ user-verified (2026-10-04)**: 训练器气刃功能关闭后, 收刀/持刀 86 (刚1) 打空/砍中
  均稳定结算 x-0.15。刚1 气刃链完结 (武装 DoAction 统一入口 + 四触发恢复 + 1s 守卫尾窗,
  最终形态 md5 51cd4c4b)。遗留备忘: 若未来要收刀刚1 真弹反 (叮+回满), 需解决 nohitSec
  注入与防御仲裁互斥问题 (09-25 open question, 状态侧/TIML 侧)。

### 2026-10-04/05 见切防御两层模型定案: L1 沉默无敌(状态侧) / L2 无效化交互(TIML 侧) + bit15 写入链钉死

- **user 判决实验 (10-04/05)**: entry 140 删掉**全部** TIML → 局内无敌帧依然生效, 但**无叮、
  无 bit15、无动作切换**。复现了 09-07 "I-frames that survive TIML deletion" 旧案并强制修正模型:
  TIML 防御窗 = 真实但只是**可授权的加法层**; 见切还有一层不经过 TIML 的防御。
- **两层模型**: **L1 沉默无敌** = FSM/动作状态侧授予 (09-25 收刀重写路径没有它 → 当时的"没无敌帧";
  查询层排除, 攻击"没发生过", 无叮无 bit15, 删 TIML 不死; 翻滚同族)。**L2 无效化交互** = TIML
  防御窗 (窗位+mTrigger 2048 结构; 140 原版长窗位实为 0x00400000, 旧文档表里写 "b26 f0-f130"
  是位编号笔误——结构才是重点) → 命中到达仲裁被判定无效 → 家族演出 (叮/蓝光) → hub 标志 →
  **bit15 上升沿** → 原生 763/764 派生重映射 + mod auto-force 刚1。pool+0xE4 注入 = L1 的
  查询层仿制品, **结构上永远产不出 L2** ("纯粹的防御")。
- **bit15 写入链钉死 (反编译 6430953 行)**: `FUN_141f6bd80` (玩家主更新每帧事件位传播步,
  调用点 6432955, 门控 player+0x1140 bit8) 每帧拷贝
  `if ((*(uint*)(*(player+0xe470)+0x170) >> 8 & 1)) { *(uint*)(player+0x8938) |= 0x8000;
  *(uint*)(player+0x8968) |= 0x8000; }` — **bit15 = [player+0xe470]+0x170 bit8 的逐帧拷贝**
  (状态阵列 +0x8938 与边沿阵列 +0x8968 同帧写)。同函数还有 bit10 ← hub+0xc bit0。
  player+0xe470 = 玩家 FSM/输入枢纽 (区域邻接 0xDE 属性链 FUN_141922190)。hub bit8 的置位方
  未钉 (别名写入静态不可见), watcher 一测即知。
- **mTrigger 语义破译 (新钉)**: mTrigger 值 = **事件位掩码**, 过帧即向 +0x8938/+0x8968 注入:
  2048=bit11 (b26 无效化窗的 commit/gate — 解释"窗内必须有 2048"配方), 1024=bit10,
  65537=bit0|16 (bit0=气刃增减公共门), 306 尾帧 f73=1024。**140 的 TIML 从不触碰 bit15** →
  bit15 只能来自 hub (状态/仲裁侧)。
- **特殊纳刀 306 无 TIML 无效化窗 (原版解码)**: mFlag 仅 f0=b25|b22 → f10 +b24 → f15+0x6
  (霸体 f10 起), mTrigger 仅 f73=1024 — **没有 b26/2048 结构**。用户实测其"防御帧(弹反)"为
  状态/原生层守卫, 不跟动作数据走。bit15 是**见切专用**锁存 (唯一消费者 = 派生重映射 763/764;
  居合用 bit250, 各机制各一位) → 特殊纳刀防御帧理论上不置 bit15, 待 VSEEK 微测确认。
- **FSM 可用属性新钉**: `オーラレベル赤以上` 存在于游戏属性表 (UTF-8 @ 文件偏移 0x30b8cd8,
  与 オーラレベル白以上 相邻) → 收刀路径 FSM 链接条件可原生表达红刃门。
- **干净化路线 (待判决实验)**: ① 140 写回标准窗 (105 模板 f0=121634822/f18=33554438/f120=6 +
  2048@f0), sheath-rewrite nohitSec=0, 测收刀刚1: 叮? bit15? force 刚1? → 成立则删
  dllmain.cpp InjectNoHitLocked(sheath); ② FSM-Studio 加 98→28 链接 (cond R AND
  オーラレベル赤以上) = 转向锁 + L1 + 28 出边派生; 进 28 后节点动作进程直发 86 (09-22
  "节点进入压过 request"), DLL 改写在红刃态变冗余保险; ③ bit250/0x23A0/CondEval631 三个
  备胎触发器在 bit15 可靠后退役。
- **部署 entry 140 现状 (10-04 dump)**: 用户已删全部 TIML; mFlag 曾为 f0=f130=0x80000000
  (无任何防御窗, 09-25 "没无敌帧"的另一半原因); 当前 mTrigger 已随删除消失。

### 2026-10-05 判定链全景钉死 + bit15 降级 (日志实锤, 推翻同日早前"bit15 见切专用"结论)

- **叮的真身 = 武器侧无效化事件处理器链 (反编译全读)**: 命中经仲裁判为无效化后向防守方武器
  派发**交互事件类型 8** → `FUN_1421038B0` (RVA 0x21038B0): 门控 = 实体标志
  (bit4→当前动作==0x45 / bit1 / bit10), 反应类型 4/7 直接返回; 路由 = byte
  `[weapon+0x3168]` (工厂 FUN_1421039F0, 0x3190 对象, 初值 0): 0→`FUN_142125060`
  (set3 且 id==86: +0x2399 一次性锁存 + FX 0x7D4=叮 + +0x23A0=playerParam+0x8A8),
  1→`FUN_142124f70` (id∈{101,102}: +0x2CED 掐点 + 0x7D4 + +0x23A4=+0x8B8)。
  动作结束 FUN_142123510 消费 +0x2399 → 气刃回满+计时延长。**FUN_142125060 只判
  "规避了"这半, 完全不碰回砍**; 第 2 半 (回砍命中) 在 0xDE 计数器 (idx0/idx2) +
  条件 631 层, 与动作 id 解耦。事件 8 的发送方 (仲裁上游) 仍未钉。
- **bit15 降级 (35MB 全历史日志统计)**: `bit15=1` 共 66 次 — 全部在收刀/移动动作
  (1:8×31, 3:9×16, 1:4×10, 1:55/56/0/15/451), **3:86 期间 0 次**; 全历史 counter
  window 触发统计: flash(+0x23A0) 275 / bit250 169 / 规则 66 / 旧路径 15 /
  **bit15 路径 0**。09-17 "bit15=见切成功锁存" 为误判 (被闪探测器抢先掩蔽);
  DLL 的 bit15 触发器是死代码, 备胎们才是真触发者。
- **bit15 唯一写入方** = `FUN_141F6BD80` (汇编逐字节核对): 每帧拷
  `[ *(player+0xE470)+0x170 ]` 的 bit8 → bts 0xF 进 +0x8938(状态)+0x8968(边沿)。
  全反编译仅此一处 +0x8938 的 0x8000 写点。hub 指针经用户实测**会话内稳定**
  (收刀/拔刀同值), 但 bit15 历史上在 1:8 升过 → hub bit8 的写入时机/路径未钉;
  见切叮时 hub+0x170 无写入 (hw 断点验证, DR 未被清, ANIM 时钟断点同法可断)。
- **新钩子 negrEvent (mod build)**: HkNegrEvent @ FUN_1421038B0, 签名已入
  sig_check (唯一命中), addresses.txt 键 `negrEvent`。被动日志
  `NEGREVT sel=<0x3168> latch2399=0x latch2ced=0x act=` (仅本地玩家武器, 上限 120 条,
  原调用先行)。**判决问题: 非家族动作 (如带 TIML 窗的 105) 被无效化时事件 8 到不到**
  — 到 (latch=0) → 任意招的叮+判定可在该 detour 泛化; 不到 → 需上挖仲裁。
- **实验部署 (md5 090914518f99d7acd4b6a18500eb0177)**: entry 140 已写标准窗
  (mFlag f0=2214592512=0x84000000, 清除键 f130→f45, 触发器 2048@f45 = 308 同构;
  工具自动 .bak); entry 105 保持验证配方 (b26+b24 f0–60 + 2048@f0);
  sheath-rewrite.txt = `86 0 3 0.15 0 11` (**nohitSec=0**, 注入关闭)。
  待测: E2=收刀红刃 R2 路径 TIML 窗是否给无敌 (停泊 FSM 状态下逐帧评估是否成立);
  E3=105 吃无效化时 NEGREVT 是否出现。

### 2026-10-05 (2) 防御/判定最终模型定案 (E2/E3 判决实验完成, 全链日志实锤)

- **E2 通过**: entry 140 写标准 b26 窗 (f0–45+2048@f45) 后, **收刀重写路径 (FSM 停泊在
  98/1000) 无敌真实生效** — dispatcher 层逐帧 REJECTED (3:86 期间 396 次, 怪物持续
  判定框每帧覆盖每帧被拒), 用户体感无伤+动作正常走完。**结论: TIML 窗逐帧评估不依赖
  FSM 节点进入, DLL 注入 (sheath-rewrite nohitSec) 正式可退役; 部署配置已置
  `86 0 3 0.15 0 11`**。
- **E3 定案: 非家族动作的 b26 无效化 = 查询层静默排除**。突刺 105 (同款窗) 在确认被
  击中的前提下: 零 REJECTED (dispatcher 根本没看到交互)、零 NEGREVT (handler 没被调)、
  无叮 — 与 09-18 翻滚结论同构。**叮/成功呈现对非家族动作不可泛化 (排除事件在玩家侧
  不可观测), 复用 86/101/102 家族槽位是唯一正道** — 现有 mod 架构 (刚1 骑 86、居合骑
  101/102) 即正确做法。
- **叮链全貌 (日志+caller RVA 实锤)**: 家族动作期间事件 8 每帧 tick 到武器**状态对象**
  → bVar10 门控过 (该帧确有被无效化交互) → FUN_142125060 (id==86 门) → FX 0x7D4
  (caller +0x21250c9) + 锁存。本会话 4 次叮全在 3:86; 0x7D4 每窗一次 (一次性锁存)。
- **双"武器"对象结构修正 (重大)**: 游戏有两个都叫 weapon 的对象 — ①**武器装备块**
  (*(player+0x76B0)): +0x9F8 类型/+0x2368 气刃/+0x2370 等级/+0x2CED? 所属待重标定;
  ②**武器状态对象** (0x3190, 工厂 FUN_1421039F0, vtable 0x1434D00C8, owner=玩家
  @+0xB40): 事件 8 tick 的接收者, FUN_1421038B0/FUN_142125060/FUN_142124f70 的
  param_1, +0x3168 路由在此。**笔记中 weapon+0x2399/0x23A0/0x2CED/0x3168 实际属
  状态对象**; 装备块上同偏移是别的数据 (NEGREVT 实测状态对象 +0x2399 恒 56=0x38)。
  闪探测器读装备块 +0x23A0 — 它被 iai 族动作开始参数拷贝 (FUN_14211E8A0 kind==7)
  每次起手点亮, **闪触发器 ≈ 每次见切起手误触发一次, 不是叮信号** (本轮 3 次点火
  ≈ 3 次起手, 钉死)。
- **bit15 降级补完**: 本会话 3:86 全程 bit15=0 (含真叮帧)。DLL 四个叮触发器的实际
  状态: 闪=起手误报, bit15=死代码, CondEval631=偶发, bit250=待重标定。
  **最可靠的真叮信号 = SheathDispatchHook 的 REJECTED 行** (dispatcher 层逐帧拒绝,
  只在家族动作的无敌窗出现) — 反击链触发器建议全部收敛到它。
- **工具**: negrEvent 钩子现为诊断版 (无过滤, 前 40 条全记, 带 weapon/owner/curWpn
  三指针+match), 稳定后应恢复 own 过滤 (own = weapon == *(ownerPlayer+0x76B0) 换成
  owner==lockedPlayer)。部署 md5 0e68408556a02d912512c11d8e1a9d6f。
- **用户原始问题最终回答**: ①蓝色无敌帧本体 = TIML b26+2048 窗, 纯数据、逐帧、
  任意动作可贴 (含停泊路径) — 泛化完成; ②叮/成功呈现 = 家族处理器对"dispatcher 可见
  无效化交互"的 id 门控反应, 仅 86/101/102 槽可触发 — 泛化即复用槽位。

### 2026-10-05 (3) 方案定案: 防御帧 + 临时防御值提升 (用户裁定)

- 家族叮链不可泛化已定案 → 自定义动作的安全层改用: **TIML 防御帧 (b26 无敌/b24 霸体,
  已验证任意动作可贴、含停泊路径) + 动作期间临时提升玩家防御值** (减伤续航, 非
  无敌)。防御帧侧配方/工具全部现成。
- **下一研究目标: 玩家防御值**。入口 = 伤害公式 FUN_141F66DD0 (RVA 0x1F66DD0,
  dmgCalc 钩子已在 mod build 常驻, a1=攻击者侧已验证)。要钉: ①防御力数值字段
  位置 (player 装备聚合值/playerParam); ②公式中防御项的读取点与格式 (减伤曲线);
  ③临时覆盖的注入位置 — dmgCalc detour 内改参 vs 直写字段 (前者对多人/多目标
  更稳)。可复用工具: FSDDMG 日志 + F9 struct-diff + F11。
- negrEvent 钩子保留为观测口 (诊断版无过滤, 稳定后恢复 own 过滤, own 改用
  owner==lockedPlayer 而非装备块指针)。

### 2026-10-05 (4) E4: mFlag2 b15 ≠ 弹反开关, 是事件位写入轨 (已回滚)

- **假设被否**: mFlag2 b15 单贴到非家族动作 (101 拔刀纵斩, b26 窗 f0–f120 + b15 f0–f50)
  → 防御整个失效 (必被炸飞); 对照组 102 (仅 b26) 无敌正常。b15 ≠ "弹反型开关"。
- **新解释 (强假设)**: mFlag2 与 mTrigger 同类 = **事件位写入轨** (值为位掩码)。b15
  (0x8000) → 事件 bit15 → 特殊纳刀重映射机器 (763/764, 162/163) 误判收刀态 → 输入
  重路由/招式被打断。这同时解释 bit15 历史上升全在收刀移动 (收刀系 motion 的 TIML
  带 b15 段 = 收刀态信号) — bit15 的真身 = **纳刀态信号位, TIML mFlag2 b15 所置**。
  (待验证: 采样收刀系 motion 的 mFlag2 轨道; VSEEK 若漏采 bit11=2048 是因边沿即时
  清除, mTrigger 2048 的位写入同理需重审。)
- **家族武装真身**: FUN_14211E8A0 与 FUN_1421038B0 同构 — param_2 为事件对象,
  vtable+8 取类型号 (==7 iai 族 / ==8 见切族)。泛化卡点 = 事件 7/8 的**投递端**
  只对家族动作发, 谁投递/判定条件未钉 (深挖方向: 状态对象事件流的上游 dispatch)。
- 101/102 实验改动已全部回滚 (expect-value 三连 setval, LMT 恢复原状)。
- 方案不变: 防御帧 (b26/b24, 已泛化) + 临时防御值 (FUN_141F66DD0 入口, 下阶段)。

### 2026-10-05 (5) 防御系统落地: 原生防御借用 + 血量/耐力/转身三钩子 (部署 md5 4c3c5c41)

- **用户定案**: 见切/特殊纳刀的"全角度无敌+自由转身"太强, 换成**带角度限制的防御**;
  防御的削伤缺点用"钩血量写入"抹平 (只要防御的被打中检测, 不要削伤); 玩家血量三值
  (动态上限 100/150/200 / 当前红条 / 可恢复黄条), 拦写必须三者自洽; FSM 侧后续把
  见切/特殊纳刀换回普通节点 (mod 按 id 挂窗, 不依赖特殊节点)。
- **逆向新钉 (三个研究面全清)**:
  - **血量**: vitals box `*(player+0x7630)`: **+0x60=maxHP(动态), +0x64=curHP(红条)**
    (微型 getter 族 0x141216780-0x141216A30: get cur 0x141216910 / get max 0x141216920 /
    比率 0x141216930 / set cur clamp 0x141216940 / set max 压 cur 0x141216980; ctor=100/100)。
    maxHP 重算 FUN_141f79cd0 (技能/吃饭/多人缩放 mgr+0x608>0 ×表)。
    **伤害唯一写入口 = FUN_141f518e0(player, delta)** (负扣正回; (0,1]→1.0 保底; 尾部
    `add rsp,0x40;pop rbx;ret` 无 rax): 内部读 cur→加 delta→0x141216940 写回。
    **黄条不在 box 里** = player+0xEBA8+slot*0x40 (登记: +0x30 量/+0x34 active) +
    player+0x1D78+slot*0x40 (每帧插值状态 [0]value/[1]base/[3]acc/[7]state), 由
    FUN_141f637a0 每帧把增量回灌红条 (经 FUN_141f628c0)。**消费顺序: 受伤红条全额
    先扣, 黄条只是待回填额度** → 拦掉 FUN_141f518e0 负 delta + 清两侧槽 = 完全免伤
    无幻影回血。免死 setter 0x141f72ab0/0x141f72ac0 (置 1 HP)。
  - **耐力**: box `*(player+0x7670)` 同款布局 (+0x60 max/+0x64 cur); 统一消耗/回复入口
    **FUN_141c49f10(player, delta)** (负=翻滚/常跑/防御消耗, 发 0x7de UI 事件; 尾部
    `add rsp,0x150;pop rdi;ret` 无 rax)。max-stamina 持久控制器 player+0x7638 (推测级)。
  - **朝向/坐标**: 权威朝向 = **em+0x170..17C 四元数** (Y-up Z-forward; yaw=atan2(fwd.x,
    fwd.z) 现算); 位置 = em+0x160/164/168 (怪物同布局); 修正: player+0x1510 是锁定目标
    方向向量**不是四元数**。转身系统唯一咽喉 = **FUN_141F755C0(em, yaw度, 时长, mode,
    turnKind)** (mode 0=空闲时/1=相对/2=绝对; 度; 第 5 参栈上 rsp+0x28; 尾部无 rax=void),
    每帧消费端 FUN_141f637a0 自带限速, 写 em+0xB220(请求)/0xB224(已应用)/0xB228(转身中)。
  - **原生防御 (可借用, 无需防御动作)**: 防住判定链 = 武器非远程 (9F8∉{11,12,13}) +
    **weapon+0x20F8** 防御等级经 FUN_142117110 (查表默认 2) + FUN_141e1feb0 守门 (>1) +
    FSM 条件求值器 FUN_140f0f830: 夹角 vs **武器参数块 `*(player+0x7810)` +0x940(下限)/
    +0x944(上限) 度** — **防御角度是每武器数据字段, 可直接写**。防住下游: 削伤
    (×_DAT_145012058+0x294/298, 运行时表), 后仰档 record+0x25C (0/1/3, 按 record+0x248/
    24C 阈值), 受击路由 FUN_142042140 case 0x16 → 每武器命令表 **{3,36/37/38}=
    DAMAGE_GUARD_S/M/L (14 武器全有, WP_00 表 0x142EEEF98)** — 太刀收刀被路由到 3:37
    的历史实测由此解释。攻击侧 mGuardType@攻击数据+0xD0 = ガード不能标记。
- **mod 实装 (16 钩子; 新增 turnSet/hpDelta/stamDelta, 签名全库唯一, sig_check.py
  TARGETS 已补)**:
  - 开窗 (DoActionHook 后置挂点, 改写后的 id 也认): GUARDOPEN = 锁起始 yaw (四元数现算)
    + 保存并写 weapon+0x20F8=guardLevel + 参数块 +0x940=-arcHalf/+0x944=+arcHalf;
    关窗 (动作变化/deadline 6s/武器丢失): GUARDCLOSE 全部恢复。
  - **"防住了"双信号**: ①GUARDREACT = 引擎排防御反应 {3,35..38} 被 DoActionHook 捕获
    (默认吞掉保持招式继续, swallowReaction 可关); ②GUARDARM = DmgCalcHook 里按
    attacker→player 位置角自查 (±arcHalf, 贴脸<1m 视为正面)。
  - **免伤**: GuardHpDeltaHook (FUN_141f518e0 detour): 窗口内本地玩家负 delta 且
    (react 1.2s latch ∨ armFallback 0.8s latch) → 整条跳过 + 清 4 槽黄条额度 +
    gaugeRefill 写 weapon+0x2399=1 (vanilla 见切成功锁存, 动作结束处理器自动回满气刃)
    + GUARDBLOCK 日志 (path/delta/hp)。无 latch 的窗口内扣血 = GUARDPASS 放行 (角度外
    该掉血就掉血)。
  - **耐力返还**: GuardStamDeltaHook (FUN_141c49f10 detour), 仅 react latch 门控。
  - **转身钳制**: GuardTurnSetHook (FUN_141F755C0 detour): 起始 yaw ±maxTurn 度
    (mode 2/0 绝对钳; mode 1 相对先算绝对目标再反解输入), TURNCLAMP 日志。
  - 配置 `nativePC/plugins/ValorGauge.guard.txt` (缺文件=功能关; Shift+F6 热重载):
    `guard 86,99 90 30 2 1 1` (arcHalf/maxTurn/level/stamRefund/gaugeRefill) +
    option maxWindowSec/armFallback/swallowReaction/counterOnBlock(默认 0)。
- **设计取舍**: GUARDBLOCK 默认**不**开反击窗 (counterOnBlock=0) — 86 槽位现在就是
  刚1 本体; TIML 旧 no-hit 窗 (entry140 f0-f12) 本轮未动, 与防御共存, FSM 换普通节点
  时再决定去留; armFallback 默认开 (引擎路由万一在 LS 上死掉也能保命), 局内 A/B 看
  GUARDBLOCK path=react/arm 分布后再定是否关。
- **局内验证清单 (待 user)**: ①GUARDOPEN/TURNCLAMP: 86/99 期间转身是否被限 ±30°;
  ②正面挨打: GUARDREACT(吞)+GUARDBLOCK path=react, 血条不动; ③背后挨打: GUARDPASS,
  正常掉血; ④超伤招正面: 不死; ⑤黄条: 防住后无幻影回血 (黄条额度被清); ⑥关窗恢复:
  GUARDCLOSE 后其它武器/正常防御行为无残留 (weapon+0x20F8/参数块弧已还原)。

### 2026-10-05 (6) 防御轮 2: 根因=架势持续动作 3:98 不在窗口 id 里 + VIT 追踪器 + HP 锁 (md5 d5daf4d2)

- **第一轮局内实测复盘**: 有防御音效但照样掉血, 日志零 GUARDOPEN/GUARDBLOCK — **窗口从未开过**。
  用户在特殊纳刀**架势持续动作 3:98** 里挨打 (12×`SHEATHGUARD: reaction 3:37 swallowed during
  3:98`), 而窗口 id 只有 99/86: 99 是进入动作, 进架势即变 98 → 立刻关窗。**叮 = 架势原生防御点**
  (引擎在 3:98 本来就路由 {3,35/36/37}, 不需要我们的 0x20F8 写入), 削伤照扣。
  (颜色更正 user: **绿条=当前血量, 红条=可恢复血量**; box+0x64=绿条=致死血量, 红条=待回填额度。)
- **修复**: ①窗口 id 默认 `86,98,99` (98=架势持续); ②GUARDREACT 吞让位: 先算 reassertActive/
  watchedHold, **架势里 (watchedHold/reassert) 不吞**, 反应走原 SHEATHREASSERT→SheathArmHitRule
  流程 (force 3:100 保持), 我们的 latch 仍记 → HP 免伤照常; 非 watch 的招 (86) 才吞反应保招式;
  family 扩到 {35..38}; GuardOnReactionAction 改 void (latch+log only); GuardConfig/g_guardCfg/
  g_guardActive 定义上移到 DoActionHook 前 (hook 体直接读)。
- **VITSET/VITADD 追踪器 (HkVitSet=0x1216940 / HkVitAdd=0x1216880, 签名唯一)**: 两个底层 box
  setter 全钩 (set cur min(v,max) / cur+=delta), 按 box==本地玩家 HP box(+0x7630)/耐力 box
  (+0x7670) 分类, 记 `VITSET HP prev->v caller=+0xRVA GW=?` — **谁扣血/扣多少/哪个函数扣, 一局
  定案**, 顺带验证 FUN_141f518e0 是不是唯一伤害入口 (若 chip 走别的 caller 立刻现形)。
- **HP 锁验证工具**: `ValorGauge.hp-lock.txt` = "<值>" (0=关; 已建文件=0), >0 每帧写 box
  max+cur=值。**验地址**: 填 200 → HUD 显示/保持 200 = 地址对; 锁定期挨打, VITSET/VITADD 行
  可见游戏写入与我们覆写的搏斗。Shift+F6 热重载。
- **待 user 验证**: ①hp-lock 200 → HUD 200? (地址定案) ②特殊纳刀架势挨打 → GUARDOPEN(98)+
  GUARDREACT+GUARDBLOCK path=react, 血条不动, SHEATHGUARD 流程照旧 (force 3:100 仍触发);
  ③86 正面/背面 A/B; ④VIT 追踪行里伤害的真实 caller RVA (若非 FUN_141f518e0 链, 免伤钩子
  换靶)。

### 2026-10-05 (7) 重大翻案: +0x7630 box = 锋利度, 整个"vitals"簇是磨刀系统 (md5 6bd2a516)

- **user 实测定案**: hp-lock 写 `*(player+0x7630)` box → **改到的是武器锋利度**。之前
  agent 报告的"血量 getter 链" (FUN_141f62320/141f62730/141f62330 → 0x7630 box, maxHP 重算
  FUN_141f79cd0, 喝药 FUN_141f628c0, 伤害 FUN_14121ea10, 黄条槽 +0x1D78/+0xEBA8, "cur≤max
  clamp" setter 族) **全部重新归类为锋利度系统** — 砥石="回复 case"、磨刀技能="max 重算"、
  攻击磨刀="伤害扣减"。MinerReadHealth(0x7630) 同错 (Lua 引擎布局假设, 从未局内验证)。
  旧 SheathArmorHitCheck 的"钳 1 HP 免死"实为钳锋利度。
- **动作 id 更正 (user)**: 98=纳刀起手/保持, 99=正式进入纳刀态; 当前 TIML 改造让动作停在
  98 不进 99 → 防御窗口挂 **98 才有效** (99 留在 id 表无害)。
- **mod 侧地址可插拔 (本轮)**: `ValorGauge.vitals.txt` = "hp <槽>" / "stam <槽>" (hex, 缺省
  7630/7670, Shift+F6 热重载)。**真槽位未配置前: 依赖血量地址的写路径 (黄条额度清理/
  SHEADRULE 免死钳制) 自动禁用**; 血量拦截钩子本身不受影响 (跳过整个写调用, 与地址无关)。
- **VITSET/VITADD 追踪器升级 (HkVitSet=0x1216940/HkVitAdd=0x1216880, 两个底层 box setter)**:
  分类器先按配置槽匹配 HP/STAM, 否则**自动扫 player+0x7400..0x77F0 指针槽** (步长 8) 找
  box 归属 → 日志 `VITADD ent -23.5 (100 -> 76.5) slot=+0x76XX caller=+0x...`。挨一刀即
  现形: 真血量 box 若在 1 级指针槽上, slot= 字段直接给出槽位; 不在则给 box 绝对地址
  (x64dbg 对照)。ent 配额 64 条。
- **x64dbg 找血量槽位手册 (user 执行)**: ①进游戏拔刀 (控制器锁定), ValorGauge.log 抄
  `ADDR player=0x...`; ②x64dbg attach MonsterHunterWorld.exe; ③Ctrl+G 跳 player 地址,
  在 +0x7400..+0x7800 找槽位; 或直接按值搜: 当前 HP 整数 (如 100) 以 float 在
  [player, player+0x22000] 搜 → 挨一刀变 87 → 再搜 87.0 交集 → 定位槽位/box;
  ④把槽位填进 vitals.txt (`hp 0x7XXX`) Shift+F6。
- 下一步 (user 验证链): vitals.txt 填真槽位 → hp-lock 200 验证 HUD → 复测 86/98 防御
  (GUARDREACT+GUARDBLOCK path=react, 血不动) → 看 VITADD 里真伤害 caller (确认是否
  FUN_141f518e0 链; 若 chip 走别的 caller, 免伤钩子换靶)。

### 2026-10-05 (8) HunterPie 破案轮: +0x20F8=锋利度铁证 + HPWATCH 周期日志 (md5 271c2c99)

- **用户提供 research/HunterPie 源码** (`HunterPie.Integrations/Datasources/MonsterHunterWorld/`),
  三记连环翻案:
  1. **MHWSharpnessStructure: weapon+0x20F8 = 锋利度(打击数 int), +0x20FC = 锋利度颜色** —
     上一轮 agent 的"防御性能指数"判定错误; GuardOpenWindow 已**移除 +0x20F8 写入** (那是在
     开窗期间把刀打成红锋利度!)。原生防御借用的"等级"前提作废: 3:98 的防御反应是架势原生
     防御点, 与 0x20F8 无关。
  2. **MHWHudStructure** (从 EQUIPMENT_ADDRESS=base+0x50139A0 链读, PLAYER_BASIC_INFORMATION_
     OFFSETS=[0x50,0x7630,0x0]): **MaxHealth@+0x60 / Health@+0x64 / Stamina@+0x12C /
     MaxStamina@+0x130 / RecoverableHealth@+0x2DD4 / HealingArrayPtr@+0x30** — 即 HunterPie
     认为 *(player+0x7630) 容器本身就是 HUD 结构; 但 user hp-lock 实测同 box 改的是锋利度,
     两说矛盾 → **不猜, HPWATCH 一锤定音**。Healing 数组 = *(c+0x30)+0xEBB0, 4 项 ×0x40
     (项内 +0x10 Heal/+0x18 MaxHeal/+0x2C Stage; 0=不在恢复/1/2 两段恢复) — 上轮 agent 的
     "player+0xEBA8 黄条槽"实为该数组 (基址差一个解引用)。
  3. **链语义校准** (ReadPtrAsync: 加偏移→解引用, 末级只加): ABNORMALITY_OFFSETS=[0x50,
     0x7D20,0x0] → *(player+0x7D20)=mgr 与我们已钉事实吻合 ✓; 玩家链根 = base+0x50139A0
     (421810 build; 旧 build 0x5011760), [root+0x50]=player — 与 spiritreckoning/ValorLS
     的 +0x50 根吻合。
- **实装 (md5 271c2c99)**: ①PollHpWatch: 每 90 帧打 `HPWATCH hpBox=[max,cur] hudStam=[..]
  rec=.. heal=h/ms+.. box7670=[..] sharp=int/color cfgHp=[..]`, 上限 400 行 — **挨一刀看哪个
  数字跟着 HUD 掉 = 真血量字段**; ②GuardOpenWindow 不再写 0x20F8; ③vitals.txt 槽位机制不变
  (真槽位确认后仍用它接管)。
- **悬案**: +0x7630 box 的 HP(HunterPie) vs 锋利度(user hp-lock 实测) 二选一待 HPWATCH
  定案; player+0x7670 box (FUN_141c49f10"耐力"钩) 语义同待验 (HunterPie 耐力在 hud+0x12C,
  不是 box) — 若 7670 也不是耐力, GuardStamDeltaHook 换靶。

### 2026-10-05 (9) 血量定案 + 免伤重构到 setter (md5 120eba28)

- **HPWATCH+VIT 局内铁证**: `VITSET HP 200 -> 199 slot=+0x7630 caller=+0x1218E18` —
  **血量 = *(player+0x7630) box 定案** (+0x60=max(=200 吃满), +0x64=cur), HPWATCH 的
  hpBox 值逐 tick 跟随 HUD 掉血 (200→199→198→197 1 点/跳 = 持续伤害类)。HunterPie 的
  HUD 结构判定**正确**; user 之前"hp-lock 改到锋利度"实为**旧版开窗 +0x20F8=2 写入**的
  锋利度掉落 (时间线吻合: 不在挨打时, 在做招开窗时)。
- **真伤害写入链**: 未知函数(返回地址 RVA +0x1218E18) → **底层 setter 0x141216940** —
  **不经过 FUN_141f518e0** (旧免伤钩子拦不到的根因)。fun_141f518e0/14121ea10 链要么是
  别的伤害类要么是反编译误读, 弃用为免伤依据。
- **免伤重构**: 否决点移到 **GuardVitSetHook (HkVitSet=0x141216940)**: box==本地玩家
  hp box + value<cur + 窗口内 + (GUARDREACT latch ∨ GUARDARM latch) → **跳过原写** +
  清黄条真数组 `*(box+0x30)+0xEBB0`×4 (Heal@+0x10/Stage@+0x2C 清零, 替代错误的
  player+0x1D78/0xEBA8) + **耐力回满** (box+0x12C←box+0x130; +0x7670 box 实测恒 [0,0]
  不是耐力, HunterPie 耐力=hud+0x12C/0x130 ✓ hpwatch 实测 175↔200) + gaugeRefill
  (+0x2399) + GUARDBLOCK vitset 日志。GuardHpDeltaHook (FUN_141f518e0) 保留为次级
  (清额同真数组)。
- **开窗写入全部退役**: param 块 +0x940/+0x944 原值实测 [1200,0] — 非角度, agent 的
  "防御弧字段"也错; GUARDOPEN/GUARDCLOSE 现在零游戏内存写入, 窗口只做: 起始 yaw 锁 +
  转身钳制 (FUN_141F755C0 detour) + react/arm latch + setter 免伤。原生防御路由本来
  就不依赖我们的写入 (3:98 实证)。
- **验收 (user)**: ①纳刀架势(98)/见切(86) 正面挨打 → `GUARDOPEN` + `GUARDREACT`(架势)
  + `GUARDBLOCK vitset negated` 血条不动; ②背身挨打 → GUARDPASS/VITSET 正常掉血;
  ③锋利度不再异常掉; ④大伤害招防住 → 无红条残留 (heal 数组同步清)。

### 2026-10-05 (10) 时序翻案: 引擎先扣削伤后发反应 → 回滚式免伤 + 致死击穿保护 (md5 e5d9affd)

- **局内实测**: 正面防住仍掉血。日志时序: `VITSET HP 200->179 (GW=1)` 在前,
  `GUARDREACT 3:37` 在后 — **引擎的削伤扣减先于防御反应动作入队**, react latch 追不上
  同一击; 第二组 `44.7 -> -76.3` 直接致死 (死亡置 0 由 caller=+0x121EDFD 写)。
  另: GUARDED 命中**不走 FUN_141f66dd0** (整场零 GUARDARM), arm 兜底在防住路径无信号源。
- **修复 (回滚式免伤)**: GuardVitSetHook 三分支 — ①react/arm latch 已置 → 直接否决;
  ②无 latch 但 value<1.0 (致死击穿) → 直接否决 (path=lethal, "超高伤害不能死"的硬保证,
  不依赖时序); ③其余削伤放行但记 `g_guardRestoreHp=prev` (600ms 窗) →
  **GuardOnReactionAction 收到反应动作瞬间把 cur 回滚到挨打前值** + 清黄条数组 +
  耐力回满 + gaugeRefill, 日志 `GUARDBLOCK restore a -> b`。视觉 = 血条闪一帧回满。
- GuardApplyBlockBenefits 提取共用 (否决/回滚两路同享: 黄条清+耐力满+气刃锁存+可选反击窗)。
- **待 user 验收**: ①架势/86 正面防住 → 血条闪一下即回 (restore 行), 大伤害致死击穿
  (lethal 行); ②背身照常掉血 (restore 只在 react 后触发, 无 react 的背身伤不会回滚 —
  这是判定"防没防住"的引擎裁决, 不是我们自己的角度); ③连续多击看回滚值是否合理
  (snapshot 只记最近一次 prev, 多击间隔内第二击会覆盖)。

### 2026-10-05 (11) 防御可用定版 + 误免伤收紧 (commit 8f4cfb0 + md5 f764da17)

- **user 定性: 防御路线可行**, 大多数情况无伤; 先提交 (8f4cfb0) 后继续调。
- **bug: 不在使用勇气纳刀时被打飞却保持血量** — 两个误免伤通道:
  ①**react latch 预否决**: 引擎先扣血后发反应, latch(1.2s) 永远罩不住自己那一击, 只会
  把"挡住之后 1.2s 内的后续挨打"全部白免 → **撤销 react 预否决** (回滚机制已覆盖防住
  那一击, latch 只保留给耐力返还/reactId 记录);
  ②**armFallback 窗口内误罩**: 98 窗口 6s, 窗口内任何正面伤 (含非防御点帧) 走
  DmgCalc→arm→免伤 → **arm+致死保护限定 armIds** (新配置 `armIds <ids>`, 空=全部;
  部署默认 `armIds 86` — 86 无原生防御点, 方向免疫是设计; 98/99 只靠原生 GP+回滚,
  非防御点帧伤害照常成立)。GuardCurrentActionInArmIds() 以 DoAction 跟踪的当前动作
  实时判定, setter (arm/lethal 两分支) 与 NotifyGuardDamageHit (武装时) 双重门控。
- 部署 guard.txt 重写 (含 armIds 86 + 注释), md5 f764da17。
- **已知残留**: ①回滚快照单槽 (多击间隔内后击覆盖前击 prev, 极速连击第二击可能只回滚
  到第一击后值); ②GUARDREACT 吞反应在非 sheath-watch 的招 (86) 上默认开启 — 若想看
  原生防御表现可 swallowReaction 0; ③GP 防御 crush (大伤害破防) 若引擎仍排 35-38 会
  回滚 — 观察后再定。

### 2026-10-06 (12) 86 真身纠正 + 时间窗绑状态机 (md5 e201dcc1)

- **user 纠正 86 真身**: 当前刚1 = 见切改的, **FSM 绑定无敌帧** (TIML 修改无效) —
  天生全角度无敌, 这就是开题抱怨的"太无敌"本体; 防御实验一直是在 **98** 上做的
  (86 上从未见过 GUARDARM 的根因: 无敌帧在上游拒掉命中, 伤害计算不跑)。86 的
  防御化等 FSM/普通节点迁移; armIds=86 目前是占位。
- **user 状态机模型 (时间窗的裁定)**: ①3:98 中被打飞 → 状态必然离开
  {98,35,36,37,100} 家族 (可检测); ②削伤与防御生效间隔 ≈1 帧 ("绝对达不到1.2s");
  ③98 防住 → 必转 3:35/36/37 (可检测)。**1.2s/600ms 的来历**: 逆向说伤害经事件位
  队列每帧结算, 我给的大余量 — 实测 (VITSET 伤害与 GUARDREACT 同帧相邻) 证明不需要,
  大窗口只制造了误免伤。
- **实装**: react/restore/arm 三个 latch 统一为 **blockWindowMs (默认 150ms,
  option blockWindowMs 可调 30-1000)**; 部署 ids 扩为 **86,98,99,100** (100 = 架势
  家族状态, user 列举的家族成员)。
- **验收语义 (user 模型)**: 98 中 GP 防住 → 转反应动作 → 回滚生效; 98 中被打飞 →
  状态离开家族 → 无回滚, 伤害成立; 86 → FSM 无敌帧原样, 与本系统基本无关。

### 2026-10-06 (13) 3:100 真身: mod 自己的防住追击, 不进防御窗口 (config-only)

- **user 纠正**: 3:100 是我们代码写的 — **98 中检测到防住 (引擎排 35/36/37) → mod 吞反应
  + 强制切 3:100** (既有 SHEATHGUARD/SHEATHREASSERT 流程, force 目标)。即 user 视角的
  "防住可检测" = 吞反应切 100 这个完整链条; 100 是防住后的追击招, 不是引擎架势家族态。
- 上一轮把 100 误加进窗口 ids 已回退 (部署 ids = 86,98,99): 100 期间无防御窗口, 追击招
  受击正常掉血。想让追击也带防御 → guard.txt 里手动加 100 即可 (Shift+F6)。
- 98 防住一击的完整事件链 (全可检测): 削伤 VITSET 落地 → 引擎排队 35/36/37 →
  DoActionHook: GUARDREACT latch+回滚 (GUARDBLOCK restore) + sheath 流程吞反应 + force
  3:100 → 玩家进入 100 (窗口关闭, 100 内正常受击)。

### 2026-10-06 (14) 耐力控制模块: drain/cost/emptyExit (md5 33a384f2)

- **user 设计要求**: 一般防御掉体力但我们的防御不掉(现状, 暂留); 勇气太刀招式**应该**
  耗体力 — 见切(86)=耗体招(一次性), 特殊纳刀(98)过程**持续耗**, 耗尽强制进收刀态+
  疲劳(机制后做, 先验证可控性)。
- **真耐力定案**: hp box+0x12C(cur)/+0x130(max) — HPWATCH 实测跟踪 HUD (跑步 175↔200)。
  引擎每帧更新器 FUN_141C4A260 经 vtable+0xC68(耗)/0xC70(回) 虚调用到写入点(静态止步,
  控制不需要钩它 — 直读写字段, gauge-lock 同款)。疲劳锚点备用: player+0x208EC(阈值),
  +0x20914(力竭 flag)。+0x7670 box 恒 [0,0] 非耐力。
- **实装 (ValorGauge.stamina.txt, 缺文件=关, Shift+F6 热重载)**:
  `drain <ids> <perSec>` 持续扣(实时 dt) / `cost <ids> <amount>` 接受一次性扣
  (DoActionHook post-accept) / `emptyExit <ids> <toSet> <toId>` 归零时写缓冲请求槽
  (controller+0xBC/0xC0, spiritreckoning 同款原语) 强制切招(一次性 latch, 动作变更重置)。
  部署默认: drain 98@10/s, cost 86-20, emptyExit 98→1:0(收刀 locomotion, 待 user 调)。
  日志 STAMCOST/STAMDRAIN(整数变化跳变记)/STAMEXIT, HPWATCH hudStam 字段可对照 HUD。
- **待验证**: ①86 出招 → STAMCOST 掉体力; ②98 架势 → STAMDRAIN 持续掉(注意引擎自然
  回复与之对抗, 速率可调); ③耗尽 → STAMEXIT 是否真把玩家切到收刀态(缓冲槽在架势动作
  可派生帧被消费?) — 若槽不被消费, 换 force-DoAction 路径; ④疲劳状态(+0x20914)后接。

### 2026-10-06 (15) 耐力轮 2: 86 原生耗体(代码写死) + 绝对值压制 drain (md5 3732023e)

- **user 实测**: ①见切消耗 ≠20 — **86 自带代码写死的体力消耗** (删光 TIML 照样掉),
  我们的 cost 20 是叠加 → 部署配置撤下 cost 86 (控制原生值需先钉 +0x12C 真写入者;
  VIT 追踪器看不见 — 不走 box setter; 引擎更新器经 vtable+0xC68/C70 虚调用)。
  x64dbg 路线: HW 断点 WRITE [box+0x12C] (box=[player+0x7630]) → 出 86 抓 caller RVA。
  ②98 只在开头掉一点 — 增量式扣跑不赢引擎每帧回填 → **drain 改绝对值压制**: 进招记
  快照, 每帧拉到 max(0, 快照-速率×elapsed), 只降不升, 引擎回填即被压回 (帧内锯齿可忍)。
- **力竭 vs 疲劳 (user)**: 两个不同状态 — 力竭疑似"力尽倒下"(类死亡), 疲劳 = 停下喘气。
  机制阶段分开处理, 锚点 +0x208EC/+0x20914 归属待定。
- 部署 stamina.txt: drain 98@10/s(绝对压制) + emptyExit 98→1:0 + cost 86 注释掉。

### 2026-10-06 (16) 再纳刀直接空 bug + 两段退出 (md5 ff49abb7)

- **bug 修复 (user: "后续我再按纳刀,体力条直接空了")**: 切到收刀态 (set 1) 后轮询在
  set!=3 提前 return, 绝对压制快照永不重置 — 再进 98 时 elapsed=巨值 → target=0 →
  秒空。修复: **StaminaOnActionAccepted 在任意动作接受时无条件 disarm 快照**, 重进招
  从当前体力重新起算。
- **两段退出 (user 设计)**: **1:324 = 喘粗气态**(user 提供); 耐力耗尽 → 先排收刀
  (phase1) → 轮询看到 phase1 动作成为当前 → 马上排 1:324 (phase2) → 到达后 stage 清零。
  `emptyExit <ids> <toSet> <toId> [thenSet thenId]` (后两参可选); 部署
  `emptyExit 98 1 0 1 324`。stage 机跨动作接受存活, 3s 超时兜底, phase2 未配置则
  单段结束。PollStaminaControl 去掉 set!=3 提前 return (exit stage 机要跟踪 set 1)。
- **user 语义记录**: 力竭(力尽倒下, 类死亡) ≠ 疲劳(1:324 喘粗气) — 两个状态。
