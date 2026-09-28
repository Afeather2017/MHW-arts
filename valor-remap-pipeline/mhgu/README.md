# MHGU → MHW 动画迁移链(mhgu-valor)

把 MHGU 勇气太刀(w07_sa)的动作经 ARP 重定向合并进 MHW 太刀槽位的完整链路。
2026-09-27 全链调通:导出二进制与局内成功的 luocha 参照**布局零差异**(82 轨)。
本文件同时是**坑目录**——每个坑都是实际踩过、消耗了排查时间的,踩一次就够。

## 工作文件与脚本

- 工作文件:`E:\MHW-arts\mhgu-valor-mhw.blend`(= MHW-Valor.blend 副本 + MHGU 骨架/动作;
  MASTER 永不改动,每步自动写 `.bak-时间戳`)
- MHGU 源:`research/mhgu/extracted/w07/player/mot/w07/w07_sa.lmt`(valor 组,8 个非空动作)
  与 `w07.lmt`(公共组);骨架 `Animation Group`(GLB 节点命名 `0:0`/`1:17`/`reference`)
- 执行顺序(编号即顺序;`mhgu/` 内为本链专属,根目录三个为两链共用):

```
0_assemble_mhgu_blend.py      组装:复制 MASTER→工作文件,清 plw_*,追加 MHGU 骨架+源动作(保留原名)
1_import_mhgu_w07sa.py        LMT 导入:LMT::w07_sa::NNN;内置自检(对旧导入 w07::106 逐键对比,必须 PASS)
2_arp_remap_mhgu_to_mhw.py    纯 ARP 重定向 + Root 轨道抽离到 TRAVEL::(见坑④⑤)
3_fill_missing_from_lmt106.py 补骨骼:缺的通道从原生 106 逐骨拷贝(手指/腰/脚趾/挂点),按键位裁剪
3b_cog_location.py            MhBone_000 位置 = 0:0 loc 逐字拷贝(高度+重心移动,轴恒等)
4_object_root_motion_mhgu.py  reference 地面位移 → object 层(首键归零;LMT Root 轨道来源)
4b_glb_reference_travel.py    GLB 版位移(reference 骨骼 locZ 取负)→ object 层
3_rebake_ik.py   [共用]       IK 重烘焙(247-253)
4_grip_left_from_lmt106.py [共用] 左手绑定(030=106 原样, scaleY=2)
7_merge_into_native.py        按 (原生entry;;remap) 对,把 remap 原位合并进原生 entry 动作
6_tag_export_metadata.py      给特殊骨骼 fcurve 补 LIMITS 元数据 + 剥 _s 通道(导出前最后一步)
9_group_channels.py           批量 fcurve 归组(组名=骨骼名,object 级归 Object;7_merge 重建
                              会丢组,批量合并后跑一遍;7_merge 重建时现已自带归组)
```

- 环境变量:`MHGU_WORK_BLEND` 工作文件;`MERGE_PAIRS="native;;remap|..."` 批量合并配对;
  `MHGU_ONLY=<动作名>` 外科式只重定向一个;`REBAKE_SKIP_VERIFY=1` 跳过慢验证(外部加密扫描替代)。
- 命名约定(用户指定):**MHGU 源动作保留原名**(`LMT::w07_sa::151`、`Motion[151]_Animation Group`、
  `LMT::w07::106`),remap = 原名 + `_remap`(ARP 标准)。链内识别 MHGU 动作靠 `mhgu_source`
  自定义属性,不靠前缀。

## 数据事实(迁移的地基,全部实测)

| 主题 | 结论 |
|---|---|
| 单位 | MHGU LMT 位置 = 厘米,导入 ÷100;GLB 烘焙版已是米 |
| 重心骨 | GLB 节点 `0:0`(rest 在地面原点)= MHW `MhBone_000` 同源通道:0:0 locY 0.4713 ≡ 原生 106 的 000 locY 0.4714(逐键一致) |
| 地面根 | `reference` 在地面、无动画;**前后左右位移在 -1/0xFF Root 轨道**(LMT)/ `reference.loc`(GLB 烘焙) |
| 轴映射 | Root 轨道 chan x→obj[0],chan y→obj[2],**chan z→obj[1] 取负**(glTF 交换);GLB reference 同式(单位已米) |
| 000 语义 | locY=高度、locX/Z=重心摆动(MHGU 有、MHW 原生不写但游戏按骨骼变换生效);旋转永远不写进 000(原生从不转 000,写了全身倾斜) |
| GLB vs LMT | GLB(Motion[])把位移烘进重心骨、24fps 有损、无 Root 轨道;LMT 60fps 精确键、通道分离。**导出用 LMT 版**;GLB 仅肉眼对照 |
| 交叉验证 | 同一动作 GLB(reference 路线)与 LMT(Root 路线)位移量完全一致(102:−1.915 / 002:−3.207 / 003:−5.917 / 151:+0.884 / 056:−0.606 / 051:−0.046 / 101:0)——位移方向符号自此定案 |
| w07::106 ≠ w03_00::106 | 同槽位只有重心高度通道同源,肢体编排是 MHW 重制的(源姿态 vs 原生差达 1.1m)。**校验正解 = remap vs 源姿态的旋转差(应为 0.00°)**,不是对比原生 |
| 骨骼对应 | `mhgu-to-mhw.bmap` 原样可用(0:0→腰013 旋转,reference→000 root);不要把 0:0 的旋转改路由到 000 |

## 坑目录(症状 → 根因 → 修复)

### 坑① 缩成一团 = 导出轨迹缺失,Blender 里看不出来
FreeKinetics 写 LMT 按**原生 entry 的轨迹布局**逐轨输出;把 remap 动作直接接进 entry,
解析不了的 transform 被 **omitted** → entry 只剩带 LIMITS 元数据的 IK+根 15 条轨迹,
身体旋转全缺 → 游戏骨架瘫成一团。Blender 播放走 data_path,**完全看不出缺轨迹**。
→ 修复 = `7_merge_into_native.py`:在原生 entry 动作上原位合并(保留 82 轨布局/元数据/
空轨迹),只替换身体旋转、000 位置、位移、IK、挂点。原生 103 的 030 全轨为空——
合并不进去,保持原生空(挂点状态继承上一个动作,与原生行为一致)。

### 坑② 四元数轨迹 4-bit 帧增量溢出(>255 帧的动作必踩)
bufferType 2/6/7/9/11/14/15 的关键帧 `frame` 字段只有 **4 bit(部分 8 bit)**,
即键间隔最大 15(或 255)。351 帧动作里任何 >15 帧的间隙都会让序列化
`struct.error: ubyte format requires 0 <= number <= 255` 或游戏端解码乱码。
→ 归一化:所有曲线按 ≤15 帧切分等值键(`7_merge` 内置)。
注意坑③:切分必须在 hold/basis 键之后、且键数组必须已排序——三个 bug 曾叠加。

### 坑③ 键数组未排序,切分静默跳过
单键常量曲线经"hold 到末帧 + basis 到 0 帧"后数组 = **[351, 0] 未排序**;
切分循环算出 gap = −351,`> 15` 不成立,**静默跳过**,最后才 sort。
症状:其它曲线都切了,唯独常量曲线(030/047 等)仍 2 键。
→ 顺序固定:hold → basis → `kp.sort()` → split。

### 坑④ ARP 烘焙时源物体不能动
MHGU Root 轨道导入后是 **object 层曲线**;若留在源动作里,ARP 烘焙时源物体自己
移动,地面位移被写进重心位置(实测 0.816 = 重心 0.043 + 物体 0.773)。
→ ARP 前 `2_arp_remap` 把 object 曲线抽到 `TRAVEL::<名>` 动作(step4 从那里取)。

### 坑⑤ 符号:不要额外镜像
Root 轨道导入时已做 glTF 交换(objY = −rawZ,与旧成功导入逐键核对过);
MHW blend 同为 +Y=后退。object 位移**直通即可**;多镜像一次 = 后退变前冲。

### 坑⑥ 场景 frame_end 污染 ARP 批量烘焙
MASTER 遗留 scene.frame_end=122 → ARP 把**所有** remap 统一烘到 121 帧,
短动作尾部是外插垃圾;且 ARP 新建动作 `freehk.frameCount=-1`,导出帧数错。
→ 烘焙前钉 `scn.frame_end = 最长源动作+1`;烘完裁剪到各源长度 + 逐属性拷贝
freehk(loopFrame 在 4.3 的 freehk 包装器上不存在,缺省跳过)。

### 坑⑦ fill 拷贝把 106 的全长键带进来
原生 106 部分曲线是 0..121 全长多键;逐字拷进 35 帧 MHGU 动作会拉长时间线,
下游 IK 验证采样到外插区假报 0.1~0.5m。
→ fill 拷贝按键位裁剪到目标动作范围(空则取 evaluate(0) 常量)。

### 坑⑧ 根管线脚本按 plw_ 前缀过滤 → 整轮空跑的"假 0.0000"
IK/绑定/验证对空列表全部"通过"。**验证脚本必须先断言处理数量**;IK 验证采样
需钳制在烘焙键范围内(超出是外插,会出现 0.1~0.5m 假差)。
→ 过滤条件改为 `endswith("_remap") and (mhgu_source in 源动作 or plw_ 前缀)`。

### 坑⑨ v67 解析器间歇性丢 entry
FreeKinetics 的 MHGU 解码**偶发**(进程级)把某 entry(观察到 056)解析成空——
同代码同文件,时有时无。
→ 导入循环多次重解析取并集,补齐缺失 id;仍缺则 FAIL。

### 坑⑩ lmt_importer 细节
- `channelNames()` 返回 (真路径, `delta_*` 干扰路径, index, var) —— **用第一个元素**;
- `basis`/`tail` 是 float[4] 列表(用索引取),keys 是 (值, 帧增量) 累积;
- 骨骼名 `N:M_s` 的 `_s` 后缀要剥(优先绑非 _s 主骨);
- Root 轨道(usage≥3 / boneId −1)按 obj[0]=x、obj[2]=y、**obj[1]=−z**、值 ÷100。

### 坑⑩.5 单键常量曲线 = 导出器 bounds() 崩溃
导出器 `bounds()` 计算 keyframe 包络时**丢弃第一个键**(`vector_keyframes[1:]`),
只有 1 个键的常量变换(030/047/手指等从 106 拷的恒定曲线)会让
`zip(maxima, minima)` 为空 → `Vector([])` → 导出崩溃。
(空轨迹 0 键反而合法——原生就用 0 键空轨表示常量。)
→ `7_merge_into_native.py` 内置 `ensure_min_two_keys`:合并前后把所有单键
曲线升级为 2 键(等值)。**同时:不要把裸 remap 直接接进 entry 导出**——
那既有本坑又必然缺轨迹(坑①);永远接合并后的原生动作 `LMT::w03_00::NNN`。
(2026-09-28 修正:裸 remap 直连的真实死因 = `freehk.tetherFrame` 未绑定,
见坑⑬;补绑定后 remap 直连也可用,252-rot 差 1 轨无影响。)

### 坑⑫ 自烘焙的 IK 通道在游戏里缩成一团(Blender 精确也没用)
tetherOps 烘焙的 IK(247-253)在 Blender 里与目标骨骼距离 0.0000,但导出进游戏
后玩家缩成一团。根因:烘焙值是 Blender 骨架空间约定,游戏的 IK 解算空间约定
不同(具体变换未钉,待研究)。**验证过的安全路线 = 保留原生 entry 自己的 IK
曲线**(`preserve-249-253` 配方,7_merge 现默认此行为,PRESERVE_NATIVE_IK=0 可关)。
A/B 证据:游戏目录 `w03_00-wrongik.lmt`(自烘焙 IK,缩成一团)vs
`w03_00_luocha.lmt` / 2026-09-27 导出(原生 IK,正常)。
副作用:动作播放期间 IK 目标是原生槽位动作的落点,脚部贴合以原生动作为准。
**✅ 最终定案(2026-09-27 局内验证)**:自烘焙 IK + 全量打标 + 导出器自动厘米转换
= **entry 103(樱花)与 104(刚1)局内正常,脚手跟随迁移动作本身**。
7_merge 的 PRESERVE_NATIVE_IK 默认已改为 0(用自烘焙 IK);原生 IK 保留路线
作为后备(PRESERVE_NATIVE_IK=1)。

**单位实测(2026-09-27 二进制解码,已闭环)**:原生文件 IK 位置 span 恰为
Blender 值的 **100.0 倍(三轴)** = LMT 的 247-253 位置单位是**厘米**,Blender 是米。
**为什么一开始导出的是米**:导出器的 m→cm 转换**只对带合法 LIMITS 元数据
(bufferType)的 fcurve 生效**。当时的导出:① 身体骨骼无元数据 → 整轨省略;
② IK 只有半截元数据(min_x=0 无 bufferType)→ 走原样路径,米值直接进文件。
游戏按厘米读 → IK 目标落在原点几毫米处 → 全身压向该点 = 缩成一团(双重原因:
缺身体轨迹 + IK 单位错 100 倍)。Blender 播放走 data_path 不需要元数据,所以
Blender 里完全看不出来。
**结论:全量打标(6_tag)之后,导出器自动做米→厘米,无需任何手动缩放**
(曾手动 ×100 反而叠加成 ×10000——IK 目标跑到 38 米外,同样拉毁骨架)。
自烘焙 IK + 全量打标的直接导出 = 应该完全正常(待游戏内复测)。

### 坑⑬ 把 remap 直接连进 entry 再导出 = wrongik 签名复刻(2026-09-28 局内复发,机制已闭环)
症状:缩成一团复发。部署文件 entry104 = **79 轨 + 缺 (-1,3)/(-1,4) 根位移轨 +
IK/重心米值**(249loc[0]=(-0.38,0.10,0.32))——与 wrongik 逐值同源,但不是同一文件。
根因(受控实验复现,逐值一致):**remap 动作缺 `freehk.tetherFrame` 绑定**。
导出器 LMTActionParser 的 tether 取自 `action.freehk.tetherFrame`(不是场景设置):
- ARP 新建动作不会设它 → remap = None;原生导入的动作(103/104)= m_mesh Armature;
- tether=None 时 ① `restoreRootFromObject` 不跑 → Root 两条轨没了;
  ② 骨架逆变换/骨骼功能表全套跳过 → fcurve 米值原样 float 直写文件。
曲线级 LIMITS 打标(6_tag)救不了它——那是另一层。**修复 = 补绑定一行**:
`act.freehk.tetherFrame = bpy.data.objects["m_mesh Armature"]`(6_tag 已内置,
verify 段会断言)。补绑后 remap 直连导出 = 81 轨 + root ✓ + cm ✓,与管线产物逐值一致
(实测)。和"接哪个动作""名字"无关——只认 tetherFrame。
**二进制签名速查**(parseLMT 一眼判定):
| 文件 | entry104 轨数 | root(-1,3)/(-1,4) | 249loc[0] |
|---|---|---|---|
| 好(管线/绑定后 remap 直连) | 81 | ✓ | ≈(-35.9, 10.4, 32.1) cm |
| 坏(未绑定动作直连) | 79 | ✗ | ≈(-0.38, 0.10, 0.32) m |
注意:remap 比 104 少 MhBone_252 旋转 4 条曲线(合并流程补的)→ 81 vs 82 轨,
局内已验证无影响(9-27 的 103/104 同为 81 轨配方)。252-rot 若要补,跑 7_merge。
一键复跑+五文件对比自检:`_rerun_and_compare.py`(MERGE_PAIRS 已含 103+104)。
坏文件留证:`nativePC/.../w03_00-curled-20260928-2000.lmt`。

### 坑⑪ 自检护身符
对 w07.lmt::106 做"我导入 vs 旧成功导入"逐键自检(max diff 5.7e-4)是一次性
锁死导入正确性的投资——之后所有导入 bug(路径错、轴错、丢键)都被它当场拦截。
新管线第一次碰新格式时,先找一份**已知正确的参照**做逐键 diff。

## 动作 ↔ 招式对照(持续更新)

| MHGU 动作 | 招式 | 槽位 | 状态 |
|---|---|---|---|
| LMT::w07::192 | 刚1 | 104 | ✅ **局内验证通过**(自烘焙IK+cm) |
| LMT::w07_sa::151 | 樱花气刃斩 | 103 | ✅ **局内验证通过**(自烘焙IK+cm) |
| LMT::w07_sa::001/002/003/051/056/101/102 | 待用户指认(妖刀罗刹/圆月斩/镜花构/特殊纳刀在其间) | 待定 | 已转换可直接导出 |
| 绝对回避 | 未找到(不在 w07_sa 8 动作内;候选=w07.lmt 其余 entry 或共通 motion) | — | 待定位 |

## 验证清单(交付前全绿)

1. 导入自检:与旧导入逐键 diff < 2e-3,无缺通道;
2. 旋转保真:remap vs 源姿态逐骨世界旋转差 = 0.00°(采样全动作);
3. 重心锚:恒等动作 106 的 MhBone_000 locY[0] = 0.4713(原生 0.4714);
4. IK:全 remap 加密扫描(每 10 帧×5 对)worst = 0.0000;
5. 位移:GLB/LMT 双路线量级一致;object 首键 = 0;方向 +Y=后退;
6. 导出二进制:entry 轨数/布局 vs luocha 零差异;无 struct.error。

## 插件修改(2026-09-27)

`MHW-FreeKinetics/blender/lmt_operators.py` 的 **Recalculate IK Bones**:
- `limit` 属性从隐藏改为对话框可见:"Only the Dopesheet action"(默认勾选 = 只处理
  Dopesheet 当前打开的动作;取消勾选 = 全文件所有动作,慎用)。
- 描述明确:位置写入 Blender 米,导出器自动转厘米,无需手动换算。
- 单位结论(实测):LMT 的 IK/位置轨迹 span 恰为 Blender 值的 100 倍 = 文件单位厘米;
  导入 ÷100、导出 ×100,对带 LIMITS 元数据的轨迹自动生效。
- "All" 指全部 IK 骨骼类型勾选框,不是全部动画——不会碰别的动作。

## 刚1 握持状态动画(2026-09-27,已写入 MASTER)

**原版换手轨道实测(2026-09-28,给 104 补换手的参照配方)**:原版 109/113 的 030/047
scale(usage 2)用 **bufType=5**,瞬时台阶用**同帧成对键**(109 的 047: f69=1→f70=2→
f128=1;030: f69=2→f158=1)。给 104 写阶梯按这个打标/排键——**不能沿用 104 自己的
keyless bufType=1**。

**9-29 闭环:阶梯"写了却不生效"的真因 = 导出器把 keyed+bufType1 压回 keyless 常量**。
用户给 192_remap 写好 5 键阶梯(030: 0/11=2→12/45=1→46=2;047 镜像)后导出部署,
游戏仍全程左手。二进制对质:部署文件 entry104 的 030/047 scale = **0 键 keyless 轨,
basis=(1,2,1)/(1,1,1)——与原版 104 的常量逐字节相同**(basis 取了阶梯首键值)。
即键被 bufType=1 完全吃掉,游戏看到的就是"原版 104 的常量态"(原版 104 本身就是
常量左插座,对它自己的动作是对的)。**修复 = scale 六条 fcurve(030/047 × xyz)重打
LIMITS min_x=5**,重导出即带键;6_tag 配方已改为 scale 行从原版 109 读 5,不再复发。
游戏侧插座切换机制经原版 109/113 局内对照排除嫌疑;语义:030.Y=2→左插座持有,
047.Y=2→右插座持有(109 右手段两者同为 2,047 优先),1=闲置。

w07::192(刚1)有左手→右手→左手的换手段。已写入 MASTER 的 `LMT::w03_00::104`:
- `[0,12]` 双手 = 030.scl[1]=2 + 047.scl[1]=1(刀在左插座)
- `[13,44]` 右手 = 030.scl[1]=1 + 047.scl[1]=2(刀切到右插座)
- `[45,61]` 双手 = 回 (2,1)
常量插值阶梯键;scale X/Z 恒 1。换手机制:游戏把刀模型 MhBone_000 吸附到
激活插座;插座常量位置已各自对准正确握点(047=剑柄靠前,030=柄尾),角度无需动。
**预览 rig**:MASTER 内已加入太刀模型(swo022 骨架+网格,来自 longsword-example.blend),
武器 MhBone_000 挂 GRIP_L_030/GRIP_R_047 两个 Copy Transforms 约束,
influence 阶梯动画在 `FK_grip_preview_104` 动作(swo022 上)。Dopesheet 切到
`LMT::w03_00::104` 拉时间线即可预览换手;调换手帧时,**LMT 侧改 030/047 的
scl[1] 键 + 预览侧改 FK_grip_preview_104 的 influence 键,两处同帧**。

## 刚1 几何换手实现(2026-09-27,已导出待局内验证)

仿原生 104 的换手模式:030 的位置轨迹烘焙为——
`[0,8]` 106 左手握点(常量)→ `[8,13]` 收敛到 047 的位置(刀交接)→ `[13,44]`
跟随右手(逐帧 = 008⁻¹∘W047 的本地位置)→ `[45,49]` 回到 106 左手握点。
scale-Y 状态键([0,12]=(2,1) / [13,44]=(1,2) / [45,61]=(2,1))与几何换手同帧。
047 位置恒定(其 cm 值由导出器 m→cm 自动转换 ✓ 实测 −5.2/−3.5/−3.6 = −0.052m×100)。
换手帧要调:030 的 location 键 + 030/047 的 scl[1] 键 + 预览约束 influence 键,三处同帧。

## ✅ 最终验证(2026-09-27,局内)

- entry 103(樱花)+ 104(刚1)= **自烘焙 IK(米)+ 全量打标 + 导出器自动 m→cm = 局内正常**。
- 无头导出脚本:`mhgu/export_lmt.py`(GUI 导出的同一代码路径)。
- 插件修改:Recalculate IK Bones 的 limit 选项已可见(只处理 Dopesheet 当前动作,默认勾选)。
- 插件"导出错"的真相:导入/导出对无父级骨骼(IK 247-253)不对称——导入 cm→m,导出不转,
  凡"导入原生→再导出"的 IK 都会 100× 缩小;自己烘焙的 IK 只要**全量打标**后导出,单位就正确。

## 已知问题(暂缓)

- **游戏内刀位置与 Blender 预览不一致**:Blender 里刀吸附 030/047 的位置和游戏不同
  (游戏的挂点数学可能包含额外偏移/约束,或绑定状态 scale-Y 在游戏内另有插值)。
  动作本身(身体/重心/IK)已验证正确;刀位待后续用 MHW_Model_Editor 导入武器模型
  对照游戏截图校准 030/047 的常量偏移。
- entry 104 为 81 轨(标准 82):一条常量轨迹在 min-2 处理中合并,游戏实测无影响。
