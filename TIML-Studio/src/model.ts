// Editor state: doc + undo/redo + feature operations (each op = undo snapshot + mutate + markDirty).
import { LmtDoc, EntryRec, buildSave, installSave, markDirty, markHeaderDirty, SaveResult } from './lmt';
import { TimlBlock, TimlKey, cloneBlock } from './timl';
import {
  CAT, MFLAG, MFLAG1, MFLAG2, MTRIGGER,
} from './names';
import {
  recipeIframes, recipeArmor, recipeGuard, recipeAttack, recipeInputPaint,
  findTransform, ensureTransform, applyBitWindow, clearBitRange, applyAttackWindow,
  upsertKey, sortKeys, ensureNoHitGate, PC_MFLAG_BASELINE,
} from './semantics';

export interface KeyRef { ti: number; xi: number; ki: number; }

interface Memento {
  entryIndex: number;
  timl: TimlBlock | null;
  loopFrame: number;
  label: string;
}

export interface OpReport { messages: string[]; }

export class Editor {
  doc: LmtDoc;
  undoStack: Memento[] = [];
  redoStack: Memento[] = [];
  statusLine = '';

  constructor(doc: LmtDoc) {
    this.doc = doc;
  }

  get dirty(): boolean { return this.doc.dirtyCount > 0; }

  private entry(i: number): EntryRec {
    const e = this.doc.entries[i];
    if (!e) throw new Error(`entry ${i} 不存在`);
    return e;
  }

  private snapshot(e: EntryRec, label: string): void {
    this.undoStack.push({
      entryIndex: e.index,
      timl: e.timl ? cloneBlock(e.timl) : null,
      loopFrame: e.loopFrame,
      label,
    });
    if (this.undoStack.length > 200) this.undoStack.shift();
    this.redoStack = [];
  }

  undo(): string | null {
    const m = this.undoStack.pop();
    if (!m) return null;
    const e = this.entry(m.entryIndex);
    // redo captures current state
    this.redoStack.push({ entryIndex: e.index, timl: e.timl ? cloneBlock(e.timl) : null, loopFrame: e.loopFrame, label: m.label });
    if (m.timl && !e.timl) markDirty(this.doc, e);
    e.timl = m.timl ? cloneBlock(m.timl) : null;
    if (e.loopFrame !== m.loopFrame) {
      e.loopFrame = m.loopFrame;
      markHeaderDirty(this.doc, e);
    }
    markDirty(this.doc, e);
    return `${m.entryIndex}: ${m.label}`;
  }

  redo(): string | null {
    const m = this.redoStack.pop();
    if (!m) return null;
    const e = this.entry(m.entryIndex);
    this.undoStack.push({ entryIndex: e.index, timl: e.timl ? cloneBlock(e.timl) : null, loopFrame: e.loopFrame, label: m.label });
    e.timl = m.timl ? cloneBlock(m.timl) : null;
    e.loopFrame = m.loopFrame;
    markDirty(this.doc, e);
    return m.label;
  }

  // ---- feature operations -------------------------------------------------

  addIframeWindow(entryIndex: number, f0: number, f1: number): OpReport {
    const e = this.entry(entryIndex);
    this.snapshot(e, `无敌窗 [${f0},${f1})`);
    e.timl = e.timl ?? newBlock();
    markDirty(this.doc, e);
    return recipeIframes(e.timl, f0, f1);
  }

  addArmorWindow(entryIndex: number, f0: number, f1: number): OpReport {
    const e = this.entry(entryIndex);
    this.snapshot(e, `霸体窗 [${f0},${f1})`);
    e.timl = e.timl ?? newBlock();
    markDirty(this.doc, e);
    return recipeArmor(e.timl, f0, f1);
  }

  addGuardWindow(entryIndex: number, f0: number, f1: number): OpReport {
    const e = this.entry(entryIndex);
    this.snapshot(e, `格挡窗 [${f0},${f1})`);
    e.timl = e.timl ?? newBlock();
    markDirty(this.doc, e);
    return recipeGuard(e.timl, f0, f1);
  }

  addAttackWindow(entryIndex: number, collisionNn: number, slot: number, f0: number, f1: number): OpReport {
    const e = this.entry(entryIndex);
    this.snapshot(e, `攻击窗 EC${collisionNn} slot${slot} [${f0},${f1})`);
    e.timl = e.timl ?? newBlock();
    markDirty(this.doc, e);
    return recipeAttack(e.timl, collisionNn, slot, f0, f1);
  }

  paintInputBit(entryIndex: number, track: number, bit: number, f0: number, f1: number, on: boolean): OpReport {
    const e = this.entry(entryIndex);
    this.snapshot(e, `输入 ${track}b${bit} [${f0},${f1})=${on ? 1 : 0}`);
    e.timl = e.timl ?? newBlock();
    markDirty(this.doc, e);
    return recipeInputPaint(e.timl, track, bit, f0, f1, on);
  }

  addGateKey(entryIndex: number, frame: number): OpReport {
    const e = this.entry(entryIndex);
    this.snapshot(e, `无敌门 2048 @f${frame}`);
    e.timl = e.timl ?? newBlock();
    const k = ensureNoHitGate(e.timl, frame);
    markDirty(this.doc, e);
    return { messages: [k ? `已加 mTrigger 2048 @f${k.frame}` : '已存在 2048 门键, 未重复添加'] };
  }

  // ---- raw key operations ---------------------------------------------------

  private ref(entryIndex: number, ref: KeyRef): { e: EntryRec; key: TimlKey } {
    const e = this.entry(entryIndex);
    if (!e.timl) throw new Error('无 TIML');
    const key = e.timl.types[ref.ti]?.transforms[ref.xi]?.keys[ref.ki];
    if (!key) throw new Error('键引用失效');
    return { e, key };
  }

  patchKey(entryIndex: number, ref: KeyRef, patch: Partial<TimlKey>, label: string): OpReport {
    const { e, key } = this.ref(entryIndex, ref);
    this.snapshot(e, label);
    Object.assign(key, patch);
    if (patch.frame !== undefined) {
      const xf = e.timl!.types[ref.ti].transforms[ref.xi];
      sortKeys(xf);
      // ki may have shifted after sort; re-locate by identity is impossible on plain objects —
      // caller re-resolves via frame. Acceptable for UI (inspector re-renders from model).
    }
    markDirty(this.doc, e);
    return { messages: [label] };
  }

  deleteKey(entryIndex: number, ref: KeyRef): OpReport {
    const { e } = this.ref(entryIndex, ref);
    this.snapshot(e, '删除键');
    e.timl!.types[ref.ti].transforms[ref.xi].keys.splice(ref.ki, 1);
    markDirty(this.doc, e);
    return { messages: ['已删除键'] };
  }

  addKey(entryIndex: number, ti: number, xi: number, frame: number, value: number): OpReport {
    const e = this.entry(entryIndex);
    if (!e.timl) throw new Error('无 TIML');
    this.snapshot(e, `加键 f${frame}`);
    const xf = e.timl.types[ti].transforms[xi];
    upsertKey(xf, frame, () => value);
    sortKeys(xf);
    markDirty(this.doc, e);
    return { messages: [`已在 f${frame} 加键 (值 ${value})`] };
  }

  clearTrack(entryIndex: number, ti: number, xi: number): OpReport {
    const e = this.entry(entryIndex);
    if (!e.timl) throw new Error('无 TIML');
    this.snapshot(e, '清空轨道');
    e.timl.types[ti].transforms[xi].keys = [];
    markDirty(this.doc, e);
    return { messages: ['轨道已清空 (保存时该轨写为空轨 offset=0)'] };
  }

  /** Create the standard PlMotionCommon.mFlag track pre-seeded with the vanilla baseline. */
  createDefenseTrack(entryIndex: number): OpReport {
    const e = this.entry(entryIndex);
    this.snapshot(e, '新建 PlMotionCommon/mFlag');
    e.timl = e.timl ?? newBlock();
    const r = ensureTransform(e.timl, CAT.PlMotionCommon, MFLAG, PC_MFLAG_BASELINE);
    markDirty(this.doc, e);
    return { messages: [`已建 PlMotionCommon.mFlag (基线 0x${PC_MFLAG_BASELINE.toString(16)}) @type${r.ti}`] };
  }

  setLoopFrame(entryIndex: number, lf: number): OpReport {
    const e = this.entry(entryIndex);
    this.snapshot(e, `loopFrame=${lf}`);
    e.loopFrame = lf;
    markHeaderDirty(this.doc, e);
    return { messages: [`loopFrame = ${lf}`] };
  }

  // ---- save --------------------------------------------------------------

  buildSave(): SaveResult {
    return buildSave(this.doc);
  }

  installSave(result: SaveResult): void {
    installSave(this.doc, result);
    this.undoStack = [];
    this.redoStack = [];
  }
}

export function newBlock(): TimlBlock {
  return {
    dataIx0: 0, dataIx1: 0,
    animationLength: 0, loopStartPoint: -1, loopControl: 0, labelHash: 0,
    types: [],
  };
}

// re-export for UI convenience
export { findTransform, applyBitWindow, clearBitRange, applyAttackWindow, CAT, MFLAG, MFLAG1, MFLAG2, MTRIGGER };
