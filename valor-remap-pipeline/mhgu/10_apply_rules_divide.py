# -*- coding: utf-8 -*-
"""STEP 10 - 对 divide_* / *_before_divide 自定义动作套用 mhgu 管线规则。

用户在 MHW-Valor.blend 新增 5 个左手握持动作(带前后左右根位移),ARP 直接烘出:
  divide_long(0..63) / divide_short(0..57) /
  left_up_before_divide / rigit_up_before_divide / right_down_before_divide (0..6)

规则(全部来自已验证配方,见 mhgu/README.md):
  坑⑬  tetherFrame 绑定 m_mesh Armature,否则导出丢 Root 轨 + 米值直写(缩成一团);
  坑⑥  freehk.frameCount = 末键帧(参照 remap 约定),starType = LMT_Action;
  坑④  object 层只留根位移:删 ["arp_retarget_bound"] 与 object scale,
        旋转恒等化成 2 键(参照形状;恒等旋转合成 Root 恒等旋转 = 原生布局 (-1,3));
  左手  030/047 全 20 通道重写为 LMT::w03_00::106 逐字常量(030.scaleY=2 左插座持有,
        047.scaleY=1 闲置) - 4_grip_left 配方;
  000  只写 location(重心),旋转/缩放曲线删除(原生从不转 000,写了全身倾斜);
  打标  6_tag v2 配方:LIMITS 元数据从原版 w03_00-origin.lmt entry104 动态读取,
        030/047 scale 行从 entry109 读 bufType 5(2026-09-29 局内验证),
        原生布局不追踪的通道(_s 骨/000 旋转等)删除;
  坑⑪.5 min-2-keys:单键常量升级 2 键(导出器 bounds() 丢首键会崩)。

Idempotent。验证全绿才保存。Run:
  blender -b MHW-Valor.blend --python mhgu/10_apply_rules_divide.py
  # 处理别的动作(复用):环境变量传动作名,逗号分隔;
  # 缺省 = 2026-09-30 那批 5 个
  RULE_ACTIONS="a,b,c" blender -b MHW-Valor.blend --python mhgu/10_apply_rules_divide.py
"""
import bpy, os, sys, re, shutil, time, importlib

BLEND = os.environ.get("VALOR_BLEND", r"E:\MHW-arts\MHW-Valor.blend")
ORIGIN = r"E:\SteamLibrary\steamapps\common\Monster Hunter World\nativePC\hm\wp\wp03\mot\w03_00\w03_00-origin.lmt"
GRIP_REF = "LMT::w03_00::106"
ORIGIN_ENTRY = 104
USAGE = {"rotation_quaternion": 0, "location": 1, "scale": 2}
DEFAULT_NAMES = ["divide_long", "divide_short",
                 "left_up_before_divide", "rigit_up_before_divide", "right_down_before_divide"]
NAMES = [s.strip() for s in os.environ.get("RULE_ACTIONS", "").split(",") if s.strip()] or DEFAULT_NAMES
BONES = ("MhBone_030", "MhBone_047")
PROPS = (("location", 3), ("rotation_quaternion", 4), ("scale", 3))

bak = BLEND + ".bak-" + time.strftime("%Y%m%d-%H%M%S")
shutil.copyfile(BLEND, bak)
print("[backup] %s" % bak, flush=True)

bpy.ops.wm.open_mainfile(filepath=BLEND)

# ---- 左手绑定常量:从 106 逐字读(同 4_grip_left) ----
ref = bpy.data.actions.get(GRIP_REF)
if ref is None:
    print("!!! FAIL: %s missing" % GRIP_REF); sys.exit(1)

def first_val(a, bone, prop, idx):
    fc = a.fcurves.find('pose.bones["%s"].%s' % (bone, prop), index=idx)
    if fc is None or not len(fc.keyframe_points):
        return None
    return fc.keyframe_points[0].co[1]

grip = {}
missing = []
for b in BONES:
    for prop, n in PROPS:
        for i in range(n):
            v = first_val(ref, b, prop, i)
            if v is None:
                missing.append((b, prop, i))
            grip[(b, prop, i)] = v
if missing:
    print("!!! FAIL: 106 missing channels: %s" % missing); sys.exit(1)
assert abs(grip[("MhBone_030", "scale", 1)] - 2.0) < 1e-6, "030.scaleY != 2 (not left-drawn)"
assert abs(grip[("MhBone_047", "scale", 1)] - 1.0) < 1e-6, "047.scaleY != 1"
print("[grip] 030 loc=%s quat=%s scale=%s | 047 loc=%s quat=%s scale=%s" % (
    tuple(round(grip[("MhBone_030", "location", i)], 4) for i in range(3)),
    tuple(round(grip[("MhBone_030", "rotation_quaternion", i)], 4) for i in range(4)),
    tuple(round(grip[("MhBone_030", "scale", i)], 2) for i in range(3)),
    tuple(round(grip[("MhBone_047", "location", i)], 4) for i in range(3)),
    tuple(round(grip[("MhBone_047", "rotation_quaternion", i)], 4) for i in range(4)),
    tuple(round(grip[("MhBone_047", "scale", i)], 2) for i in range(3))), flush=True)

# ---- 打标配方:原版 entry104 布局 + 109 的 030/047 scale 行(bufType 5) ----
Lmt = importlib.import_module("MHW-FreeKinetics.struct.Lmt")
lmt = Lmt.parseLMT(ORIGIN)
recipe = {}
for b in [x for x in lmt.ActionHeaders if x.id == ORIGIN_ENTRY][0].bones:
    if b.boneId >= 0:
        recipe[(b.boneId, b.usage % 3)] = b.bufferType
for b in [x for x in [y for y in Lmt.parseLMT(ORIGIN).ActionHeaders if y.id == 109][0].bones
          if b.boneId in (30, 47) and b.usage == 2]:
    recipe[(b.boneId, 2)] = b.bufferType
print("[recipe] %d (bone,usage) entries from origin entry %d (scale rows from 109)" % (
    len(recipe), ORIGIN_ENTRY), flush=True)

arm = bpy.data.objects.get("m_mesh Armature")
if arm is None:
    print("!!! FAIL: m_mesh Armature missing"); sys.exit(1)

def write_const(fc, frame, value):
    kp = fc.keyframe_points
    kp.clear()
    kp.add(1)
    k = kp[0]
    k.co = (frame, value)
    k.interpolation = 'BEZIER'
    k.handle_left_type = 'AUTO_CLAMPED'
    k.handle_right_type = 'AUTO_CLAMPED'
    fc.update()

def tag(fc, bone_id, buf):
    for m in list(fc.modifiers):
        if m.type == "LIMITS":
            fc.modifiers.remove(m)
    l = fc.modifiers.new("LIMITS")
    l.use_restricted_range = True
    l.mute = True
    l.frame_start = l.frame_end = 0
    l.min_x = buf
    l.min_y = bone_id

report = []
for nm in NAMES:
    a = bpy.data.actions.get(nm)
    if a is None:
        print("!!! FAIL: action %s missing" % nm); sys.exit(1)
    n0 = len(a.fcurves)
    hi = int(round(a.frame_range[1]))

    # -- freehk 元数据(坑⑥/坑⑬) --
    a.freehk.tetherFrame = arm
    try:
        a.freehk.starType = "LMT_Action"
    except Exception as e:
        print("!!! FAIL: starType on %s: %s" % (nm, e)); sys.exit(1)
    a.freehk.frameCount = hi
    a.use_fake_user = True

    # -- object 层整形(坑④):只留根位移 --
    removed_obj = 0
    for fc in list(a.fcurves):
        if fc.data_path.startswith("pose.bones["):
            continue
        if fc.data_path == "scale" or fc.data_path.startswith('["'):
            a.fcurves.remove(fc)          # object scale / ["arp_retarget_bound"]
            removed_obj += 1
    for i in range(4):                    # 旋转恒等化成 2 键(参照形状)
        fc = a.fcurves.find("rotation_quaternion", index=i)
        if fc is None:
            fc = a.fcurves.new("rotation_quaternion", index=i)
        write_const(fc, 0, 1.0 if i == 0 else 0.0)
        kp = fc.keyframe_points
        kp.add(1)
        kp[1].co = (hi, 1.0 if i == 0 else 0.0)
        kp[1].interpolation = 'BEZIER'
        kp.sort()
        fc.update()

    # -- 000 只写 location --
    removed_000 = 0
    for prop in ("rotation_quaternion", "scale"):
        for i in range(4 if prop == "rotation_quaternion" else 3):
            fc = a.fcurves.find('pose.bones["MhBone_000"].%s' % prop, index=i)
            if fc is not None:
                a.fcurves.remove(fc)
                removed_000 += 1

    # -- 030/047 = 106 逐字常量(左手持有) --
    for (b, prop, i), v in sorted(grip.items()):
        path = 'pose.bones["%s"].%s' % (b, prop)
        fc = a.fcurves.find(path, index=i)
        if fc is None:
            fc = a.fcurves.new(path, index=i, action_group=b)
        write_const(fc, 0, v)

    # -- 打标 + 删原生布局外的通道 --
    removed_nn = warned = 0
    tagged = 0
    for fc in list(a.fcurves):
        if not fc.data_path.startswith("pose.bones["):
            continue
        m = re.match(r'pose\.bones\["MhBone_(\d+)"\]\.(\w+)$', fc.data_path)
        if not m:
            print("[warn] %s: unrecognized path %s left untagged" % (nm, fc.data_path))
            warned += 1
            continue
        bone_id, usage = int(m.group(1)), USAGE.get(m.group(2))
        if usage is None or (bone_id, usage) not in recipe:
            a.fcurves.remove(fc)
            removed_nn += 1
            continue
        tag(fc, bone_id, recipe[(bone_id, usage)])
        tagged += 1

    # -- min-2-keys(坑⑪.5) --
    bumped = 0
    for fc in a.fcurves:
        kp = fc.keyframe_points
        if len(kp) == 1:
            f0, v0 = kp[0].co
            kp.add(1)
            kp[1].co = (f0 + 1.0, v0)
            kp[1].interpolation = kp[0].interpolation
            kp.sort()
            fc.update()
            bumped += 1

    report.append((nm, n0, len(a.fcurves), hi, removed_obj, removed_000, removed_nn, tagged, bumped, warned))
    print("[fix] %-26s fcurves %d->%d end=%d obj-=%d 000-=%d nonnative-=%d tagged=%d min2+=%d warn=%d" % (
        nm, n0, len(a.fcurves), hi, removed_obj, removed_000, removed_nn, tagged, bumped, warned), flush=True)

# ---- verify(全绿才保存) ----
bad = []
for nm in NAMES:
    a = bpy.data.actions.get(nm)
    hi = int(round(a.frame_range[1]))
    if a.freehk.tetherFrame is not arm:
        bad.append((nm, "tether"))
    if a.freehk.starType != "LMT_Action":
        bad.append((nm, "starType=%s" % a.freehk.starType))
    if a.freehk.frameCount != hi:
        bad.append((nm, "frameCount=%s" % a.freehk.frameCount))
    for fc in a.fcurves:
        if fc.data_path.startswith("pose.bones["):
            m = re.match(r'pose\.bones\["MhBone_(\d+)"\]\.(\w+)$', fc.data_path)
            if not m:
                bad.append((nm, fc.data_path, "unrecognized")); continue
            bone_id, usage = int(m.group(1)), USAGE[m.group(2)]
            got = [(int(md.min_x), int(md.min_y)) for md in fc.modifiers if md.type == "LIMITS"]
            if (recipe[(bone_id, usage)], bone_id) not in got:
                bad.append((nm, fc.data_path, "LIMITS %s != %s" % (got, (recipe[(bone_id, usage)], bone_id))))
        else:
            if fc.data_path == "scale" or fc.data_path.startswith('["'):
                bad.append((nm, fc.data_path, "object curve should be gone"))
            elif fc.data_path == "rotation_quaternion" and len(fc.keyframe_points) != 2:
                bad.append((nm, "obj rot kp=%d" % len(fc.keyframe_points)))
        if len(fc.keyframe_points) == 1:
            bad.append((nm, fc.data_path, "single key"))
    # 左手状态 + 000 形状 + 位移存在
    s30 = a.fcurves.find('pose.bones["MhBone_030"].scale', index=1)
    s47 = a.fcurves.find('pose.bones["MhBone_047"].scale', index=1)
    if not (s30 and abs(s30.keyframe_points[0].co[1] - 2.0) < 1e-6):
        bad.append((nm, "030.scaleY"))
    if not (s47 and abs(s47.keyframe_points[0].co[1] - 1.0) < 1e-6):
        bad.append((nm, "047.scaleY"))
    for prop in ("rotation_quaternion", "scale"):
        if any(a.fcurves.find('pose.bones["MhBone_000"].%s' % prop, index=i) is not None
               for i in range(4)):
            bad.append((nm, "000.%s present" % prop))
    travel = [a.fcurves.find("location", index=i) for i in range(2)]
    if not all(t and len(t.keyframe_points) > 1 for t in travel):
        bad.append((nm, "object location travel missing"))
    if a.fcurves.find('pose.bones["MhBone_000"].location', index=1) is None:
        bad.append((nm, "000.location missing"))

print("[verify] problems: %s" % (bad[:10] if bad else "none"), flush=True)
if bad:
    print("!!! NOT SAVING"); sys.exit(1)

bpy.ops.wm.save_mainfile()
print("[done] saved:", BLEND, flush=True)
