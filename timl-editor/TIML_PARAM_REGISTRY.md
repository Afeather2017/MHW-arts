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
