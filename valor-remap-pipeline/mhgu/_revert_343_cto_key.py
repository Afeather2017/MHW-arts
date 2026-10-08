# -*- coding: utf-8 -*-
# 把 E343 接线的 CollisionTimelineObject 动作里值=85 的键改回 63 (0.0.1 状态)。
# 期望恰好 1 个键。 SAVE=1 才保存。
import bpy, os, sys

tree = bpy.data.node_groups["w03_00"]
entry = next(n for n in tree.nodes if n.bl_idname == "LMTEntryNode" and int(n.entryNum) == 343)
sock = next(i for i in entry.inputs if i.name == "TIML Data")
data = sock.links[0].from_node
tsock = next(i for i in data.inputs if i.name == "TIML Animation")
acts = [l.from_node.input_action for l in tsock.links]
ctos = [a for a in acts if "CollisionTimelineObject" in a.name]
print("E343 wired actions:", [a.name for a in acts])
print("CTO actions:", [a.name for a in ctos])

hits = []
for a in ctos:
    for fc in a.fcurves:
        for kp in fc.keyframe_points:
            v = kp.co[1]
            if abs(v - 85.0) < 0.4 or abs(v - 63.0) < 0.4:
                hits.append((a.name, fc.data_path, fc.array_index, kp.co[0], v))
print("keys near 85/63:")
for h in hits:
    print("   %s %s[%d] frame=%.3f value=%.3f" % h)

to_fix = [h for h in hits if abs(h[4] - 85.0) < 0.4]
if len(to_fix) != 1:
    print("ABORT: expected exactly 1 key with value 85, got %d" % len(to_fix))
    sys.exit(3)
for a_name, dp, idx, fr, v in to_fix:
    a = bpy.data.actions[a_name]
    fc = next(f for f in a.fcurves if f.data_path == dp and f.array_index == idx)
    for kp in fc.keyframe_points:
        if abs(kp.co[1] - 85.0) < 0.4:
            kp.co[1] = 63.0
            print("FIXED: %s %s[%d] frame=%.3f 85 -> 63" % (a_name, dp, idx, kp.co[0]))
print("SAVE=1" if os.environ.get("SAVE") == "1" else "DRY-RUN (no save)")
if os.environ.get("SAVE") == "1":
    bpy.ops.wm.save_mainfile()
    print("SAVED", bpy.data.filepath)
