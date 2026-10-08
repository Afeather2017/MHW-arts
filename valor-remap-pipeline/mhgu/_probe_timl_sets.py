# -*- coding: utf-8 -*-
# 对 7 个受影响 entry 的 TIML 动作按名字输出属性指纹 (两文件各跑一次, 文本 diff)。
# 用法: blender -b <blend> --python _probe_timl_sets.py  (读 E:/MHW-arts/_wiring_001.json)
import bpy, json, sys

with open(r"E:\MHW-arts\_wiring_001.json", encoding="utf-8") as f:
    snap = json.load(f)

FOCUS = ["106", "140", "306", "342", "343", "344", "345", "346"]
print("FILE:", bpy.data.filepath)
for en in FOCUS:
    e = snap[en]
    print(f"--- E{en} ---")
    for t in e["timl"][0]["tims"]:
        a = bpy.data.actions.get(t["action"])
        if a is None:
            print(f"  {t['action']:60s} <MISSING>")
            continue
        props = {k: v for k, v in a.items() if not str(k).startswith("_")}
        # 每条 fcurve 的键数指纹 (data_path 略, 只看数量分布)
        kps = sorted(len(fc.keyframe_points) for fc in a.fcurves)
        print(f"  {t['action']:60s} fc={len(a.fcurves):3d} kps={kps[:10]}{'...' if len(kps)>10 else ''} props={props}")
