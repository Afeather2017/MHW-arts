# 从 0.0.1 提取 w03_00 树完整接线快照 -> JSON (LMT link + 每 entry 的 TIML 有序名单)。
# 只读, 永不保存 blend。
# 用法: OUT=/e/.../_wiring_001.json blender -b MHW-Valor-0.0.1.blend --python _extract_wiring_json.py
import bpy, os, json

tree = bpy.data.node_groups["w03_00"]

snapshot = {}
for n in tree.nodes:
    if n.bl_idname == 'LMTEntryNode':
        en = int(getattr(n, "entryNum", -1))
        e = snapshot.setdefault(str(en), {"lmt": [], "timl": []})
        for i in n.inputs:
            for l in i.links:
                if i.name == "LMT Animation":
                    a = getattr(l.from_node, "input_action", None)
                    e["lmt"].append({
                        "action": a.name if a else None,
                        "node": l.from_node.name,
                        "pos": [round(l.from_node.location.x, 1), round(l.from_node.location.y, 1)],
                    })
                elif i.name == "TIML Data":
                    d = l.from_node
                    tims = []
                    for i2 in d.inputs:
                        for l2 in i2.links:
                            if i2.name == "TIML Animation":
                                a = getattr(l2.from_node, "input_action", None)
                                tims.append({
                                    "action": a.name if a else None,
                                    "node": l2.from_node.name,
                                    "pos": [round(l2.from_node.location.x, 1), round(l2.from_node.location.y, 1)],
                                })
                    e["timl"].append({"data_node": d.name, "pos": [round(d.location.x, 1), round(d.location.y, 1)], "tims": tims})

out_path = os.environ.get("OUT", r"E:\MHW-arts\_wiring_001.json")
with open(out_path, "w", encoding="utf-8") as f:
    json.dump(snapshot, f, ensure_ascii=False, indent=1)

print("WROTE", out_path, "entries:", len(snapshot))
multi = {k: v for k, v in snapshot.items() if len(v["lmt"]) > 1 or len(v["timl"]) > 1}
print("entries with >1 LMT or >1 data node:", list(multi.keys()) or "none")
