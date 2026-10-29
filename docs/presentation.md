# 呈现层：FX / 喊声 / 拖尾 / 屏幕震动 / 动作音效

> 过程见 `SESSIONS.md`（10-17 声音系列、10-20/21 刀光拖尾系列、10-21 屏震、10-22 喊声前史、
> 10-24 喊声落地）。编辑器：FreeKinetics 内 timl_sound / timl_trail / timl_shake 三编辑器
> + 窗口编辑器，用法见 `timl-editor/README.md`。

## 身体特效（ADD670 族）

- 通知函数 FUN_140ADD670（emitter, id, 0），**返回句柄**（消费返回值的调用方必须转发，
  丢返回值=野指针崩溃）。0x140AE0D70 = 纯 thunk。
- **FX id 按"当前手持武器"分表解析**（user 实测）：同一 id 0x7D0 大剑在手=蓄力红光、
  太刀在手=普通气刃特效。跨武器借特效靠改 id 做不到。
- LS 表速查：0x7D2=气刃斩系通用红光 / 0x7D3=红光(架势) / 0x7D4=居合音效+见切受击音 /
  0x7D7=见切起手蓝光+音效(86 双发) / 0x7DC=大居合专用 / 0x7D1/0x7D9 无效 / 0x7D0=GS 表蓄力红光。
- 红光完整判据（原生）：招的 begin 处理器走 wrapper(cost) ∧ cost>0 ∧ 练气≥cost ∧ bit0 →
  置 weapon+0x2398 → 发射器 FUN_1421224d0 消费（87/102 另有等级门）→ ADD670 0x7D2/0x7DC。
  自定义招免 col/原生处理器：fx 规则（gauge-rules）或 F7 试枪（fx-test.txt，func 0-5）。
- 刀身常驻光环（升级驱动）走另一套：FUN_142110a90(auraSet) 写 weapon+0xAD4。

## 武器拖尾（三条件模型，user-verified 闭环）

1. **动作含 MhBone_254/252 变换轨**（恒等单键即可，数值无关——ribbon 形状来自武器模型
   本体路径；迁移管线默认剥 254 轨 = "迁移动作无拖尾"根因，merge 后必须补回）。
2. **TIML PlMotionVisual mFlag 的 bit3(值 8) 窗**覆盖想要的帧段（bit4=16=槽1 拔刀/纳刀光、
   bit11=2048 **不是拖尾**、bit14=16384 空中系、2056=过渡）。多段挥砍=多对 (起,8)(止,0)
   CONSTANT 键，段间自动断开。
3. **weapon+0x2398 武装**：`trail <set> <id>` 规则每帧写 1 / 原生气刃付费 / 白名单
   {87,89,90,91,92,102}。
- 纯 DLL 后备路线：每帧写 +0x2398=1 且置 [*(player+0xE490)+8] bit3 → 任意动作出拖尾。
- 本体 = 效果 id 0x9C4（FUN_140AE27E0 生成 keep-alive / 0x140AE1700 释放）。
- 窗收窄法：手部骨逐帧变化量剖面→峰值扩展（20% 阈值/6 帧容差）→ 挥砍段+linger。
- 编辑器：timl_trail_editor（entry 驱动，一键配置+按序号增删段）。
- ⚠ 面板 operator 必须显式传参（op.entry_num=...）——裸 operator() 静默用类默认值，
  曾把 user 对 343 的删除操作打到 345 头上（10-24 事故）。

## 喊声（EG02 语音）

- 载体 = EventGroup02（voice），行 A..F=语音请求号，mTrigger 键帧=触发帧、键值=ReqNo 行
  位掩码（bit0=A…）。**铁律：ReqNo 行 fcurve 必须全部先于 mTrigger**（后补行=掩码选中
  触发轨本身→无声；给已有事件后补行=整 action 从规范 donor 重建）。
- 语音号（user 听音定案）：**1309=爆发"哈"（307 小居合单行双触发 {f7,f31}=标准结构）/
  1409=前摇较劲"哦" / 1119=中段喊 / 1019=挥砍喊**；行值 155 变体可闻。
- EG02 在自定义槽位（156/343）完全可用（EG02 原块整拷即出声）。新建最稳路径=复制 156
  现成 EG02 action 改键（freehk ID 属性自动继承）。
- 编辑器：`freehk.timl_snd_voice_add(entry,frame,voice_id)` 一键加（自动建行+保序+幂等）、
  `timl_snd_canon` 规范化行序、VOICE_ID_HINTS 速查贯穿。
- 铁律补充：**类别角色未 isolated 前，"改了没反应"不能当语义否定**（345 喊声曾在 EG01
  上空转三轮，真身在 EG02）。

## 屏幕震动（配方定稿，可复制到任意招）

- 三件套（PlMotionCommon 块内）：
  ① `m252961CD` = mCameraShakeIndex 强度档窗（LINEAR + guard 键 1 帧沿：
  `(0,0),(窗起-1,0),(窗起,档),(窗止,档),(窗止+1,0)`）；
  ② **mTrigger bit10(1024) 点火 5 帧爆窗**（挥空也震的关键；ix0=`(0,0),(窗起,1024),(窗起+5,0)`，
  ix1 同帧全零镜像）；不碰 bit11=2048（无敌族）；
  ③ 可选 `mF4D12F36` = mViewParamId 拉镜头（CONSTANT，覆盖攻击窗，动作末 -1 标记回正）。
- 档位表 = `hm/wp/wpNN/wpNN_shake.csk`（16B 头 + 120B/条；索引超武器表落全局
  common/camera/shake_pl.csk）。**LS 表 43 号槽原生弱 → 已移植 wp00[41-43] 入 wp03 同槽**
  （部署 csk md5 7b304e5c）。GS 真蓄配方参考：43@打击帧→32→20 滑降。
- 命中震 = 所有攻击自带默认反馈，与本曲线无关；挥空震 = 上述配方。
- 编辑器：timl_shake_editor（一键配方：强度窗+点火爆窗+拉镜头，`freehk.timl_shake_setup`）。

## 动作音效（自定义 WAV）

- ACTIONSND（`ValorGauge.action-sound.txt`：`<set> <id> <frame60> <wavPath>`）：motion 时钟
  ≥cue 帧时 PlaySoundA（PCM wav；无音量/3D 参数；同刻多规则互斥）。武装-取消语义：动作
  切换/被打飞即取消；mot 倒退=重新武装。文件缺失=内置默认规则（3:108 f50 swo_fx.wav）。
- **clang 触发模式**（`<set> <id> clang <wavPath>`，2026-10-30）：cue 帧走 motion 时钟会被
  卡肉（hitstop）一起拖慢——计数动作的声音改在**防御触发瞬间**播。信号=negrEvent 钩子
  （FUN_1421038B0 事件 8 递送，游戏播 0x7D4 叮特效的同一链），owner==本地玩家过滤，
  每次动作实例只发一声（fired 随动作离开复位）；事件逐帧 tick，时序竞态自愈。**只对
  86/101/102 家族槽有效**（非家族无效化=查询层静默，E3 定案）——自定义反击招必须骑这些槽。
  日志 `ACTIONSND clang fire` / 取消时 `clang not delivered`。
- TIML 声音轨家族（数据侧）：EG02=喊声 / EG06=金属摩擦（**触发语义存疑，收编触发曾全灭，
  动前先单触发 A/B**）/ GameParameter=连续参数轨（刀风声=逐帧采样，灭声=归零或删 action）
  / EffectParameter1=踏步 FX（3 槽使能窗+id 切换）。
- 插值护栏同全局：重建 fcurve 后必须设插值（BEZIER 崩导出）。

## 观感调参速查

| 想要 | 动哪里 |
|---|---|
| 挥砍拖尾 | 三条件（见上）；343 终态窗 = 24-38 + 58-90（user 定值） |
| 红刃身体红光 | fx 规则 0x7D2 / 原生付费链 |
| 大力卡肉 | col 行 HitLag_a/b（0.15~0.4）+ 冻结倍率 0 |
| 打击屏震 | shake 三件套配方 |
| 起手"哈" | EG02 行 1309 + mTrigger 起手帧 |
| 相机拉远 | mViewParamId=14 窗（343 用 31-72） |
