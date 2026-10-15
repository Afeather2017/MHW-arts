// Semantic metadata layer: known hashes, flag-bit meanings, dispatch constants,
// and the Long Sword entry name table. Facts sourced from
// LMT_TIML_Hitbox_IFrame_SuperArmor_Sheath_Research.md + AGENTS.md session notes.
import { TIMELINE_PARAM_NAMES, DATATYPE_TABLE } from './timlnames_gen';

export { TIMELINE_PARAM_NAMES, DATATYPE_TABLE };

// ---- timeline parameter (category) hashes -------------------------------
export const CAT = {
  CollisionTimelineObject: 956269290,
  PlMotionCommon: 1221988687,
  PlMotionInput: 1875693838,
  PlMotionVisual: 1190390017,
  EventCollision00: 217946204,
  EventCollision01: 2080024778,
  EventCollision02: 1660156272,
  EventCollision03: 368364006,
} as const;

export function catName(hash: number): string {
  return TIMELINE_PARAM_NAMES.get(hash) ?? `unk%08X`.replace('%08X', hash.toString(16).toUpperCase().padStart(8, '0'));
}

const DT_MAP = new Map<number, [string, number]>(DATATYPE_TABLE.map(r => [r[0], [r[1], r[2]]]));
export function transformName(hash: number): string {
  return DT_MAP.get(hash)?.[0] ?? 'unk_' + hash.toString(16).toUpperCase();
}
export function transformDataType(hash: number): number | undefined {
  return DT_MAP.get(hash)?.[1];
}

// ---- transform hashes we author ----------------------------------------
export const MFLAG = 0x08FD20A6;     // 150806694  mFlag   (uint)
export const MFLAG1 = 0x6E63FBC7;    // mFlag1  (uint) — PlMotionInput combo-key switch
export const MFLAG2 = 0xF76AAA7D;    // mFlag2  (uint) — event-bit write track
export const MTRIGGER = 0x99897C43;  // 2575924291 mTrigger (uint)

// ---- TIML_Type.unkn0 = loader category dispatch id ----------------------
// Game crashes on boot when a PlMotionCommon block carries the wrong id
// (AGENTS.md 2026-10-04, 坑: unkn0).
export const UNKN0_BY_CATEGORY: ReadonlyMap<number, number> = new Map([
  [CAT.PlMotionCommon, 11],
  [CAT.PlMotionInput, 9],
]);
export function unkn0For(hash: number): number {
  return UNKN0_BY_CATEGORY.get(hash) ?? 0;
}

// ---- PlMotionCommon.mFlag bit meanings (empirically verified) -----------
export interface BitInfo {
  bit: number;
  label: string;
  short: string;
  color: string;
  needsGate?: boolean;   // requires mTrigger 2048 key inside the window
  exclusiveWith?: number[]; // bit indices that must not coexist in the window
}

// PlMotionCommon.mFlag
export const PC_BITS: BitInfo[] = [
  { bit: 26, label: '无敌帧 (no-hit / i-frames)', short: '无敌', color: '#39c2ff', needsGate: true, exclusiveWith: [28] },
  { bit: 24, label: '霸体帧 (super armor)', short: '霸体', color: '#ffb039', exclusiveWith: [] },
  { bit: 28, label: '防御帧 (角度格挡有效, directional guard)', short: '防御', color: '#7ddd6a', exclusiveWith: [26] },
  { bit: 25, label: '攻击动作基线 (universal attack baseline)', short: 'b25', color: '#8a8a8a' },
  { bit: 22, label: '基线位 (baseline)', short: 'b22', color: '#8a8a8a' },
  { bit: 27, label: '未知 (armor? seen in 115/108/109)', short: 'b27?', color: '#b06ad8' },
  { bit: 31, label: '未知 (held whole move on 见切/收刀族)', short: 'b31?', color: '#b06ad8' },
  { bit: 1, label: '基线位', short: 'b1', color: '#8a8a8a' },
  { bit: 2, label: '基线位', short: 'b2', color: '#8a8a8a' },
  { bit: 15, label: '事件位写入 (mFlag2 b15 = 纳刀态信号候选)', short: 'b15', color: '#d86a9c' },
];

// PlMotionInput.mFlag (input accept windows)
export const PI_BITS: BitInfo[] = [
  { bit: 8, label: '翻滚接受 (roll accept)', short: '翻滚', color: '#4fd1c5' },
  { bit: 14, label: '连段预输入 (combo buffer)', short: '连段', color: '#63b3ed' },
  { bit: 16, label: '连段预输入 (combo buffer)', short: '连段', color: '#63b3ed' },
  { bit: 30, label: '连段预输入 (combo buffer)', short: '连段', color: '#63b3ed' },
  { bit: 17, label: '特殊派生窗 (见切→必殺攻撃, pairs with b31)', short: '特殊', color: '#f6ad55' },
  { bit: 31, label: '特殊派生窗 (pairs with b17)', short: '特殊', color: '#f6ad55' },
  { bit: 9, label: '未知 (与 b8 伴出)', short: 'b9?', color: '#8a8a8a' },
  { bit: 15, label: '未知 (与 b14/16 伴出)', short: 'b15?', color: '#8a8a8a' },
  { bit: 0, label: '未知 (循环/收势族)', short: 'b0?', color: '#8a8a8a' },
];

// PI.mFlag1 = 组合键开关 (R2+方向 解锁; 2026-10-12 假设, 单变量测试文件已造未测)
export const PI_MFLAG1_BITS: ReadonlyMap<number, string> = new Map([
  [12, '组合键开关?(对b12)'], [26, '组合键开关?(对b26)'],
  [13, '组合键开关?(对b27)'], [27, '组合键开关?(对b13)'],
]);
// PI.mFlag2 = 事件位写入轨候选 (值族 0x150/0x3F0/0x3C300, 未解)
export const PI_MFLAG2_BITS: ReadonlyMap<number, string> = new Map([
  [4, '未解'], [5, '未解'], [6, '未解'], [7, '未解'],
  [8, '未解'], [9, '未解'], [22, '未解'], [23, '未解'],
]);

/** 语义标签: "翻滚(b8)" / "未解(b9)". trackHash 区分 mFlag/mFlag1/mFlag2. */
export function piBitLabel(trackHash: number, bit: number): string {
  if (trackHash === MFLAG1) return `${PI_MFLAG1_BITS.get(bit) ?? '未解'}(b${bit})`;
  if (trackHash === MFLAG2) return `${PI_MFLAG2_BITS.get(bit) ?? '未解'}(b${bit})`;
  const known = PI_BITS.find(b => b.bit === bit);
  return `${known ? known.short : '未解'}(b${bit})`;
}

/** PlMotionCommon.mFlag 已知位标签 (防御系), 未知返回 undefined. */
export function pcBitLabel(bit: number): string | undefined {
  return PC_BITS.find(b => b.bit === bit)?.short;
}

// mTrigger special values
export const TRIGGER_NOHIT_GATE = 2048; // 0x800 — arms the b26 i-frame window

// ---- collision slot values (EventCollision00.mFlag) ---------------------
// 0=off, 1/2/3 = collision slot group on (damage window open).
export function slotLabel(v: number): string {
  return v === 0 ? 'off' : `slot ${v}`;
}

// ---- Long Sword entry names (action-list.md) ----------------------------
// LMT entry index -> move name (w03_00).
export const LS_ENTRY_NAMES: ReadonlyMap<number, string> = new Map([
  [1, '人物持刀站立'],
  [101, '拔刀纵斩'],
  [102, '踏步斩'],
  [127, '右袈裟斩'],
  [110, '袈裟斩后接的气刃2'],
  [108, '气刃3'],
  [109, '气刃大回旋'],
  [306, '特殊纳刀'],
  [312, '纳刀架势派生'],
  [307, '小居合 (特殊纳刀后的拔刀斩)'],
  [105, '突刺'],
  [104, '气刃斩1'],
  [119, '上捞斩'],
  [140, '看破斩 (见切)'],
  [170, '气刃突刺'],
  [171, '气刃兜割 起手'],
  [172, '气刃兜割 中段'],
  [174, '气刃兜割 攻击'],
  [155, '跳跃气刃二连斩 一段目'],
  [156, '跳跃气刃二连斩 二段目'],
]);
