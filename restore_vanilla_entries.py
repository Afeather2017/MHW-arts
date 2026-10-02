# -*- coding: utf-8 -*-
"""Restore pristine vanilla motions onto every NON-converted entry of
MHW-Valor-new.blend, by importing the vanilla origin LMT and curve-copying
its actions over the blend's (possibly messed-up) same-entry actions.

The CONVERTED entries (valor work) are skipped - they keep their animations.
All actions get Fake users. Saves the blend.

Run INSIDE Blender's GUI (Text editor > Open this file > Run Script):
the FreeKinetics import operator needs the GUI context (it touches
dopesheet areas), so headless (-b) will crash in the addon.

Steps it performs, in one click:
  1. freehk.import_lmt: import the vanilla origin LMT (arrives as
     LMT::w03_00::NNN.001 duplicates)
  2. every NON-converted entry gets the vanilla curves copied over
     (fixes built-ins accidentally swapped in the old blend); the
     converted valor entries keep their animations
  3. all actions get Fake users; blend is saved
"""
import bpy

VANILLA_LMT = r"E:\SteamLibrary\steamapps\common\Monster Hunter World\nativePC\hm\wp\wp03\mot\w03_00\w03_00-origin.lmt"
NEW_BLEND = r"E:\MHW-arts\MHW-Valor-new.blend"

CONVERTED_ENTRIES = [
    103, 140, 141, 142, 143, 306, 312, 338, 341,
    339, 340, 343,
    357, 358, 359, 360, 361, 362, 363, 364, 365, 366, 367,
]
CONVERTED = set(CONVERTED_ENTRIES)


def copy_action(src, dst):
    for fc in list(dst.fcurves):
        dst.fcurves.remove(fc)
    for s in src.fcurves:
        d = dst.fcurves.new(s.data_path, index=s.array_index,
                            action_group=s.group.name if s.group else None)
        d.extrapolation = s.extrapolation
        d.color_mode = s.color_mode
        d.keyframe_points.add(len(s.keyframe_points))
        for sk, dk in zip(s.keyframe_points, d.keyframe_points):
            dk.co = sk.co
            dk.interpolation = sk.interpolation
            dk.easing = sk.easing
            dk.handle_left_type = sk.handle_left_type
            dk.handle_right_type = sk.handle_right_type
            dk.handle_left = sk.handle_left
            dk.handle_right = sk.handle_right


def main():
    # import the vanilla origin LMT (its actions arrive with .001-style
    # suffixes because the blend already has same-named actions)
    res = bpy.ops.freehk.import_lmt(filepath=VANILLA_LMT)
    print("vanilla import:", res)

    # index vanilla-imported actions by entry: the freshly imported ones are
    # the highest-generation duplicates of each LMT::w03_00::NNN name
    from collections import defaultdict
    gen = defaultdict(list)
    for a in bpy.data.actions:
        if a.name.startswith("LMT::w03_00::"):
            base = a.name.split(".")[0]
            gen[base].append(a)
    restored, skipped_conv, no_vanilla = 0, 0, []
    for eid in range(0, 465):
        base = "LMT::w03_00::%03d" % eid
        variants = sorted(gen.get(base, []), key=lambda a: a.name)
        if not variants:
            continue
        vanilla_copy = variants[-1]          # freshest import = vanilla
        base_action = None
        for v in variants:
            if v.name == base:
                base_action = v
                break
        if base_action is None:
            no_vanilla.append(eid)
            continue
        if eid in CONVERTED:
            skipped_conv += 1
            continue
        copy_action(vanilla_copy, base_action)
        restored += 1
    print("restored vanilla motion on %d entries; skipped %d converted; "
          "no-vanilla: %s" % (restored, skipped_conv, no_vanilla or "none"))

    fake = 0
    for a in bpy.data.actions:
        if not a.use_fake_user:
            a.use_fake_user = True
            fake += 1
    print("fake users set on %d actions; total %d" % (fake, len(bpy.data.actions)))
    bpy.ops.wm.save_mainfile()
    print("SAVED:", NEW_BLEND)


main()
