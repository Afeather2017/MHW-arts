# ValorGauge

Native Stracker Loader plugin that renders a prototype Valor gauge inside MHW's
DirectX 11 frame. It does not require SharpPluginLoader.

The 100-pixel gauge at the top-left starts at 0% and gains 10% when a mapped
normal-mode Long Sword attack transition occurs. The current charge and player
action set/id are shown in large red text on its right; full charge displays
`VALOR READY`. Press `F8` to hide or show it.

At full charge, the plugin resolves the documented Long Sword weapon pointer at
`player + 0x76B0`, then the spirit level at `weapon + 0x2370`. It sets the aura
to red (`3`), matching the strongest level used by the working Lua Long Sword
mod. The overlay changes from blue to red and logs the resolved address.
Runtime diagnostics are written to `nativePC/plugins/ValorGauge.log`.

The plugin signature-scans and hooks `ActionController::DoAction`. Long Sword
action transitions are logged automatically as action set/id pairs. No address
file or hotkey is required for this automatic logger.

## Manual state-change logger (fallback)

Put the hexadecimal address of a 32-bit live FSM/action state in
`nativePC/plugins/ValorGauge.state-address.txt`, for example:

```text
0x7FF612345678
```

Press `F7` in game to reload the file. The plugin polls the readable address and
writes only the initial value and changes to `ValorGauge.log`. The address is a
runtime virtual address and will normally change after restarting the game; this
logger deliberately does not assume an unverified player/FSM pointer or offset.

Build from a Visual Studio developer environment:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
```

The post-build step installs `ValorGauge.dll` into the active game plugin folder.
