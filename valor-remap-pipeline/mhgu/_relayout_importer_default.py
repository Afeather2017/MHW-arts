"""把 w03_00 树重排成 FreeKinetics 导入器的默认布局(复刻 LMTFileLoad/uiNodeManager):
四列 —— x=0 action 节点(LMT action 与其 TIML action 相邻堆叠), x=300 TIML data,
x=600 entry, x=900 文件节点;每列独立游标, 按 entry 顺序(=导入顺序)推进,
节点折叠(hide=True, 即导入操作器的默认状态)。

用法:
  blender -b MHW-Valor.blend --python mhgu/_relayout_importer_default.py
"""
import bpy

main = bpy.data.node_groups["w03_00"]
GRID = 20.0
H = GRID * 15.0          # HORIZONTAL
ACT = GRID * 5.0         # action node pitch
DATA = GRID * 14.0       # timl data pitch
ENTRY = GRID * 15.0      # lmt entry pitch
FILE = GRID * 6.0
HIDE = GRID * 1.5        # collapsed pitch (importer hide=True default)

c0 = c1 = c2 = c3 = 0.0   # fresh-tree cursors
placed = set()
entries = sorted((n for n in main.nodes if n.bl_idname == "LMTEntryNode"),
                 key=lambda n: n.entryNum)

for entry in entries:
    entry.location = (2 * H, -c2)
    c2 += HIDE if True else ENTRY
    try:
        entry.hide = True
    except Exception:
        pass

    anim = entry.inputs["LMT Animation"].links[0].from_node \
        if entry.inputs["LMT Animation"].links else None
    if anim is not None and anim not in placed:
        placed.add(anim)
        anim.location = (0.0, -c0)
        c0 += HIDE
        anim.hide = True

    timl = entry.inputs["TIML Data"].links[0].from_node \
        if entry.inputs["TIML Data"].links else None
    if timl is not None and timl not in placed:
        placed.add(timl)
        timl.location = (1 * H, -c1)
        c1 += HIDE
        timl.hide = True
        for l in timl.inputs["TIML Animation"].links:
            ta = l.from_node
            if ta in placed:
                continue
            placed.add(ta)
            ta.location = (0.0, -c0)
            c0 += HIDE
            ta.hide = True

out = next(n for n in main.nodes if n.bl_idname == "LMTFileNode")
out.location = (3 * H, -c3)
out.hide = True

# sanity: wiring intact
ok = sum(1 for e in entries if e.inputs["LMT Animation"].links)
okt = sum(1 for e in entries if e.inputs["TIML Data"].links)
print("[layout] entries=%d anim-linked=%d timl-linked=%d nodes-placed=%d cols(y): act=%.0f data=%.0f entry=%.0f"
      % (len(entries), ok, okt, len(placed), c0, c1, c2))

bpy.ops.wm.save_mainfile()
print("[layout] saved.")
