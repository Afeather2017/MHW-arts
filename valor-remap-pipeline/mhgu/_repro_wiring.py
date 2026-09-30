"""无头复现接线+正式导出:把动作接进 entry 再跑 exportNodeToFile(=GUI 按钮代码路径)。
只写 LMT,不保存 blend。GUI 会话若开着,必须重载文件后才看不到/看得到新接线。

用法:
  ENTRY=103 ACTION=divide_short OUT="E:/MHW-arts/xxx.lmt" \
    blender -b MHW-Valor.blend --python mhgu/_repro_wiring.py
"""
import bpy, importlib, sys, os

ENTRY = int(os.environ.get("ENTRY", "103"))
ACTION = os.environ.get("ACTION", "divide_short")
OUT = os.environ.get("OUT", r"E:\MHW-arts\mhgu-export-repro.lmt")

# 1) 找 entry 的 LMT Entry Node 与其 action node
tree = bpy.data.node_groups["w03_00"]
entry = next(n for n in tree.nodes if n.bl_idname == "LMTEntryNode" and n.entryNum == ENTRY)
inp = next(i for i in entry.inputs if i.name == "LMT Animation")
old = inp.links[0].from_node
print("[wiring] entry%d old <-" % ENTRY, old.name,
      getattr(old, "input_action", None) and old.input_action.name)
# 2) 新建 action node 指向目标动作,照抄旧节点位置
new = tree.nodes.new("LMTActionNode")
new.location = (old.location.x, old.location.y - 200)
new.input_action = bpy.data.actions[ACTION]
links = list(inp.links)
for l in links:
    tree.links.remove(l)
tree.links.new(new.outputs[0], inp)
print("[wiring] entry%d new <-" % ENTRY, new.name, new.input_action.name)
# 3) 正式导出(同 GUI 按钮代码路径)
scn = bpy.context.scene
scn.freehk_tether = bpy.data.objects["m_mesh Armature"]
out = [n for n in tree.nodes if n.bl_idname == "LMTFileNode"][0]
out.filepath = OUT
eo = importlib.import_module("MHW-FreeKinetics.operators.export_ops")
addon = bpy.context.preferences.addons.get(eo.__package__.split(".")[0])
try:
    eo.exportNodeToFile(out, addon.preferences)
    print("[export] OK ->", out.filepath)
except Exception as e:
    import traceback
    traceback.print_exc()
    print("[export] FAILED:", type(e).__name__, e)
