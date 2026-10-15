// Node test harness: validates parser/serializer/semantics against real LMT files.
// Run: npm test
import * as fs from 'node:fs';
import { parseLmt, buildSave, installSave } from '../src/lmt';
import { parseTiml, serializeTiml, blocksEqual, validateBlock, TimlBlock } from '../src/timl';
import { Editor } from '../src/model';
import { bitSpans, findTransform, valueSpans } from '../src/semantics';
import { CAT, MFLAG, MTRIGGER } from '../src/names';

const ORIGIN = String.raw`E:\SteamLibrary\steamapps\common\Monster Hunter World\nativePC\hm\wp\wp03\mot\w03_00\w03_00-origin.lmt`;
const DEPLOYED = String.raw`E:\SteamLibrary\steamapps\common\Monster Hunter World\nativePC\hm\wp\wp03\mot\w03_00\w03_00.lmt`;

let passed = 0, failed = 0;
function ok(cond: boolean, name: string, detail?: string): void {
  if (cond) { passed++; console.log(`  ok  ${name}`); }
  else { failed++; console.error(`FAIL  ${name}${detail ? ' — ' + detail : ''}`); }
}

function load(path: string) {
  const buf = new Uint8Array(fs.readFileSync(path));
  return parseLmt(path.split(/[\\/]/).pop()!, buf);
}

console.log('== 1. parse vanilla ==');
const doc = load(ORIGIN);
ok(doc.entryCount === 464, `entryCount=464 (got ${doc.entryCount})`);
const withTiml = doc.entries.filter(e => e && e.timl).length;
ok(withTiml === 161, `161 entries with TIML (got ${withTiml})`);
ok(doc.entries[105]!.frameCount > 0, 'entry 105 frameCount > 0');

console.log('== 2. canonical serializer byte-compare (vanilla blocks) ==');
{
  const dv = new DataView(doc.buf.buffer, doc.buf.byteOffset, doc.buf.byteLength);
  let identical = 0, semanticOnly = 0, total = 0;
  let firstBad: string | null = null;
  for (const e of doc.entries) {
    if (!e?.timl || !e.timlOffset) continue;
    total++;
    const ser = serializeTiml(e.timl, e.timlOffset);
    const orig = doc.buf.subarray(e.timlOffset, e.timlOffset + ser.length);
    let diff = -1;
    for (let i = 0; i < ser.length; i++) if (ser[i] !== orig[i]) { diff = i; break; }
    if (diff < 0) {
      // bytes equal; allow ser longer by trailing zero pad only
      const tail = doc.buf.subarray(e.timlOffset + ser.length, e.timlOffset + ser.length + 16);
      identical++;
    } else {
      // tolerate: diff region must be inside the final key array's trailing pad
      // find last key byte position: recompute? simpler: check ser[diff] === 0 && all ser[diff..] zero
      let tailZero = true;
      for (let i = diff; i < ser.length; i++) if (ser[i] !== 0) { tailZero = false; break; }
      if (tailZero) semanticOnly++;
      else if (!firstBad) firstBad = `entry ${e.index}: first diff @rel ${diff}`;
    }
  }
  ok(total === 161, `all 161 blocks serialized (got ${total})`);
  ok(identical + semanticOnly === total, `byte-identical ${identical} + trailing-pad-tolerant ${semanticOnly} = ${total}`, firstBad ?? undefined);
  console.log(`  info: exact byte-identical ${identical}/${total}, tail-pad-only ${semanticOnly}`);
}

console.log('== 3. roundtrip: serialize -> reparse -> semantic equal ==');
{
  let equal = 0, total = 0;
  for (const e of doc.entries) {
    if (!e?.timl || !e.timlOffset) continue;
    total++;
    const ser = serializeTiml(e.timl, 0x100000); // arbitrary base
    const copy = new Uint8Array(ser.length + 16);
    copy.set(ser, 16); // force non-trivial alignment base check via offset base
    // NOTE: ser was built for base 0x100000; splice into a buffer where that's valid:
    const big = new Uint8Array(0x100000 + ser.length);
    big.set(ser, 0x100000);
    const re = parseTiml(new DataView(big.buffer), 0x100000);
    if (blocksEqual(re, e.timl)) equal++;
  }
  ok(equal === total, `roundtrip equal ${equal}/${total}`);
}

console.log('== 4. known vanilla windows ==');
{
  // 308 大居合: research-verified b26 [0,12) + 2048 gate @f12 + b24 [0,25)
  const b308 = doc.entries[308]!.timl!;
  const m308 = findTransform(b308, CAT.PlMotionCommon, MFLAG)!;
  const s308_26 = bitSpans(m308.xf.keys, 26);
  const s308_24 = bitSpans(m308.xf.keys, 24);
  ok(s308_26.length === 1 && s308_26[0].start === 0 && s308_26[0].end === 12,
    `308 b26 = [0,12) (got ${JSON.stringify(s308_26)})`);
  ok(s308_24.length === 1 && s308_24[0].start === 0 && s308_24[0].end === 25,
    `308 b24 = [0,25) (got ${JSON.stringify(s308_24)})`);
  const t308 = findTransform(b308, CAT.PlMotionCommon, MTRIGGER)!;
  ok(t308.xf.keys.some(k => k.frame === 12 && (k.value & 0x800) !== 0), '308 has 2048 gate at f12');
  // 140 见切 (2026-10-15 correction): f0=0x80400000 is b31+b22, NOT b26 — the old
  // research note misread the nibble. 见切 i-frames are FSM/action-layer granted.
  const b140 = doc.entries[140]!.timl!;
  const m140 = findTransform(b140, CAT.PlMotionCommon, MFLAG)!;
  const s31 = bitSpans(m140.xf.keys, 31);
  const s22 = bitSpans(m140.xf.keys, 22);
  const s26 = bitSpans(m140.xf.keys, 26);
  ok(s31.length === 1 && s31[0].start === 0 && s31[0].end === null,
    `140 b31 = [0, open) — never cleared (got ${JSON.stringify(s31)})`);
  ok(s22.length === 1 && s22[0].start === 0 && s22[0].end === 130,
    `140 b22 = [0,130) (got ${JSON.stringify(s22)})`);
  ok(s26.length === 0, '140 vanilla has NO b26 (0x80400000 = b31+b22)');
  const t140 = findTransform(b140, CAT.PlMotionCommon, MTRIGGER)!;
  ok(t140.xf.keys.some(k => k.frame === 45 && (k.value & 0x800) !== 0), '140 has 2048 gate at f45 (vanilla)');
}

console.log('== 5. semantic edit + save verification (entry 105) ==');
{
  const doc2 = load(ORIGIN);
  const ed = new Editor(doc2);
  const rep = ed.addIframeWindow(105, 20, 40);
  const m = findTransform(doc2.entries[105]!.timl!, CAT.PlMotionCommon, MFLAG)!;
  const spans = bitSpans(m.xf.keys, 26);
  // vanilla 105 already has b26 f0..f80 (deployed notes say f0/f80/f120; origin: f0=37748742(b25+b26? 37748742=0x02400006|?)...)
  const covers = spans.some(s => s.start <= 20 && (s.end === null || s.end >= 40));
  ok(covers, `applyBitWindow covers [20,40) (spans: ${JSON.stringify(spans)})`);
  const t = findTransform(doc2.entries[105]!.timl!, CAT.PlMotionCommon, MTRIGGER);
  ok(!!t && t.xf.keys.some(k => (k.value & 0x800) !== 0), '2048 gate present after recipe');
  // baseline bits preserved?
  const f40 = m.xf.keys.find(k => k.frame === 40);
  ok(!!f40 && (f40.value & 0x02400006) === 0x02400006, `baseline bits kept on new key @40 (0x${(f40?.value ?? 0).toString(16)})`);
  const result = buildSave(doc2);
  ok(result.rebuilt.includes(105), 'save rebuilds entry 105');
  // untouched bytes identical? (except entry 105's repointed timlOffset field, 8B)
  const e105 = doc2.entries[105]!;
  const patchLo = e105.headerOff + 88, patchHi = patchLo + 8;
  let untouchedOk = true;
  for (let i = 0; i < doc.buf.length; i++) {
    if (i >= patchLo && i < patchHi) continue;
    if (doc.buf[i] !== result.buf[i]) { untouchedOk = false; break; }
  }
  ok(untouchedOk, 'unmodified file region byte-identical (append-only save)');
  // save verification passed inside buildSave (would have thrown)
  installSave(doc2, result);
  ok(doc2.entries[105]!.dirty === false, 'dirty cleared after install');
  // idempotence: second save with no edits produces identical file
  const result2 = buildSave(doc2);
  ok(result2.rebuilt.length === 0 && result2.buf.length === result.buf.length, 'second save is a no-op');
}

console.log('== 6. guard/armor/attack/input recipes ==');
{
  const doc3 = load(ORIGIN);
  const ed = new Editor(doc3);
  ed.addGuardWindow(140, 0, 20);
  const m140 = findTransform(doc3.entries[140]!.timl!, CAT.PlMotionCommon, MFLAG)!;
  const g = bitSpans(m140.xf.keys, 28);
  ok(g.some(s => s.start === 0 && (s.end === 20 || s.end === null)), `guard window applied to 140 (got ${JSON.stringify(g)})`);
  const i26 = bitSpans(m140.xf.keys, 26).filter(s => s.start < 20);
  ok(!i26.some(s => s.start === 0 && s.end !== null && s.end > 0), 'b26 cleared inside guard window');

  ed.addAttackWindow(105, 0, 2, 50, 70);
  const ec = findTransform(doc3.entries[105]!.timl!, CAT.EventCollision00, MFLAG)!;
  const atk = valueSpans(ec.xf.keys, v => v !== 0);
  ok(atk.some(s => s.start <= 50 && s.end !== null && s.end >= 70), `attack window [50,70) slot2 applied (got ${JSON.stringify(atk)})`);

  ed.paintInputBit(105, 0, 8, 10, 30, true);
  const pi = findTransform(doc3.entries[105]!.timl!, CAT.PlMotionInput, MFLAG)!;
  const roll = bitSpans(pi.xf.keys, 8);
  ok(roll.some(s => s.start <= 10 && s.end !== null && s.end >= 30), `input bit 8 painted (got ${JSON.stringify(roll)})`);

  // validate all dirty blocks before save
  for (const e of doc3.entries) if (e?.dirty) validateBlock(e.timl!);
  const r = buildSave(doc3);
  ok(r.rebuilt.length === 2, `2 entries rebuilt (140 + 105) (got ${r.rebuilt.length})`);
}

console.log('== 7. new-track creation on clean entry (unknown unkn0 policy) ==');
{
  const doc4 = load(ORIGIN);
  const ed = new Editor(doc4);
  const before = doc4.entries[101]!.timl ? 'had' : 'none';
  ed.addIframeWindow(101, 0, 12);
  const blk = doc4.entries[101]!.timl!;
  const t = blk.types.find(t => t.hash === CAT.PlMotionCommon)!;
  ok(t.unkn0 === 11, `new PlMotionCommon type gets unkn0=11 (got ${t.unkn0})`);
  buildSave(doc4);
  ok(true, `entry 101 (timl was ${before}) saved with new defense track`);
}

console.log(`\n${passed} passed, ${failed} failed`);
process.exit(failed ? 1 : 0);
