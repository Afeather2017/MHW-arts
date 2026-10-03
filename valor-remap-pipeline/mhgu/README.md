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
10_apply_rules_divide.py      对用户手搓的 divide_*/*_before_divide 自定义动作(左手握持+根位移)
                              套全套规则:坑⑬ tether 绑定 + frameCount/starType + object 层整形
                              (删 arp_retarget_bound/scale,旋转恒等 2 键)+ 030/047=106 左手常量
                              + 000 删旋转缩放 + 6_tag v2 动态打标(030/047 scale 行=109 的 5)
                              + min-2-keys。2026-09-30 跑过 5 个动作,导出器级验证 valid=True
                              ×5/82 变换轨/2 Root 轨,警告签名与局内验证过的 151/192_remap 同族。
                              **复用**:`RULE_ACTIONS="动作名1,动作名2" blender -b MHW-Valor.blend
                              --python mhgu/10_apply_rules_divide.py`(缺省=divide 批次 5 个)。
_new_actions 工具链            新动作接入槽位的四步(全部无头、只写 LMT 不动 blend):
  _verify_exporter_divide.py  ① parser 预检(-- 传动作名),valid=True 才可接线;
  _repro_wiring.py            ② 无头接线+正式导出(ENTRY/ACTION/OUT 环境变量,=GUI 按钮代码路径);
  _check_repro_entry.py       ③ 产物解析(轨数/Root 双轨/挂点形态,-- 传 lmt+entryId);
  _preflight_tree.py          ④ 全树 154 动作预检(接线后跑,problems 必须=0)。
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

### 坑⑭ 无头改完 blend,GUI 旧会话直接接线导出 = 崩在 bounds() 空包络(2026-09-30)
症状:divide_short 接进 103 导出报 `ValueError: mathutils.Vector(): sequence size is 0,
expected >= 2`,traceback 在 LMTActionParser.__init__ → compile → bounds;但换两个
居合槽位又能导出,看似灵异。机理:脚本/headless 保存的 blend **不会进入已开着的 GUI
会话**,会话里还是改前的旧动作(无 tether/starType/旧键),接线上去的和磁盘上判若两物。
崩溃点精确条件(导出器代码实锤):bounds() 对非 Root 轨取 `vector_keyframes[1:]`、
对 Root 轨取 `[1:-1]`——**Root 轨恰好 2 键**(或非 Root 恰 1 键且 buffered)时包络为空。
正常管线产物出不了这种状态,出现即 = 动作数据不是管线产物。
→ 修复 = **File > Revert 重载后再接线导出**(用户实测重启即好)。
诊断工具:`_preflight_tree.py`(全树预检)+ `_verify_exporter_divide.py`(单动作
parser 预检)+ `_repro_wiring.py`(无头接线+正式导出复现)。
注意接线细节:entry 的 LMT Animation 输入上**只能留一条连线**,旧连线不删会被
`next(iter(actionNodes))` 取到旧动作。

### 坑⑪ 自检护身符
对 w07.lmt::106 做"我导入 vs 旧成功导入"逐键自检(max diff 5.7e-4)是一次性
锁死导入正确性的投资——之后所有导入 bug(路径错、轴错、丢键)都被它当场拦截。
新管线第一次碰新格式时,先找一份**已知正确的参照**做逐键 diff。

## 动作 ↔ 招式对照(持续更新)

| MHGU 动作 | 招式 | 槽位 | 状态 |
|---|---|---|---|
| LMT::w07::192 | 刚1 | 104 | ✅ **局内验证通过**(自烘焙IK+cm) |
| LMT::w07_sa::151 | 樱花气刃斩 | 103 | ✅ **局内验证通过**(自烘焙IK+cm) |
| divide_short | 居合斩(用户自定义) | 103 | ✅ **导出验证通过**(2026-09-30 无头接线导出,82轨+Root 双轨;局内待测) |
| divide_long / left_up / rigit_up / right_down_before_divide | 待用户指认 | 待定 | 已按规则处理,可直接接线导出(`RULE_ACTIONS` 可复用 10 号脚本) |
| LMT::w07_sa::001/002/003/051/056/101/102 | 待用户指认(妖刀罗刹/圆月斩/镜花构/特殊纳刀在其间) | 待定 | 已转换可直接导出 |
| LMT::w07::195 | 勇气纳刀·松开按键收刀 | 待定(收刀系槽位候选 5/6/9/10) | ✅ **已转换入 MASTER**(2026-10-01,parser 预检 valid=True/81轨/Root双轨,保真 1mm) |
| LMT::w07::196 | 勇气纳刀·保持按下(构,无位移) | 待定 | ✅ **已转换入 MASTER**(同上) |
| LMT::w07::095 | 勇气纳刀·被打断触发 | 待定 | ✅ **已转换入 MASTER**(同上) |
| 绝对回避 | 未找到(不在 w07_sa 8 动作内;候选=w07.lmt 其余 entry 或共通 motion) | — | 待定位 |

### 增量迁移配方(2026-10-01 验证,新 MHGU 动作照此复用)

源动作在 `E:\MHW-arts\mhgu.blend`(全部 0 引用,**该文件永不可保存**,只许作为
append 源);增量链已固化为脚本:

1. `mhgu/_append_sources.py` — 从 mhgu.blend append 源动作进工作文件
   `mhgu-valor-mhw.blend`,补 `mhgu_source="w07.lmt#<id>"` + fake_user;
2. `MHGU_ONLY=<动作名>` 跑 2_arp_remap(每动作一次);
3. 3_fill / 3b_cog 照跑(幂等);
4. **4_object_root_motion 是 ONE-SHOT**——增量用 `mhgu/_4_object_root_motion_new.py`
   (同逻辑,只处理还没有 object 曲线的 remap);
5. 3_rebake_ik(VALOR_BLEND=工作文件;原脚本 verify 段 `frame_set(float)` 会崩,
   已修 `int(min(f,maxk))`);4_grip → 6_tag → 8_min2keys;
6. `mhgu/_transfer_to_master.py` — append 3 个 `_remap` 进 MASTER + 重绑
   tether 到 MASTER 自己的 m_mesh Armature(append 不搬运指针,坑⑬变体) + fake_user;
7. `_verify_exporter_divide.py`(MASTER 里跑)+ `mhgu/_fidelity_check.py`(保真抽查)。

三个新动作落盘形态(2026-10-01 MASTER 实测):81 变换轨/Root 双轨/tether 绑定/
030=(106 常量,scaleY=2)左手/047 scaleY=1/000 与 247-253 全长轨/object 位移轨
(195 后撤 -1.44m,196 构静止,095 前移 +4.38m)。警告码 7/50/2/43 与局内验证过的
151/192_remap 同族。**挂点态定案(用户确认 2026-10-01):三招都是左手,落盘的
030/047 左手常量 = 终态,无需换手/刀入鞘精调。**

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
**✅ 局内验证通过(2026-09-29,用户确认)**:重打 bufType=5 后导出部署,刚1 在游戏内
左→右→左换手完全正常。部署实测:030/047 = bufType 5 成对键台阶(10/11 换右、44/45
换回、60/60 收尾)。

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

## 特殊纳刀系 TIML 无敌帧(2026-10-01)

- **配方**(307/308 实测闭合):`PlMotionCommon mFlag` b26(0x04000000)窗口 + 一枚
  `mTrigger` 2048 键在窗口内;`b24`(0x01000000)= 超甲,双开时 no-hit 胜。
- **已应用**(`apply_sheathe_iframes.py`,幂等,仓库根):
  - entry 306(动作 98 特殊纳刀):mFlag f0/f10/f15 OR b26(全程窗口,无清除键),
    mTrigger f0 0→2048。
  - entry 312(动作 100):mFlag f0 OR b26(与 b24 双开,全程)+ **结构化插入**
    PlMotionCommon mTrigger 轨 {f0=2048}(文件尾追加 16 对齐数组 + 类型槽重指针;
    entry 的 types/transforms/keys 数组已验证无共享)。
  - 目标文件 = 游戏目录 `nativePC/hm/wp/wp03/mot/w03_00/w03_00.lmt`
    (改前快照 `.pre-iframes.bak` 在同目录)。
- **⚠️ 导出覆盖警告(坑⑮)**:blend 里 entry 306/312 的 "TIML Data" 输入接了
  TIMLDataNode(原生快照),`exportNodeToFile` inject 模式 = **整个 entry 替换**,
  下次导出会把这两个 entry 的 TIML 打回原版(306 无敌帧消失)。**凡导出动过
  306/312 的 LMT,重跑 `python apply_sheathe_iframes.py`**(幂等,已应用则跳过)。
  blend 侧根治(bake 进 TIML fcurve,u32 拆高低 16 位两条,见 timl_importer.py
  TIMLFlagImport)= 待办。

## 原版动作重置 + TIML 回灌(2026-10-03, `_vanilla_reset.py`)

**用途**:混合项目被 MHRS/MHGU 占用的原版槽位一键还原。动作 = 原版 LMT 重导入,
TIML = 改版 LMT(#1, 部署文件)整体回灌 —— 攻击帧等文件级 TIML 编辑(含 306/312
无敌帧补丁、140 见切窗口)**从此烤进 blend 树**,坑⑮的"导出后重跑
apply_sheathe_iframes.py"不再需要。

```
cp MHW-Valor.blend MHW-Valor.blend.bak-<ts>   # 先备份
blender -b MHW-Valor.blend --python mhgu/_vanilla_reset.py
blender -b MHW-Valor.blend --python mhgu/_vanilla_reset_verify_final.py   # 验收
```

- 机制:按 #2 的 `fcurveOffset` 分组 KBRAction 重导入(共享组共享一个动作,与原生
  导入同构),`LMTEntryNode` 重接;按 #1 的 `timlOffset` 分组 `processTIMLData +
  TIMLStructure` 重建 TIML 节点;旧动作全部先打 fake_user(含 1358 个旧 TIML 动作),
  旧同名动作改 `.pre_reset` 后缀,失连节点删除。#2 里不存在的 id(141/142/143
  divide 系)动画保持不动。
- 2026-10-03 实跑:164 槽替换/3 槽保留/164 TIML 组重建;占用清单(替换前):
  140=192_remap(MHGU 刚1)、312=095_remap、307/343=MHRS plw remap、
  339/340/358-363=divide 系。被替换动作都以 fake_user 留在文件里,重新接线即可恢复。
- **验收口径**(全部通过,`_vanilla_reset_export.lmt` = 验收导出):
  - TIML:掩掉块首 u32(块在文件里的偏移,布局不同必然不同)后 md5 与 #1 逐 entry
    一致,164/164。
  - 动作:结构逐骨全等(boneId/bufferType/fold/键数/键时间零漂移),数值残差为导出器
    再量化噪声(max 0.125,中位 0.067)+ loc basis w 1.0→0.0 —— 两者都是工具链既有
    签名,已部署文件 #1 同样携带(w=0 共 2661 处),游戏实测无影响。
  - entry 头部(loopFrame/flags)与原版零差异(树里本来就是原版的,未动)。
- **坑⑯(TIML 字节比对)**:timl 块序列化首 u32 = 块在文件内的偏移,直接对两个文件
  的 timl 做 md5 永远"不等",先掩掉前 4 字节再比。
- 验收脚本:`_vanilla_reset_verify_final.py`(树接线 + 容差内容比对,容差 = 0.26 +
  放行 w 1↔0);调试链:`_inspect_vanilla_reset.py`(状态盘点)、`_verify_debug*.py`。

## 树重排 + 重置前取证(2026-10-03 续)

- **布局**:FreeKinetics 只有"按列堆叠"一种默认排布(importer 的 uiNodeManager:
  所有 LMT action 一列、TIML data 一列、entry 一列),没有"action 在左、entry 在右、
  TIML 在下"的配对布局——那是手工摆的。`_relayout_tree.py` 把树重排成配对布局:
  每 entry 一行(按 entryNum 升序),action 在左、entry 在右、该 entry 的 TIML
  data+timl action 竖排其正下方,全部节点取消折叠;顺手删死节点。
  重跑 `_vanilla_reset_verify_final.py` 确认接线无损。
- **取证(`_backup_forensic*.py`,对重置前备份整体导出比对)**:
  - 备份树的动作与部署文件 #1 **164/164 完全一致** —— 重置前 blend 里的原版动作
    就是局内在跑、已验证的那套,没有 MHRS/MHGU 内容渗入原版槽。
  - 备份与原版 #2 有 151 槽逐键不符,但全部是管线重编码签名:frameCount 全等、
    81/82 轨(常量轨合并,已知)、键时间网格零差异、数值最大差 0.01;
    不匹配任何一条原版动作(内容没换过,只是重新编码过)。
  - "entry 接了别的 id 的 action"(112←105、149/158←101、162←154、
    309/310/311←308、344/345/346←332/333/334)= **镜像招共享 fcurveOffset 的正常
    现象**,动作以组内最小 id 命名,不是接错。

## 导出 LMT 进不了存档(卡死)排查 + game-safe 构建法(2026-10-03, `_build_firstaid_lmt.py`)

- **现象**:vanilla-reset 导出(`_vanilla_reset_export.lmt`)部署后进存档卡死;改名停用后
  能进(= vanilla 兜底,锅在文件内容)。
- **排查**(`_hang_diagnosis.py`,与最后已知可用备份 bak-20261003-010916-valorcfg 全量对比):
  entryCount/167 头部/TIML(161 swap+3 双方都无)/自定义 141-143(内容逐字节等价)全部一致;
  增量扫描排除 4-bit 溢出(零/负增量三文件都有,分布一致);
  唯一全局差异 = **动作编码层**:E 复用导入时的原版 bufferType(1/2/7, LIMITS min_x),
  R 是管线重算过的编码(12/13 等)。导出器 `calculateValidateEncoding` 单一编码直接复用,
  旧树动作被 min2keys/strip 处理过走"重算"路径 —— E 的"复用原版类型再编码"路径
  从未被游戏吃过,卡死高度疑似在此(未单字节定罪)。
- **game-safe 构建**(绕开编码器):V(原版)round-trip 字节稳定已证明 →
  F = V 的动作本体字节 + R 的 TIML + R 的 141-143 完整 ActionHeader →
  `_vanilla_firstaid.lmt` (md5 0d673ddd, 3813748B)。与 R 的差异只剩:
  原版槽=真原版字节(替代旧重编码)+ 10 个刻意槽回原版(= reset 诉求)。
  自校验:164 动作 digest==V、TIML masked-md5==R、customs==R,全绿。
- **教训**:(1) 全树 standalone 导出 ≠ 游戏可读 —— blend 导出物进游戏前必须先
  A/B 验证一次;(2) 部署链两次发生"扩展名前多空格"(`.l mt`/`.d ll`),复制后
  一定 `ls` 核对文件名;(3) parse→serialize 对原版字节稳定,对手术文件会规范化
  (R 差 1296B),规范化布局=原版布局,游戏可读。
- 若 F 仍卡:退回 bak-010916 作 w03_00.lmt(必能进,= 旧全套自定义 moveset),
  然后再查 vanilla 槽与 divide 期 TIML 的交互。
- **F 也卡 → 毒在 TIML 编辑链**(用户实测:原版 override 能开,E/F 都卡;
  E/F 唯一共同的非原版内容 = R 的 TIML)。逐结构 diff:R 的 TIML 与原版 161/164 同构,
  唯一结构新增 = addcto 链在 339/340/343 共享块追加的占位类型
  (timelineParam=0x4BD36121 + transform hash=0x1 + **0 keyframe**)。
  **F2 = `_build_firstaid_f2.py`** = F 剥占位类型(必须同步 TIML_Data.count,否则解析器
  按旧 count 错位多读一个类型,曾据此误判"剥离没生效") →
  `_vanilla_firstaid_f2.lmt` (md5 ee6e195c)。F2 保留:原版动作 + 全部关键帧级 TIML 编辑
  (R vs V 161/161 有差异=编辑都在) + 141-143 自定义。
  局内验证:**✅ F2 能进(2026-10-03 用户确认)= addcto 占位结构定罪**;
  关键帧级编辑(161/161)全部无罪。F2 非 Blender 导出——是 struct 级拼装
  (原版动作字节 + R 的 TIML + R 的 141-143),Blender 只当 Python 运行时
  (struct 库依赖 mathutils)。全树 Blender 导出仍是不可部署路径,待修。
- **✅ 导出路径平反(2026-10-03 晚)**:E(全树 Blender 导出)卡死同样是 addcto 占位结构
  所致,与动作编码无关——用户判断正确,原版导入→导出链路本来就能用。
  `_strip_placeholder_and_export.py` = 树侧删占位 TIMLActionNode(×3, 339/340/343)+
  正式 exportNodeToFile 导出 = **F3 `_blend_export_f3.lmt`**(md5 8a5e3541,3813556B,
  与 F2 同尺寸)。验证全绿:无占位、164 原版动作 vs 原版容差等价、TIML==F2、
  141-143==R。树侧修改已保存进 blend,**以后该工程导出天然无占位**。
  F2(拼装)与 F3(Blender 导出)等价可互换,均为"原版动作+全套 TIML 编辑+141-143 自定义"。
- **✅ F3 局内验证通过(2026-10-03 深夜,用户确认)**:正式 Blender 导出 + 树侧摘占位
  = 能进游戏。**整条链定案**:addcto 占位结构是 10-02 21:22 之后所有文件卡死的唯一
  毒源;Blender 导入→改→导出→部署链路完全无罪且已恢复。
- **当前定态**:部署 = F3 内容(md5 8a5e3541);MHW-Valor.blend = 干净基座
  (原版重导入动作 + 全套 TIML 编辑 + 141-143 自定义,树内无占位),以后
  改动作→导出→部署可直接用。备份链:bak-20261003-124129(reset 前)、
  bak-20261003-154331(树侧摘毒前)。拼装法脚本(_build_firstaid*.py)留作
  应急工具(绕开导出器的 game-safe 构建法)。
- **✅ 重导入动作补绑骨架(2026-10-03 深夜,`_tether_vanilla_imports.py`)**:用户指出
  vanilla-reset 的重导入没勾 "Tether to Skeleton" —— 旧动作 fcurve 是 `MhBone_XXX`
  (可编辑预览),新动作是 `BoneFunction.xxx/Root/reference` 合成路径(导出没问题但
  编辑器里没法对着骨架改)。修复 = 对 154 个未绑定动作补跑 `transferTether`
  (= 勾那个选项时导入器做的事):路径重命名、Root(-1) 通道转物体级、记 tetherFrame。
  **F4 `_blend_export_f4.lmt`** (md5 54eda879) = 绑定后的正式导出:
  - Root 通道烘焙为逐帧键(entry1: loc buf5×609 + rot buf7×812, 值恒定) —— 与旧管线
    导出(BK, 局内跑过数周)的 -1 形态**完全同构**;
  - 零星 basis 差异 = 四元数符号规范化(语义等价);轨道结构 = 原版 82 轨原样;
  - TIML==F2、141-143==R、无占位,全绿。
  blend 已保存(绑定态持久化);备份 bak-20261003-160823(绑前)。
  **注意:E/F3 的"未绑定导出"与 F4 的"绑定导出"是两种合法形态,F4 是项目原生约定。**
- **✅ 新原版动作骨骼归组(2026-10-03 深夜)**:vanilla-reset 重导入的动作没跑归组
  (Action Editor 里 fcurve 全平铺)。现成 `9_group_channels.py` 直接对当前 blend 跑:
  59057 条重新归组 + 177596 条本就合规,全量校验通过(备份 bak-20261003-162355)。
  归组后重导出 **md5 == F4 逐字节一致** = 分组纯外观、不影响部署物。
  至此 vanilla-reset 收尾完毕:原版动作(绑定+归组+可编辑)+ 全套 TIML 编辑 +
  141-143 自定义,Blender 导出可部署。

## 勇气纳刀 valor_sheath (2026-10-04, 根目录 `_valor_sheath_build.py`)

195×196 全帧 2D 姿态扫描实测最佳拼接对 **(tA=46, tB=131)**(手 IK 目标差 2.6/3.4cm;
指标必须含 247/248 手部目标位置——体骨 FK 差被手 IK 运行时解算部分掩蔽;
`bone.isdigit()` 对 "MhBone_NNN" 恒 False 曾致整版扫描全错)。
产物(MASTER, 全部 valid=True, 81 轨+Root 双轨):

- `valor_sheath_start` 84f = 195[0..38]+渐变[38..46]+196[131..169](尾 8 帧回渐变,
  循环回绕无缝); 位移通道 B 段加 A@46-B@131 偏移(rootY -0.2187 连续);
  247 符号翻转; loopFrame=46(**导出生效在 entry 节点属性, freehk 里那份仅记录**)。
- `valor_sheath_done` 97f = 195[46..143] 前移(松手衔接; 首键=A@46 与 start 基线一致)。
- `TIML::nTimelineParam::PlMotionCommon`(b26 0x04000000@f0 常开 + mTrigger 2048@f0,
  无关闭键=穿透循环; 用户已验证配方——见切/312 同款)。TIML 编码: action 级
  `timelineParamRaw="m48D6114F"`, fcurve=`FreeHKTiml.m<hash>`(mFlag=m08FD20A6 /
  mTrigger=m99897C43), bigFlags 拆 low16/high16 双轨键帧对齐。
- 树 w03_00 已建 3 个未接线节点; 接线时 entry.loopFrame=46 + "TIML Data"←TIML 节点。

坑(详见 AGENTS.md 2026-10-04 节): 段截取后骨 4 通道必须齐全(缺→导出器 Quaternion
空序列崩), done 全曲线强制补 0/97 边界键(eval_hold 端点保持); min2 兜底键末帧向前补;
四元数比较用 2*acos(|dot|)(逐分量差被 q/-q 双覆盖污染)。
