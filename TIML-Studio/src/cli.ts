// CLI: per-entry TIML diff between two LMT files + feature-window report.
// Built with the same parser the test suite byte-verified against vanilla.
// Usage: node timl-diff.js <a.lmt> <b.lmt> [--names] [--entry 140 ...]
import * as fs from 'fs';
import { parseLmt } from './lmt';
import { CAT, MFLAG, MTRIGGER, MFLAG1, MFLAG2, TIMELINE_PARAM_NAMES, LS_ENTRY_NAMES, transformName, piBitLabel, pcBitLabel } from './names';
import { bitSpans, valueSpans, varyingBits } from './semantics';

function load(path: string) {
  const buf = new Uint8Array(fs.readFileSync(path));
  return parseLmt(path, buf);
}

type Entry = NonNullable<ReturnType<typeof parseLmt>['entries'][number]>;
type Timl = NonNullable<Entry['timl']>;

function cat(hash: number): string {
  return TIMELINE_PARAM_NAMES.get(hash) ?? `unk_${hash.toString(16).toUpperCase()}`;
}
function hex(v: number): string { return '0x' + (v >>> 0).toString(16).toUpperCase(); }

interface Window { label: string; start: number; end: number | null; extra?: string }

function featureWindows(timl: Timl | null): Window[] {
  if (!timl) return [];
  const out: Window[] = [];
  timl.types.forEach(t => {
    const cn = cat(t.hash);
    t.transforms.forEach(xf => {
      const tn = transformName(xf.hash);
      if (cn === 'nTimelineParam::PlMotionCommon' && xf.hash === MFLAG) {
        const known: [number, string][] = [[26, '无敌'], [24, '霸体'], [28, '防御格挡'], [25, 'b25'], [22, 'b22'], [31, 'b31?'], [27, 'b27?'], [15, 'b15']];
        for (const [bit, label] of known) {
          for (const s of bitSpans(xf.keys, bit)) {
            out.push({ label: `PC.${tn} ${label}(b${bit})`, start: s.start, end: s.end });
          }
        }
      }
      if (cn === 'nTimelineParam::PlMotionCommon' && (xf.hash === MFLAG1 || xf.hash === MFLAG2)) {
        const tn2 = xf.hash === MFLAG1 ? 'mFlag1' : 'mFlag2';
        for (const bit of varyingBits(xf.keys)) {
          for (const s of bitSpans(xf.keys, bit)) {
            out.push({ label: `PC.${tn2} ${xf.hash === MFLAG1 ? '零伤格挡?' : '事件位写入?'}(b${bit})`, start: s.start, end: s.end });
          }
        }
      }
      if (cn === 'nTimelineParam::PlMotionCommon' && xf.hash === MTRIGGER) {
        for (const k of xf.keys) {
          if (k.value === 0) continue;
          if (k.value === 2048) out.push({ label: 'PC.mTrigger 2048门', start: k.frame, end: k.frame });
          else out.push({ label: `PC.mTrigger 值${k.value}${(k.value & 0x800) ? '(含门)' : ''}`, start: k.frame, end: k.frame });
        }
      }
      if (cn.startsWith('nTimelineParam::nWwiseTimeline::EventCollision') && xf.hash === MFLAG) {
        for (const s of valueSpans(xf.keys, v => v !== 0)) {
          const slot = xf.keys.find(k => k.frame === s.start)?.value ?? 0;
          out.push({ label: `${cn.split('::').pop()} 攻击窗`, start: s.start, end: s.end, extra: `slot${slot}` });
        }
      }
      if (cn === 'nTimelineParam::PlMotionInput' && (xf.hash === MFLAG || xf.hash === MFLAG1 || xf.hash === MFLAG2)) {
        for (const bit of varyingBits(xf.keys)) {
          for (const s of bitSpans(xf.keys, bit)) {
            out.push({ label: `PI.${tn} ${piBitLabel(xf.hash, bit)}`, start: s.start, end: s.end });
          }
        }
      }
    });
  });
  return out.sort((a, b) => a.label.localeCompare(b.label) || a.start - b.start);
}

function windowKey(w: Window): string {
  return `${w.label}[${w.start},${w.end === null ? 'open' : w.end})${w.extra ? '@' + w.extra : ''}`;
}

function main(): number {
  const args = process.argv.slice(2);
  const paths: string[] = [];
  const entryFilter = new Set<number>();
  for (let i = 0; i < args.length; i++) {
    if (args[i] === '--entry') {
      let j = i + 1;
      while (j < args.length && !args[j].startsWith('--')) { entryFilter.add(Number(args[j])); j++; }
      i = j - 1;
    } else if (!args[i].startsWith('--')) paths.push(args[i]);
  }
  if (paths.length < 2) {
    console.error('用法: node timl-diff.js <a.lmt> <b.lmt> [--entry N ...]   (N 可多个直到下一个 -- 参数)');
    return 2;
  }
  const A = load(paths[0]), B = load(paths[1]);
  const nameOf = (i: number) => LS_ENTRY_NAMES.get(i) ?? '';
  console.log(`A = ${A.name} (${A.entries.filter(e => e?.timl).length} TIML entries)`);
  console.log(`B = ${B.name} (${B.entries.filter(e => e?.timl).length} TIML entries)`);

  const allIdx = new Set<number>();
  A.entries.forEach(e => { if (e) allIdx.add(e.index); });
  B.entries.forEach(e => { if (e) allIdx.add(e.index); });

  let changed = 0;
  const onlyA: number[] = [], onlyB: number[] = [];
  const diffs: string[] = [];
  for (const i of [...allIdx].sort((x, y) => x - y)) {
    if (entryFilter.size && !entryFilter.has(i)) continue;
    const ea = A.entries[i], eb = B.entries[i];
    if (ea && !eb) { onlyA.push(i); continue; }
    if (!ea && eb) { onlyB.push(i); continue; }
    if (!ea || !eb) continue;
    const wa = featureWindows(ea.timl), wb = featureWindows(eb.timl);
    const ka = new Map(wa.map(w => [windowKey(w), w]));
    const kb = new Map(wb.map(w => [windowKey(w), w]));
    const removed = wa.filter(w => !kb.has(windowKey(w)));
    const added = wb.filter(w => !ka.has(windowKey(w)));
    // non-feature raw track diff (values/frames per category/transform)
    const rawDiff = rawTrackDiff(ea.timl, eb.timl);
    const hdrDiff = headerDiff(ea, eb);
    if (removed.length || added.length || rawDiff.length || hdrDiff.length) {
      changed++;
      diffs.push(`entry ${i}${nameOf(i) ? ' ' + nameOf(i) : ''}:`);
      if (hdrDiff.length) hdrDiff.forEach(d => diffs.push('    ' + d));
      removed.forEach(w => diffs.push(`    - ${windowKey(w)}`));
      added.forEach(w => diffs.push(`    + ${windowKey(w)}`));
      rawDiff.forEach(d => diffs.push('    ~ ' + d));
    }
  }
  console.log(`\n== 仅 A 有 entry: ${onlyA.join(', ') || '无'}`);
  console.log(`== 仅 B 有 entry: ${onlyB.join(', ') || '无'}`);
  console.log(`== TIML 有差异的 entry: ${changed}\n`);
  diffs.forEach(l => console.log(l));
  return 0;
}

function headerDiff(ea: Entry, eb: Entry): string[] {
  const out: string[] = [];
  if (ea.frameCount !== eb.frameCount) out.push(`frameCount ${ea.frameCount} -> ${eb.frameCount}`);
  if (ea.loopFrame !== eb.loopFrame) out.push(`loopFrame ${ea.loopFrame} -> ${eb.loopFrame}`);
  const ta = ea.timl, tb = eb.timl;
  if (!!ta !== !!tb) { out.push(`TIML ${ta ? '有' : '无'} -> ${tb ? '有' : '无'}`); return out; }
  if (ta && tb) {
    if (ta.animationLength !== tb.animationLength) out.push(`animationLength ${ta.animationLength} -> ${tb.animationLength}`);
    if (ta.loopStartPoint !== tb.loopStartPoint) out.push(`loopStartPoint ${ta.loopStartPoint} -> ${tb.loopStartPoint}`);
    if (ta.loopControl !== tb.loopControl) out.push(`loopControl ${ta.loopControl} -> ${tb.loopControl}`);
  }
  return out;
}

function fmtKey(k: { frame: number; value: number }): string {
  return `f${Math.round(k.frame)}=${hex(k.value)}`;
}

function rawTrackDiff(a: Timl | null, b: Timl | null): string[] {
  if (!a || !b) return [];
  const out: string[] = [];
  const keyOf = (ch: number, th: number) => `${hex(ch)}.${transformName(th)}(${hex(th)})`;
  const collect = (t: Timl) => {
    const m = new Map<string, string[]>();
    t.types.forEach((typ, ci) => typ.transforms.forEach(xf => {
      const k = keyOf(typ.hash, xf.hash) + `#${ci}`;
      m.set(k, xf.keys.map(fmtKey));
    }));
    return m;
  };
  const ma = collect(a), mb = collect(b);
  for (const [k, va] of ma) {
    const vb = mb.get(k);
    if (!vb) { out.push(`${k}: 轨道仅A有 (${va.length}键)`); continue; }
    if (va.join(' ') !== vb.join(' ')) out.push(`${k}: ${va.join(' ')}  =>  ${vb.join(' ')}`);
  }
  for (const [k, vb] of mb) if (!ma.has(k)) out.push(`${k}: 轨道仅B有 (${vb.length}键)`);
  return out;
}

process.exit(main());
