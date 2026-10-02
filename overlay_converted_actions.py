# -*- coding: utf-8 -*-
"""Overlay the converted valor actions onto the vanilla-imported LMT actions
in MHW-Valor-new.blend (Blender-side; no binary LMT merging).

Workflow:
  1. (GUI, once) FreeKinetics > Import > MHW LMT: import the VANILLA
     w03_00-origin.lmt into MHW-Valor-new.blend. This gives every entry a
     pristine action named "LMT::w03_00::NNN".
  2. (headless) python overlay_converted_actions.py:
     for each converted entry, the matching action from MHW-Valor.blend (the
     old working project) is curve-copied over the vanilla-imported action,
     replacing the animation while keeping the vanilla TIML node wiring.
  3. All actions get Fake users. Save.

The converted entries' TIML (attack windows / input windows / shot removal)
is re-applied to the exported LMT by apply_valor_attack_config.py - the
blend-side TIML for those entries stays vanilla and that is fine.

Run: blender -b MHW-Valor-new.blend --python overlay_converted_actions.py
     (or --python-expr with the body below)
"""
import bpy

OLD_BLEND = r"E:\MHW-arts\MHW-Valor.blend"
NEW_BLEND = r"E:\MHW-arts\MHW-Valor-new.blend"

CONVERTED_ENTRIES = [
    103,               # 樱花 (sa::151)
    140,               # 刚1 (w07::192)
    141, 142, 143,     # 见切 clones (pipeline additions)
    306, 312,          # 见切窗口实验态
    338, 341,          # 一文字族
    339, 340, 343,     # 一文字短/长/无双斩
    357, 358, 359, 360, 361, 362, 363, 364, 365, 366, 367,  # 衔接 motions
]


def copy_action(src, dst):
    """Replace dst's fcurves with src's (all channels)."""
    for fc in list(dst.fcurves):
        dst.fcurves.remove(fc)
    for src_fc in src.fcurves:
        d = dst.fcurves.new(src_fc.data_path, index=src_fc.array_index,
                            action_group=src_fc.group.name if src_fc.group else None)
        d.extrapolation = src_fc.extrapolation
        d.color_mode = src_fc.color_mode
        d.keyframe_points.add(len(src_fc.keyframe_points))
        for sk, dk in zip(src_fc.keyframe_points, d.keyframe_points):
            dk.co = sk.co
            dk.interpolation = sk.interpolation
            dk.easing = sk.easing
            dk.handle_left_type = sk.handle_left_type
            dk.handle_right_type = sk.handle_right_type
            dk.handle_left = sk.handle_left
            dk.handle_right = sk.handle_right


def main():
    # pull the old blend's actions in as temporary data
    bpy.ops.wm.read_homefile(use_factory=False) if False else None
    with bpy.data.libraries.load(OLD_BLEND, link=False) as (src, dst):
        dst.actions = [n for n in src.actions if n.startswith("LMT::w03_00::")]

    done, missing = [], []
    for eid in CONVERTED_ENTRIES:
        name = "LMT::w03_00::%03d" % eid
        src = bpy.data.actions.get(name)
        dst = bpy.data.actions.get(name)
        if src is None or dst is None:
            missing.append((eid, src is None, dst is None))
            continue
        if src == dst:
            done.append(eid)  # same datablock already
            continue
        copy_action(src, dst)
        done.append(eid)

    fake = 0
    for a in bpy.data.actions:
        if not a.use_fake_user:
            a.use_fake_user = True
            fake += 1

    bpy.ops.wm.save_mainfile()
    print("OVERLAY DONE: entries=%s" % done)
    print("MISSING (no source or no target): %s" % (missing if missing else "none"))
    print("fake users set on %d actions; total actions %d" % (fake, len(bpy.data.actions)))
    print("SAVED:", NEW_BLEND)


main()
