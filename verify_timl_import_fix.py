# -*- coding: utf-8 -*-
"""End-to-end offline verification of the FreeKinetics timl_importer patch:
runs the real processTIML/remapAnalyzer/applyRemap code (bpy stubbed) over
every TIML in an LMT and asserts no unresolved-hash int leaks through."""
import sys, types

# ---- mathutils stub (4-component vector, enough for keyframe decoding) ----
class _V:
    __slots__ = ("d",)
    def __init__(s, v): s.d = [float(x) for x in v]
    def _op(s, o, f):
        if isinstance(o, _V): return _V([f(a, b) for a, b in zip(s.d, o.d)])
        return _V([f(a, o) for a in s.d])
    def __mul__(s, o): return s._op(o, lambda a, b: a * b)
    __rmul__ = __mul__
    def __add__(s, o): return s._op(o, lambda a, b: a + b)
    __radd__ = __add__
    def __sub__(s, o): return s._op(o, lambda a, b: a - b)
    def __truediv__(s, o): return s._op(o, lambda a, b: a / b)
    def __getitem__(s, i): return s.d[i]
    def __setitem__(s, i, v): s.d[i] = float(v)
    def __len__(s): return len(s.d)
    def __iter__(s): return iter(s.d)
    def freeze(s): return s
_mu = types.ModuleType("mathutils"); _mu.Vector = _V
sys.modules["mathutils"] = _mu

# ---- bpy stub: only needs to survive module-level class/prop definitions ----
class _PropGroup: pass
class _BpyNS:
    def __getattr__(self, name):
        if name == "PropertyGroup":
            return _PropGroup
        if name == "Object":
            return type("Object", (object,), {})
        def _anything(*a, **k):
            return None
        return _anything
_bpy = types.ModuleType("bpy")
_bpy.types = _BpyNS()
_bpy_props = types.ModuleType("bpy.props")
_bpy_props.__getattr__ = lambda name: (lambda *a, **k: None)
_bpy.props = _bpy_props
sys.modules["bpy"] = _bpy
sys.modules["bpy.props"] = _bpy_props

ADDON = r"C:\Users\Afeather\AppData\Roaming\Blender Foundation\Blender\4.3\scripts\addons\MHW-FreeKinetics"
pkg = types.ModuleType("fk"); pkg.__path__ = [ADDON]; sys.modules["fk"] = pkg

from fk.blender.timl_importer import processTIMLData, remapAnalyzer, applyRemap
from fk.blender.timl_controller import timl_typemap, timl_propmap
from fk.struct.Lmt import parseLMT

ok = True
for path in sys.argv[sys.argv.index("--")+1:]:
    print("=" * 70)
    print("LMT:", path)
    lmt = parseLMT(path)
    n_types = n_transforms = 0
    fallbacks = {}
    for action in lmt.ActionHeaders:
        timl = getattr(action, "timl", None)
        if not timl:
            continue
        processTIMLData(timl, remapAnalyzer)   # exact crash-site code path (lmt_importer.LMTFileLoad)
        for typing in timl.types:
            n_types += 1
            assert isinstance(typing.timelineParameterString, str), "timeline param int leaked"
            for tr in typing.transforms:
                n_transforms += 1
                assert isinstance(tr.datatypeString, str), "datatypeString int leaked"
                assert isinstance(tr.datatype, list) and isinstance(tr.datatype[0], str)
                if tr.datatype[0].startswith("FreeHKTiml."):
                    fallbacks[tr.datatype[0]] = (tr.datatypeHash, tr.dataType, tr.count)
    print("  processed OK: actions-with-timl, %d types, %d transforms" % (n_types, n_transforms))
    for prop, (h, dt, cnt) in sorted(fallbacks.items()):
        registered = timl_typemap.get(prop.split(".")[-1], "<MISSING>")
        print("  fallback fcurve %s hash=0x%08X dataType=%d keyframes=%d typemap->%s" %
              (prop, h, dt, cnt, registered))
        # export does timl_typemap[dp] % 5; flag-type canonical entries store 5+dt
        assert registered % 5 == dt % 5, "typemap registration mismatch"
print("ALL CHECKS PASSED")
