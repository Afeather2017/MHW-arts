// TIML block model, parser and canonical serializer.
//
// Layout (confirmed byte-level against w03_00-origin.lmt, 2026-10-15):
//   [pad16] TIML_Data(40B) [pad16]
//   all TIML_Type records packed (24B each)
//   [pad16]
//   per type: its TIML_Transform records packed (24B each), then [pad16]
//   per transform (type order): its key array (20B per key), then [pad16]
//   Every offset field is an ABSOLUTE file offset.
//   Key record: value(4) controlL(f32) controlR(f32) frame(f32) transition(i16) typeTag(i16).
//   The final i16 mirrors the transform dataType; preserved verbatim.

export const KEY_SIZE = 20;
export const TRANSFORM_RECORD = 24;
export const TYPE_RECORD = 24;
export const TIML_DATA_SIZE = 40;

export interface TimlKey {
  value: number;      // raw 32-bit pattern (u32 view; float/color packed)
  controlL: number;
  controlR: number;
  frame: number;      // f32, integer-valued in all vanilla data
  transition: number; // i16 (observed: 1 constant, 5 flag tracks, 6 trigger tracks)
  typeTag: number;    // i16, mirrors transform dataType
}

export interface TimlTransform {
  hash: number;
  dataType: number;   // 0=i32 1=u32 2=f32 3=color 4=bool
  keys: TimlKey[];
}

export interface TimlType {
  hash: number;
  unkn0: number;      // loader category dispatch id (PlMotionCommon=11, PlMotionInput=9, else 0)
  transforms: TimlTransform[];
}

export interface TimlBlock {
  dataIx0: number;
  dataIx1: number;
  animationLength: number;
  loopStartPoint: number;
  loopControl: number;
  labelHash: number;
  types: TimlType[];
}

function align16(p: number): number { return p + ((16 - (p % 16)) % 16); }

export function parseTiml(dv: DataView, blockOffset: number): TimlBlock {
  const typesOffset = Number(dv.getBigUint64(blockOffset, true));
  const typeCount = Number(dv.getBigUint64(blockOffset + 8, true));
  const block: TimlBlock = {
    dataIx0: dv.getInt32(blockOffset + 16, true),
    dataIx1: dv.getInt32(blockOffset + 20, true),
    animationLength: dv.getFloat32(blockOffset + 24, true),
    loopStartPoint: dv.getFloat32(blockOffset + 28, true),
    loopControl: dv.getInt32(blockOffset + 32, true),
    labelHash: dv.getUint32(blockOffset + 36, true),
    types: [],
  };
  if (!typesOffset || !typeCount) return block;
  for (let ti = 0; ti < typeCount; ti++) {
    const rec = typesOffset + TYPE_RECORD * ti;
    const xfOff = Number(dv.getBigUint64(rec, true));
    const xfCount = Number(dv.getBigUint64(rec + 8, true));
    const typ: TimlType = {
      hash: dv.getUint32(rec + 16, true),
      unkn0: dv.getInt32(rec + 20, true),
      transforms: [],
    };
    if (xfOff && xfCount) {
      for (let xi = 0; xi < xfCount; xi++) {
        const xrec = xfOff + TRANSFORM_RECORD * xi;
        const keyOff = Number(dv.getBigUint64(xrec, true));
        const keyCount = Number(dv.getBigUint64(xrec + 8, true));
        const xf: TimlTransform = {
          hash: dv.getUint32(xrec + 16, true),
          dataType: dv.getInt32(xrec + 20, true),
          keys: [],
        };
        if (keyOff && keyCount) {
          for (let k = 0; k < keyCount; k++) {
            const base = keyOff + KEY_SIZE * k;
            xf.keys.push({
              value: dv.getUint32(base, true),
              controlL: dv.getFloat32(base + 4, true),
              controlR: dv.getFloat32(base + 8, true),
              frame: dv.getFloat32(base + 12, true),
              transition: dv.getInt16(base + 16, true),
              typeTag: dv.getInt16(base + 18, true),
            });
          }
        }
        typ.transforms.push(xf);
      }
    }
    block.types.push(typ);
  }
  return block;
}

/** Serialize canonically. `base` = absolute offset where TIML_Data will land. */
export function serializeTiml(block: TimlBlock, base: number): Uint8Array {
  const nT = block.types.length;

  // pass 1: sizes / offsets (absolute)
  const typeOff: number[] = new Array(nT).fill(0);
  const xfOff: number[][] = block.types.map(() => [] as number[]);
  const keyOff: number[][] = block.types.map(() => [] as number[]);

  let pos = 0;
  pos = align16(pos);            // TIML_Data
  const dataRel = pos;
  pos += TIML_DATA_SIZE;
  pos = align16(pos);            // all type records
  for (let ti = 0; ti < nT; ti++) { typeOff[ti] = base + pos; pos += TYPE_RECORD; }
  pos = align16(pos);
  for (let ti = 0; ti < nT; ti++) {
    const t = block.types[ti];
    for (let xi = 0; xi < t.transforms.length; xi++) { xfOff[ti][xi] = base + pos; pos += TRANSFORM_RECORD; }
    pos = align16(pos);
  }
  for (let ti = 0; ti < nT; ti++) {
    const t = block.types[ti];
    for (let xi = 0; xi < t.transforms.length; xi++) {
      const keys = t.transforms[xi].keys;
      if (!keys.length) { keyOff[ti][xi] = 0; continue; }
      keyOff[ti][xi] = base + pos;
      pos += KEY_SIZE * keys.length;
      pos = align16(pos);
    }
  }

  const out = new Uint8Array(pos);
  const dv = new DataView(out.buffer);
  const firstTypeAbs = nT ? typeOff[0] : 0;
  dv.setBigUint64(dataRel, BigInt(firstTypeAbs), true);
  dv.setBigUint64(dataRel + 8, BigInt(nT), true);
  dv.setInt32(dataRel + 16, block.dataIx0, true);
  dv.setInt32(dataRel + 20, block.dataIx1, true);
  dv.setFloat32(dataRel + 24, block.animationLength, true);
  dv.setFloat32(dataRel + 28, block.loopStartPoint, true);
  dv.setInt32(dataRel + 32, block.loopControl, true);
  dv.setUint32(dataRel + 36, block.labelHash, true);

  for (let ti = 0; ti < nT; ti++) {
    const t = block.types[ti];
    const rec = typeOff[ti] - base;
    dv.setBigUint64(rec, BigInt(t.transforms.length ? xfOff[ti][0] : 0), true);
    dv.setBigUint64(rec + 8, BigInt(t.transforms.length), true);
    dv.setUint32(rec + 16, t.hash, true);
    dv.setInt32(rec + 20, t.unkn0, true);
    for (let xi = 0; xi < t.transforms.length; xi++) {
      const xf = t.transforms[xi];
      const xrec = xfOff[ti][xi] - base;
      dv.setBigUint64(xrec, BigInt(keyOff[ti][xi]), true);
      dv.setBigUint64(xrec + 8, BigInt(xf.keys.length), true);
      dv.setUint32(xrec + 16, xf.hash, true);
      dv.setInt32(xrec + 20, xf.dataType, true);
      for (let k = 0; k < xf.keys.length; k++) {
        const key = xf.keys[k];
        const b = keyOff[ti][xi] - base + KEY_SIZE * k;
        dv.setUint32(b, key.value, true);
        dv.setFloat32(b + 4, key.controlL, true);
        dv.setFloat32(b + 8, key.controlR, true);
        dv.setFloat32(b + 12, key.frame, true);
        dv.setInt16(b + 16, key.transition, true);
        dv.setInt16(b + 18, key.typeTag, true);
      }
    }
  }
  return out;
}

// ---- deep compare / clone (verification + undo) -------------------------

export function cloneBlock(b: TimlBlock): TimlBlock {
  return {
    ...b,
    types: b.types.map(t => ({
      ...t,
      transforms: t.transforms.map(xf => ({ ...xf, keys: xf.keys.map(k => ({ ...k })) })),
    })),
  };
}

export function keysEqual(a: TimlKey[], b: TimlKey[]): boolean {
  if (a.length !== b.length) return false;
  for (let i = 0; i < a.length; i++) {
    const x = a[i], y = b[i];
    if (x.value !== y.value || x.controlL !== y.controlL || x.controlR !== y.controlR ||
        x.frame !== y.frame || x.transition !== y.transition || x.typeTag !== y.typeTag) return false;
  }
  return true;
}

export function blocksEqual(a: TimlBlock, b: TimlBlock): boolean {
  return a.dataIx0 === b.dataIx0 && a.dataIx1 === b.dataIx1 &&
    a.animationLength === b.animationLength && a.loopStartPoint === b.loopStartPoint &&
    a.loopControl === b.loopControl && a.labelHash === b.labelHash &&
    a.types.length === b.types.length &&
    a.types.every((t, i) => t.hash === b.types[i].hash && t.unkn0 === b.types[i].unkn0 &&
      t.transforms.length === b.types[i].transforms.length &&
      t.transforms.every((xf, j) => xf.hash === b.types[i].transforms[j].hash &&
        xf.dataType === b.types[i].transforms[j].dataType &&
        keysEqual(xf.keys, b.types[i].transforms[j].keys)));
}

/** Sanity checks before serialize. Throws with a human-readable reason. */
export function validateBlock(block: TimlBlock): void {
  for (const t of block.types) {
    for (const xf of t.transforms) {
      let prev = -Infinity;
      for (const k of xf.keys) {
        if (!Number.isFinite(k.frame)) throw new Error(`track ${xf.hash.toString(16)}: 非法帧值 ${k.frame}`);
        if (k.frame < 0) throw new Error(`track ${xf.hash.toString(16)}: 负帧 ${k.frame}`);
        if (!Number.isInteger(k.frame)) throw new Error(`track ${xf.hash.toString(16)}: 小数帧 ${k.frame} (整数帧强制 — 小数帧是已知崩溃源)`);
        if (k.frame === prev) throw new Error(`track ${xf.hash.toString(16)}: 重复帧 ${k.frame}`);
        if (k.frame < prev) throw new Error(`track ${xf.hash.toString(16)}: 键未按帧排序 @${k.frame}`);
        prev = k.frame;
      }
    }
  }
}
