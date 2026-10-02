# 强化射击（特殊射击/強化射撃）TIML 研究（2026-10-02）

## 问题

太刀气刃斩中可穿插的强化射击（LMT 341 一族）在 TIML 里带的"特殊攻击帧"是什么；
能否把这套帧搬给普通招式，让普通招也能出射击/承载新动作。

## TIML 签名（vanilla w03_00.lmt）

- `PlMotionCommon.mTrigger`：**`f34=1024(bit10) + f36=1(bit0)` 成对 = 点火通知对**；
  332/338/341/344/347 另有 `f0=2048`（b26 窗内的无敌门变体）。
- `PlMotionCommon.mSlingerShellAttach`：`f0=3 → f36=0 → f114=3`（弹壳挂点 attach→分离→
  attach，分离帧与 bit0 同帧）。
- 配套：`mClawGimmickState f15=2→f36=0`、`mCameraShakeIndex=0x63`。
- 家族：**LMT 332~349 共 15 个 entry，全部 134 帧、键位完全一致**；296/297 = 同款键对
  @f9/f12（101 帧，早期变体）。306~311（特殊纳刀/居合系）带孤立 1024 键，语义不同勿混；
  53/109/174/221/337/326/140 也零散用到 bit10/bit0（消费方是通用事件处理器）。
- 动作类（类表 0x142EF5C30，`Reverse/action_class_table.py`）：
  **id 43/44/45 = `cKijin1ComboShot` Base/F/R；46/47/48/51 = `cKijin2ComboShot` Base/F/L/R；
  49/50/98 = `cKijin3ComboShot` Base/F/L；81 = `cActWpOnSlingerShot`**（持刀站桩射击）。
- 104（気刃斬り1，原版能接强化射击）没有任何射击专属键，PlMotionInput 窗口与 105 形状一致
  → **接受射击输入的门不在 TIML**（与气刃增量/红光同理，代码按动作 id 硬编码）。

## 实验（user-verified 2026-10-02）

1. 105 突刺 mTrigger 重指向 338 的三键数组（`sharekeys.py`，staging 文件未部署）
   → **只有画面震动**。
2. 341 整套 TIML 复制到 105 → **投石器音效 + 震动，无伤害、无投射物**。

**定案：TIML = 纯表现层**（音效/震动/弹壳动画）。点火、投射物生成、伤害、输入接受
全部在动作类代码侧（`cKijinNComboShot` 类）。"TIML 塞帧给普通招"只能出表现，
不能出弹/伤害/输入口，路线关闭。

## 对改造计划的意义

- 槽位托管：43~51/98 这些强化射击槽位换上 divide 动作的 motion（mod 已有 motion-swap
  工厂补丁，配置 `ValorGauge.motion-swap.txt`）。
- DLL combo-force：`duringIds + VK → force {3,43}` 族（counter / dir-rewrite 机制组合），
  用于普通招中按键触发。

## 附带发现：部署版 341 TIML 残缺

部署版 w03_00.lmt 的 entry 341 丢了整个 PlMotionCommon（7 轨：mTrigger/mFlag/
mSlingerShellAttach/mClawGimmickState/mCameraShakeIndex/mCameraShakePage/
mAttackAdjustAngleIndex）——blend 导出管线把该 entry 写残（141/142/143 为管线有意增益，
全表审计仅 341 一处丢失）。若还要 341 当射击参照需在 blend 侧修复；
**每次导出后建议跑 `analyze.py diff` 做 vanilla vs 导出审计**。
另：手改部署 LMT 会被下次 blend 导出冲掉。

## 工具

- `analyze.py scan|diff|windows [entries]` — mTrigger 值全表扫描 / 双文件逐 entry 轨道
  diff（丢轨审计）/ PlMotionCommon+PlMotionInput 位窗口解码。
- `sharekeys.py <lmt> <dstEntry> <category-substr> <transform> <srcEntry>` — dst transform
  头（xoff/xcount）重指向 src 的键数组，零字节增长；alias 只读安全，但工具编辑一侧
  会影响另一侧，正式方案应改为复制数组。
- staging：`w03_00-shot105-test.lmt`（105 mTrigger ← 338 三键），未部署，可删。
