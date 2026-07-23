# MHW-arts: Hunter Arts Mod for Monster Hunter World

## Project Overview

**Goal:** Introduce Monster Hunter XX (MHXX) Hunter Arts system into Monster Hunter World (MHW) as a mod.

**Approach:** Start with a minimal milestone: introduce a simple damaging motion on Long Sword, then scale to full Hunter Arts system with meter, multiple arts, and eventual MHXX motion import.

**Current Phase:** Native Valor system prototype — replace the incompatible 2021 `ValorLS.dll`, render an in-game gauge, then connect it to the Valor FSM state.

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

### 🔄 In Progress
- Find the player/weapon FSM pointer and the Valor-related state or variable.
- Replace the fixed 65% prototype value with live game state.

### ⏳ Pending
- Detect Valor charge gains and depletion.
- Detect entry into and exit from Valor mode.
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
- **Current behavior:** Displays a fixed 65% blue gauge; `F8` toggles visibility

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

*Last Updated: 2026-07-23*
*Current Milestone: Connect the native Valor gauge to live FSM state*
