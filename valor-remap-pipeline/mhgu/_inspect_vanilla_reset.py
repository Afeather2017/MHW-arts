"""Vanilla-reset 前置检查:解析改版 LMT(#1)/原版 LMT(#2) + 主树当前接线状态 + 交叉诊断。
输出 JSON 到 _inspect_vanilla_reset.json,控制台只打摘要。

用法:
  blender -b MHW-Valor.blend --python mhgu/_inspect_vanilla_reset.py
"""
import bpy, json, importlib, hashlib

OUT = r"E:\MHW-arts\valor-remap-pipeline\mhgu\_inspect_vanilla_reset.json"
LMT_MOD = r"E:\SteamLibrary\steamapps\common\Monster Hunter World\nativePC\hm\wp\wp03\mot\w03_00\w03_00.lmt"
LMT_VAN = r"E:\MHW-arts\chunk-extracted\chunk\hm\wp\wp03\mot\w03_00\w03_00.lmt"

report = {"errors": []}

def sec(name):
    def deco(fn):
        try:
            report[name] = fn()
        except Exception as e:
            import traceback
            report["errors"].append("%s: %s" % (name, traceback.format_exc()[-800:]))
        return fn
    return deco

Lmt = importlib.import_module("MHW-FreeKinetics.struct.Lmt")

@sec("lmt")
def _():
    out = {}
    for tag, path in (("mod", LMT_MOD), ("van", LMT_VAN)):
        lmt = Lmt.parseLMT(path)
        entries = []
        for a in lmt.ActionHeaders:
            ent = {"id": a.id, "fcOff": a.fcurveOffset, "timlOff": a.timlOffset,
                   "frameCount": a.frameCount, "loopFrame": a.loopFrame,
                   "flags": a.Flags, "flags2": a.Flags2}
            if a.timlOffset:
                try:
                    ent["timl_md5"] = hashlib.md5(a.timl.serialize()).hexdigest()
                    ent["timl_len"] = len(a.timl.serialize())
                except Exception:
                    ent["timl_md5"] = "ERR"
            if a.fcurveOffset:
                try:
                    ent["nbones"] = len(a.bones)
                    ent["nkf"] = sum(len(b.keyframes) if hasattr(b, "keyframes") else 0 for b in a.bones)
                except Exception:
                    ent["nbones"] = -1
            entries.append(ent)
        out[tag] = {"entryCount": lmt.Header.entryCount, "headers": len(lmt.ActionHeaders),
                    "entries": entries}
    return out

@sec("blend")
def _():
    out = {"trees": [ng.name for ng in bpy.data.node_groups if ng.bl_idname == "FreeHKNodeTree"]}
    out["tether"] = str(bpy.context.scene.freehk_tether) if hasattr(bpy.context.scene, "freehk_tether") else None
    main = bpy.data.node_groups.get("w03_00")
    if main is None:
        out["main_tree"] = "MISSING"
        return out
    fnodes = []
    for n in main.nodes:
        if n.bl_idname == "LMTFileNode":
            fnodes.append({"name": n.name, "filepath": n.filepath, "inject": bool(n.inject),
                           "entryCount": n.entryCount})
    out["lmt_file_nodes"] = fnodes
    entries = []
    for n in main.nodes:
        if n.bl_idname != "LMTEntryNode":
            continue
        e = {"entryNum": n.entryNum, "loopFrame": n.loopFrame,
             "flags": sum((bool(b) << (7 - i)) for i, b in enumerate(n.byteflag)),
             "flags2": sum((bool(b) << (7 - i)) for i, b in enumerate(n.byteflag2))}
        inp = n.inputs.get("LMT Animation")
        if inp is not None and inp.links:
            an = inp.links[0].from_node
            act = getattr(an, "input_action", None)
            if act is not None:
                e["anim"] = act.name
                e["star"] = act.freehk.starType
                e["act_frames"] = act.freehk.frameCount
                e["n_fc"] = len(act.fcurves)
        tinp = n.inputs.get("TIML Data")
        if tinp is not None and tinp.links:
            tn = tinp.links[0].from_node
            e["timl_node"] = tn.name
            if tn.bl_idname == "TIMLDataNode":
                e["timl_props"] = [tn.unkn1, tn.unkn2, int(tn.animLength),
                                   int(tn.loopStartPoint), str(tn.loopControl)]
                links = tn.inputs.get("TIML Animation")
                if links is not None:
                    e["timl_acts"] = sorted({l.from_node.input_action.name
                                             for l in links.links if l.from_node.input_action})
        entries.append(e)
    out["entries"] = entries
    return out

@sec("actions")
def _():
    acts = []
    for a in bpy.data.actions:
        acts.append({"name": a.name, "star": a.freehk.starType,
                     "frames": a.freehk.frameCount, "n_fc": len(a.fcurves),
                     "fake": a.use_fake_user, "users": a.users})
    acts.sort(key=lambda x: x["name"])
    return acts

@sec("timl_sample")
def _():
    out = []
    for a in bpy.data.actions:
        if a.freehk.starType == "TIML_Action":
            out.append({"name": a.name,
                        "remap": {k: str(getattr(a.freehk, k)) for k in
                                  ("transx", "transy", "transz", "rotx", "roty", "rotz",
                                   "sclx", "scly", "sclz")},
                        "reorder": str(a.freehk.timl_reorder),
                        "param": str(a.freehk.timelineParam), "n_fc": len(a.fcurves)})
        if len(out) >= 3:
            break
    return out

with open(OUT, "w", encoding="utf-8") as f:
    json.dump(report, f, ensure_ascii=False, indent=1, default=str)

# ---- console summary ----
print("=== SUMMARY ===")
print("errors:", len(report["errors"]))
for e in report["errors"]:
    print("  ERR", e.splitlines()[-1] if e else e)
lmt = report.get("lmt", {})
for tag in ("mod", "van"):
    if tag in lmt:
        d = lmt[tag]
        nmot = sum(1 for e in d["entries"] if e["fcOff"])
        ntim = sum(1 for e in d["entries"] if e["timlOff"])
        print("%s: entryCount=%d headers=%d motions=%d timl=%d" %
              (tag, d["entryCount"], d["headers"], nmot, ntim))
mod = {e["id"]: e for e in lmt.get("mod", {}).get("entries", [])}
van = {e["id"]: e for e in lmt.get("van", {}).get("entries", [])}
only_v = sorted(set(van) - set(mod))
only_m = sorted(set(mod) - set(van))
print("motion ids vanilla-only:", only_v[:20], "count", len(only_v))
print("motion ids mod-only:", only_m[:20], "count", len(only_m))
bl = report.get("blend", {})
ents = bl.get("entries", [])
print("main tree entries:", len(ents), "wired anim:", sum(1 for e in ents if "anim" in e),
      "wired timl:", sum(1 for e in ents if "timl_node" in e))
# mess diagnostics: wired action frameCount vs vanilla entry frameCount
mess = []
for e in ents:
    if "anim" not in e:
        continue
    v = van.get(e["entryNum"])
    vf = v["frameCount"] if v else None
    if v is None or e.get("act_frames") != vf:
        mess.append((e["entryNum"], e["anim"], e.get("act_frames"), vf))
print("wired entries with frameCount mismatch vs vanilla (# or missing):", len(mess))
for m in mess[:40]:
    print("   entry %d: %s frames=%s vanilla=%s" % m)
same = 0
for e in ents:
    if "anim" in e and e["entryNum"] in van and e.get("act_frames") == van[e["entryNum"]]["frameCount"]:
        same += 1
print("wired entries frameCount == vanilla:", same)
acts = report.get("actions", [])
print("total actions:", len(acts), "| LMT_Action:", sum(1 for a in acts if a["star"] == "LMT_Action"),
      "| TIML_Action:", sum(1 for a in acts if a["star"] == "TIML_Action"),
      "| other:", sum(1 for a in acts if a["star"] not in ("LMT_Action", "TIML_Action")))
print("timl sample:", json.dumps(report.get("timl_sample", []), ensure_ascii=False)[:600])
print("report ->", OUT)
