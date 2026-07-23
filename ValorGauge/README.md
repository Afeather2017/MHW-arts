# ValorGauge

Native Stracker Loader plugin that renders a prototype Valor gauge inside MHW's
DirectX 11 frame. It does not require SharpPluginLoader.

The current milestone displays a fixed 65% gauge. Press `F8` to hide or show it.
Runtime diagnostics are written to `nativePC/plugins/ValorGauge.log`.

Build from a Visual Studio developer environment:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
```

The post-build step installs `ValorGauge.dll` into the active game plugin folder.
