"""Splice plw_LongSword_100_111_Armature_remap.short (frames >= 9) onto the tail of
divide_valor_sheath, IN PLACE. Follows valor-remap-pipeline/splice_actions.py conventions:
 - SEAM_GAP=0: source frame 9 lands ON the target's last frame (poses match there, d9<<d8)
 - object location + MhBone_000 location get a VALUE offset so the appended tail continues
   from the sheath's end root/COG position (no teleport at the seam)
 - seam-frame key collisions: keep the TARGET's key, warn when values differ
 - target curves whose keys end before the seam get a HOLD key at the seam (dst's last
   value) so the authored hold doesn't turn into a 30-frame interpolation drift
 - source curves whose keys all start after the cut get a boundary key sampled at the cut
"""
import bpy

SRC_NAME = "plw_LongSword_100_111_Armature_remap.short"
DST_NAME = "divide_valor_sheath"
CUT = 9.0
EPS = 1e-4

SRC = bpy.data.actions[SRC_NAME]
DST = bpy.data.actions[DST_NAME]
assert SRC and DST, "actions not found"

seam = max(k.co.x for f in DST.fcurves for k in f.keyframe_points)
shift = seam - CUT
print("[splice] src=%s dst=%s" % (SRC_NAME, DST_NAME))
print("[splice] dst last key = %.0f  -> shift = %+.0f (src f9 lands on f%.0f)" % (seam, shift, seam))

def read_keys(fc):
    return [dict(co=tuple(k.co), hl=tuple(k.handle_left), hr=tuple(k.handle_right),
                 interp=k.interpolation, hlt=k.handle_left_type, hrt=k.handle_right_type)
            for k in fc.keyframe_points]

def write_keys(fc, keys):
    keys.sort(key=lambda d: d["co"][0])
    kp = fc.keyframe_points
    kp.clear()
    kp.add(len(keys))
    for k, d in zip(kp, keys):
        k.co = d["co"]
        k.handle_left = d["hl"]
        k.handle_right = d["hr"]
        k.interpolation = d["interp"]
        k.handle_left_type = d["hlt"]
        k.handle_right_type = d["hrt"]
    fc.update()

def fam_off(prefix):
    o = {}
    for i in range(3):
        fa = SRC.fcurves.find(prefix, index=i)
        fb = DST.fcurves.find(prefix, index=i)
        o[i] = ((fb.evaluate(seam) if fb else 0.0) - (fa.evaluate(CUT) if fa else 0.0))
    return o

COG = 'pose.bones["MhBone_000"].location'
OFF = {"location": fam_off("location"), COG: fam_off(COG)}
print("[splice] value offset object.location   = %s" % {i: round(v, 4) for i, v in OFF["location"].items()})
print("[splice] value offset MhBone_000.location = %s" % {i: round(v, 4) for i, v in OFF[COG].items()})

dst_idx = {(f.data_path, f.array_index): f for f in DST.fcurves}
groups = {g.name: g for g in DST.groups}

new_curves = appended = dropped = holds = bound = 0
warns, skipped_const, sparse_src = [], [], []
for fc in SRC.fcurves:
    keys = read_keys(fc)
    tail = [k for k in keys if k["co"][0] >= CUT - EPS]
    if not tail:
        v = keys[0]["co"][1] if keys else 0.0
        skipped_const.append((fc.data_path, fc.array_index, round(v, 4)))
        continue
    off = OFF.get(fc.data_path, {}).get(fc.array_index, 0.0)

    # boundary key when the tail starts after the cut (sparse 2-key ramp curves)
    if tail[0]["co"][0] > CUT + EPS:
        tail.insert(0, dict(co=(CUT, fc.evaluate(CUT) + off),
                            hl=(CUT, fc.evaluate(CUT) + off),
                            hr=(CUT, fc.evaluate(CUT) + off),
                            interp=tail[0]["interp"], hlt="VECTOR", hrt=tail[0]["hlt"]))
        bound += 1
    shifted = [dict(co=(k["co"][0] + shift, k["co"][1] + off),
                    hl=(k["hl"][0] + shift, k["hl"][1] + off),
                    hr=(k["hr"][0] + shift, k["hr"][1] + off),
                    interp=k["interp"], hlt=k["hlt"], hrt=k["hrt"]) for k in tail]

    dfc = dst_idx.get((fc.data_path, fc.array_index))
    if dfc is None:
        gname = fc.group.name if fc.group else None
        if gname and gname not in groups:
            groups[gname] = DST.groups.new(gname)
        dfc = DST.fcurves.new(fc.data_path, index=fc.array_index,
                              action_group=gname if gname else "")
        new_curves += 1
        old = []
    else:
        old = read_keys(dfc)

    # drop appended keys colliding with existing dst key frames (keep dst's value)
    kept = []
    for k in shifted:
        hit = [o for o in old if abs(o["co"][0] - k["co"][0]) < 1e-3]
        if hit:
            if abs(hit[0]["co"][1] - k["co"][1]) > 1e-3:
                warns.append((fc.data_path, fc.array_index, round(k["co"][0], 1),
                              round(hit[0]["co"][1], 4), round(k["co"][1], 4)))
            dropped += 1
            continue
        kept.append(k)
    if not kept:
        continue

    # hold key: dst's keys end before the seam -> anchor dst's last value at the seam
    if old:
        old_max = max(o["co"][0] for o in old)
        if old_max < seam - EPS:
            hold_v = min(old, key=lambda o: abs(seam - o["co"][0]))["co"][1]
            if abs(kept[0]["co"][0] - seam) < 1e-3:
                kept[0] = dict(co=(seam, hold_v), hl=(kept[0]["hl"][0], hold_v),
                               hr=(kept[0]["hr"][0], hold_v), interp=kept[0]["interp"],
                               hlt=kept[0]["hlt"], hrt=kept[0]["hrt"])
            else:
                kept.insert(0, dict(co=(seam, hold_v), hl=(seam, hold_v), hr=(seam, hold_v),
                                    interp="LINEAR", hlt="VECTOR", hrt="VECTOR"))
            holds += 1

    if len(tail) < 5 and (fc.data_path, fc.array_index) not in dst_idx:
        sparse_src.append((fc.data_path, fc.array_index, len(tail)))
    write_keys(dfc, old + kept)
    appended += len(kept)

# ---------------- report + sanity ----------------
dup = 0
for f in DST.fcurves:
    xs = sorted(k.co.x for k in f.keyframe_points)
    dup += sum(1 for a, b in zip(xs, xs[1:]) if b - a < 1e-4)
mod_src = sum(len(f.modifiers) for f in SRC.fcurves)
mod_dst = sum(len(f.modifiers) for f in DST.fcurves)
fr = DST.frame_range
print("[splice] dst now: %.0f..%.0f, curves=%d (was 310, +%d new), keys appended=%d, "
      "seam dups dropped=%d, hold keys=%d, boundary keys=%d" %
      (fr[0], fr[1], len(DST.fcurves), new_curves, appended, dropped, holds, bound))
print("[splice] duplicate frames after merge: %d  (must be 0)" % dup)
print("[splice] fcurve modifiers src=%d dst=%d (expect 0)" % (mod_src, mod_dst))
print("[splice] seam value-mismatch warns: %d" % len(warns))
for w in warns[:10]:
    print("   warn %s[%d] @f%.0f dst=%.4f src+offset=%.4f" % w)
if warns:
    from collections import Counter
    per_bone = Counter(w[0].split('["')[1].split('"]')[0] for w in warns)
    print("   mismatch by bone: %s" % dict(per_bone))
print("[splice] skipped constant-only source curves (<f9): %d" % len(skipped_const))
nonid = [s for s in skipped_const
         if (s[0].endswith(".location") and abs(s[2]) > 1e-3)
         or (s[0].endswith(".scale") and abs(s[2] - 1.0) > 1e-3)
         or (s[0].endswith("rotation_quaternion") and abs(s[2] - 1.0) > 1e-3)]
print("   non-identity among them: %d %s" % (len(nonid), nonid[:8]))
if sparse_src:
    print("[splice] sparse source tails: %s" % sparse_src)

# spot checks: seam continuity of the offset channels
ok = True
for i in range(3):
    fa = SRC.fcurves.find("location", index=i)
    fb = DST.fcurves.find("location", index=i)
    v = fb.evaluate(seam) - (fa.evaluate(CUT) + OFF["location"][i])
    if abs(v) > 1e-4:
        ok = False
        print("[splice] OBJ LOC MISMATCH axis %d at seam: %.6f" % (i, v))
print("[splice] object-location seam continuity check: %s" % ("OK" if ok else "FAIL"))

DST.use_fake_user = True
bpy.ops.wm.save_mainfile()
print("[splice] SAVED %s" % bpy.data.filepath)
