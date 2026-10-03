"""把 w03_00 树重排成配对布局: 每个 entry 一行 —— action 在左, entry 在右,
该 entry 的 TIML data + timl action 竖排在其正下方, 行按 entryNum 升序。
顺带: 全部节点取消折叠(hide=False), 删除失去连线的死节点。

用法:
  blender -b MHW-Valor.blend --python mhgu/_relayout_tree.py
"""
import bpy

main = bpy.data.node_groups["w03_00"]
GRID = 20.0

# ---- 0. drop dead nodes (no output links) among plumbing types ----
def has_outputs(n):
    return any(s.links for s in n.outputs)

dead = [n for n in main.nodes
        if n.bl_idname in ("LMTActionNode", "TIMLActionNode", "TIMLDataNode")
        and not has_outputs(n)]
for n in dead:
    main.nodes.remove(n)
print("[layout] dead nodes removed:", len(dead))

# ---- 1. collect + unhide ----
entries = sorted((n for n in main.nodes if n.bl_idname == "LMTEntryNode"),
                 key=lambda n: n.entryNum)
for n in main.nodes:
    try:
        n.hide = False
    except Exception:
        pass

X_ACTION = 0
X_ENTRY = 420
X_FILE = 840

placed = set()
y = 0.0
for entry in entries:
    anim = entry.inputs["LMT Animation"].links[0].from_node \
        if entry.inputs["LMT Animation"].links else None
    timl = entry.inputs["TIML Data"].links[0].from_node \
        if entry.inputs["TIML Data"].links else None
    entry.location = (X_ENTRY, y)
    if anim is not None and anim not in placed:
        anim.location = (X_ACTION, y)
        placed.add(anim)
    row_h = 400.0
    if timl is not None and timl not in placed:
        placed.add(timl)
        timl.location = (X_ENTRY, y - 360.0)
        tacts = [l.from_node for l in timl.inputs["TIML Animation"].links]
        ty = y - 360.0 - 320.0
        for k, ta in enumerate(tacts):
            if ta in placed:
                continue
            placed.add(ta)
            ta.location = (X_ENTRY, ty - k * 110.0)
        row_h = 400.0 + 320.0 + len(tacts) * 110.0 + 60.0
    y -= row_h

out = next(n for n in main.nodes if n.bl_idname == "LMTFileNode")
out.location = (X_FILE, y / 2.0)
print("[layout] rows:", len(entries), "final y:", y,
      "placed action/timl nodes:", len(placed))

bpy.ops.wm.save_mainfile()
print("[layout] saved.")
