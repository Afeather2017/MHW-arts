# -*- coding: utf-8 -*-
"""Find TIML datatype/timeline-parameter hashes in an LMT that the FreeKinetics
hash tables cannot resolve (the 'int' object has no attribute split' importer
crash). Standalone: imports the addon's struct/common modules via a synthetic
package so bpy is never touched."""
import sys, types

# --- stub Blender's mathutils with a minimal 4-component vector (only the
# arithmetic LmtBuffers' keyframe decoding needs; TIML parsing never runs it) ---
class _V:
    __slots__ = ("d",)
    def __init__(self, vals): self.d = [float(v) for v in vals]
    def _op(self, o, f):
        if isinstance(o, _V): return _V([f(a, b) for a, b in zip(self.d, o.d)])
        return _V([f(a, o) for a in self.d])
    def __mul__(self, o): return self._op(o, lambda a, b: a * b)
    __rmul__ = __mul__
    def __add__(self, o): return self._op(o, lambda a, b: a + b)
    __radd__ = __add__
    def __sub__(self, o): return self._op(o, lambda a, b: a - b)
    def __truediv__(self, o): return self._op(o, lambda a, b: a / b)
    def __getitem__(self, i): return self.d[i]
    def __setitem__(self, i, v): self.d[i] = float(v)
    def __len__(self): return len(self.d)
    def __iter__(self): return iter(self.d)
    def freeze(self): return self

_mu = types.ModuleType("mathutils")
_mu.Vector = _V
sys.modules["mathutils"] = _mu

ADDON = r"C:\Users\Afeather\AppData\Roaming\Blender Foundation\Blender\4.3\scripts\addons\MHW-FreeKinetics"
pkg = types.ModuleType("fk")
pkg.__path__ = [ADDON]
sys.modules["fk"] = pkg

from fk.struct.Lmt import parseLMT
from fk.common.HashUtils import masterTable, hashResolver


def walk(path):
    lmt = parseLMT(path)
    print("LMT:", path)
    print("  entries:", lmt.Header.entryCount, "actions:", len(lmt.ActionHeaders))
    bad_dt = {}      # hash -> {(entry): [typingHash...]}
    bad_tp = {}      # timeline param hashes
    resolved_tp = set()
    for action in lmt.ActionHeaders:
        timl = getattr(action, "timl", None)
        if not timl:
            continue
        for typing in timl.types:
                tp = typing.timelineParameterHash
                try:
                    name, ok, uniq = masterTable.resolve(tp)
                except Exception:
                    ok, name = False, None
                if isinstance(name, set):
                    ok, name = False, None
                if ok:
                    resolved_tp.add(tp)
                else:
                    bad_tp.setdefault(tp, set()).add(action.id)
                for tr in typing.transforms:
                    h = tr.datatypeHash
                    try:
                        cand, ok, uniq = masterTable.resolve(h)
                    except Exception:
                        ok, cand = False, None
                    if isinstance(cand, set):
                        ok = False
                    if not ok:
                        bad_dt.setdefault(h, set()).add(
                            (action.id, tp, tr.dataType, tr.count))
    print("  unresolvable datatypeHashes:", len(bad_dt))
    for h, locs in sorted(bad_dt.items()):
        entries = sorted({e for e, _, _, _ in locs})
        print("    0x%08X  entries=%s  samples=%s" % (
            h, entries, sorted(locs)[:4]))
    print("  unresolvable timelineParameterHashes:", len(bad_tp))
    for h, entries in sorted(bad_tp.items()):
        print("    0x%08X  entries=%s" % (h, sorted(entries)))
    return bad_dt, bad_tp


if __name__ == "__main__":
    for p in sys.argv[1:]:
        print("=" * 70)
        walk(p)
