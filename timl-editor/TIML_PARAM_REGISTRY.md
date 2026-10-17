# TIML 参数注册表 (PlMotionCommon 全 65 项 + 方法论)

来源: 反编译 `FUN_14191f7d0` (PlMotionCommon 类别注册函数) — 每个参数一条描述项
{名字串, 类型码, owner, **owner+槽位**}。TIML 轨的值在动作期间被采样进运行时参数块
对应槽位, 消费者读槽位 = 字段语义。复现: `python extract_timl_registry.py`。

**方法论** (2026-10-17 与 mStaminaParamIndex 定案同日确立):
1. 数值轨: 注册表拿名字/槽位 (名字即语义的一半) → xref 扫槽位读者 → 消费系统 → 局内 A/B 盖章。
2. bigFlags 位轨 (mFlag/mFlag1…): 位被消费时 `bts` 进 player+0x8938 事件位数组
   → 枚举位读者即位语义 (bit15 见切锁存 / bit250 居合否决 同法)。
3. mTrigger 值轨: 事件码分发 (已知 2048=无敌门, 65537=见切起手码), 找分发 switch 枚举全部码。
4. 其余类别 (PI/PlMotionVisual/CollisionTimelineObject/GameParameter) 各有同款注册函数,
   经共用分配器 FUN_142171fe0 的 xref 可枚举 — 待扩。

类型码观察: 12=float (时间/倍率), 6/10=int (索引/编号); 与 TIML 值格式 (int/uint/float) 的
精确对应待定。

## PlMotionCommon 全 65 项 (owner = 运行时参数块, 1-based 行号即 mStaminaParamIndex 那种查表语境)

| 槽位 | 类型 | 字段 | 作用 (按名推断, 未标注者=猜测) |
|---|---|---|---|
| +0x60 | 10 | mLookAtParam | look-at (注视) 参数页/索引 |
| +0x64 | 12 | mLookAtSpeedRate | look-at 朝向速度倍率 |
| +0x68 | 12 | mLookAtOfsAngleX | look-at 视角偏移角 X |
| +0x6c | 12 | mLookAtOfsAngleY | look-at 视角偏移角 Y |
| +0x70 | 10 | mSlingerShellAttach | 发射器 (slinger) 弹药附着状态/类型 |
| +0x74 | 12 | mPosInterpolationProgress | 位置插值进度 |
| +0x78 | 12 | mAngleInterpolationProgress | 角度插值进度 |
| +0x7c | 12 | mTurnSpeed | 转身速度 (GUARD 转身钳制元凶, 2026-10-11 案例) |
| +0x80 | 12 | mTurnSpeed2 | 转身速度 2 |
| +0x84 | 12 | mMotionSpeed | **动画播放速度倍率** |
| +0x88 | 12 | mMotionTransRateX | 根位移 X 轴倍率 |
| +0x8c | 12 | mMotionTransRateY | 根位移 Y 轴倍率 |
| +0x90 | 12 | mMotionTransRateZ | 根位移 Z 轴倍率 |
| +0x94 | 12 | mSuperArmorTime | **霸体时间(秒)** = b24 位窗的数值版 |
| +0x98 | 12 | mHyperArmorTime | **超甲(强霸体)时间(秒)** |
| +0x9c | 12 | mNoHitTime | **无敌(no-hit)时间(秒)** = b26+2048 门位窗的数值版 |
| +0xa0 | 6 | mDamageHitNo | 伤害判定编号 |
| +0xa4 | 10 | mScrHitNo | 削韧 (scratch) 判定编号 |
| +0xa8 | 6 | mStaminaParamIndex | **強G 掉耐力档位 → plp mCore 表** 【确证 2026-10-17】 |
| +0xac | 12 | mStaminaRecoveryRate | 耐力回复倍率 |
| +0xb0 | 12 | mCameraSmoothTime | 相机平滑时间 |
| +0xb4 | 12 | mCameraResetTime | 相机重置时间 |
| +0xb8 | 12 | mCameraResetAngle | 相机重置角度 |
| +0xbc | 10 | mViewParamPage | 视角参数页 |
| +0xc0 | 10 | mViewParamId | 视角参数 ID |
| +0xc4 | 10 | mViewParamIdCamColAdj | 相机碰撞调整用视角参数 ID |
| +0xc8 | 10 | mAimViewParamPage | 瞄准视角参数页 |
| +0xcc | 10 | mAimViewParamId | 瞄准视角参数 ID |
| +0xd0 | 10 | mCameraShakePage | 相机震动页 |
| +0xd4 | 10 | mCameraShakeIndex | 相机震动索引 |
| +0xd8 | 10 | mMotionCameraCutNo | 运镜切换编号 |
| +0xdc | 12 | mMotionCameraMoveAngle | 运镜移动角度 |
| +0xe0 | 12 | mGimmickWaitCamMoveAngle | 机关等待时运镜角度 |
| +0xe4 | 10 | mMoveBankType | 移动 bank 类型 (locomotion 分支选择?) |
| +0xe8 | 10 | mWeaponMotionNo | **武器模型动画号覆盖** (纳刀合并研究关联) |
| +0xec | 10 | mNoSetWeaponMotionNo | 禁用武器动画号设置 |
| +0xf0 | 12 | mWeaponMotionSpeed | 武器动画速度 |
| +0xf4 | 12 | mWeaponMotionInterpolationTime | 武器动画插值时间 |
| +0xf8 | 12 | mWeaponMotionStartFrame | **武器动画起始帧** (FRAMESEEK 关联) |
| +0xfc | 10 | mWeaponGaugeIndex | 武器量表索引 |
| +0x100 | 10 | mFingerMotionType | 手指动作类型 (握持姿态) |
| +0x104 | 10 | mActionPhase | 动作阶段 |
| +0x108 | 10 | mActionPhaseSub | 动作子阶段 |
| +0x10c | 10 | mLinkMotionPhase | 衔接动作阶段 |
| +0x110 | 6 | mAttackAdjustAngleIndex | 攻击修正角度索引 (自动转向) |
| +0x114 | 10 | mRopeHoldState | 虫丝/绳索抓住状态 |
| +0x118 | 12 | mAttackAdjustGroundSpeedRate | 攻击中地面速度修正 |
| +0x11c | 12 | mDisableIKInterpolationFrame | 禁用 IK 插值帧数 |
| +0x120 | 10 | mCamViewState | 相机视角状态 |
| +0x124 | 10 | mCamBasePosOffsetNo | 相机基准位置偏移编号 |
| +0x128 | 12 | mCamSpring | 相机弹簧 |
| +0x12c | 12 | mCamMarginDist | 相机边距距离 |
| +0x130 | 12 | mCamMarginEndItpTime | 相机边距结束插值时间 |
| +0x134 | 10 | mCamMarginEndEaseType | 相机边距缓动类型 |
| +0x138 | 10 | mClawGimmickState | 钩爪机关状态 |
| +0x13c | 6 | mOtRideFootOffsetIndex | 骑乘脚部偏移索引 |
| +0x140 | 6 | mOtRideAlphaFootOffsetIndex | 骑乘 (另一侧) 脚部偏移索引 |
| +0x144 | 12 | mOtRideBustUseItemBlendRate | 骑乘中用道具混合率 |
| +0x148 | 12 | mOtRideMuteRotRate | 骑乘旋转抑制率 |
| +0x14c | 12 | mWpSnowShovelScaleX | 武器铲雪缩放 X |
| +0x150 | 12 | mWpSnowShovelScaleY | 武器铲雪缩放 Y |
| +0x154 | 12 | mWpSnowShovelScaleZ | 武器铲雪缩放 Z |
| +0x158 | 12 | mWpSnowShovelCurvePower | 铲雪弯曲力度 |
| +0x15c | 12 | mPowderScaleX | 雪粉粒子缩放 X |
| +0x160 | 12 | mPowderScaleY | 雪粉粒子缩放 Y |

## 待办

- [ ] 其余类别注册函数定位 (经 FUN_142171fe0 xref) → PI / Visual / Collision / GameParameter
- [ ] mNoHitTime/mSuperArmorTime 消费者追踪 → 坐实"数值窗=位窗等价物" → 编辑器加时长模式
- [ ] mWeaponMotionNo/mWeaponMotionStartFrame 消费者追踪 → 接 09-20 纳刀合并研究
- [ ] 类型码 6/10/12 与 TIML 值格式 (int/uint/float) 对应关系

## nWwiseTimeline 类别 (无注册描述符, 轨值即载荷; 2026-10-17 经验破译, 当日晚触发语义定案)

Wwise 系类别不出现在注册函数里 (描述符模式不适用) — 轨 hash 名 + 值 = 帧→请求号。
部署 LMT 实测 (E106/E110/E140):

| 类别 | 轨 | 值特征 | 推断 |
|---|---|---|---|
| **EventGroup02** | ReqNo A-C + mTrigger | 1119/1409/1019 (跨招部分共享) | **喊叫载体 (2026-10-17 晚 user A/B 翻案: 删 EG02.086 → 345 灭喊, 当时 EG01 完整在场)** |
| EventGroup01 | ReqNo A-E | 111/115/135/139/145, 全招共用同一组 | **角色存疑** — 曾推断为气合声 (值像语音库), 但 345 上 EG01 在场无 EG02 时喊声全无; 单删 EG01 未干净测过 |
| EventGroup01 | mTrigger | 帧上小事件码 (1/2/4/8/16) | 触发器: 值 = 行位掩码 (定案见下), 但其音频效果未isolated |
| EventGroup03/05/09 | ReqNo | 543/902/7844 | 杂项 SE |
| EventGroup06/07 | ReqNo + TargetUnitNo + mTrigger | 56xx | 落地/摩擦等音效 |
| EventCollision00 | ReqNo(On) A/B + SkipTime + mFlag | 1/2 (= col bank 的 ReqNo) | **攻击窗** (col 已破译, 非声音) |

- 原版基础攻击 (84 纵斩/79 突刺) **没有任何 TIML** 也有喊叫 → 那些是动作类代码层驱动;
  TIML 驱动的喊叫只存在于带 TIML 的 entry (106/110/140/306/345/…)。
- EventGroup ReqNo 与 col 的 ReqNo 同名不同物 (声音请求 vs 碰撞 bank 请求)。

### mTrigger 触发语义定案 (2026-10-17 晚, E170/E345 二进制+blend 双侧核对)

- **mTrigger bigflag (m99897C43) 的键 = 触发事件: 键帧 = 触发帧, 键值 = 行位掩码**
  (bit0=ReqNo A, bit1=B, bit2=C, bit3=D, bit4=E → 值 1/2/4/8/16)。f0=0 是基线键。
- **行序 A~E = 二进制轨序 = blend fcurve 插入序** (patch_timl_defense.py dump 直接显示
  "ReqNo A"~"ReqNo E" 轨名)。E170: A=115 B=145 C=135 D=111 E=139;
  E345: A=115 B=145 C=135 D=111 E=155。
- 交叉证据: EG02 里 E170/E345 的 code4 都命中 C 行 (1019 挥砍声); E170 EG01 的两声喊叫
  = **B(145)@f6 + B(145)@f141** (user 听到的"两声"), A(115)@f182/f235 = 末尾气音,
  C(135)@f98 一声。170 共 246 帧。
- **E170 喊叫模式 = 3 段式: 起手喊 (2%) + 中段喊 (57%) + 收招气音 (74%/96%)**。
- E345 (valor_1, 142 帧) 旧触发 {36:C 90:C 111:E(155) 131:B 155:A} 三病灶:
  **f155 超出动作长永不触发; f111/f131 落在收招死区** (f100 后骨骼活动 <0.2 rad/帧,
  主发力爆发在 f30-54); 与挥砍声 (EG02 f2/72/130) 全程错位 → "有两声但时机不对"。
- **E345 终态 (2026-10-17 晚, user A/B 三轮定案)**:
  - **喊声真载体 = EventGroup02.086** (翻案实验: user 删整个 EG02.086 → 灭喊,
    EG01 完整在场; 撤销恢复)。行 A/B/C = 1119/1409/1019 = 真正的喊叫请求号
    (4 位数库), 触发 {f2=2(B), f72=1(A), f130=4(C)}。
  - **此前 v1/v2/v3 对 EG01 的改时机/删键全部无效** (用户听到的两声从未移动) —
    教训: 类别角色未isolated前, 别拿"改了没反应"当语义否定。
  - **"第二声" = f72 事件 (A 行/1119, 1.2s 主挥砍后)** — user 先说"应在 40 帧"
    (主发力爆发 f30-54), 后裁定直接删除。v4 终版 EG02 mTrigger =
    {f0=0, f2=2(B=1409 拔刀喊), f130=4(C=1019 二刀喊)}。
  - EG01.207 保持 v3 状态 {36:C, 113:C, 122:B, 138:A} (5 键含基线), 其作用待isolated
    (候选: 低音量气音层/语音变体选择/或对此 move 无音频效果)。
  - **撤回**: 之前"155 可闻/C(135)@36 可闻"结论作废 — 那两声实为 EG02 的 f72/f130。
  - 部署备份链: pre-345-shout-retime → pre-345-shout-f40 → pre-345-shout-del2nd
    → pre-eg02-f72-del (= user 删 EG02 实验版, 现已恢复)。
- 旁证: E170 的 EG02 触发 {f22, f84, f191} 与其"两声喊叫"同构 (3 触发 2 可闻),
  EG01 角色对 170 同样存疑。

### 345 声音组件全景 (2026-10-17 深夜 user 逐个删除辨认 + blend 数据定案; v5 重排已部署)

| 组件 | 类别/轨 | 结构 | v5 终态数据 |
|---|---|---|---|
| 金属摩擦声 | **EventGroup06.182** | 同款 ReqNo 行 + 位掩码触发 | 6 行 56xx {A=5645, B=5616, C=5655, D=5620, E=5645, F=5660}; **v5 收 3 触发进 40-55 → 游戏内金属声全灭 (user 实测), v6 还原原版 8 键 {f1:4, f20:8, f31:32, f60:2, f148:16, f157:8, f176:0} 后恢复**。⚠EG06 触发语义存疑: 可能非行位掩码 (行序配对? 触发值=子事件码?), 或 A/B/C 行是哑行 — 再改前先做单触发 A/B |
| **刀风声** | **GameParameter.206** | **无 mTrigger! 连续参数轨** (音频逐帧采样) | TargetReqNo A=1 / B=2 (dataType 1/0); 强度曲线 **GameParam A (dataType 2 float)** {f0=0, **f30=0, f31=30**, f45=30, f125=15} 与 **GameParam B** {f0=0, f30=0, f31=28, f38=30, f45=25, f155=15} — **v6 起音点提前到 f30** (user: 破空声音资源文件开头有几帧静音, 曲线窗提前补偿; 30 前硬静音, f31 瞬起) |
| 踏步泥土 FX(自带声) | **EffectParameter1.106** | KeyFrame 使能窗 (dataType 4 bool) × 3 槽 + WorkNo FX id | KeyFrame00 {f5-20 拔刀步, **f50-55 (原 f60-65 挪入)**, f140-150 收招步} + WorkNo00 {3199→f130:3198}; KeyFrame01 f69 + 2018; KeyFrame02 f73 + 2019 未动 |

- GameParameter 是第三种 TIML 形态 (触发式 EG / 位掩码 / **连续参数**) — "删触发帧"
  的编辑模型对它不适用; 改曲线 = blend 里删 fcurve 重建 (keyframe_points.add 不返回
  新键, 取 [-1]; remove 会使引用失效, 整条重建最稳)。
- **工具坑**: patch_timl_defense.py dump 只显示 dataType=1 (uint) 轨 — float (2) /
  bool (4) / raw (0) 轨全不可见 (GameParam 曲线/EP1 KeyFrame 都是)。看全量要用
  struct 原始解析 (本节 dump 方法)。
- 备忘: 重载工程后插件 grip-sync 会在 load 时重写 FK_grip_preview_auto (日志可证),
  文件随即 dirty 属正常, 不是未保存的用户改动。撤销栈跨 MCP/GUI 共用, 改动频繁期
  建议"以磁盘为准"重载 (open_mainfile) 而不是连续撤销。
- **删键注意**: 删 TIML 键会让导出文件整体变短 (v3 比 v2 少 16 字节), 后续所有
  entry 的文件偏移前移 → `cmp -l` 字节对比失效; 回归验证改用全 entry TIML 语义
  diff (iter_tracks 快照对比, 464 entry 全过 = 只有 345 变化)。
- 编辑手段: blend EG01 action 的 mTrigger **双通道键必须同步移动** (ix0=值/ix1=高位0),
  键保持帧排序; 345 = `TIML::nTimelineParam::nWwiseTimeline::EventGroup01.207`,
  170 = `...EventGroup01.224`。blend 已存
  (`MHW-Valor-0.0.2-shout-config.blend.bak-20261017-pre-shout-retime`),
  部署备份 `w03_00.lmt.bak-20261017-2100-pre-345-shout-retime`。
- 语义留白: C(135) 在 170 里是否出声未定 (若出声 170 应有三声 — user 记两声,
  故 135 疑似气音/不出声); 各 ReqNo 具体音色对照未做。
