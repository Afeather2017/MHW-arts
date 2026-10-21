# -*- coding: utf-8 -*-
"""_shake_validate.py — 校验已部署 w03_00.lmt 的屏幕震动配方 (343/156 金标准)

独立运行 (需 Blender 的 FreeKinetics struct 包, mathutils 用桩替代):
  python _shake_validate.py [lmt路径]
默认校验游戏目录部署文件; 六通道 (343/156 x shake/trigger/view) 逐键对照配方,
全 MATCH = PASS。
"""
import sys, io, os, types, importlib
import struct as _stdlib_struct  # 预加载: 防 addon/struct 包目录抢占 stdlib 名字

GAME_DEFAULT = (r"E:\SteamLibrary\steamapps\common\Monster Hunter World"
                r"\nativePC\hm\wp\wp03\mot\w03_00\w03_00.lmt")

# 金标准 (2026-10-22 user-verified 部署):
EXPECT = {
    343: {
        0x252961CD: [(0, 0), (68, 0), (69, 43), (90, 43), (91, 0)],          # shake 窗
        0x99897C43: [(0, 0), (69, 1024), (74, 0)],                           # bit10 点火
        0xF4D12F36: [(0, 0), (30, 0), (31, 14), (72, 14), (73, 0), (165, -1)],  # 拉镜头
    },
    156: {
        0x252961CD: [(0, 0), (5, 0), (6, 43), (16, 43), (17, 0)],
        0x99897C43: [(0, 0), (6, 1024), (11, 0)],
        0xF4D12F36: None,                                                    # 无镜头
    },
}
NAMES = {0x252961CD: "shake", 0x99897C43: "trigger", 0xF4D12F36: "view"}


def _load_lmt_module():
    class Vector(list):
        def __init__(self, v=()): super().__init__(v)
        def __add__(self, o): return Vector([a + b for a, b in zip(self, o)])
        def __sub__(self, o): return Vector([a - b for a, b in zip(self, o)])
        def __mul__(self, o):
            if isinstance(o, (int, float)): return Vector([a * o for a in self])
            return Vector([a * b for a, b in zip(self, o)])
        def __truediv__(self, o):
            if isinstance(o, (int, float)): return Vector([a / o for a in self])
            return Vector([a / b for a, b in zip(self, o)])
        def dot(self, o): return sum(a * b for a, b in zip(self, o))
        def length(self): return sum(a * a for a in self) ** 0.5
        def normalized(self):
            L = self.length(); return Vector([a / L for a in self])
        def resize(self, n):
            while len(self) < n: self.append(0.0)
            del self[n:]
            return self
        def negated(self): return Vector([-a for a in self])
        def cross(self, o): return Vector([
            self[1] * o[2] - self[2] * o[1], self[2] * o[0] - self[0] * o[2],
            self[0] * o[1] - self[1] * o[0]])
        def to_tuple(self): return tuple(self)
    mm = types.ModuleType("mathutils"); mm.Vector = Vector
    sys.modules["mathutils"] = mm
    base = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..",
                        "tools", "blender-4.3.2-windows-x64")
    addon = None
    for cand in (r"C:\Users\Afeather\AppData\Roaming\Blender Foundation\Blender\4.3"
                 r"\scripts\addons\MHW-FreeKinetics",):
        if os.path.isdir(cand):
            addon = cand
            break
    if addon is None:
        raise SystemExit("FreeKinetics addon 目录未找到")
    sys.path.insert(0, addon)
    sys.path.insert(0, os.path.join(addon, "struct"))
    sys.path.insert(0, os.path.join(addon, "common"))
    for m in ("Cstruct", "FileLike", "BitUtils", "Constants", "HashUtils",
              "ExtensibleList", "TIML", "TIMLPropertyOrder", "LmtBuffers"):
        try:
            importlib.import_module(m)
        except Exception:
            pass
    import Lmt
    return Lmt


def pm_channels(lmt, entry):
    for a in lmt.ActionHeaders:
        if a.id != entry:
            continue
        t = getattr(a, "timl", None)
        if not t or not getattr(t, "types", None):
            return {}
        d = {}
        for ty in t.types:
            if ty.timelineParameterHash != 0x48D6114F:
                continue
            for tr in ty.transforms:
                if tr.datatypeHash in NAMES:
                    d[tr.datatypeHash] = sorted(
                        (round(float(k.frameTiming), 3), round(float(k.value), 4))
                        for k in tr.keyframes)
        return d
    return {}


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else GAME_DEFAULT
    Lmt = _load_lmt_module()

    class LenBytesIO(io.BytesIO):
        def __len__(self): return len(self.getvalue())

    raw = open(path, "rb").read()
    lmt = Lmt.LMT()
    s = LenBytesIO(raw)
    lmt.marshallHeader(s)
    lmt.marshallBody(lmt.ActionHeaders, s)

    ok = True
    for entry, expect in sorted(EXPECT.items()):
        got = pm_channels(lmt, entry)
        for h, want in sorted(expect.items()):
            have = got.get(h)
            match = (have == want and (have is not None) == (want is not None))
            ok &= match
            print("entry %-4d %-8s %s" % (entry, NAMES[h],
                  "MATCH" if match else "DIFF want=%s got=%s" % (want, have)))
    import hashlib
    print("file:", path)
    print("md5 :", hashlib.md5(raw).hexdigest(), "size:", len(raw))
    print("RESULT:", "PASS" if ok else "FAIL")
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
