# LMT / TIML / col 数据面 + blend 管线 + 收刀挂点

> 过程见 `SESSIONS.md`；FreeKinetics 编辑器用法见 `timl-editor/README.md`；
> 动作 id↔LMT entry 映射以 `action-list.md` 为权威；TIML 参数注册表见
> `timl-editor/TIML_PARAM_REGISTRY.md`。

## 基本纪律

- **FSM action id ≠ LMT entry**（映射查 action-list.md；见切 86→140、突刺 79→105、
  気刃突刺 87→170、特殊纳刀 99→306…）。三个编号空间并存：请求 id / FSM 节点 ActionNo /
  set-3 类表槽位（武器枚举号）。
- 工作文件 `nativePC/hm/wp/wp03/mot/w03_00/w03_00.lmt`（464 槽）；vanilla 基准
  `w03_00-origin.lmt`（`.bak` 是旧会话的不是 vanilla）。从游戏拷出的文件带只读位，
  先 `attrib -R`。工具 `patch_timl_defense.py dump|move|setval`（auto .bak）。
- **改 LMT 必须完全冷启动**才生效（启动时读；任务重进不清缓存）。
- TIML 帧 60fps（12f=0.2s）；motion 时钟 = anim+0x10C（TIML 帧数）。
- **导出/部署铁律**：整文件 blend 导出部署前必跑全 entry TIML 语义 diff
  （`timl-editor/_shout_semantic_diff.py`；433/435/437 双侧解析失败=已知盲区无害）。
  diff 不干净不许覆盖。部署/工作区/导出产物三处一致才算落定。

## TIML 防御/窗口配方（全部 user 局内验证）

- **无敌帧**：PlMotionCommon mFlag b26（0x04000000）窗 + 窗内一枚 mTrigger 2048 门键
  （位置无关）。**b26 单独=无声无敌帧**。
- **霸体**：b24（0x01000000）窗，无门键。
- **防御帧（削韧格挡）**：mFlag b28（0x10000000 格挡有效核）+b31，配方单键 f0=0x90080000
  或 0x90080006，无 mTrigger。**窗口化**：off-key 拆窗（f0=0x90080000→f12=0），时长随意。
- **強格挡（零削韧+掉耐力）**：**mFlag b30 + mFlag1 b28 两处都在**（如 0xD0080006）→
  引擎发強G反应 38/39/40；任缺 → 通常格挡 35/36/37（削韧）。staminaParam 非判定必需
  （掉耐力跟随強G状态本身）。S/M/L 档引擎按伤害定。反应族：34=弹かれ / 35-37=G S/M/L /
  38-40=強G S/M/L（零入边引擎请求，FSM 补节点+出边接反击 = CB/长枪式"強G→反击"）。
  长枪位词汇表：b28=核/b31=族公共/b30=強G/b29=蓄力反击族（长枪类代码专属，LS 抄位只得格挡）。
- **零伤吸收格挡（南风焓 306 配方）**：mFlag1=0x50000000(b30+b28) + mStaminaParamIndex=256
  + GP 窗（mFlag 开头 b31+b26）→ 受击 38 族零伤。我们強G 档实测用 staminaParam=10。
- **格挡反应由伤害结果决定，与动作 id/时机/方向无关**；普通受击走身体控制器 1:339 族
  （武器 FSM 看不到）。受击请求 {3,38} 由伤害系统直发（零入边请求进入）。
- 原版见切 140：mFlag f0=0x80400000 = b31+b22（**不是 b26**，09-13 笔记 nibble 读错已更正）；
  原版见切无敌在 FSM/动作层，不在 TIML。
- 306（特殊纳刀）入场保护 = PlMotionInput 分段档 f0 全关（防进架势瞬间出招）。

## PlMotionInput（输入属性使能——每动作生效！）

- **静态类型布局（2026-10-31 逆向）**：`FUN_141921e80` 用类型 hash `0x6FCCD10E`
  注册 `nTimelineParam::PlMotionInput`，实例大小 `0x60`；构造器 `FUN_14231eb50`
  清零字段。反射字段注册 `FUN_14231f190` 把 8 个 32-bit `mFlag` 家族字段依次映射
  到实例 `+0x08..+0x24`（`mFlag` 到 `mFlag7`），8 个 `mTrigger` 家族字段映射到
  `+0x28..+0x44`（`mTrigger` 到 `mTrigger7`）。这证明 TIML 参数以独立字段读取，
  不能从同一位号在不同字段的出现推断同一输入语义；字段位到纳刀请求的运行时映射尚未钉死。
- **运行时对象绑定（2026-10-31 静态）**：玩家 `+0xe480` 是两个 `0x60` PI 实例的
  数组头；`FUN_141fbc8d0(player,0,array[0])` 和 `(player,1,array[1])` 把实例
  地址注册到玩家 `+0x6b8 + index*0x40` 的通用对象列表。`FUN_14231ed80` 去重、
  扩容并设置列表变更标记 `+0x24`；绑定函数再把列表地址写给最多四个下游对象的
  `+0x168/+0x190`。同一绑定函数也用于其他参数对象，不能把它当作 PI 逐帧求值器；
  这条链确认 PI 到达输入侧对象，逐位消费函数仍未定位。
- **输入属性（含否定态）全部随动作的 PlMotionInput 使能位写入**：R / R押してない /
  △押してない / ○押してない / アイテムボタンを押してない / レバー前キープ 族 /
  R+方向组。原版 wp03 FSM 0 个按键否定引用 ≠ 引擎没有——加引用+动作带使能位即激活。
- **R+方向组**：PlMotionInput mFlag1 位簇 {b2,b3,b6,b7,b18-b25,b30,b31}（NFH 撒网值
  0xCFFC10CC/0xCFFC30CC；仅 f8/b17:16=方向组的旧字段序号判读有误，最小位集未 bisect，
  部署维持全簇）。配方：**该招 entry 的 mFlag1 非零键 OR 使能值 + FSM 接 R+方向边**
  （FreeKinetics 窗口编辑器已实现，操作要点见 fsm-input.md）。
  ⚠ 使能位只对**当前播放动作**生效——边挂在哪招，位就要打在哪招。
- **107/110 纳刀与 PI（2026-10-31 局内 A/B）**：把两者的 TIMLActionNode 指向
  106 的同一个 PI Action 后，纳刀恢复；全 entry TIML 语义 diff 仅两者的 PI 四轨变化。
  导出 LMT 里三者的 PI 记录仍各自独立，因此有效变量是导出后的 PI 内容，
  不是运行时共享指针。此前 OLD-exact 块回填失败只能否定“各自旧 PI 足以修复”，
  **不能**推出“PI 与纳刀无关”；具体起效位待 bisect。此后 107/110 已复制为独立
  PI Action，106 未动。107 约早两帧，user 决定保留。110 单移 mFlag2 无效；以成功版
  PI 为底，将四轨前中段键整体后移 14 帧（首窗 f38、主衔接窗 f58 附近），user 局内
  确认全部派生及勇気納刀时机正确。部署 LMT md5 `ad6c379b`；主 blend 已同步保存。
- **当前故障修法**：遇到同类 PI 导致纳刀/派生失效，先用已知基本正常的 106 PI
  内容复制到目标动作的独立 Action，再参照目标动作原版 PI 窗口调整时机，并逐项局内
  验证。106 在本例恰好可用，不能假定它对所有动作都正确；若复现新故障，再沿对象
  绑定链继续追 PI 逐位消费及请求缓冲。
- 09-22 "LS 分类器不写 R+方向" 结论已翻案（当年失败根源=原版动作没带使能位）。
  combo→请求分类器仍存在（原版引用集），但动作使能位是第二把锁。
- 306 按住保持 = mFlag/mFlag1/mFlag2 分段：f0 全关（入场保护）→ f50 partial → f60 全开；
  松开 → `R押してない` 边 → 100 → 312。

## FSM→LMT 通道与"新招"两条路

- **LinkMotion_W03**（MOT 节点，process 参数 {MotionNo, MotionNo_Phase1}）：进节点直接播
  LMT[entry]，不经过动作层。原版组合衔接真身 = ACT --按键条件--> MOT --无条件--> 下一ACT。
  MOT 语义：往当前动作注入骨架动画（动作层停在进入前动作，非 bug）；播满全长才交还骨架；
  **带完整 TIML（攻击+防御判定都工作）**。四代价：无动作身份（派生/气刃表/规则系统看不到）、
  反应按底层动作算、底下动作 motion 播完后从 0 重启、联机未实测。
- 新招两路：**改既有槽位**（nfh 式，有身份，牺牲原版招）= 主力；**新 MOT 节点+新 entry**
  （300+ 空槽，加法式）= MHGU 迁移招用。
- 南风焓数据侧全部机制 = 复用（改名/补 ACT 节点/原地替换 entry 内容/文件尾追加），无私有属性。
- 受击反应接管配方 = 给 38/39/40 补节点 + cond id18(OR 状态组近恒真) → 反击招。
- **长按检测**：`R押してない` 边（.rdata 0x1430B9BE0 注册属性）= 原生长按，纯数据。

## 类表与动作类（第二重门）

- set-3 类表 0x142EF5C30（113 槽，stride 0x20）；气刃/居合招 = cKijin*/cIaigiri* 30 类；
  104-111 = cKijin{1,2,3}ComboShot*（强化射击融合族）；110/111/112 = cWpOn/cWalkWpOn/
  cWalkWpOff（拔刀/持刀走/收刀走）。类名 = vtable+0x20 ASCII。
- **红光/拖尾 = Kijin 类（FX 处理器）∧ TIML 视觉数据，缺一不可**（交换实验实锤）。
  类跟请求 id 走，不跟 FSM 路径；FSM 编辑改不了类。
- **槽位锁死**：entry 345/344（及 110）拒绝一切非原生导入内容（播放即崩/加载期连存档崩），
  原生导入重编码随意换；346/104-109 宽容。机制未明（class/animLen/TIML/col 全排除）。
  **凡动内容一律走树导出**（二进制手术本身引入第二层崩溃）。

## col（伤害/判定/卡肉/声纹）

- 路由链：TIML CollisionTimelineObject ノードID 键 → col names[unk4==值] → {cgp=CLGM bank
  判定几何, move=ATK 行}。**TIML 只开窗+选行；数值在行里**。改引用=伤害+几何一起换；
  空闲 names 节点（name 必须保持空！写了=条目变长=结构手术）：85/90/94/98/99/100
  （91/92 已用=343 无双斩 hit1/hit2 专用，2026-10-28 全 entry 扫描实锤；10-19 档案的
  "90/94 重指"未落部署）、孤儿 ATK 行 22/31/42/45/50/54/58（42/45 与 51/52 同值但未引用；
  51/52=343 无双斩现行行）。攻击窗 EC00 mFlag：2=伤害槽 1=挥空槽。
- ATK 行：Motion_Value=动作值乘区（MV/100×攻×斩味×会心×部位吸收×补正）；Cond_MV=条件
  替换值（気刃斬り1 实际最高 26 不是 14）；Part_Modifier=部位积累乘数（0=不打部位槽，
  user-verified）；HitLag_a/b=卡肉秒（成对设）；**fUnk16/fUnk19=卡肉冻结倍率（0=全冻）**；
  **bUnk23=命中反应/音效表索引（255=无效→弱音）**——克隆声纹照抄指纹（気刃斬り1=
  bUnk23:12/fUnk13:2/fUnk14:6/fUnk26:260）。工具 `col_research/col_tool.py`
  （⚠ 2026-10-20 两个写毁 bug 已修：u16/u8 切片赋值删短文件、pack_val 急切字典；
  改前 diff 基准 `wp03.col.bak-20261007-pre343mv`）。**只做等长数值编辑+改后结构校验**。
- **目录名带空格 = 禁用**（`collisi on/`、`sh ell/` 教训）：nativePC col 曾因此 10 天没生效。
- LS wp03.col 档位参考：LS 原版卡肉上限 0.40（空中气刃突刺）；**弹反 156 走 node89→ATK49
  （HitLag a/b=0.40 全冻，与 311 大居合赤共行，user 定案该行可随意用）**——改此行同时影响
  156+311；刚一 345 走 node63→ATK25（見切り攻撃行，独占，MV12/Cond26/破坏1.0，未动）；
  一文字 339/340/342 共走 node54→ATK17（気刃斬り2 行，与原版 106 共行，MV30/破坏1.0，
  user 定案不变）；343 无双斩=行51+52（独占，2026-10-28 调：MV40+80、破坏 0/1.5，一刀目
  残留 Cond_MV=26 已清零归 vanilla）；156 的 0.4s=24 帧卡肉吃衔接窗，补偿配方=PI 连段窗+mFlag1 簇结束键 f41/f55
  值改为窗内值（开到动作末尾，2026-10-27 已落，见 SESSIONS）；
  GS 真蓄 0.85 全游戏顶配。
- 多段判定 = CTO ノードID 轨多键（第一段键通常在 f0）；多段不同伤先例 = LMT155。

## blend / FreeKinetics 管线（坑清单）

- 结构：nodetree `w03_00`（FreeHKNodeTree）→ LMTEntryNode.entryNum → TIML Data Node →
  **TIMLActionNode 链式串联**（只有链头直连 DataNode——遍历必须从 DataNode 沿 input
  links BFS，只扫直连会漏链）。全部 TIMLDataNode 同名，`bl_idname` 过滤+连线遍历。
  动作名 `.NNN` 后缀=Blender 去重号无语义；真实 entry 的参数块以 nodetree 反查为准
  （如 345 的 PlMotionCommon=`.348`）。entry↔TIML 映射数据：LMTEntryNode→"TIML Data"输入。
- fcurve：data_path `FreeHKTiml.m<hash>`；bigFlags 双通道 array_index 0/1=低/高 16 位，
  二进制 u32=(ix1<<16)|ix0；ノードID=mB296CA66、mFlag 位包/EC00 ReqNo=m08FD20A6、
  mTrigger=0x99897C43、PlMotionVisual=m46F3E901、PlMotionCommon=0x48D6114F、
  PlMotionInput=0x6FCCD10E、mStaminaParamIndex=0x85A14934。
- **插值**：导出器只认 CONSTANT/LINEAR/QUAD/CUBIC/QUART/EXPO/SINE——**新建键默认 BEZIER
  会在导出时被静默丢弃或崩**。bigflag 轨用 SINE（vanilla 惯例），数值曲线 LINEAR，
  位窗用 CONSTANT。含负值通道(SINT) 要求逐键 `back=0; period=0`。
- **删/改键**：`keyframe_points.remove()` 高频静默失败+兄弟引用失效——**整条 fcurve
  remove+new 重建最稳，重建后必须重查残键**；删键用容差匹配（`abs(co.x-38)<0.5`）；
  `keyframe_points.add()` 不返回键（取 [-1]）。
- **新建 TIML action 必补 freehk 元数据**（`starType="TIML_Action"` + timelineParam(Raw)=类
  id + `timl_reorder="BLENDER"` + frameCount=-1/unkn0=0），缺=A_TIML_INVALID_ACTION_TYPE
  **静默剔除**。IDProperty 双层坑：必须按 importer 原始写法纯属性赋值
  （`a.freehk.starType=...`），`a['freehk']['x']=...` 会写到另一层。
- 导出：MCP/无头 = `sys.modules['MHW-FreeKinetics.operators.export_ops'].exportNodeToFile`
  + `scn.freehk_tether = m_mesh Armature`（**tetherFrame 缺失 = Root 轨丢失+米值**=缩成一团
  根因）；export 前 globalCacheClear 内建；**ops 改内存 blend，导出必须同会话或先保存**；
  GUI 开着旧会话必须 File>Revert（坑⑭）；导出用新进程防 structureCache。
- **动画改长（裁帧/加帧）必须手动同步两处元数据**：动画 action 的 `freehk.frameCount`
  （≠-1 时 calculateFrameCount **直接用元数据**，按键算帧数只是 -1 兜底+校验）+
  TIML Data 节点 `animLength`（=motion 时钟 0x10C）。只删键不动元数据 → 导出带定格尾巴
  （2026-10-27 156 实例：71/70→66/65）。
- **TIML 整链平移配方**（动画前段裁 N 帧）：entry 全部 TIMLActionNode 的全部 fcurve 键
  -N；落负的键钳到 f0、值取裁切点现值（f0 基线键被吞；声音/特效触发=提前到 f0，
  窗口=前段被裁）；整条 fcurve remove+new 重建（复制 LIMITS modifier 的 min_x/min_y +
  键 interpolation/back/period）；f0 常数轨不动。导出后 timl-diff 必须只剩目标 entry。
- **blend/部署漂移**：二进制侧调过的值必须回填 blend，否则下次导出静默回退 user 调参。
  旧谱系 blend 开新工作文件前先查关键参数块在不在（分叉事故洗掉过拖尾窗）。
- ARP/迁移（mhgu 管线）：LMT 单位厘米；tetherFrame 绑定必设；6_tag 全量打标+min-2-keys
  （末帧单键向前补，向后 +1 顶爆 frameCount）；0 键空曲线删除（exporter bounds() 崩）；
  排序后 4-bit 帧增量切分；根位移分段锚定（offset 双加坑）；拼接一律**纯复制**（焊接只适用
  循环类衔接）；LIMITS 钳制下 fc.evaluate() 是钳过的值（对齐计算读原始 co.y）；
  030/047 状态对=编舞（不是携带态开关）；251/253 可能整段空（缺轨走游戏默认）。
- 030/047（挂点表选择）：scale-Y (1/2/3)=挂点表选择，只管视觉滑入；**携带态（拔刀/纳刀）
  由当前动作 id 每帧决策+边沿触发**（收刀家族 {5,6,9,10,69,87}；+0xA50/+0x23C0 强制）。
  收刀系动作槽位复用定案：自定义纳刀动作用 5/6/9/10 槽位（状态/特效/挂点自动继承）。
- 312 尾段走动：id100 类是攻击类不实现移动取消，**任何 TIML 窗都给不了** → DLL force
  （motion≥96+按方向 → {1,0}/{1,4}/{1,8}）或 {3,112} cWalkWpOff。原生 c71 脉冲握手只存在
  于原版链（自定义 130f 版无脉冲不进纳刀状态）。

## LMT 二进制（低层）

- 头 "LMT\0"+ver95+校验；逐 entry 独立重定位（offset/track bufferOffset+0x10/lerp+40/
  timl+0x58 全 +=文件基址），头部无需连续。Flags2 bit0=跳过轨道重定位、bit7=跳过 TIML。
- TIML 变长记录：头 40B + types 行 24B + transform 行 24B + 键 20B；键 20B =
  {value:i32, controlL, controlR, frameTiming:f32, transition:i16, dataType:i16}；
  tr.offset = 键数组直接起点（原位补丁用）；transition 6=CONSTANT。引擎按 datatypeHash
  哈希映射 transform（顺序无关——语义 diff 用排序归一，字节 md5 只适用未重排 entry）。
- mbd（w03_00.mbd）= IK/链图，截断覆盖零行为变化（非槽位约束源）。
