# MHW Long Sword LMT/TIML Research Notes

## Scope

This note records the current discoveries about Long Sword attack hitboxes, invulnerability frames, super-armor frames, and true weapon sheath state in Monster Hunter: World. The goal is to provide a practical starting point for researching and editing newly added motions. Need to verify the correctness of this doc.

## Confirmed observations

### The behavior is present without the DLL

The files under:

```text
E:\SteamLibrary\steamapps\common\Monster Hunter World\nativePC\hm
```

are a copy of the Spirit Reckoning mod's `nativePC\hm` files. The attack behavior, including super-armor behavior, still works when the related DLL files are absent. This proves that the relevant behavior is authored in game resource files, not supplied exclusively by native code.

### Replacing only the LMT changes the hit behavior

In the MHRS-based test setup, replacing the modded LMT with the original game's LMT changes the visible/functional hitbox behavior. Therefore the LMT is an important source of per-motion attack data.

The working Long Sword resource is the weapon motion bank:

```text
hm\wp\wp03\mot\w03_00\w03_00.lmt
```

(`wp03` is the Long Sword data in this mod setup.)

### Blender test file

The test file is:

```text
E:\MHW-arts\mhrs-mhw-armature-mapping-test.blend
```

Free Kinetics loads the LMT bank as a node tree named `w03_00`. The file contains LMT action entries and linked TIML action nodes.

Relevant entries observed:

```text
LMT::w03_00::101   frame range 0-152
LMT::w03_00::307   frame range 0-139
```

The Blender file contains separate TIML action nodes for categories including:

```text
EventCollision00
PlMotionCommon
PlMotionInput
GameParameter
EventGroup
```

This confirms that a motion is not just skeletal animation. It is an LMT animation plus linked event/parameter timelines.

## Verified TIML mechanism (2026-09-07 dump of w03_00 / w03_00-origin / w03_00-rs)

Binary dump via `dump_timl_frames.py` + `scan_defense_bits.py` (no Blender needed);
output in `timl_dump/`. Blender node suffixes like `PlMotionCommon.102` are just
duplicate-name counters, NOT game IDs. A TIML block links to an LMT entry via
`timlOffset` in the LMT action header; `frameTiming` values ARE 30 fps logic frames.

### Move identity (from action-list.md)
101=拔刀纵斩(normal), 102=踏步斩, 105=突刺, 108=气刃3, 109=大回旋, 110=气刃2连,
127=右袈裟, 140=看破斩(见切), 306=特殊纳刀, 307=小居合, 308=大居合(居合气刃斩),
309/310/311=大居合 variants.

### Attack frames
- `nWwiseTimeline::EventCollision00.mFlag` = activation state machine: 0=off,
  1/2/3=collision slot group. These are the damage windows.
- `CollisionTimelineObject` = per-window config (`mFlag` on/off, `ノードID[0]`,
  `バンク[0]`). `EventCollision01` = secondary window (e.g. 308 f17-62, bit16 =
  the counter window).
- Geometry/damage live in `.col` + damage tables referenced via ReqNo/NodeID/Bank.

### Defense bits in `PlMotionCommon.mFlag` (bit 23+ scanned bank-wide)
- `b25` = universal attack-motion baseline, held whole move.
- `b26` = **i-frame (no-hit) window**. 307: f0-5 (0.17s), 308: f0-12 (0.4s).
  The Spirit Reckoning mod's ONLY change to 308's TIML moves b26's clear key
  f12 -> f18 (buffs iframes to 0.6s) — strong confirmation.
- `b24` = **super-armor window**, nested longer: 307 f0-10, 308 f0-25 (0.83s),
  306(特殊纳刀) f10-15. Nested b26 ⊂ b24 = "先无敌后霸体" (matches gameplay).
- Long pure-b26 moves elsewhere: 214-217 (1.17s), 351 (4.33s), 451 (2.93s).
- `b31` held: 140 见切 f0-130 (4.33s), sheathe-family 10-13 f0-40. 见切's observed
  ~0.5-1s iframes do NOT match a held 4.33s window, so b31 is unconfirmed
  (FSM-gated? or not the iframe source). In `mTrigger` tracks b29/b30/b31 are
  one-shot event codes — different namespace, don't mix.
- `b27`: opening window in 115 (vanilla f0-10; rs extended to f0-45) + mFlag2
  f10-136, and end-of-move pulses in spirit moves 108/109/338-340. rs-115's
  in-game armor died with the PlMotionCommon block, so its armor bit is b27
  and/or b31. Semantics unresolved.
- 101 (normal attack) has NO b24/b26 — no defense, as observed.

### I-frames that survive TIML deletion
Deleting all TIML from a move left its i-frames intact, and header Flags/Flags2
don't correlate. Possible reconciliations: the surviving "invincibility" was
actually the 居合/见切-family guard point (FSM-provided, like the Valor guard
point), or native code grants iframes by action id, or FSM state properties.
Next experiments:
1. 308 single-bit test: clear only b24 (expect: iframes stay, armor gone);
   clear only b26 (reverse). Pins both bits definitively.
2. 见切: move b31's clear key f130 -> f15; if the ~1s iframe is unchanged, its
   iframes are not TIML-side — hook the player no-hit flag writer next
   (same approach as the spirit-level search).

### Two mod-side defense mechanisms (decompiled, 2026-09-07)
- `Reverse/ValorLS.dll.c` (early valor mod): i-frames are a RUNTIME MEMORY WRITE,
  not resource data. Flow: hooks the action-request function at 0x14119faa0
  (msg struct {+0 type, +4 id, +0x10, +0x14 action-id}; writes +0x10=3 and
  +0x14=<action id> to force actions); on the skill key it requests action 0x6a,
  spawns a thread that Sleep(1000)s, verifies current action == 0xc153, then
  writes **weapon+0x2388 = 90.0f** (float; game decrements it per frame; 90 =
  3 s). Also reads weapon+0x2388 (>0) as its "skill active" gate, writes
  spirit level 0x2370/gauge 0x2374/0x2390, zeroes 0x472, NOPs 8 bytes at
  0x14213fa12, and hooks 8 game addresses total. ValorGauge can replicate this
  directly: on custom-action start, write weapon+0x2388.
- `Reverse/spiritreckoning.dll.c`: NO hooks, no memory writes. SR's defense is
  purely resource-side (the hm LMT TIML edits), consistent with it working
  DLL-less. Two mods, two completely different routes.
- Copying 308's PlMotionCommon block onto 105 (突刺) had NO effect. Candidates:
  (a) 105 already has a PlMotionCommon block and the game only honors the first /
      merge conflict — instead EDIT 105's existing mFlag keys in place;
  (b) per-move gating (FSM/native only evaluates defense bits for the 居合
      family) — discriminator: clear b24/b26 on 307 itself; if 307 loses its
      defense the pipeline works;
  (c) mTrigger may matter too: 307/308/rs-115 all post mTrigger b11(2048) exactly
      at the b26 window boundary (307 f10, 308 f12 vanilla, rs-115 f10) and
      308 posts b10(1024) at f25 = b24 armor end. Bits may be the state and
      mTrigger the notification; copying only one half could break the pair.
- The 84 MB game decompile (Reverse/MonsterHunterWorld.exe.c) has large broken
  regions (halt_baddata / unaff_*), and TIML hash comparisons live in .rdata
  tables, so text search for 0x2388/0x48dd4fcf etc. finds nothing. In Ghidra,
  create functions manually at 0x14119faa0, 0x142134b70, 0x141ce1670 and
  disassemble around 0x14213fa12 to see what ValorLS NOPed.

### Unpacked dump (2026-09-07, VERIFIED)
The shipped exe is packed: disk .text entropy ~7.95 (encrypted); the .bind
section stub decrypts it at process start. `Reverse/MonsterHunterWorld_dump.exe`
is a Scylla dump of the running game, fully verified: base 0x140000000,
SizeOfImage 0x55d3000, 8 sections incl .bind, .text entropy 6.46, anchor bytes
at 0x140310030 / 0x14119faa0 / 0x141ce1670 / 0x14213fa12 identical to live
memory, DoAction signature at VA 0x140269cea. Import into Ghidra as-is (PE base
already correct). `mhw_text_probe.py` re-checks decryption of the live process.

TIML property-name strings survive in .rdata. CORRECT VAs (the earlier list was
file offsets, wrong): mNoHitTime 0x1433C6618, mSuperArmorTime 0x1433C65F8,
mHyperArmorTime 0x1433C6608, mWeaponMotionNo 0x1433C6720, mTurnSpeed 0x1433C65D8,
full category name "nTimelineParam::PlMotionCommon" 0x1433C6598.
Note: `igc64_dump.dll` (first attempt) was the Intel Graphics Compiler DLL that
rides in the game process, not the game — wrong module, discard.

### Unpacked-decompile findings (2026-09-08)
`Reverse/MonsterHunterWorld_dump.exe.c` (280 MB) is CLEAN: 3 halt_baddata total.

- Category registration: `FUN_1401e15c0` calls
  `FUN_14216ed60(&DAT_145000fc0, "nTimelineParam::PlMotionCommon", classDesc@0x1451C5C30, 0x168, ...)`
  -> controller instance size 0x168; category object 0x145000FC0 stores hash
  0x48D6114F (= FHK's PlMotionCommon) + class desc (parent "nTimeline::Object").
- PlMotionCommon controller layout (from registrar FUN_14191F7D0, lines ~5174891):
  +0x7C mTurnSpeed, +0x84 mMotionSpeed, +0x88..0x90 mMotionTransRateXYZ,
  **+0x94 mSuperArmorTime, +0x98 mHyperArmorTime, +0x9C mNoHitTime (floats)**,
  +0xA0 mDamageHitNo, +0xA4 mScrHitNo, +0xA8 mStaminaParamIndex,
  +0xE8 mWeaponMotionNo, +0xF0 mWeaponMotionSpeed, +0xF8 mWeaponMotionStartFrame,
  +0x110 mAttackAdjustAngleIndex, +0x104 mActionPhase. mFlag/mTrigger int-props
  are registered by small helpers (type 0x4001 = uint array; e.g. FUN_141958C80
  binds "mFlag"@+8) — which helper belongs to PlMotionCommon not yet pinned.
- Damage-layer defense query family: FUN_140AEE050 (rate by reaction type 1-6;
  case3 = (+0x98 hyper, +0x94 super), case4 = (+0xA0 damageHitNo, +0x9C noHit)),
  FUN_140AEE690, FUN_140AEE7E0 (case0 reads weapon via player+0x76B0 ->
  weapon+0x8A0/0x8A4). These operate on a per-player REACTION POOL object
  (callers pass `something+0x28`; pool+8 = player ptr; ~30 timers + bool
  selectors at +0x13D..0x17B; rates from param table func_0x140462040).
  NOT the PlMotionCommon controller itself (offset collision was a trap).
  Open question: the bridge that copies/arms pool timers from PlMotionCommon
  controller fields (or from the mFlag bits) per action.
- ValorLS NOP decoded: 8 bytes at 0x14213FA12 kill `mov dword [rcx+0xA84], ebp`
  inside a reset block that zeroes +0xA4C/+0xA88/+0xA8A/+0xA8C(FLT_MAX)/+0xA94/
  +0xC1C/+0xC18 (defense-ish timers; rcx likely the weapon object). I.e. the mod
  prevents a timer clear so its state survives the reset. Side effect: the tail
  2 bytes of the preceding `mov word [rcx+0xC18], bp` are also NOPed (disp
  becomes 0x90900C18) — presumably a cold/never-taken path.
- Implication for the 105-copy failure: PlMotionCommon fields are real and
  consumed at the damage layer, but arming the per-player pool is likely gated
  per action/FSM natively. Next GUI steps in Ghidra: xref DAT_145000FC0 ->
  controller manager; xref the pool functions FUN_140AEE050/690/7E0 -> damage
  handlers at decompile lines ~3.80M; find the pool-timer writer.

## Runtime defense mechanism pinned (2026-09-09, live diff + decompile) — SUPERSEDES EARLIER ValorLS READING

ValorGauge now has an F9 struct-diff capture (weapon 0..0x2400 + player 0..0x12608,
two-phase: 5 s idle baseline learns noisy offsets, then armed phase logs every
zero-cross/single-bit transition with `act=set:id` context; analyzer
`analyze_diff_log.py` groups arms/clears per action).

### Established
- **Roll (FSM 3:18) and Foresight Slash (3:86) arm NO duration field** in weapon or
  player at action start (only the player+0xED04..0xEE84 ring of 0.116667=7/60 s
  rotating input-buffer slots ticks). Conclusion: normal-move i-frames/armor are
  evaluated PER FRAME against the TIML defense window (current motion frame vs
  mFlag windows), not stored as a countdown. Consistent with: deleting
  PlMotionCommon TIML killed rs-115 armor; SR buffs 308 by TIML-only edit.
- **Iai family stages explicit per-motion times.** At iai-type (kind==7) motion
  start `FUN_14211e8a0` copies THREE floats from the motion param block
  ([weapon+0x998] + 0x160/0x164/0x168) into weapon+0x23A0/0x23A4/0x23A8 and sets
  +0x23B0=1. Measured: 小居合 3:101 arms weapon+0x1C24=0.2 (also from a global
  param table: FUN_141f5f540 copies *(_DAT_145011f58+0xd4)); 大居合 3:102 arms
  +0x1C24=0.2 AND +0x23A4=2.0; stance 3:99 had +0x1C28=0.5. The 2.0 decays by
  frame-delta inside the sheathe-state updater FUN_1421237b0 (it also calls
  FUN_142110a90(weapon,0x65/0x66,..) = actions 101/102). The 0x23A0 triple is a
  serialized sub-struct of the weapon block 0x2348..0x23F4 (bulk deserializer at
  decompile line ~3892200).
- **weapon+0x2388 is UI-ONLY.** Its consumer FUN_140617730 is a UI updater:
  `if (weapon+0x2378 > 0 || weapon+0x2388 > 0) clear bit3 of two gauge components
  else set bit3 + reset component`. No damage-layer reader found.
  REINTERPRETATION of ValorLS: its custom motion 0xC153 came from its own modded
  LMT — its i-frames were TIML windows on that motion (same route as SR); the
  2388 write was gauge/UI dressing, and "game decrements it per frame" was an
  unverified guess (now disproven for defense purposes).
- FSM ids seen: 3:18 roll (drawn), 3:79 thrust, 3:85/3:1/3:2/3:11 other LS
  normals, 3:86 foresight slash, 3:98 special sheathe, 3:99 sheathed stance
  (5 s timeout), 3:101 small iai, 3:102 big iai. player+0x8950/0x8980/0x89B0/
  0x89E0 (four 0x30-strided slots) get a 1-2 frame 2.0 pulse on attacks =
  broadcast, not a timer.
- Next: find the per-frame TIML-window consumer in the damage/reaction layer
  (xref DAT_145000FC0 = PlMotionCommon category); test custom-action defense by
  authoring TIML b26/b24 windows + mTrigger pairing on our own motions; the
  居合-style staged-times route (param block 0x160/0x164/0x168) is the second
  data source to map.

## Action-id mapping located (2026-09-10) — wp03_action.fsm node processes

- The DoAction numeric ids (set 3: 79/85/86/101/102...) live in
  `hm/wp/wp03/wp03_action.fsm` (cFSMPl_W03 rAIFSM): each node's `Action_W03`
  process carries `nPlFSM::ActionSet_W03.ActionNo`; `LinkMotion_W03` processes
  carry `MotionNo`/`MotionNo_Phase1` = LMT motion ids. Extract by parsing the
  exported XML's `mpProcessList` (XFsm shows names only — search by ActionNo,
  not name).
- Vanilla (90 nodes): 突き=79, 踏込斬り=85, 見切り=86, 居合斬り=101,
  居合気刃=102, 気刃斬2/3=67/68, 斬り上げ=80, 移動抜刀=7; 1000-series are
  internal (待機=1000, 居合斬り開始=-10). Matches ALL live DoAction ids.
- **No vanilla node has ActionNo 98/99, and no MotionNo=306 anywhere in the
  graph** — 特殊納刀/構え are native-driven states. The installed valor mod's
  wp03_action.fsm (59 nodes, all renamed 南风焓) ADDED them: node 1 = 98,
  node 35 = 99.
- Stance (node 35) exits: `アクション終了`→26 (the ~3.5 s auto-exit = native
  action 99's compiled duration; motion 306 loops but native cuts it),
  `△`, `R`, `居合斬り`, `オーラレベル白以上+△+○` (inputs),
  `オーラレベル白以上+着地+抜刀中+納刀中` (valor branch).
- Conclusion: the action-id→motion binding table and per-action durations are
  compiled into the exe (see the `+0x6278 == 0x87` style constants in the
  decompile). Modding surface = graph conditions (wp03_action.fsm, XFsm-editable)
  + LMT entry CONTENTS (motion data + TIML). To give a custom action a new
  motion/defense, overwrite the LMT entry the reused action id plays; never
  chase new bindings. Synthlight's MHW-Editor (tools/MHWEditor) is an .mbd
  editor, not an FSM tool.

## Defense-window editing: mechanism pinned by A/B tests (2026-09-10/11)

Reference files: SR's actual LMT = `MHW-MHRS-migrate\spirit-reckoning\nativePC\hm\
wp\wp03\mot\w03_00\w03_00.lmt`; the valor author's Rise port = `w03_00-rs.lmt`
(nativePC mot folder). w03_00-rs is NOT Spirit Reckoning (two different mods).

- SR/RS 308 edit (identical in both): move the PlMotionCommon mFlag b26-clear
  key f12 -> f18, **leave mTrigger untouched (2048 stays at f12)**. Result:
  大居合 iframes clearly longer (user-confirmed on RS; SR reported 完美无伤).
  Note the window f0–18 is b26+b24 SIMULTANEOUSLY on — and it plays as clean
  i-frames, so when both bits are on, NO-HIT wins over armor.
- Our A/B on the live nativePC LMT (patch_timl_defense.py):
  - b24 window extended f12→f70 (values set) WITH mTrigger 2048@f12 removed →
    armor became much more lenient (works, per-frame, trigger-INdependent) but
    i-frames DIED entirely (hits landed, armor-absorbed).
  - Restored trigger 2048@f12 (vanilla layout) → predicts iframes return.
- **Model: b26 bit window = i-frames, but requires the mTrigger 2048 event
  somewhere to function (gate/commit, NOT the end-time — SR proves window end
  f18 can be after trigger f12). b24 bit window = super armor, plain per-frame.
  Priority both-on: no-hit wins.**
- TIML frames for player motions tick at ~60fps real time (user-measured:
  vanilla 12f = 0.2s feel).
- Tool: `patch_timl_defense.py` — `dump <lmt> <entry>` lists every
  category/transform track with full names (Blender/FreeKinetics flattens
  duplicate category names to [0]/[1] and loses which is which); `move` and
  `setval` do byte-exact keyframe edits with expected-value checks, sorted-order
  checks and an automatic .bak. Recipe values: b26=0x04000000 (67108864),
  b24=0x01000000 (16777216), keep b25=0x02000000, b22=0x00400000, b1|b2=0x6;
  vanilla 308 keys f0=121634822 / f12=54525958 / f25=37748742.
- Open: whether the no-hit window is consumed by the first damage instance
  (single-charge shield) — multi-hit test pending with the trigger restored.

## FINAL defense model (2026-09-12, empirically closed)

Pool ELIMINATED: the defense pool ([player+0x7D20]+0x28, hooked FUN_140aee050
RVA 0xAEE050) keeps ALL timers zero during player iframes/armor — it belongs to
the hit-reaction system, not player move-defense. POOLQ hook + DIFF[pool] proved
it (type 4/6 queries, ret=0, [94]/[98]/[9C]/[A0] all 0 even during 无伤大居合).

Three distinct iframe/armor mechanisms, per move:

| move | mechanism | evidence |
|---|---|---|
| roll (3:18) | native timer player+0xEE90 armed to base 0.2 s; Evade Window skill extends beyond (user L2 = 0.3 s; 0.2→0.3 non-crossing so DIFF-blind) | two rolls armed +0xEE90=0.2 exactly |
| 见切 140 | TIML b26 window f0–f130 (~2.2 s @60fps!), mTrigger 2048 gate at f45 (mid-window), NO b24 (pure iframes) | entry-140 decode: mFlag f0=0x80400000 →f130=0x80000000 |
| 大居合 308 | b26 f0–12 + 2048@f12 vanilla; our f0–70 edit produced 1.17 s clean iframes (user-confirmed 无伤) | live A/B |
| 小居合 307 | b26 f0–5 (negligible), b24 f5–10 | decode |

FINAL RECIPE (resource-side, per LMT entry PlMotionCommon):
- I-frames: mFlag b26 (0x04000000) window + ONE mTrigger 2048 (0x800) key
  anywhere INSIDE the window (见切 fires it mid-window @f45; 308 at window
  start f12 — both work). Window length free (f18 SR-style, f70 tested).
- Super armor: mFlag b24 (0x01000000) window, no trigger needed (per-frame).
- Nesting 先无敌后霸体: b26 clears where b24 begins (vanilla 307/308 style).
- Keep baseline bits b25/b22/b1/b2; keep mFlag2 b15 over defended span.
- Both-on priority: no-hit wins (SR f0–18 both-on = clean no-damage).
- Roll-style native timer (player+0xEE90) is NOT resource-editable; ignore.
- 104-frame 见切-style windows are legit: iframes can span the whole move.

TEST STATE STILL INSTALLED: nativePC 308 mFlag = b26+b24 f0–70 (1.17 s).
Revert with: setval f24→54525958, f25→37748742 (or restore w03_00.lmt.bak).
- **PROVEN END-TO-END (2026-09-12): thrust 105 given NEW iframes from scratch —
  f0=121634822 (iframes+armor), f80-key moved to f18 (value 33554438 = off),
  vanilla trigger 2048@f0 kept as gate → 0.3 s iframe poke, user-confirmed
  无伤 in game.** Final 105 state: mFlag f0/f18/f120 = 121634822/33554438/6,
  mTrigger 2048@f0. The old "308-copy onto 105 ineffective" mystery: the copy's
  0.2 s window was real but imperceptible on a thrust.
- 叮+蓝光 deflect presentation: NOT TIML-switchable. b31 tested on 105 — no
  effect (reverted). Full track diff 307 vs 105: no missing category/track.
  Conclusion: it is the iai/foresight family's native "success-dodge" feedback
  (blue flash + clang, latched to the FIRST nullified hit per window — multi-hit
  tests show only one 叮). To inherit it on a custom action, reuse an
  iai-family action slot (98–102 chain / kind-7 moves). Roll and thrust stay
  silent by design.
Remaining unclear for Hunter Arts: sound (nWwiseTimeline::EventGroup tracks are
the sound/req side — mTrigger values there fire wwise events).

## Important distinction: animation, hitbox, defense

These should be treated as separate timelines:

```text
Skeletal animation
    Character and weapon bone movement.

EventCollision timeline
    Attack collision/damage event activation and hit-event parameters.

PlMotionCommon timeline
    Motion-level properties such as defensive windows and weapon-motion parameters.

FSM
    Which action is entered, and how actions transition.

wp03.col
    Weapon collision geometry/resources available to the weapon system.
```

`mNoHitTime` does not create a sword hitbox. It controls a no-hit/invulnerability property for the player motion. A damage hit still requires the appropriate collision event and hit parameters.

Likewise, `mSuperArmorTime` does not create damage. It controls the period during which the player resists interruption, usually with the game's corresponding damage-reduction rules.

## Likely TIML properties

Free Kinetics exposes the following hashed TIML properties with readable names:

| Property | Current interpretation |
|---|---|
| `mNoHitTime` | Invulnerability/no-hit duration or timing value |
| `mSuperArmorTime` | Super-armor duration or timing value |
| `mHyperArmorTime` | Hyper-armor duration or timing value |
| `mDamageHitNo` | Damage/hit-event selector or number |
| `mHitIndex` | Hit/collision event index or selector |
| `mWeaponMotionNo` | Weapon motion identifier |
| `mWeaponMotionStartFrame` | Start frame for the weapon motion |
| `mWeaponMotionSpeed` | Weapon motion playback speed |
| `mWeaponMotionInterpolationTime` | Weapon motion interpolation/blend time |
| `mNoSetWeaponMotionNo` | Flag/identifier preventing or changing automatic weapon-motion assignment |

The exact semantics and units of every field still need to be verified in-game. Some values may be durations, while others may be start/end markers, selector values, or references to another table.

## Current interpretation of entries 101 and 307

### `LMT::w03_00::101`

Gameplay observation: approximately frames 43-88 cause damage.

Working hypothesis: one or more `EventCollision00` TIML tracks activate a hit event in this interval. The bone animation determines where the weapon is, but the collision event determines when the attack is active and which hit data is used.

### `LMT::w03_00::307`

Gameplay observation: the beginning has invulnerability, and a later portion has super armor.

Working hypothesis: linked `PlMotionCommon` TIML data contains `mNoHitTime` and `mSuperArmorTime`, potentially as separate keyed intervals or values. There may also be related motion flags in the same TIML action.

These are hypotheses until the linked TIML actions and keyframes are printed and compared with controlled edits.

## True sheath state

Moving the hand/weapon-related bones alone is not enough to create a true sheathed state. It can place the sword and sheath visually at the back, but the game may still consider the weapon drawn. Later weapon shake or weapon-specific motion then causes the sword and sheath to move independently.

The true sheath state is likely controlled by the weapon-motion part of the motion data, together with action/FSM state. The most relevant fields to investigate are:

```text
mWeaponMotionNo
mWeaponMotionStartFrame
mWeaponMotionSpeed
mWeaponMotionInterpolationTime
mNoSetWeaponMotionNo
```

The correct model is:

```text
Changing bones 30/47
    = changes pose/attachment coordinates only

Weapon-motion TIML + action/FSM state
    = controls drawn/sheath behavior of the weapon object
```

This explains why a visually back-positioned weapon can fail to behave as sheathed.

(2026-09-20 在反编译层面实锤并补全：执行侧是「当前动作 id → 边沿触发 → 挂点表切换」状态机，
见文末《真正纳刀（刀入鞘"合并"）的引擎机制》。030/047 通道只携带状态请求；动作层不判"纳刀"时
刀节点留在手部挂点表上——正是本节观察到的"视觉在背后、行为仍拔刀、抖动分离"的根因。)

## Recommended authoring workflow for a new attack

1. Open the original Long Sword LMT in Blender with Free Kinetics.
2. Duplicate a complete existing attack entry, preferably one with similar behavior.
3. Keep the complete linked TIML structure when duplicating; do not copy only skeletal animation curves.
4. Replace the skeletal animation with the new motion.
5. Edit the `EventCollision00` tracks to place the damage window at the desired frames.
6. Edit `mNoHitTime` for invulnerability.
7. Edit `mSuperArmorTime` or `mHyperArmorTime` for interruption resistance.
8. Edit weapon-motion fields if the motion draws, holds, or sheathes the weapon.
9. Connect the action through the FSM.
10. Export the LMT and test in-game.

Duplicating a known-good attack is safer than creating a new action with no TIML metadata, because a bone-only motion can look correct while producing no damage or defensive behavior.

## Controlled experiments to identify exact fields

Use one copied attack and change one thing at a time:

### Hitbox experiment

- Keep the skeletal animation unchanged.
- Disable or move one `EventCollision00` keyframe.
- Test whether damage disappears or shifts in time.
- Change `mHitIndex` or `mDamageHitNo` independently to determine whether they select the hitbox/hit-data entry.

### Invulnerability experiment

- Keep collision events unchanged.
- Set `mNoHitTime` to zero.
- Test whether the opening invulnerability disappears.
- Restore it and move its keyframe or value to determine whether it is a duration, start frame, or end frame.

### Super-armor experiment

- Keep collision events and no-hit data unchanged.
- Set `mSuperArmorTime` to zero, then to a large value.
- Test interruption, damage reduction, and timing.
- Repeat with `mHyperArmorTime` if the move has a separate stronger armor behavior.

### Sheath experiment

- Compare a normal sheath motion with the modified motion.
- Dump and compare all weapon-motion fields listed above.
- Keep hand/back bone positions unchanged while changing only weapon-motion metadata.
- Use shake-heavy movement to determine whether the weapon is truly in the sheath state.

## Practical inspection target

The next useful script should traverse the `w03_00` Free Kinetics node tree and, for entries 101 and 307, print:

```text
LMT entry number
linked TIML action name
TIML category
timeline parameter hash
property name
keyframe frame/time
keyframe value
```

The first target categories should be:

```text
EventCollision00
PlMotionCommon
PlMotionInput
EventGroup
```

This will turn the gameplay observations into an exact frame/value map.

## Bottom line

The LMT is not merely a bone-animation file. It contains or references TIML timelines that control attack events, defensive windows, and weapon-motion behavior. For newly added motions, the research and authoring problem is therefore to preserve and edit the complete LMT/TIML action structure, not just to position the weapon bones.


---

# 叮+蓝光来源：成功回避演出链路（2026-09-12，F11 属性直方图定位）

## 工具

ValorGauge 新增 F11 直方图：钩住 FSM 条件树属性读取器三件套
（`FUN_141922190` int / `FUN_1419221f0` bool / `FUN_141922240` float，RVA
0x1922190 / 0x19221F0 / 0x1922240），记录 `(实体, id, 索引) -> 次数 + 调用点RVA`，
F11 dump 到日志并清零。按 F11 时还会一次性枚举属性注册表
（对象 RVA 0x5012048，条目数 +0xB8，行查找 RVA 0x462700，
`FUN_141979770` id→名字，RVA 0x1979770）。

签名：`int __fastcall(entity, id(rdx), index(r8), default(r9))`，
读取 `row + 0x24 + index*2` 的 int16 计数器；float 版 = int16 × 帧常数。
行来自 `func_0x1412f80f0(entity + 0x126D8)`。
注意：id 的名字表给出的是英文技能名（Marathon Runner 等），
大量 id 名字为空 —— 这些才是 FSM 专用状态属性。

## 对照捕获差集结论（A=见切被打穿叮 / B=见切无接触 / C=硬吃 / D=翻滚无敌）

A 独有（其余三段完全没有）的 PLAYER 读，恰好 302 次一组：

- 条件叶子 `FUN_141194fc0`（0x82|0x89|0xB5）、`FUN_141195010`
  （max(INT 200 idx1, INT 0x96) > 0）、`FUN_141195060`
  （0x97|0x98|200|0xA8|0xE0:1|0x95|199）—— 特殊回避系标志组。
- 每帧刷新器 `FUN_1411a2860`（RVA 0x11A2860）：读这三组 → 写反应管理器
  `[player+0x7D20]` 的 +0x774 / +0x775 / +0x776（演出候选开关）。
- `FUN_142045350`（RVA 0x2045350，玩家动作更新）：`0xDE idx1 >= 1` →
  反应管理器 +0x7C0 = 1（成功待命），并启用加长无敌
  （调参表 `_DAT_145011F58 + 0x31C`）。
- `FUN_1411c26e0`（RVA 0x11C26E0，反应候选评估器）：`if (+0x7C0)`
  读 `0xDE idx2`（成功加成计数）返回时长，否则 0。
  同族 FUN_1411c2780/27c0/2800…（读 0x35 查表）是其他反应候选。
- `FUN_1412fdaf0`（RVA 0x12FDAF0，玩家当前无敌时长取值）：
  `FUN_1411c26e0(管理器)` → 0 时回退 `FUN_140aee690(pool, 8)`
  （pool+0xE4 = 当前动作无伤窗剩余 —— TIML b26 的运行时着陆字段）
  → 加全局基础值 `_DAT_145011F58 + 0x334`。
  bool 版 `FUN_1412fdb50`。

## 结论

叮+蓝光不是 TIML 位决定的，是 **FSM 属性 0xDE（idx1=招式属于见切/居合系，
idx2=成功计数）+ 反应管理器 +0x7C0** 驱动的"成功回避反应候选"。
命中被 TIML 无敌窗否定时：

1. pool type 8（+0xE4）只有当前动作无伤窗时长 —— 正常翻滚/突刺到这一步
   就结束，没有候选 → 无演出。
2. 见切/居合 因 0xDE 属性活跃，+0x7C0 置位 → 成功候选激活 →
   演出（叮+蓝光），且每窗只 latch 一次（0xDE idx2 计数消耗/清零）。

**给自定义猎魂动作白得叮+蓝光的方法**：复用居合/见切系 action 槽位
（86/101/102 家族），槽位的动作处理会置 0xDE 属性，演出自动继承；
纯 TIML 加 b26 窗（如突刺 105）只有无敌没有演出。
0xDE idx2 的写入方（成功瞬间置数的那处）在事件系统里，尚未定位 ——
如需精确到音效调用点，下一钩子挂在 FUN_1411c26e0 或监视 0xDE idx2 写入即可。

## 相关偏移速查

- 反应管理器 = `[player+0x7D20]`；实体在 +8；候选开关 +0x774/775/776/7C0
- 池 = 管理器 +0x28；type8 无伤窗剩余 = pool+0xE4；armor = +0x94/+0x98
- FSM 属性行 = `func_0x1412f80f0(entity+0x126D8)` → `row+0x24+idx*2`
- 属性注册表对象 RVA 0x5012048；名字解析 FUN_141979770（RVA 0x1979770）
- 调参表对象 RVA 0x5011F58：+0x31C 见切加长无敌、+0x334 无敌基础加成

---

# 方向性防御：mFlag bit28 = 格挡有效（2026-09-13，运行时实锤）

## 位扫描结论（scan_timl_bits.py，IB 版 LMT：hm/wp/wpNN/mot/wNN_00）

对太刀(wp03)/大剑(wp00)/片手(wp01)/长枪(wp06)/铳枪(wp07)/盾斧(wp09) 全量扫描：

- **PlMotionCommon mFlag bit28（0x10000000）= 格挡有效**。
  CB/SnS/GL/GS 均有、太刀完全没有。守备动作（每把武器 entry 5,6,7…、
  SnS 47-50、CB 40-45 等）全是"单键 f0 整段有效"：
  值 0x10000000 或 0x10000000|bit31（架势整段）。
- bit28 与 b26（无敌）同设时 b26 优先：命中被 no-hit 否定，
  格挡处理根本不运行 —— 即"带无敌的格挡"不存在，两者选一。
- 注意区分：G0 老版 wp/caxe 等 LMT 是旧格式，本工具链读不了；
  必须用 Iceborne 的 hm/wp/wpNN 数据。

## 运行时验证（太刀突刺 105 设 f0=306184198=bit28+基线）

- 正面被打：**格挡成立** —— 沉闷"咚"格挡音效、不掉血不硬直、
  动作正常播完（太刀没有防御动作，所以没有举盾格挡步，
  格挡成功=原地继续动作）。
- 背面被打：**正常受击、被打飞** —— 方向判定实锤，锥体随玩家朝向。
- 角度规格（防御锥具体度数）是引擎/武器数据侧，未定位；
  当前"正面格挡/背面受击"的行为已满足"只能防前方"需求。

## 格挡反应入口（反编译侧记）

- 伤害分类写反应管理器 `+0x769 = 0x100/0x101`（attacker +0x327D/F 门）；
- 仲裁 `FUN_1411b8880` 读 `+0x768/769/76A` → `FUN_1411cac40`（格挡反应装填）；
- 格挡参数 weapon+0x8A0/0x8A4（守备时长，`FUN_140aee7e0` case0），
  weapon+0x8A8 起三组等级表（防御等级→削伤/硬直）；
- 池 type0 = 格挡（pool+0x58/0x5C，选择 bool 0x14E/0x14F）。
- 池全部计时器字段由 `FUN_14191f7d0`（RVA 0x191F7D0）按名字注册
  （+0x94/+0x98/+0x9C/+0xE4… 与 FUN_140aee050 查询偏移一一对应）。

## 对刚1（刚气刃斩1）设计的结论

刚1 = 起手 0.2s 弹反窗：**不要用 bit28 格挡，用见切模板**：

1. 无敌帧：b26 窗 f0→f12（0.2s=12帧）+ mTrigger 2048（配方已证）——
   零伤害（格挡有削伤，无敌没有），这正好满足"这段时间不应该受伤"。
2. 弹反演出（叮+蓝光）：复用居合/见切系槽位白得，或 DLL 置 0xDE 属性。
3. 分支（弹反成功→一文字，失败→刚2）：克隆见切的
   見切り回避成功後の攻撃ヒット 条件分支结构（0xDE idx2 属性）。

## 弹反检测最终方案：输入层 Ctrl 自动连发 (2026-09-13)

内存侧检测否定命中三连败：`FUN_1411CAAF0`（仅初始化期）、
`FUN_141195010` 条件叶（南风 FSM 专属）、`FUN_1411B8880` 仲裁锁存
（否定命中根本到不了仲裁层）。结论：**不再检测**。

新方案（ValorGauge.dll 已实现并部署）：
- 玩家每按一次 Ctrl（=手柄 R2/气刃输入的键盘映射），DLL 在接下来
  N 秒内持续脉冲补发 Ctrl（默认 2s 窗、50ms 半周期，即 10 次/秒）；
- 任何成功窗口（叮后的延长无伤窗）一旦打开，里面必然有新鲜按键沿，
  原版代码自己点亮 見切り回避成功後の攻撃ヒット（BOOL 222 idx0），
  FSM 分支（见切→后续 / 刚1→一文字）由原版条件树完成；
- 纯输入层实现：`SendInput` + `GetAsyncKeyState`，不碰任何游戏内存布局；
  仅游戏窗口前台时注入，失焦立即中止并补发 key-up 防卡键。
- 配置：`nativePC\plugins\ValorGauge.counter-repeat.txt`，两个数
  `<秒> <毫秒半周期>`，缺省 2.0 / 50。
- 删除：`WriteCounterSuccessProperty*`、`ReactionApplyHook`(0x11B8880)、
  counter-action.txt 门控——属性直写路径未验证过，留档在 git 历史。

验证信号：见切叮成功后应自动出 R 后续招；F11 直方图
`PROP kind=bool id=222 idx=0` 的 max 应首次出现 0x1。
已知副作用：窗口期内每次 Ctrl 都会连发→气刃连段自动接续，
调短窗口（如 0.5）可抑制。

## 见切窗口收缩为 0.2s (2026-09-13)

基座已换回原版 FSM+LMT（nativePC 的 wp03_action.fsm / w03_00.lmt，
与 w03_00-origin.lmt 逐字节一致；两者还原时带了只读位，已清除）。
输入层 Ctrl 自动连发 DLL 已实测触发 R 事件（用户确认）。

entry 140 编辑（patch_timl_defense.py，均通过期望值校验）：
- mFlag：b26 回落键 f130 → **f12**（无伤/弹反窗 2.2s → **0.2s**）
- mTrigger：f10 值 1 → **2048**（门控移入窗口内；f45 值 2048 → 1，
  只换值不挪帧，保持关键帧数组有序）
- 最终：mFlag f0=0x80400000 → f12=0x80000000；
  mTrigger f0=65537 / f10=2048 / f20=1 / f45=1。
- 布局 = 已实证的 308 原版模式（b26 f0–12 + 窗口内 2048 单键）。
- 回退：copy w03_00-origin.lmt → w03_00.lmt（.bak 是旧会话的，不是原版）。

## XFsm 条件树编写模式 + 崩溃修复 (2026-09-14, 用户实测确认)

崩溃三连修复（全部已在 XFsm.dll 部署）：
1. **条件树根节点类型切换 double-free**：`AIConditionTreeInfo.RootNode` setter 原实现
   "先 getter 销毁再覆盖字段"——字段在销毁后仍留悬空指针；编辑器调用点还手动
   pre-destroy 了旧树 → 同一棵子树销毁两次，堆损坏（即崩或延后崩）。
   修复：setter 改为"先覆盖字段 → 旧指针仅当不同才销毁"；调用点删除手动销毁。
2. **包装缓存 use-after-free**：`XFsmConditionTreeInfo.RootNode` 构造时缓存包装对象，
   根节点被替换后每帧读写已释放内存。修复：属性实时透传 `BackingInfo.RootNode`。
3. **未知节点类型 throw**：`AIConditionTreeNode` 构造函数对 9 个映射名之外的 DTI
   直接抛异常，且构造发生在每次 getter/Children 枚举里（渲染循环内）→ 加载/浏览
   307 棵条件树"有时崩有时不崩"。修复：回退 `ConditionTreeNodeType.None` 不抛；
   类型下拉禁选 None。
保险网：`Plugin.OnImGuiFreeRender` 顶层 try/catch → 任何 C# 异常不再直接杀进程，
写入 `nativePC/plugins/CSharp/XFsm/render-error.log`（带堆栈；SPL 日志也有一份）。
日志为空还闪退 = native 崩溃 → x64dbg 拦 0xC0000005/0xC0000409，或 dnSpy 附加。

**链接条件的最小写法（实测有效）**：
- 顶层 1 个 `OperationNode`；只有一个子节点时 Operator = None 即直通；
  多条件用 AND(16) / OR(17)。
- `Add Child` → `VariableNode`：**Property Name = "R"，Owner Name = "cFSMPl_W03"**
  （= 武器 FSM 的 owner 对象名；属性名走 skill 名空间解析 FUN_141979770）。
- 其余字段保持默认：Is Singleton Owner / Is BitNo / Is Array / Is Dynamic Index /
  Use Index Enum 全不勾，Index = 0，Index Variable 留 N/A。
- 实例：見切り → 気刃大回転斬り 链接（Condition Id 48），条件只剩一个"按R"。
- 注意：条件在 FSM 检查点求值，按键被缓冲到检查点——见切中途按 R，动作结束时才转向。

## 刚1 路线定案：属性语义不符，转内存侧检测 (2026-09-14)

`見切り回避成功後の攻撃ヒット`（BOOL 222 idx0）语义 = **"见切成功后的（后续）攻击
打中了"**——命中门控，不是"闪避成功"标志。用户扫遍全武器动作：不存在
"无敌规避成功即成立、不要求命中"的原版招式（防御系全是"防御后有反应"，带削伤）。
刚1 的设计要求是"窗口内攻击被否定"这一事件本身（弹反→打断刚1→下一击接一文字，
不关心反击是否命中）→ **模拟按键 R 方案语义不符，弃用**（机制本身有效，保留作
原版见切接续的便利功能与测试工具）。

### 内存侧检测：已败的三案（勿重试）
| 检测点 | 失败原因 |
|---|---|
| FUN_1411CAAF0 (0x11CAAF0) 钩子 | 仅初始化期执行，命中时根本不跑 |
| 条件叶 FUN_141195010/5060/4FC0 钩子 | 南风 FSM 专属，原版树不经过 |
| FUN_1411B8880 (0x11B8880) 仲裁锁存 +0x9BA | 否定命中在上游就被拒，到不了仲裁层 |

### 下一步首选实验：轮询 reaction manager 的 +0x7C0 标志（无需新钩子！）
- 反应管理器 = `*(player + 0x7D20)`；叮链路里 `FUN_142045350`（RVA 0x2045350）
  在否定命中时置 `mgr+0x7C0 = 1` 并开延长无伤窗；`FUN_1411c26e0` 以 +0x7C0==1
  为门读 0xDE idx2。**+0x7C0 是"成功窗激活"标志**。
- 关键未验证点：它是在**攻击被实际否定时**才置位，还是见切起手就置位（随
  0xDE idx1 属性武装）？前者=现成的弹反检测器，后者=无用。
- 实验：DLL 每帧读 `[player+0x7D20]+0x7C0`（Present 循环里已有逐帧轮询框架），
  A/B 对照：挥空见切（无接触）vs 叮成功见切。也可顺带看 +0x774/775/776。
- 若成立：检测侧闭环（无钩子、纯读内存）→ 写入侧用已实现的属性直写路径
  （resolver 0x12F80F0 → lookup 0x462700 @0x5012048 → row+0x24，**端到端未验证**）
  或直接强制动作切换；F11 验证 BOOL 222 idx0 的 max 是否变 0x1。
- 备选：钩 TIML 无伤窗的运行时消费点（b26 窗在哪段代码否决碰撞）——
  入口在 FUN_141f66dd0（RVA 0x1F66DD0，玩家伤害计算）上游的碰撞仲裁；
  对命中 vs 否定命中做 F11 差集。这也是无敌帧机制的最后一环。

### 属性直写路径状态（已实现、未触发过）
`WriteCounterSuccessProperty` 已从 ValorGauge 删除（git 历史可找回）：
entity+0x126D8 → component → resolver(0x12F80F0) → key → lookup(0x462700,
registry 0x5012048) → `*(int16*)(row+0x24) = 1`。恢复时注意 C2712
（SEH 函数体内不得有 C++ 对象，拆 Inner 函数）。

---

# 真正纳刀（刀入鞘"合并"）的引擎机制（2026-09-20，反编译实锤）

## 结论

**合并不是物理布尔，游戏从未把刀和鞘变成一个对象。** 引擎的做法是：武器对象的绑定控制器
在多套**挂点表**之间切换——拔刀态刀模型节点绑到手部挂点，纳刀态绑到**鞘坐标系内**的挂点
（刚体跟随鞘）→ 视觉"合并"，抖动同步。

机制分两层，缺一不可：

| 层 | 载体 | 作用 |
|---|---|---|
| 表选择（3 态） | 030/047 骨骼 Y-scale 通道（1/2/3，grip-states.md） | 决定刀/鞘各绑到哪套挂点（手/鞘内/居合腰）——即"位置" |
| 携带态开关（0/1） | 当前动作 id 每帧决策 + 边沿触发 | 决定武器对象的"拔刀/纳刀"运行态——即"游戏认为刀在不在手里"（独立运动/抖动/武器动作跟随） |

自定义动作只写 (1,1) 通道：表选择层生效（刀到了背后），但携带态层仍判"拔刀" → 刀保持
拔刀态行为（独立抖动、武器动作继续跟随）→ "都在背后但没合并为同一物体"。

## 链路（字段/RVA 均在 dump exe 核实）

### 1. 每帧决策：FUN_1421237B0（RVA 0x21237B0，即已知的纳刀架势更新器）

- 当前动作 id：`func_0x141BFF740(player,0)`；同函数另一分支特判 {6,9,98,99}，与实测
  3:6/3:9/3:98/3:99 收刀/架势族吻合 → id 空间就是 3:x
- **收刀家族 switch（set==3 武器动作层）：动作 ∈ {5, 6, 9, 10, 0x45(69), 0x57(87)} →
  期望=纳刀；default → 期望=拔刀**。6/9 为用户实测收刀；5 疑似纳刀待机；
  **0x45/0x57 的招式名未对表，复用时优先用已实测的 5/6/9/10**
- **weapon+0xA50 或 weapon+0x23C0 非 0 → 无条件期望=纳刀**（反编译两处：主路径 +
  player 指针失效的清理路径；疑似納刀系状态标志，写入方待钉）
- 动作 ∈ {98(0x62), 100, 101(0x65), 102(0x66)}（特殊纳刀/居合系）→ 锁存 `weapon+0xAB8=1`
- 每帧刷 **weapon+0x2D0** 位：bit0x20=納刀中（spirit==0 或 player+0x76A8==0）/
  bit0x40=拔刀中 → **疑似 FSM 抜刀中/納刀中 条件的数据源（推定）**
- 最前段输入处理（门控 spirit+0x2370 / weapon+0xAA1 / weapon+0x2D60）：
  动作∈{6,9,98,99} 且缓冲请求==100 → 起动作 101；其他动作时 weapon+0x1C9C=100 武装、
  缓冲请求==1 → 起动作 100（架势输入转居合的武器侧一半，细节待解）
- **set≠3 分支**（非武器动作层，且 player+0x76A8==0 等条件）：id<0x214 → 强制拔刀
  （func_0x142150D10(_,1) + 直接 vtable110(0) + ctrl+0x1D24=1 锁存）；
  **id ∈ {0x214,0x215} → 期望=纳刀**（3:99 架势合并大概率走这里，待动态验证）；
  id 0x262..0x266 → +0x2D68 锁存 + weapon vtable+0x298 特例；slot-1 动作==0x53 → 不强制

### 2. 边沿触发：func_0x142150D10（RVA 0x2150D10）

```c
// w: 1=拔刀系动作, 0=收刀系动作（更新器算出）
// cur = *(u8*)(weapon+0xC71): 0=当前已拔刀, 非0=当前已纳刀
w==1 && cur!=0 → 执行 vtable+0x110(obj, 0)   // 拔刀=分离
w==0 && cur==0 → 执行 vtable+0x110(obj, 1)   // 纳刀=合并
其余（状态已一致）→ 不动作
```

obj = `*(weapon+0x2350)`，其 +0xAF0 与更新器直呼的 weapon+0xAF0 命中同一控制器 →
推定 `*(weapon+0x2350)==weapon`（自指针）。**+0xC71 的写入方未定位**（触发器只读）。

### 3. 执行器：控制器 vtable+0x110 = 0x142374710

- 控制器类 vtable 基址 **0x1435195B8**（构造 FUN_14236B750 / 析构 FUN_14236BC80）
- `*(u8*)(ctrl+0x181) = 参数`（**0=拔刀、1=纳刀**——极性由边沿逻辑推定，DLL 实测一次确认）
- 尾调 FUN_142373BF0

### 4. 重绑：FUN_142373BF0 → FUN_142373D50

- vtable+0x158（0x14031FC80）= `return *(ctrl+0x170)` = **当前激活挂点表**
- 每节点：`row = [表+0x4B0][节点id & 0x1FF]`（字节映射，0xFF=无效）
  → `[表+0x4A8] + row*0xC0`，行 +0x20 起 16 float 变换
- 写入节点 +0x90 / +0x170 / +0x180 三份变换，+0x1C4=0
- 锁存：+0x181==1 且 +0x4A2==0 → +0x4A2=1；表持有者 +0x4A4≠0 且 (+0x1A8&0x400) →
  jump-table 备用路径；( +0x1A8&0x100 )==0 才做常规重绑（0x100 疑似暂停位，待解）

### 5. 挂点表的加载：FUN_140325130(持有者, 槽 0..4, 状态值)

- 一次刷新 **5 个部件槽**；静态描述符表 0x142F220A0=(0..4)、0x142F220B8=(1..5)、
  0x142F220D0=(0..4)
- 每槽 → FUN_1403253F0 → FUN_140326520 按资源 id 加载（资源管理器 _DAT_145123A00
  vtable+0x58；类型描述符 0x1451C4330/1451C6B80/1451C6B48 三个）
- 资源 id 由 0x1419A1370 从（类型字节=[持有者+0x478]+0x111D, 槽号, 状态值）构造；
  0x7FFF/0x7FFE 特例映射 500/501/502
- **状态值参数即 030/047 那个 1/2/3 的最终去向**；"采样指令 → 此参数"的链路 Ghidra
  在调用点丢 r8 未钉死（见待钉清单 #1）

### 6. 三个控制器 + 特效同源

- 武器对象上有**三个同款控制器**：weapon+0xAF0 / +0xFA0 / +0x1450（析构 FUN_14210B710
  逐个调 FUN_14236BC80 证实）→ 推测分别管 刀 / 鞘 / 配件，对应 030 与 047 各自独立绑定
- 特效用**同一张挂点表**取位：`pos = [ctrl+0x4A8] + 行号*0xC0 + 0x20`
  - FUN_142125150（RVA 0x2125150）：通用收/拔刀特效；weapon+0x23D0+槽*0x18 =
    控制器指针数组（+0x23C4=数量 3）、weapon+0x25B0+槽*4 = 当前行号、
    weapon+0x2740+槽*4 = 计时器、weapon+0x27E0+槽*0x40 = 变换
  - FUN_142125860（RVA 0x2125860）：动作 102 专用（weapon+0x2CF0/+0x2D08/+0x2D24）

## 定案与利用路线（2026-09-20 定案：复用动作槽）

- **首选：复用收刀系动作槽（优先 5/6/9/10）装载自定义纳刀动作**——决策 switch、携带态、
  特效、挂点全部自动继承（与"叮+蓝光复用居合槽 86/101/102"同一策略）。id→LMT entry
  映射按 action-list.md 流程查（FSM id ≠ LMT entry）；LMT entry 里的 030/047 翻转帧
  （如 003 槽 f51 2→1）负责视觉滑入的对时，携带态重绑由动作 id 边沿自动完成
- 备选：DLL 置 weapon+0xA50 / +0x23C0（先钉语义与写入方）
- DLL 直接测试配方（ValorGauge 热键 A/B，勿与 ValorProbe 同装）：

```c
// weapon = *(player+0x76B0)
void (**fn)(void*, int) = *(void***)(weapon + 0xAF0);  // vtable
fn((void*)(weapon + 0xAF0), 1);  // 1=纳刀合并(推定)  0=拔刀分离
```

若下一帧被更新器拉回 → want 路径仍判拔刀；在收刀系动作期间测，或对照观察
+0xC71/+0x181 的实际走向（见待钉 #2/#3）。

## 待钉清单

1. ValorProbe 钩 RVA 0x325130 / 0x3253F0：log（槽号, 状态值）序列，游戏内对照
   030/047 通道 → 钉死采样链路（Ghidra 丢 r8 的那一步）
2. 钩 RVA 0x2150D10 / 0x2374710：log (w, cur, 参数) 翻转时序 → 实测极性
3. weapon+0xC71 的维护方（触发器只读；谁把它同步成"已应用状态"？）
4. weapon+0xAB8（98/100/101/102 锁存）的读取方
5. weapon+0xA50 / +0x23C0 的语义与写入方
6. 三个控制器（+0xAF0/+0xFA0/+0x1450）与 刀/鞘/配件 的对应；挂点表资源本体是哪个
   文件（顺 0x1419A1370 的 id 构造 + 资源管理器类型描述符查）
7. 0x214/0x215（set≠3 分支的纳刀期望 id）与 3:99 架势的关系

---

## 特殊纳刀系(98/306 + 100/312)无敌帧 + 受击切换(2026-10-01)

### TIML 已改(游戏目录 w03_00.lmt,快照 .pre-iframes.bak)

| entry | 动作 | 修改 |
|---|---|---|
| 306 | 98 特殊纳刀 | mFlag f0=0x06400000 / f10=0x07400000 / f15=0x07400006(b26 全程),mTrigger f0=2048 |
| 312 | 100 | mFlag f0=0x05000000(b26+b24 全程),**新插入** PlMotionCommon mTrigger 轨 {f0=2048} |

- 幂等脚本 `apply_sheathe_iframes.py`(仓库根);**blend 导出会整个替换 entry(坑⑮),
  导出后必须重跑**。312 的插入 = 文件尾 16 对齐追加 keys/transforms 数组 + PlMotionCommon
  类型槽 {toff,tcount} 重指针(types/transforms/keys 数组已验证 entry 独占)。
- 98→99(入鞘状态)→100 的链按用户描述;99 无独立 LMT 映射,未改。

### 受击检测 = 未决主问题(与 09-18 翻滚结论的关系)

- 翻滚式 i-frame = 纯碰撞排除,玩家侧零信号;见切/居合可测是因为有弹刀交互
  (type-6 + 呈现 + event bit15/250)。**TIML b26+2048 配方的无敌帧落在哪一边,
  局内挖矿一测便知**——若 306/312 被打时 IFRMINE 挖到任何 bit 翻转/FXNOTIFY 行,
  走既有触发链;若全静默(翻滚同款),下一步 = 钩 TIML no-hit 碰撞拒绝点
  (FUN_141f66dd0 上游,当年 IFrame 机制的"最后一格")。
- mod 已带挖矿器(部署 md5 48d4f78705e1e7d0623b866c794bdc8a):
  `ValorGauge.sheath-counter.txt` `mine 98,99,100` → 日志 `IFRMINE evN 0x.. -> 0x.. rose=[bit..] act=3:x`;
  **F5 = "被打到了"手动标记**(与 gauge-lock 共用 F5,gauge-lock 当前为关不冲突)。
  rule 行(注释待启用):`rule <ids> <eventBit> <toSet> <toId>` = bit 上升沿 →
  counter 窗口 + no-hit(g_forceInvulnSec)+ force DoAction(toSet:toId)。
  见切同款顺序(先叮后注入),不会踩 09-25 gate 杀弹反链的坑。

### 2026-10-01 挖矿结果 + 分层定案(用户确认: TIML 处理的是"普通无敌帧")

- **首局挖矿(ValorGauge.log 14:17 会话)**:5 次 3:98 的 event-bit 翻转全部确定性
  (ev2 75/76/77 清→76/77 升、ev3 bit127 清、进 99 时 ev0 bit9 升 = 时间轴事件);
  唯一一次 F5 MARK(受击,发生在 3:99)旁**零 bit 翻转、零 FXNOTIFY**。且本次挖到
  一个设计缺陷:bit250 在会话里从头到尾是置位态(ev7=0x18400000 恒定),"只记边沿"
  会漏掉重复置位——下轮挖矿若要盯 bit250 必须先清位或记绝对值。
- **分层定案**:TIML b26(+pool+0xE4 注入、翻滚)= **查询层排除**,命中候选被剔除,
  零事件、零表现,游戏层面"没被攻击过";见切(b22 窗口 f0-f12!)/居合(handler
  发 0x7D4)= **仲裁层弹反**,命中到达仲裁被转成弹反,可观测(叮/bit15/bit250/
  蓝光)。叮的呈现是动作 id 门控 handler 发的(86/101/102 各有专属),98/100 没有。
- **bit250 置位方(decompile)**:四处调用全在伤害结算函数 FUN_141217e50 /
  FUN_14121ea10 内——只有伤害真正结算才走;查询层排除的命中到不了这里。
  pool 查询 FUN_140aee690(pool,type) 的 **type 8 = no-hit 检查**(pool+0xE4<=0 →
  bail),但其调用方(3999698 区)全是 FSM 属性 getter(FUN_1412fdaf0/fdb50,
  "我是否无敌"条件求值),不是命中仲裁路径。
- **下轮实验 = 分发器监视(SHEATHDISPATCH)**:mod 版已装 HitDispatch 钩子
  (FUN_141f71960,签名表第 4 项),type-6 分发在 mine ids + 86(对照组)期间记
  `SHEATHDISPATCH type=6 code=.. dmg=.. REJECTED`;已知:见切被弹命中 = non-0xD
  + 无伤害结算(仲裁层),翻滚 = 零 type-6。若 98/100 的否定在日志里出现 →
  启用 `drule <ids> <toSet> <toId>`(分发器否定触发,配置已带注释行,
  Shift+F6 生效)= 纯无敌帧+受击切换闭环;若为空 → b26 排除在分发器上游,
  只能走超甲窗路线(b24 让命中连上,DmgCalcHook 检测,代价是挨伤害)。
  注意:drule 按玩家动作门控,不分交互双方,多人/怪物互殴存在误触发可能,
  出现再按交互对象字段收紧。

### 2026-10-01 (2) SHEATHDISPATCH 零输出 = 钩子 bug,不是阴性结果

- 第二局(部署 48d4f787→f9ef49e3):见切弹反完整发生(FX 0x7D4+flash 链+auto-force
  全工作)但 SHEATHDISPATCH 一行没记(86 对照组必中都没记)。日志考古:**当前源码的
  分发器日志格式(`rejected during 见切 (type=`)从未在任何会话产出过(0 条)**;
  旧的 133 条 HITDISPATCH(code=0x6B)是"[log only]"老格式=类型镜像加入**之前**的
  代码。结论:vtable+8 类型镜像读法(interactType 永远≠6)是坏的,钩子在入口
  提前返回。09-17 的"clang = non-0xD + 无伤害结算"语义仍然有效(那是老代码验证的)。
- **修复(md5 ab2d10039ba8a5344a41ed211e30ecbd)**:去掉类型门控(类型仅记录),
  回到验证过的语义:0xD=环境洪泛排除;non-0xD=真实交互尝试,记
  `SHEATHDISPATCH code=0x.. type=.. dmg=.. REJECTED/DAMAGE act=3:x`。
  drule 触发条件同步改为 non-0xD && 无伤害。
- **已知混杂因素(下局判读要点)**:老笔记记录 non-0xD 在 no-hit 窗口开着期间
  持续出现("负 float -136→-350 与 0x6B 交替"),连空挥(窗口开但没被攻击)也有
  ——这是当年弃用"分发器 counter window"的原因。所以下局必须做**空挥对照组**:
  同样出 98/100 但不被攻击,对比被攻击时 F5 标记附近的 code 值。若 hit/whiff 的
  code 无差别 → 分发器不能区分"被攻击",转超甲窗方案(b24+DmgCalcHook)。
- 另:用户本局的三次 F5 MARK 落在 3:99/3:99/3:0(98→99 链后段)——下次尽量在
  98/100 本体动作里被打时按,数据更纯。

### 2026-10-01 (3) 方案定案: 超甲尾窗 + arule(霸体命中→切换 + 不能死)

- **用户定案**:分发器不可判别 → 选超甲路线。设计 = "吃伤害但不能死掉";
  否决怪物出招触发(空挥误触)与手动触发;另提案"复用见切 FSM 播不同 LMT"
  (让原生弹反机制整套为纳刀工作)留待后续讨论。
- **TIML(apply_sheathe_iframes.py 已更新,幂等)**:306 的 f15 回退原值
  0x03400006(去掉 b26)= **无敌 f0-f14(0.25s)+ 霸体 f15-motion 末**(b24 从
  f10 延续无清除键)。霸体段命中可连接 → DmgCalcHook 可检。312 保持全程无敌
  (100 是拔刀攻击,不需要受击反应)。99 无 LMT 映射,未处理。
- **mod arule(部署 md5 84ef445a7a1b5b2321fb8c7200a7a9e7)**:配置
  `arule <ids> <toSet> <toId>`(已启用,目标 3:77 与 force-action 一致)。
  DmgCalcHook 内:攻击者≠本地玩家 && 当前动作∈ids → ①HP 钳制(设计"不能死")
  ②每动作一次:counter 窗口 + force 切换。
- **HP 链(decompile 钉死)**:FUN_141f62320 = `mov rcx,[rcx+0x7630]; jmp
  FUN_141216910` = `movss xmm0,[rcx+0x64]` → **HP = *(float*)(*(player+0x7630)+0x64)**,
  同对象 +0x60 疑似 MaxHP(未验)。伤害数值无法在 DmgCalc 出参上钳(param_3 是
  out-only,调用方拿它做分类,扣血在函数内部已完成)→ 钳 HP 字段本身。
- **已知边界**:①多人时"攻击者≠我 && 我在纳刀动作"会把打别人的命中也算进来
  ——a2(疑似防御方)已在前 8 条 `SHEADRULE hit` 日志里记录,局内对照 a2==player
  即可钉死防御方字段收紧;②HP 钳制依赖死亡判定晚于伤害算术(不屈类技能证明
  1HP 可存续),若出现钳了仍猫车 → 需再挖死亡判定点。

### 2026-10-01 (4) b31 = 防御/弹开窗位(突刺实验考古 + 已应用到 306 尾窗)

- **用户回忆**:之前把突刺(79→LMT105)改成带防御,防御时有音效;怀疑特殊纳刀
  不允许防御帧。仓库根 `105.txt` = 当年改过的 105 TIML dump:**mFlag f0=
  2269118470 = 0x87400006(b31+b26+b25+b22+b1|b2),窗口 f0-f18**;原版突刺
  f0=0x02400006(无 b31/b26)。新增位只有 b31+b26 → **b31(0x80000000)= 防御/
  弹开窗**,b26 是我们已知的静默无敌。全部吻合:见切叮窗 f0=0x80400000 =
  **b31+b22**;307/308 无 b31(叮靠 handler);突刺加 b31 出防御音 = 
  **叮类机制 TIML 可写,不需要动 handler**。
- **注意**:当前部署的 105 已是原版(用户的突刺防御改动被 10-01 导出冲掉,坑⑮
  再+1;要恢复按 105.txt 配方把 f0 改 0x87400006 即可)。
- **已应用(apply_sheathe_iframes.py 更新)**:306 f15 = 0x87400006(照抄配方)
  = 无敌头 f0-f14 + **防御尾 f15-末**。arule(霸体命中→切换+HP钳制)保留兜底:
  若纳刀家系拒绝防御(命中照连),arule 照常触发;若防御生效,命中被弹开,
  是否留下可测信号(音效走 Wwise/FX?bit15?)看局内日志。
- **306 当前布局**:f0=0x06400000(无敌+霸体头)/ f10=0x07400000 / 
  f15=0x87400006(防御尾)/ mTrigger 2048@f0。312 不动(全程无敌)。

### 2026-10-01 (5) 用户规则: b26 与 b31 同窗时只有无敌生效 → 防御窗必须无 b26

- 第一轮 b31 实验(105 f0 与 306 f15 都用 0x87400006 = b31+b26 同窗)无声。
  用户回忆关键规则:**无敌帧和防御帧重叠时,只有无敌帧生效**——解释了一切:
  b26 在查询层把命中先吃掉,防御交互(b31)根本不发生。当年的可用配方应是
  无 b26 的纯防御窗(105.txt 是含 b26 的后-state,不是当初的工作态)。
- **已改**:105 f0 与 306 f15 → **0x82400006(b31+b25+b22+b1|b2,无 b26)**;
  306 无敌头 f0-f14(b26)不动,与防御尾无重叠。脚本目标值已同步(幂等验证)。
- 306 当前布局:f0=0x06400000(无敌+霸体头 f0-f14)/ f10=0x07400000 /
  **f15=0x82400006(纯防御尾到末)** / mTrigger 2048@f0。
- 判读:105 出防御音 = 配方修复成立;306 出音 = 纳刀家系允许防御,叮+切换检测
  看日志;306 仍无音 = 纳刀家系真封死防御 → 见切 FSM 播不同 LMT 方案上位。

### 2026-10-01 (6) ★bit28 = 格挡位(定案,修正 b31 误判)★

- **用户提供的历史会话**(早前已验证有效的实验):全武器 LMT 位分布扫描结论——
  **mFlag bit28(0x10000000)= 格挡有效**。CB/SnS/GL/GS 都有,太刀家系完全没有;
  守备架势动作全是"单键 f0 整段有效"形状(CB 5/40/41/42、SnS 5/6/7/47-50 =
  0x10000000 或 0x10000000|bit31)。当时实验:突刺 105 f0 → 306184198
  (=0x12400006 = bit28+b25+b22+基线,**无无敌帧**)→ 正面格挡成立(格挡音效/
  削伤/掉耐力/不硬直),背对照常掉血。**修正:b31 不是防御位**(此前 (4)/(5) 节的
  b31 推断作废;b31 在守备架势里只是伴随位,语义未定)。
- **机制补充(该会话已摸清)**:方向判定在引擎侧(按玩家朝向防御锥过滤,"背后照
  打");格挡后果链(防御动画/掉耐力/伤害大减)是引擎在"格挡成功"分支统一做的。
  入口:伤害分类写反应管理器 **+0x769=0x100/0x101**,仲裁读 +0x768/769/76A 后调
  **FUN_1411cac40(格挡反应装填)**;格挡参数 **weapon+0x8A0/0x8A4(守备时长)、
  +0x8A8 起三组等级表**——若太刀参数块缺数据导致不格挡,DLL 补参数即可。
  特殊反应(叮+蓝光)的候选评估器表在反应管理器里是**通用**的(FUN_1411c26e0
  只是其一,旁边一族读属性 0x35 按等级查表);复用居合系槽位或 DLL 置同样属性
  即可白得演出。
- **用户规则(有效)**:无敌帧与格挡帧重叠时只有无敌生效——格挡窗必须无 b26。
- **已应用(apply_sheathe_iframes.py 目标已同步)**:105 f0 → 0x12400006
  (对照,当年配方原样);**306 f15 → 0x12400006**(无敌头 f0-f14 + 格挡尾到末,
  无重叠)。306 布局:f0=0x06400000 / f10=0x07400000 / f15=0x12400006 /
  mTrigger 2048@f0。
- 判读:105 正面格挡有音效 = 配方在当前版本成立;306 尾段格挡 = 纳刀家系允许
  (接着钉切换信号);306 无格挡但 105 有 = 家系差异(查 306 自身或 +0x8A0 块),
  arule 兜底仍在;105 也无 = LS 武器参数块问题 → DLL 补 +0x8A0(方案位置已知)。

### 2026-10-01 (7) ★全链闭环: bit28 整段格挡成立,bit250 = 受击信号,3:37 = 格挡反应★

- **整段单值实验定案**:306 三键全部 0x12400006(bit28 整段)→ **格挡音效成立**!
  此前"全程无敌"假象 = f0/f10 的 b26 键把整段罩死(按动作而非按键窗生效,机制
  未挖,但结论可用:**带 b26 的多键布局在 306 上不可用,防御动作用单值 bit28**)。
- **3:37 = LS 格挡反应动作**:格挡成功 → FUN_1411cac40 装填格挡反应 → 当前动作
  换成 37(太刀原生不格挡故从未见过)。此前"预知被打马上换动作"之谜 = 同一现象。
- **★bit250 在 3:98 期间上升(IFRMINE 实录)**:格挡交互点亮居合家系的否定事件位
  ——**受击检测信号到手**,现有 rule 机制直接可用。
- **已接线**:`sheath-counter.txt` 启用 `rule 98,99,100 250 3 77`(bit250 上升 →
  counter 窗口 + force 77)。格挡链全程不走 DmgCalc(SHEATHDISPATCH dmg=0,
  arule 保持静默 ✓)。竞态:引擎先装 37,rule 下一两帧 force 切走 → 37 会闪
  1-2 帧;介意则后续钩 FUN_1411cac40 对纳刀动作空转抑制。
- **306 终态**:三键 0x12400006 + mTrigger 2048@f0(整段格挡纳刀)。无敌头设计
  与 b26 多键方案暂时放弃(b26 毒化整段;若要"前段无敌+后段格挡"需先挖 b26
  的按动作锁存机制)。apply_sheathe_iframes.py 目标值待最终定案后同步(当前
  重跑会把 306 洗回头尾布局——实验期间勿跑)。

- **(补)rule 首测未触发的原因推断**:引擎在仲裁同帧就把动作切成 37,渲染线程
  轮询采样时动作门(只放 98/99/100)已失效 → 错过 bit250。已把 37 加入 rule/mine
  监听(bit250 在格挡演出期保持,37 态可采样)。仍不触发则把检测移进游戏线程钩子
  或直接钩 FUN_1411cac40(抑制 37 + 原地 force,零闪帧)。

### 2026-10-01 (8) 37 覆盖竞态 + latch 污染修复(部署 md5 5d1031bb9f6607efe5373695e506673f)

- **诊断版实录真相**:rule 其实触发并切换成功了(`FORCE 3:77 interrupting 3:98
  accepted` + `action changed (forced): id=77`)——但**引擎的格挡反应晚一拍把 77
  覆盖成 37**;且我们自己的 fire 置位了全局 g_clangLatch,而该 latch 只在
  86/101/102 开始时复位(历史防 force 循环补丁)→ 纳刀系的后续格挡全被拦死
  (= "两个方式都没有切换")。
- **修复三件套**:① rule 独立 per-rule 触发锁(随监听动作 id 变化重置),不再
  读写全局 latch;② fire 时开 1.2s SHEATHREASSERT 窗口;③ DoActionHook 窗口内
  吞掉 {3,37}(return true 不调 original,37 彻底不入管道,77 稳定播完)。
  顺手修:bit 规则循环跳过 drule/arule(它们的 eventBit 残值 0 曾被当 bit0 规则
  误评估 = 626711 "event bit 0 rose" 之谜)。
- SHEATHRULEDBG 保留(只在 bit 高电平时记,共享 480 行配额)。

- **✅ 用户局内确认(2026-10-01 终态)**:格挡被击 → 音效 + bit250 → rule 强制切 77,
  37 被吞,不再覆盖。306 终态 = 三键 0x12400006 + mTrigger 2048@f0;mod 部署
  md5 5d1031bb9f6607efe5373695e506673f。

---

## 方案2开题: 蓝色无敌帧(见切/居合弹反)判定层(2026-10-01 开挖)

### 今天钉死的结构

- **格挡分类链(补全)**:伤害结算 FUN_141f66dd0 内,`uVar7 = FUN_1413a2bf0(交互上下文)`
  是分类码(**2=格挡**;uVar7 同时写进调用方出参);uVar7==2 且防御方组件字节
  +0x327f≠0 时:`mgr+0x769 u16 = 0x100`(+0x327d==0)或 `0x101`(+0x327d≠0,
  顺手清 +0x327e)。u16 小端跨 +0x769/+0x76A 两字节,仲裁按字节读(这就是
  "+0x768/769/76A 判断"的真身)。格挡会进伤害结算(削伤);**弹反(叮)不走
  伤害结算** → 决策在更上游。
- **评估器家族定性**:FUN_1411c2780/27c0/2800/2840/2880… 全是同形 getter——
  读属性 **0x35(气刃等级)** 查分级参数表(_DAT_145011f68+0x5c0 起,步长 0xC,
  每级 3 个 u32,默认 1.0)= "数值随等级缩放"的取参层,非决策层。
  FUN_1411c26e0(mgr)= `if (mgr+0x7C0) 消费属性 0xDE idx2(成功计数)` +
  0xa3/0xb6/0x84(3) 读取——供 FSM 条件用。

### 工作假设(下一步验证)

**弹反窗 = 反应池里的一个计时器**(与 no-hit pool+0xE4=查询 type8 同族;
FUN_140aee690 的 type0-10 各查一个池偏移:0x58/5c、0x84-0xa0、0xc8、
0xe4、0x108、0x10c…)。仲裁按"哪个计时器在走"分流 格挡/弹反/掉血。
见切/居合的流程把各自的计时器写上 → 叮+蓝光+无效化。**挖到偏移后,mod 在
98/100 期间直接写它 = 原生弹反(太刀私有演出,不碰通用格挡,无串味问题)。**

### 工具已就位

- **POOLDIFF(probe 专用,md5 6d92a24c416dc685d31a7a2c8dd86d61)**:每帧池快照
  (0x180B)diff,只记**上升沿**(0.5 以下 → 以上 = 防御窗开启),带 act 标签。
  probe 轮询环已挂 PollPoolDiff()。**单 DLL 纪律**:挖矿会话把 ValorGauge.dll
  挪走、ValorProbe.dll 换上;完事换回来。
- **会话协议**:见切弹反 3+ 次(被击瞬间无需按键,池 diff 连续)→ 空挥见切 2 次
  (区分"开窗"与"被打")→ 居合 101/102 各 1 次 → 日常几刀(基线)。
  发 ValorProbe.log,POOLDIFF 行里在见切时竖起、空挥也有的 = 弹反窗计时器候选;
  只在被击时竖起的 = 成功 latch 类。

### 2026-10-01 (9) 池假设证伪 + bluesim(模仿 handler 输出)上桌

- **POOLDIFF 两次会话结论**:13 次 86 在池区(0x180B 全量)与 mgr 分类区
  (+0x760-790)**零痕迹**——池计时器假设证伪(池 = no-hit/排除类计时器,弹反
  不在其中)。+0x768 = 居合开场状态旗(102 开打置 1、待机清零),非受击信号。
  首轮零数据的另一原因:滤器阈值 0.5 > 弹反窗时长,已修(0.05 + 跳变)。
- **嫌疑收拢到武器字段家族**:86 的 handler FUN_142125060(按动作 id 经函数
  指针表派发,自定义动作没有)写 +0x2399 锁存 + **+0x23A0 = playerParam+0x8A8**
  (矿证 4/4 弹反 vs 0/5 空挥的字段!);居合版 FUN_142124F70 写 +0x2CED +
  +0x23A4 = +0x8B8;+0x23A0/0x23A4>0 期间每帧发 event bit 151(超强呈现)。
- **bluesim 已实装(mod,部署 md5 8c954f6a877988eb7fe99b4fa7cbd20e)**:
  配置 `bluesim <ids>`(已启用 98,100)——期间模仿两个 handler 的全部输出:
  +0x2399/+0x2CED 锁存、FX 0x7D7(蓝光)+0x7D4(音)、每帧 +0x23A0/+0x23A4 ←
  参数表(playerParam = *(weapon+0x7810),FUN_142125060 同款链)。
  TIML 实验态:306 三键与 312 f0 全部 = **0x80400000(见切 f0 原样:b31+b22)**,
  mTrigger 2048@f0。bit28 格挡临时下线(若 bluesim 不成立一键回滚)。
- **判读**:格挡被击时出现 叮+蓝光+无伤 = 原生弹反被点亮(bit250 应同步上升,
  rule 250 自动切 77 = 完全体);起手有蓝光但被打穿 = 缺 FSM/属性层,继续挖;
  连蓝光都没有 = bluesim 字段写错/被覆盖。BLUESIM armed 行确认函数在跑。

### 2026-10-01 (10) bluesim 阴性结论 + F7 试枪改 0x7D7

- **bluesim 阴性**:函数确认在跑(BLUESIM armed ×3 @3:98,配置/部署 ✓),但局内
  无蓝光、无免伤 → **+0x2399/+0x23A0/+0x23A4/+0x2CED 全是 aftermath(成功后的
  后果字段),不是弹反的因**。用户结论成立:蓝色无敌帧焊死在 86 的代码路径里
  (begin handler FUN_1417FDAE0/通用 begin FUN_141783470 的 arm 逻辑 + FSM 状态)。
- **疑点**:bluesim 直接调 ADD670(0x7D7)却无蓝光——要么 emitter(+0x8808)在
  出招首帧还没就绪,要么 **0x7D7 经 ADD670 根本不出蓝光**(0x7D7=蓝光只来自
  86 出招时 FUN_1417FDAE0 尾部双发的 FXNOTIFY 旁证,从未被直接调用验证过;
  F7 默认 id 一直是 0x7D3=红光)。fx-test.txt 已改 `0 0x7d7`,**F7 一下即验**。
- **两条真路线**:A) F5 矿法换参照系——之前是 弹反 vs 空挥(找 aftermath),
  改成 **出招 vs 待机** 的 f0-f12 窗口内 diff(找 arm 写入,两种出招共有的
  开窗写入才是因);B) **方案1(见切 FSM 播不同 LMT)**——蓝色焊死在 86 的
  代码路径,让引擎真的跑 86、只换 LMT,整套原生机制直接成立。证据越挖越
  指向 B 是正解。

### 2026-10-01 (11) ★方案 B 落地: motion-swap 工厂补丁, 见切播自定义 LMT★

- **用户局内确认**:SHEATHEMOTION 补丁生效——见切(86)成功播放 entry 141 的动作,
  原生见切机制(arm/FSM/弹反)原封不动。fx-test 0x7D7 = 见切蓝光+音效直调验证 ✓。
- **实现**:mod 初始化扫描 LS set-3 动作类表(RVA 0x2EF5C30, 113 项), 对每个类
  工厂(vtable+8/+0x10)前 0x140B 扫描 motion id 立即数(fromMotion), VirtualProtect
  改写 + FlushInstructionCache。配置 `ValorGauge.motion-swap.txt` = "140 141"
  (缺失=关闭)。已修一个条件编译嵌套 bug(补丁器曾落进 F11 probe 区永不编译)。
- **141(用户新增基底)**:blend 克隆的见切 entry(142f);其 blend 快照 TIML 无
  b31 → 已修成 140 模式(f0=0x80400000 → f100=0x80000000,b31 全程 b22 前段),
  并入 apply_sheathe_iframes.py(换皮导出后重跑即恢复)。
- **脚本工作流实战验证**:用户导出(新增 141)把 306/312/105/141 全部冲回快照 →
  `python apply_sheathe_iframes.py` 一键重打全部布局 ✓。当前脚本目标:306/312 =
  见切形窗口(0x80400000, 实验态)、105 = bit28 格挡、141 = b31 窗。
- **设计待定(用户暂缓)**:见切=决策点(受击→反击/未受击→进收刀)的 decide 规则;
  98 最终防御(普通 b26 无敌 vs 见切形窗口);306/312 终态布局。定案后同步脚本终态。
