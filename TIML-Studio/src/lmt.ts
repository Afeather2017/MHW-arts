// LMT container: header, entry table, 96-byte action headers, TIML linkage.
// Only TIML-related fields are interpreted; fcurves are carried through untouched.
import { parseTiml, TimlBlock, cloneBlock, blocksEqual, serializeTiml, validateBlock } from './timl';

export const ACTION_HEADER_SIZE = 96;
export const TIML_OFFSET_FIELD = 88;   // u64 @ +88
export const LOOPFRAME_FIELD = 16;     // i32 @ +16

export interface EntryRec {
  index: number;
  headerOff: number;        // absolute; 0 = no action
  frameCount: number;
  loopFrame: number;
  timl: TimlBlock | null;   // editable model
  timlOffset: number;       // current offset in buf (0 = none)
  dirty: boolean;
  dirtyHeader: boolean;     // loopFrame changed
}

export interface LmtDoc {
  name: string;
  buf: Uint8Array;
  version: number;
  entryCount: number;
  entries: (EntryRec | null)[];
  dirtyCount: number;
}

export function parseLmt(name: string, buf: Uint8Array): LmtDoc {
  const dv = new DataView(buf.buffer, buf.byteOffset, buf.byteLength);
  const sig = String.fromCharCode(buf[0], buf[1], buf[2], buf[3]);
  if (sig !== 'LMT\0') throw new Error(`不是 LMT 文件 (签名 ${JSON.stringify(sig)})`);
  const version = dv.getInt16(4, true);
  const entryCount = dv.getInt16(6, true);
  const entries: (EntryRec | null)[] = new Array(entryCount).fill(null);
  for (let i = 0; i < entryCount; i++) {
    const off = Number(dv.getBigUint64(16 + 8 * i, true));
    if (!off) continue;
    const frameCount = dv.getUint32(off + 12, true);
    const loopFrame = dv.getInt32(off + 16, true);
    const timlOffset = Number(dv.getBigUint64(off + TIML_OFFSET_FIELD, true));
    const timl = timlOffset ? parseTiml(dv, timlOffset) : null;
    entries[i] = {
      index: i, headerOff: off, frameCount, loopFrame,
      timl, timlOffset, dirty: false, dirtyHeader: false,
    };
  }
  return { name, buf, version, entryCount, entries, dirtyCount: 0 };
}

function dvOf(buf: Uint8Array): DataView {
  return new DataView(buf.buffer, buf.byteOffset, buf.byteLength);
}
void dvOf;

export interface SaveResult {
  buf: Uint8Array;
  rebuilt: number[];
  notes: string[];
}

/**
 * Build the output file: original bytes + canonical TIML blocks appended for
 * every dirty entry + repointed timlOffset / loopFrame fields.
 * Verifies by re-parsing the output; throws on any mismatch.
 */
export function buildSave(doc: LmtDoc): SaveResult {
  const notes: string[] = [];
  const rebuilt: number[] = [];
  const serialized: { e: EntryRec; ser: Uint8Array }[] = [];

  // pass 1: validate + serialize into fresh buffers, size the output
  let total = doc.buf.length;
  for (const e of doc.entries) {
    if (!e || !e.dirty) continue;
    if (!e.headerOff) throw new Error(`entry ${e.index}: 无动作头, 无法保存`);
    if (!e.timl) throw new Error(`entry ${e.index}: dirty 但无 TIML`);
    validateBlock(e.timl);
    const ser = serializeTiml(e.timl, 0); // placeholder base; rewritten with real base below
    serialized.push({ e, ser });
    total += ser.length + 16;
  }

  const out = new Uint8Array(total);
  out.set(doc.buf, 0);
  const odv = new DataView(out.buffer);
  let appendPos = alignUp16(doc.buf.length);

  for (const { e, ser } of serialized) {
    const real = serializeTiml(e.timl!, appendPos);
    out.set(real, appendPos);
    odv.setBigUint64(e.headerOff + TIML_OFFSET_FIELD, BigInt(appendPos), true);
    appendPos += real.length;
    rebuilt.push(e.index);
    notes.push(`entry ${e.index}: TIML 重建 @0x${appendPos.toString(16)}`);
  }
  for (const e of doc.entries) {
    if (!e || !e.dirtyHeader) continue;
    if (!e.headerOff) throw new Error(`entry ${e.index}: 无动作头, 无法保存`);
    odv.setInt32(e.headerOff + LOOPFRAME_FIELD, e.loopFrame, true);
    notes.push(`entry ${e.index}: loopFrame = ${e.loopFrame}`);
  }

  // verification pass: reparse the output and require semantic equality
  const check = parseLmt(doc.name, out);
  for (const e of doc.entries) {
    if (!e || !e.dirty) continue;
    const fresh = check.entries[e.index];
    if (!fresh || !fresh.timl || !e.timl) throw new Error(`entry ${e.index}: 保存校验失败 (回读为空)`);
    if (!blocksEqual(fresh.timl, e.timl)) {
      throw new Error(`entry ${e.index}: 保存校验失败 (回读内容与编辑模型不一致 — 已中止, 未写入任何文件)`);
    }
  }
  return { buf: out.subarray(0, appendPos), rebuilt, notes };
}

/** Install a SaveResult into the doc (models are replaced by the re-parse, verified equal). */
export function installSave(doc: LmtDoc, result: SaveResult): void {
  const fresh = parseLmt(doc.name, result.buf);
  for (const e of doc.entries) {
    if (!e) continue;
    const f = fresh.entries[e.index]!;
    e.timl = f.timl ? cloneBlock(f.timl) : null;
    e.timlOffset = f.timlOffset;
    e.loopFrame = f.loopFrame;
    e.frameCount = f.frameCount;
    e.dirty = false;
    e.dirtyHeader = false;
  }
  doc.buf = result.buf;
  doc.dirtyCount = 0;
}

export function markDirty(doc: LmtDoc, e: EntryRec): void {
  if (!e.dirty) { e.dirty = true; doc.dirtyCount++; }
}

export function markHeaderDirty(doc: LmtDoc, e: EntryRec): void {
  if (!e.dirtyHeader && !e.dirty) { e.dirtyHeader = true; doc.dirtyCount++; }
  else e.dirtyHeader = true;
}

function alignUp16(p: number): number { return p + ((16 - (p % 16)) % 16); }
