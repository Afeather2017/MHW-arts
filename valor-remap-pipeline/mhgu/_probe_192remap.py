# 探针: 列出指定 blend 里名字含 '192' 或 'remap' 的动作 (只读)。
import bpy, sys

found = []
for a in bpy.data.actions:
    if "192" in a.name or "remap" in a.name.lower():
        n = len(a.fcurves)
        users = a.users
        fake = a.use_fake_user
        found.append((a.name, n, users, fake))
print("PROBE-FILE:", bpy.data.filepath)
if not found:
    print("  (no 192/remap actions)")
for name, n, users, fake in sorted(found):
    print(f"  {name:50s} fcurves={n:4d} users={users} fake={fake}")
# 若存在 LMT::w07::192_remap, 顺带确认 030/047 scale fcurve 键数 (bufType5 阶梯应有多键)
a = bpy.data.actions.get("LMT::w07::192_remap")
if a:
    s30 = [fc for fc in a.fcurves if ".030" in fc.data_path or "030" in fc.data_path.split('"')[0]]
    scal = [fc for fc in a.fcurves if "scale" in fc.data_path]
    print(f"  [192_remap] total={len(a.fcurves)} scale-fcurves={len(scal)}")
    for fc in scal[:12]:
        print(f"    {fc.data_path[:70]} [{fc.array_index}] kp={len(fc.keyframe_points)}")
