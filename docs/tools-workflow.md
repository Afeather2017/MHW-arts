# 工具链与工作流程

> 构建/部署/逆向/调试环境的操作手册。坑的完整事故记录见 `SESSIONS.md`。

## 游戏数据路径

- 游戏根 `E:\SteamLibrary\steamapps\common\Monster Hunter World\`；chunk 层叠
  （G0 基础，G1-G11 Iceborne，G60 高清包；高层覆盖低层）。
- **`chunk-extracted/` = 只读基线，永不改**（user 定案）；编辑一律放游戏目录
  `nativePC/` 副本。基线 md5：wp03.col=d8cb2d4f。
- LS 数据：`hm/wp/wp03/`（mot/w03_00/w03_00.lmt、wp03_action.fsm、collision/wp03.col、
  wp03_shake.csk、wp03_param.w03p）。武器目录速查：two=GS swo=SNS mus=LS one=DB
  saxe=SA caxe=CB ham=Hammer sou=HH lan=Lance slg=GL rod=IG bow=Bow lbg/HBG。
- **nativePC 目录/文件名带空格 = 禁用**（Windows 字面匹配不到）——曾让 col 修改 10 天
  未生效。改完数据确认目录名无空格。
- Stracker's Loader 已装；CRC Bypass = nativePC/plugins/!CRCBypass.dll。

## ValorGauge 构建/部署

```powershell
& 'E:\MSVC2022\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' -S ValorGauge -B ValorGauge\build -A x64
& 'E:\MSVC2022\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build ValorGauge\build --config Release
```

- **一个源码树两个 target**：ValorGauge.dll（mod）/ ValorProbe.dll（VALOR_PROBE=1 纯研究）。
  **绝不同时装两个**（双 MinHook 实例互踩崩溃；两边有 sibling 自检兜底）。
- **源码布局（2026-11-01 拆分定案）**：曾经的单文件 `src/dllmain.cpp`（8925 行）拆成
  `src/` 下 23 个模块 .cpp + 同名声明头；`prelude.h`=统一 include 环境，
  `all.h`=全量声明伞（各 cpp 都 include 它），`game_types.h`=ActionInfo，
  `dllmain.cpp` 只剩 LoadForceActionConfig/Install*/InitializePlugin/DllMain 编排。
  模块：log/util/memorymap（钩子签名+地址解析+守卫）/player（锁链+tracker+武器门）/
  env（任务·区判定）/art（战技条）/valor（生命周期+gauge-lock）/spirit（SetSpiritLevel
  配方）/gauge（规则引擎+GaugeAddHook+GAUGEWATCH）/sheath（收刀刚1+gang1+DRAWGANG1+
  SHEATHWATCH）/actions（DoActionHook）/damage（伤害层钩子+全部充能）/trigger（弹反
  窗口+CondEval+force）/fx/motion（motionbind+帧seek+时钟矿机）/prop（属性钩子+
  PROPTRACE+F11 直方图）/rollsheathe（翻滚切納刀+分类器补丁）/sound/vitals/overlay
  （ImGui）/probe+probe_tools（纯研究：RXN/POOLDIFF/miner/diff/ATKTRACE）。
  mod/probe 行为分界仍在各文件内 `#ifdef VALOR_PROBE` 守卫。**改动落在对应模块 .cpp**；
  跨模块符号 = 该模块 .h 里的 extern/声明。`tools/split_dllmain.py` 保留原文→模块的
  段落映射与守卫校验（原文在 git 6c630fc，可
  `git show 6c630fc:ValorGauge/src/dllmain.cpp > src/_dllmain_orig.cpp` 后复现拆分）。
- **POST_POST auto-install 已移除**——部署显式拷贝：游戏关闭后拷
  `ValorGauge/build/Release/ValorGauge.dll` → `nativePC/plugins/ValorGauge.dll`，
  记 md5 链。运行中 DLL 被锁。
- 源码含大量中日注释：**两个 target 都必须 `/utf-8`**（C4819 是烟雾弹，真危害=注释吞换行
  出幽灵 C2065，行号还错位）。git stash 会触发 autocrlf 重写，复现编码问题时避开。
- cmake -B 传 Windows 路径必须带引号（Git Bash 吃反斜杠建出假目录）。
- 配置全在 `nativePC/plugins/ValorGauge.*.txt`；**Shift+F6 = 全量配置链热重载**
  （weapon-type/hit-charge/gauge-lock/gauge-log/gauge-rules/sheath-rewrite/...）。
  路径一律正斜杠（`nativePC\p...` 单反斜杠 = C4129 = 文件从未加载，犯过两次）。
- 配置清单：weapon-type（=3）/ force-action / sheath-rewrite / draw-gang1 / gauge-rules /
  gauge-lock / gauge-log / hit-charge(=-1 退役) / counter-auto / counter-repeat /
  fx-test / action-sound / motion-swap / stamina / vitals / hp-lock / prop-*/state-address /
  addresses（探针另有 atktrace）。

## 逆向工具

- 反编译用 `Reverse/MonsterHunterWorld_dump.exe.c`（盘上 exe 带壳）。工具：`vadump.py`（VA
  dump）、`disasm_rva.py`（**第一参 RVA 不是 VA；第二参是字节数不是指令数**）、
  `fsm_table.py`、`scan_277.py`、`find_xrefs.py`（字符串→xref）、`xref_to_rva.py`、
  `scan_hitlag.py`（**从 .c 抽的 FUN 名是完整 VA，与 RVA 比较前统一单位**）、
  `action_class_table.py`、`scan_lmt_entries.py`、`sig_check.py --emit|--verify`。
- `w03p_tool.py`（w0Xp Blowfish-ECB 解包/编辑；全武器 key 在 gauge-valor/memory-map）；
  `plp_decrypt.py`；`col_research/col_tool.py`（⚠10-20 两 bug 已修，用前确认）；col 格式
  模板 `col_research/mhw-editor-src/010 Templates/col.bt` + 命名 CSV。
- `_cto_report.py`（CTO ノードID/mFlag 窗 dump；**过滤必须按类型全名精确匹配**，
  EventCollision00 也含 "Collision"）；`_patch_cto_node.py`（CTO 单键补丁）；
  `col_research/_atk_table.py`（路由→ATK 数值表，读游戏目录实况）。

## 调试器 / MCP

- **x64dbg MCP**（duty1g v1.3，HTTP 127.0.0.1:9094，Bearer 在 x64dbg 旁 mcp_config.json）：
  实战可用 AttachProcess/EvalExpression/ReadMemory/WriteMemToAddress(参数 byteString)/
  SetHardwareBreakpoint。**坑**：HW 断点的 break condition 与 fast-resume 在该版本不生效
  （每次访问都暂停，暂停风暴杀过游戏两次）；ExecuteDebuggerCommand 回显当前 cip 非命令
  目标；MCP 命令长度 ~256 上限；python socket 直连必须完整 HTTP 头（用 curl 最稳）；
  暂停态写内存先 PauseDebug；TLS 回调断点每次附加从数据库回灌。进程内 VEH+SetThreadContext
  挂 DR 与游戏冲突（两次崩溃）——**硬件断点走 x64dbg 外部调试器，不走 DLL 内**。
- **Ghidra MCP**（LaurieWired 1.4 + 11.3.2）：插件装 `%APPDATA%\ghidra\ghidra_11.3.2_PUBLIC\
  Extensions\`（**必须扁平无 Ghidra 子目录**；启用分类=Analysis；8080 只在 CodeBrowser 开着
  时存活）；bridge `tools/ghidra-mcp/bridge_mcp_ghidra.py` 依赖 pin `mcp<2`；下载走代理
  localhost:1080。12.1.3 工程不兼容 11.3.2——dump exe 在 11.3.2 重建过
  （headless 全量分析 ~28min；headless 与 GUI 抢锁，先杀 GUI+删 .lock）。
- **MCP 配置位置**：ZCode 读 `<repo>/.zcode/config.json` 的 mcp.servers（`.mcp.json` 是
  Claude-Code 惯例 ZCode 不读）；**会话启动时连接，新配置要重开会话**。
  固定顺序：先开两个宿主程序 → `bash tools/check_mcp.sh` 双绿 → 开/重开会话。
- **键鼠注入** `tools/game-control/gameio.py`：SendInput 扫描码级（游戏只认扫描码）+
  截屏 + 焦点。坑：等目标窗口完全起来；独占全屏截屏全黑（游戏用无边框窗口）；
  git-bash 起程序用 `cmd //c start`。截屏超时先怀疑锁屏。
- **LuaEngine**（TCP 28991 单向，命令在 main_loop_begin 帧钩子执行，输出 io.open 落文件
  读回；状态持久）：有 RunFsmAction/RunFsmAction(set,id)/RunLmtAction(0xC000|entry) 强制
  原语（部署版比工作区源码新）；player=GetAddress(0x1450139A0,{0x50})。**未知签名 C++
  注册函数绝不裸调**（strlen(NULL) 崩过游戏）。

## Blender / FreeKinetics 导出部署（MCP 定稿流程）

1. 当前工作 blend 导出（export_ops.exportNodeToFile，绕过操作符 poll；需 scn.freehk_tether）
2. `patch_timl_defense dump`（或 `_shout_semantic_diff.py` 全量）验证目标字段
3. 才拷贝部署；**diff 不干净不部署**（blend 分叉/漂移是最大事故源）。
- 大文件 diff 别在 Blender 里两文件同跑（MCP 30s 超时）——分两次存 JSON 再比。
- struct 包可独立 python 跑，LmtBuffers 依赖 mathutils 只能 Blender 内解析。

## 文档与提交纪律

- 提交信息 = 高密度中文单行（本次改了什么+定案+md5 链+待验证）；AGENTS.md 记入新知识
  （现规则：结论进 docs/ 主题手册，过程进 docs/SESSIONS.md，见 AGENTS.md 文档纪律节）。
- 部署备份命名 `*.bak-<日期>-<用途>`；col 改前先 diff 基准；LMT 事故档留证不删
  （如 w03_00-curled-20260928-2000.lmt）。
- 实验代码退场纪律：会话结束把一次性实验从对应模块 .cpp checkout 回 HEAD，知识留文档，
  签名成果进 sig_check.py/signatures.json（10-25 追捕收尾先例；2026-11-01 起源码按模块
  分文件，不再有单一 dllmain.cpp）。
