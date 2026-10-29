# MHW-arts: Hunter Arts Mod for Monster Hunter World

> **本文件 = 项目状态 + 全局铁律 + 文档索引**（目标 ≤300 行，必须能一次性读完）。
> 主题知识在 `docs/` 各手册；过程档案在 `docs/SESSIONS.md`。
> **写码前**：先读本文件与相关主题手册；**完事后**：按文末"文档纪律"归档新知识。

## Project Overview

**Goal:** 把 MHXX 的 Hunter Arts 系统引入 MHW（当前武器：太刀 wg10/wp03）。
**方法**：最小里程碑起步（一个伤害动作），逐步扩展到完整系统（气焰槽/多战技/MHXX 动作导入）。

### 当前状态（2026-10-29）

- **战技系统 DLL 侧落地（2026-10-26）**：战技条=DLL 外置 `g_artGauge` 0..100（overlay
  文字行 `ART n%`，先文字后画条）；连接命中充能（`ValorGauge.art.txt` charge 行，白挥不充，
  **只认大型怪物目标**——10-29 目标过滤，地形/可破坏物不充，战技/勇气/练气全在门后）；
  释放闸门=条 <100 时 DoAction 钩子吞 3:87（R+△ = 键鼠 Ctrl+左键 的内部请求），
  满放行、游戏接受即清零。**FSM 侧接线铁律：战技节点走请求路径入边（ActionNo=87），
  按键属性链接边读原始按键会绕过闸门**。闸门局内验证通过（user：吞请求不截断当前动作）。
  任务门（10-27）：条=战斗区会话状态，清零三触发器（进真任务 quest id 0→正边沿——
  探索→同图任务只有它能抓到/换区边沿/战斗区外），显示=InCombatZone（State==2 ∪
  stage 白名单：猎场 101-109/竞技场 201-203/剧情区 401-417/**训练场 504**；据点/房间/
  主菜单排除；✅user 局内验证通过 10-28）。缺 art.txt=系统关；Shift+F6 热重载。
  目标过滤（10-29 两轮）：所有充能（含原生练气回充）统一挂 DealDamage 钩子 em 直读门
  （arg1=em，+0x12280 查 bitset；dmgCalc a2 三候选探测已退役——a2 非 em，log 实锤）；
  原生回充 +0x2124f13 按 `g_lastAppliedTargetMonster` 锁存归零；放行=bitset ∪ 训练场
  木桶 {57,58}（user 定案原版等价）；DealDamage arg2=伤害 int 已进 ARTGAIN 日志=按伤害
  充能地基；**伤害缩放已启用**（`charge-dmg 100 15` / `chargehit-dmg 100 0.30`：
  额度×伤害÷100，删行回固定）。
  部署 DLL md5 `bb590658`。
  详见 `docs/gauge-valor.md` 战技槽节。
- **勇气模式 plan B（现行，user 定案，局内验证通过）**：白刃=充能相位，红刃=勇气模式。
  普通/纳刀链命中充勇气条（=等级计时器 0x2374），白刃满条跨域 → DLL `SetSpiritLevel(3)`
  直写红刃入场；红刃到期/被扣级 → 折返白刃清条。FSM 勇气分支挂 `オーラレベル赤以上`
  （FSM 侧归 user）。HUD 刻度条色不随任务中写入刷新（仅入场初始化）→ workaround 已落：
  keep-white（太刀掉 0 即回白，进任务前已白）+ overlay 模式行（SPIRIT lv/相位/条%）。
  详见 `docs/gauge-valor.md`。
- 动作/数据侧：勇気纳刀链、刚1（156/345 槽、DRAWGANG1 拔刀刚一）、无双斩 343（专属伤害行/
  双段拖尾/屏震/喊声）已落地；強格挡配方、方向派生使能位、LinkMotion 新招管线均已闭环。
- **进行中**：战技系统 FSM 侧接线（user）+ 战技链路局内验证 + 战技条文字→画条；
  勇气纳刀后衔接连段补充；部分招式后游戏仍认为收刀态（312 尾段，DLL force 路线待做）；
  气刃无双斩声音等特效补充。
- 远期：**战技发动/技能选择/多战技槽**；完整 MHXX 动作迁移。组合键/方向派生（R+方向）
  数据侧已解决：TIML 使能位 + FSM 配边，FreeKinetics 窗口编辑器已有实现
  （配方见 `docs/fsm-input.md`）。

## 数据位置速查

- 游戏根 `E:\SteamLibrary\steamapps\common\Monster Hunter World\`；mod 放 `nativePC/` 同路径
  覆盖（Stracker's Loader + !CRCBypass 已装）。
- **`chunk-extracted/` 只读基线永不改**；编辑一律放游戏目录副本。
- LS：`hm/wp/wp03/{mot/w03_00/w03_00.lmt, wp03_action.fsm, collision/wp03.col, wp03_shake.csk}`。
- id↔entry 映射 `action-list.md`；FSM 全图 `wp03_fsm_map.md`；TIML 参数注册表
  `timl-editor/TIML_PARAM_REGISTRY.md`。
- **目录/文件名带空格 = 禁用**（Windows 字面匹配失效）；从游戏拷出的文件先 `attrib -R`。

## 构建与部署

```powershell
& 'E:\MSVC2022\...\cmake.exe' -S ValorGauge -B ValorGauge\build -A x64
& 'E:\MSVC2022\...\cmake.exe' --build ValorGauge\build --config Release
```

- 一源码两 target：ValorGauge.dll（mod）/ ValorProbe.dll（VALOR_PROBE=1，纯研究）。
  **绝不同装**（双 MinHook 崩溃）。auto-install 已移除——显式拷贝到
  `nativePC/plugins/`，游戏必须关闭，记 md5。
- CMakeLists 已含 `/utf-8`（勿删）；配置文件路径一律正斜杠。
- 配置在 `nativePC/plugins/ValorGauge.*.txt`；**Shift+F6 = 全量热重载**。清单见
  `docs/tools-workflow.md`。
- 武器门：`weapon+0x9F8 == 3`（LS）——所有 LS 专属偏移/原生调用的硬前置。
- 实验代码退场纪律：一次性实验 checkout 回 HEAD，知识留文档，签名成果进 sig_check.py。

## 全局铁律（踩过的坑，违反必翻车）

1. **FSM 属性名必须是注册表字符串**——没有 `オーラレベル黄以上`，只有白以上/赤以上。
2. **裸写 +0x2370 不驱动条件真值；SetSpiritLevel 配方（14211c5f0 复刻）驱动**。
3. **FSM action id ≠ LMT entry ≠ 类表槽位**（三套编号；映射查 action-list.md）。
   同招双 id：FSM 链接发节点 ActionNo，按键路径发请求 id。
4. **改 LMT 必须完全冷启动**；**凡动内容一律走树导出**（二进制手术引入第二层崩溃）；
   entry 345/344/110 只吃原生导入血统。
5. **blend 导出部署前必跑全 entry TIML 语义 diff**；二进制侧调参必须回填 blend。
6. **输入属性随动作 PlMotionInput 使能位写入**——条件/组合键不生效先查当前动作的位。
7. **pool+0xE4 no-hit = "游戏层面没被攻击过"**，与弹反计数互斥（帧杀保护必须后置于叮）。
8. **detour 必须转发原函数返回值**（rax/xmm0）；HW 断点走 x64dbg 外部不进 DLL。
9. **新建 TIML action 必补 freehk 元数据**；新键插值禁 BEZIER（导出静默丢弃/崩）；
   fcurve 删键用整条重建+重查残键。
10. **"改了没反应"≠语义否定**——先 isolated 类别角色、查插件面板传参、查目录空格、
    查使能位、查属性注册。
11. 本地玩家锁 + 武器门是所有 gameplay 写入的前置（多手柄/异武器防误伤）。
12. 术语：气刃值=0x2374 计时器条（白刃期=勇气充能条）；练气=0x2368 **红条**（原版自理，
    完全不管）；等级=0x2370；战技槽=外挂新字段（蓝条，DLL `g_artGauge` 已落地，
    不复用练气）。字段全图 `docs/memory-map.md`。

## 文档地图

| 文档 | 内容 / 什么时候读 |
|---|---|
| `docs/memory-map.md` | 玩家/武器/控制器结构偏移、关键函数 RVA、静态表、w03p 参数。查地址/字段语义 |
| `docs/gauge-valor.md` | 气刃/勇气/战技槽系统：字段、SetSpiritLevel、条件真值、plan B、gauge-rules/art.txt 语法、刚1、退役系统。动充能/等级/刚1/战技条前必读 |
| `docs/motion-data.md` | LMT/TIML 编辑铁律、防御/窗口配方、PlMotionInput、LinkMotion、col 伤害/卡肉、blend 管线坑、收刀挂点。动数据文件前必读 |
| `docs/presentation.md` | FX id 表、拖尾三条件、喊声 EG02、屏震配方、动作音效。做观感/音效 |
| `docs/fsm-input.md` | FSM 路由模型、条件属性注册、组合键/方向派生、受击反应、FSM-Studio 操作。改 FSM 前 |
| `docs/tools-workflow.md` | 构建/部署/逆向/调试 MCP/导出流程/工具坑。搭环境、跑工具 |
| `docs/SESSIONS.md` | 2026-09~10 全部过程档案（追捕/事故/弯路/md5 链/备份名）。考古、查"为什么" |
| `Reverse/ADDRESS_RECOVERY.md` | 游戏更新后全地址恢复手册（锚点/步骤/验证） |
| `timl-editor/README.md` + `TIML_PARAM_REGISTRY.md` | FreeKinetics 编辑器用法 + TIML 参数注册表 |
| `SPIRIT_SHOT_Research.md` | 强化射击族专档（动作 id→LMT 非线性映射、类门控） |
| `COL_AttackBox_Research.md` / `LMT_TIML_..._Research.md` | col 早期研究 / 纳刀研究汇总 |
| `mhgu/README.md`（valor-remap-pipeline） | MHGU→MHW 动画迁移管线手册与已知问题 |
| `TODO.md` | 当前待办（user 维护） |

## 文档纪律（2026-10-25 起）

- **结论** → 对应 `docs/` 主题手册（改既有条目，不另开新档）。
- **过程**（追捕轮次、事故复盘、A/B 记录、md5/备份链）→ `docs/SESSIONS.md` 末尾追加，
  格式沿用 `### <日期> <标题>`。
- **AGENTS.md** 只更新：当前状态节、铁律（新铁律才加）、文档地图。保持 ≤300 行。
- 每次提交信息照旧高密度单行（改动+定案+md5 链+待验证）——它是 SESSIONS.md 的天然摘要。

## Hunter Arts 设计（远期参考）

- 充能槽（造成/承受伤害充能）→ 按键发动 → 每武器专属技 + 泛用技（绝回/治愈等）。
- LS 候选：樱花斩（多段突进充练气）/ 气刃强化 / Critical Juncture。
- 已知挑战：MHXX 动作需重定向+mot 转换（管线已通）；自定义 UI（ImGui 已有）；
  新动作注册（LinkMotion/槽位复用两条路已验证）；联机同步未验证（MOT 同步假说乐观）。
