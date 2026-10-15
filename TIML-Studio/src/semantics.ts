// Feature semantics: the five authoring recipes as decode/apply functions.
// Recipes are the empirically closed ones from LMT_TIML research + AGENTS.md:
//  - 无敌帧: PlMotionCommon.mFlag b26 window + ONE mTrigger 2048 key inside (gate, position-free)
//  - 霸体帧: PlMotionCommon.mFlag b24 window (no trigger)
//  - 防御帧: PlMotionCommon.mFlag b28 window (directional guard; mutually exclusive with b26)
//  - 攻击帧: EventCollisionNN.mFlag windows (0=off, 1/2/3=slot on) + CTO config
//  - 按键:   PlMotionInput mFlag/mFlag1/mFlag2 bit windows
import {
  TimlBlock, TimlTransform, TimlKey,
} from './timl';
import { CAT, MFLAG, MFLAG1, MFLAG2, MTRIGGER, TRIGGER_NOHIT_GATE, unkn0For, transformDataType } from './names';

// ---- bit helpers (u32 domain) -------------------------------------------
export function bitMask(bit: number): number {
  return (2 ** bit) >>> 0;
}
export function setBit(v: number, bit: number): number { return (v | bitMask(bit)) >>> 0; }
export function clearBit(v: number, bit: number): number { return (v & ~bitMask(bit)) >>> 0; }
export function hasBit(v: number, bit: number): boolean { return ((v >>> 0) & bitMask(bit)) !== 0; }

// ---- track lookup / creation --------------------------------------------
export function findType(block: TimlBlock, catHash: number): TimlTypeRef[] {
  const out: TimlTypeRef[] = [];
  block.types.forEach((t, ti) => { if (t.hash === catHash) out.push({ ti, type: t }); });
  return out;
}
export interface TimlTypeRef { ti: number; type: TimlBlock['types'][number]; }

export function findTransform(block: TimlBlock, catHash: number, tfHash: number): { ti: number; xi: number; xf: TimlTransform } | null {
  for (const { ti, type } of findType(block, catHash)) {
    const xi = type.transforms.findIndex(x => x.hash === tfHash);
    if (xi >= 0) return { ti, xi, xf: type.transforms[xi] };
  }
  return null;
}

/** Vanilla-baseline bits for a fresh PlMotionCommon.mFlag track (b25|b22|b1|b2). */
export const PC_MFLAG_BASELINE = 0x02400006;

/** Get or create category/transform. New types get the loader dispatch unkn0; new
 *  transforms get the dataType from the hash table. Empty flag tracks are seeded
 *  with a baseline key at f0 so windows can be cut immediately. */
export function ensureTransform(block: TimlBlock, catHash: number, tfHash: number, seedValue = 0): { ti: number; xi: number; xf: TimlTransform; created: boolean } {
  let type = block.types.find(t => t.hash === catHash);
  if (!type) {
    type = { hash: catHash, unkn0: unkn0For(catHash), transforms: [] };
    block.types.push(type);
  }
  let xf = type.transforms.find(x => x.hash === tfHash);
  if (!xf) {
    xf = { hash: tfHash, dataType: transformDataType(tfHash) ?? 1, keys: [] };
    type.transforms.push(xf);
  }
  if (!xf.keys.length) {
    xf.keys.push(makeKey(seedValue, xf.dataType, 0, defaultTransitionFor(catHash, tfHash)));
  }
  return { ti: block.types.indexOf(type), xi: type.transforms.indexOf(xf), xf, created: true };
}

export function defaultTransitionFor(catHash: number, tfHash: number): number {
  if (tfHash === MTRIGGER) return 6;
  if (tfHash === MFLAG || tfHash === MFLAG1 || tfHash === MFLAG2) return 5;
  return 1;
}

export function makeKey(value: number, dataType: number, frame: number, transition: number): TimlKey {
  return { value: value >>> 0, controlL: 0, controlR: 0, frame, transition, typeTag: dataType };
}

// ---- decode: step spans --------------------------------------------------
export interface Span { start: number; end: number | null; } // end=null = open (till anim end/loop)

/** Spans where pred(value) holds, treating the track as a step function
 *  (value holds from its key until the next key). Before the first key: off. */
export function valueSpans(keys: TimlKey[], pred: (v: number) => boolean): Span[] {
  const spans: Span[] = [];
  let start: number | null = null;
  for (const k of keys) {
    const on = pred(k.value);
    if (on && start === null) start = k.frame;
    else if (!on && start !== null) { spans.push({ start, end: k.frame }); start = null; }
  }
  if (start !== null) spans.push({ start, end: null });
  return spans;
}

export const bitSpans = (keys: TimlKey[], bit: number): Span[] => valueSpans(keys, v => hasBit(v, bit));

/** Bits that actually vary somewhere in the track (for auto lane generation). */
export function varyingBits(keys: TimlKey[]): number[] {
  const or = keys.reduce((a, k) => (a | k.value) >>> 0, 0);
  const bits: number[] = [];
  for (let b = 0; b < 32; b++) if (hasBit(or, b)) bits.push(b);
  return bits;
}

// ---- apply: window edits --------------------------------------------------
function sortedInsert(keys: TimlKey[], k: TimlKey): void {
  const i = keys.findIndex(x => x.frame > k.frame);
  if (i < 0) keys.push(k); else keys.splice(i, 0, k);
}

function keyAt(keys: TimlKey[], f: number): TimlKey | undefined {
  return keys.find(k => k.frame === f);
}

function holdingKey(keys: TimlKey[], f: number): TimlKey | undefined {
  let hold: TimlKey | undefined;
  for (const k of keys) { if (k.frame <= f) hold = k; else break; }
  return hold ?? keys[0];
}

/** Ensure a key at exact frame f with value = valueFn. Clones the holding key
 *  (transition/typeTag/control points preserved) when inserting. */
export function upsertKey(xf: TimlTransform, f: number, valueFn: (v: number) => number): TimlKey {
  const existing = keyAt(xf.keys, f);
  if (existing) { existing.value = valueFn(existing.value) >>> 0; return existing; }
  const model = holdingKey(xf.keys, f);
  const k: TimlKey = model
    ? { ...model, frame: f, value: valueFn(model.value) >>> 0 }
    : makeKey(valueFn(0), xf.dataType, f, defaultTransitionFor(0, xf.hash));
  sortedInsert(xf.keys, k);
  return k;
}

/** Toggle `bit` across [f0, f1): boundary keys at f0 (on) / f1 (off), interior keys OR'd.
 *  Other bits are preserved on every touched key. Integer frames enforced. */
export function applyBitWindow(xf: TimlTransform, bit: number, f0: number, f1: number): void {
  if (!Number.isInteger(f0) || !Number.isInteger(f1) || f0 < 0 || f1 <= f0) {
    throw new Error(`非法窗口 [${f0}, ${f1}) — 需要整数帧且 f0 < f1`);
  }
  upsertKey(xf, f0, v => setBit(v, bit));
  upsertKey(xf, f1, v => clearBit(v, bit));
  for (const k of xf.keys) {
    if (k.frame > f0 && k.frame < f1) k.value = setBit(k.value, bit);
  }
  sortKeys(xf);
}

/** Clear `bit` inside [f0, f1) without touching boundaries (exclusivity helper). */
export function clearBitRange(xf: TimlTransform, bit: number, f0: number, f1: number): void {
  for (const k of xf.keys) {
    if (k.frame >= f0 && k.frame < f1) k.value = clearBit(k.value, bit);
  }
}

/** Attack window: full-value semantics on EventCollisionNN.mFlag (on=slot, off=0). */
export function applyAttackWindow(xf: TimlTransform, slot: number, f0: number, f1: number): void {
  if (!Number.isInteger(f0) || !Number.isInteger(f1) || f0 < 0 || f1 <= f0) {
    throw new Error(`非法攻击窗 [${f0}, ${f1})`);
  }
  if (slot < 0 || slot > 3) throw new Error(`非法槽位 ${slot} (0=off, 1/2/3=on)`);
  upsertKey(xf, f0, () => slot);
  upsertKey(xf, f1, () => 0);
  for (const k of xf.keys) {
    if (k.frame > f0 && k.frame < f1) k.value = slot;
  }
  sortKeys(xf);
}

export function sortKeys(xf: TimlTransform): void {
  xf.keys.sort((a, b) => a.frame - b.frame);
}

// ---- recipes --------------------------------------------------------------
export interface RecipeResult { messages: string[]; }

/** 无敌帧: b26 window + mTrigger 2048 gate; clears b28 inside the window (mutually exclusive). */
export function recipeIframes(block: TimlBlock, f0: number, f1: number): RecipeResult {
  const messages: string[] = [];
  const { xf } = ensureTransform(block, CAT.PlMotionCommon, MFLAG, PC_MFLAG_BASELINE);
  const b28 = findTransform(block, CAT.PlMotionCommon, MFLAG);
  applyBitWindow(xf, 26, f0, f1);
  if (b28 && b28.xf !== xf) {
    clearBitRange(b28.xf, 28, f0, f1);
    const had28 = bitSpans(b28.xf.keys, 28).length > 0;
    if (had28) messages.push('已在窗口内清除 b28 (格挡) — b26 与 b28 互斥');
  }
  const gate = ensureNoHitGate(block);
  if (gate) messages.push(`已写入无敌门 mTrigger 2048 @f${gate.frame}`);
  messages.push(`无敌窗 b26 [${f0}, ${f1}) = ${(f1 - f0)} 帧 ≈ ${((f1 - f0) / 60).toFixed(2)}s`);
  return { messages };
}

/** mTrigger 2048 gate: add if absent (position-free). Returns the key when created. */
export function ensureNoHitGate(block: TimlBlock, frame?: number): TimlKey | null {
  const { xf } = ensureTransform(block, CAT.PlMotionCommon, MTRIGGER);
  if (xf.keys.some(k => (k.value & TRIGGER_NOHIT_GATE) !== 0)) return null;
  const f = frame ?? (xf.keys.length ? Math.round(xf.keys[0].frame) : 0);
  const k = upsertKey(xf, f, v => (v | TRIGGER_NOHIT_GATE) >>> 0);
  sortKeys(xf);
  return k;
}

/** 霸体帧: b24 window (no trigger). */
export function recipeArmor(block: TimlBlock, f0: number, f1: number): RecipeResult {
  const { xf } = ensureTransform(block, CAT.PlMotionCommon, MFLAG, PC_MFLAG_BASELINE);
  applyBitWindow(xf, 24, f0, f1);
  return { messages: [`霸体窗 b24 [${f0}, ${f1}) = ${(f1 - f0)} 帧 ≈ ${((f1 - f0) / 60).toFixed(2)}s`] };
}

/** 防御帧: b28 directional-guard window; clears b26 inside (mutually exclusive, no trigger here). */
export function recipeGuard(block: TimlBlock, f0: number, f1: number): RecipeResult {
  const messages: string[] = [];
  const { xf } = ensureTransform(block, CAT.PlMotionCommon, MFLAG, PC_MFLAG_BASELINE);
  applyBitWindow(xf, 28, f0, f1);
  clearBitRange(xf, 26, f0, f1);
  messages.push(`格挡窗 b28 [${f0}, ${f1}) = ${(f1 - f0)} 帧 ≈ ${((f1 - f0) / 60).toFixed(2)}s (b26 已在窗内清除)`);
  return { messages };
}

/** 攻击帧: EventCollisionNN.mFlag window. collisionNn selects 00/01/02/03. */
export function recipeAttack(block: TimlBlock, collisionNn: number, slot: number, f0: number, f1: number): RecipeResult {
  const cat = [CAT.EventCollision00, CAT.EventCollision01, CAT.EventCollision02, CAT.EventCollision03][collisionNn] ?? CAT.EventCollision00;
  const { xf } = ensureTransform(block, cat, MFLAG, 0);
  applyAttackWindow(xf, slot, f0, f1);
  return { messages: [`攻击窗 EC${collisionNn.toString().padStart(2, '0')} slot ${slot} [${f0}, ${f1})` ] };
}

/** 按键: paint a bit window on a PlMotionInput track. track: 0=mFlag 1=mFlag1 2=mFlag2 */
export function recipeInputPaint(block: TimlBlock, track: number, bit: number, f0: number, f1: number, on: boolean): RecipeResult {
  const tfHash = [MFLAG, MFLAG1, MFLAG2][track] ?? MFLAG;
  const { xf } = ensureTransform(block, CAT.PlMotionInput, tfHash, 0);
  if (on) applyBitWindow(xf, bit, f0, f1);
  else {
    clearBitRange(xf, bit, f0, f1);
    // explicit close: vanilla convention adds a zero key at f1 when window ends there
    const hasLater = xf.keys.some(k => k.frame >= f1);
    if (!hasLater) upsertKey(xf, f1, v => clearBit(v, bit));
  }
  return { messages: [`输入位 ${trackLabel(track)}b${bit} [${f0}, ${f1}) ${on ? '开' : '关'}`] };
}

export function trackLabel(track: number): string {
  return ['mFlag', 'mFlag1', 'mFlag2'][track] ?? 'mFlag';
}
