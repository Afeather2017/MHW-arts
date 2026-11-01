#pragma once

// --- 翻滚切勇気納刀 (ROLLSHEATHE, 2026-10-31 定稿) ---------------------------
// DLL 方案: 鼠标侧键轮询 + 翻滚动作帧阈值 -> DoAction(3,98) 勇気納刀。
// 配置 nativePC/plugins/ValorGauge.roll-sheathe.txt; PatchRollSheatheRows 把
// 勇気納刀候选行写进翻滚分类器子表 (vanilla 等价路径, addresses.txt
// rollSheathePatch=0 可停用); SheathCapHook 捕获分类器 mgrCtx 供研究。
void LoadRollSheatheConfig();
bool InstallSheathCapHook();
void PollRollSheathe(const void* entity);
void PatchRollSheatheRows();
bool InstallSheathClsfyProbeHook();
// COMBOPROBE (addresses.txt comboProbe=1): 分类器检查器数组被动观测 + ba00
// 评估钩子, 钉死"游戏内部特殊納刀输入态"用; 详见 rollsheathe.cpp 该节注释。
void PollSheathComboProbe(const void* entity);
bool InstallSheathEvalProbe();
