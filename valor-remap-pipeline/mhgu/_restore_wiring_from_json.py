# 把 0.0.1 的接线快照恢复到当前 MHW-Valor.blend (只改连线, 不动任何动作数据, 不清理孤儿)。
# 前提: SAVE=1 才会保存。任何所需动作数据块缺失 -> 中止不保存。
# 用法: WIRING=E:/MHW-arts/_wiring_001.json SAVE=1 blender -b MHW-Valor.blend --python _restore_wiring_from_json.py
import bpy, os, json, sys

WIRING = os.environ.get("WIRING", r"E:\MHW-arts\_wiring_001.json")
SAVE = os.environ.get("SAVE", "0") == "1"

with open(WIRING, "r", encoding="utf-8") as f:
    snap = json.load(f)

tree = bpy.data.node_groups["w03_00"]
log = []
missing = []

def entry_node(en):
    return next((n for n in tree.nodes if n.bl_idname == "LMTEntryNode" and int(n.entryNum) == en), None)

def lmt_link_of(e):
    sock = next(i for i in e.inputs if i.name == "LMT Animation")
    return (sock.links[0].from_node, sock) if sock.links else (None, sock)

def data_node_of(e):
    sock = next(i for i in e.inputs if i.name == "TIML Data")
    return (sock.links[0].from_node, sock) if sock.links else (None, sock)

def timl_links_of(d):
    sock = next(i for i in d.inputs if i.name == "TIML Animation")
    return [l.from_node for l in sock.links], sock

def node_action(n):
    a = getattr(n, "input_action", None)
    return a.name if a else None

# ---------- pass 0: 校验所有目标动作数据块存在 ----------
targets = {}  # en -> {"lmt": rec, "tims": [recs]}
for en_s, e in snap.items():
    en = int(en_s)
    lmts = e["lmt"]
    tims = e["timl"][0]["tims"] if e["timl"] else []
    if not lmts:
        continue
    targets[en] = {"lmt": lmts[0], "tims": tims}
    if lmts[0]["action"] and lmts[0]["action"] not in bpy.data.actions:
        missing.append(f"LMT action {lmts[0]['action']} (E{en})")
    for t in tims:
        if t["action"] and t["action"] not in bpy.data.actions:
            missing.append(f"TIML action {t['action']} (E{en})")
if missing:
    print("ABORT: missing action datablocks in current file:")
    for m in sorted(set(missing)):
        print("  " + m)
    sys.exit(3)

# ---------- pass 1: 断开受影响 entry 的旧线 ----------
touched_lmt, touched_timl = [], []
for en, tgt in sorted(targets.items()):
    e = entry_node(en)
    if e is None:
        print(f"ABORT: E{en} entry node not found"); sys.exit(3)
    cur, sock = lmt_link_of(e)
    if cur and node_action(cur) != tgt["lmt"]["action"]:
        for l in list(sock.links):
            tree.links.remove(l)
        touched_lmt.append(en)
    if not tgt["tims"]:
        continue  # 0.0.1 里该 entry 本来就没有 TIML 数据线 (E433/435/437)
    d, dsock = data_node_of(e)
    if d is None:
        print(f"ABORT: E{en} has no TIML Data link"); sys.exit(3)
    cur_t, tsock = timl_links_of(d)
    tgt_names = [t["action"] for t in tgt["tims"]]
    cur_names = [node_action(n) for n in cur_t]
    if cur_names != tgt_names:
        for l in list(tsock.links):
            tree.links.remove(l)
        touched_timl.append(en)

print(f"pass1: rewiring LMT on {touched_lmt}")
print(f"pass1: rewiring TIML on {touched_timl}")

# ---------- pass 2: 按 0.0.1 快照重连 ----------
used_lmt, used_timl = set(), set()

def pick_node(tree, want_action, rec, kind, used):
    # 1) 按快照记录的节点名找, 且动作名一致 (名字可能被新建节点复用, 必须双重校验)
    n0 = tree.nodes.get(rec["node"])
    if n0 is not None and node_action(n0) == want_action and n0 not in used:
        return n0, "by-name"
    # 2) 找空闲 (无输出连线) 且动作一致的节点
    for n in tree.nodes:
        if n.bl_idname == kind and node_action(n) == want_action and n not in used:
            if not any(l.to_socket.name == "LMT Animation" or l.to_socket.name == "TIML Animation"
                       for o in n.outputs for l in o.links):
                return n, "free"
    # 3) 新建
    n = tree.nodes.new(kind)
    n.location = (rec["pos"][0], rec["pos"][1])
    n.input_action = bpy.data.actions[want_action]
    return n, "new"

for en, tgt in sorted(targets.items()):
    e = entry_node(en)
    _, sock = lmt_link_of(e)
    if not sock.links:
        want = tgt["lmt"]["action"]
        n, how = pick_node(tree, want, tgt["lmt"], "LMTActionNode", used_lmt)
        used_lmt.add(n)
        tree.links.new(n.outputs[0], sock)
        log.append(f"E{en:<4d} LMT <- {want} [{how} node '{n.name}']")
    if not tgt["tims"]:
        continue
    d, _ = data_node_of(e)
    _, tsock = timl_links_of(d)
    if not tsock.links and tgt["tims"]:
        for t in tgt["tims"]:
            n, how = pick_node(tree, t["action"], t, "TIMLActionNode", used_timl)
            used_timl.add(n)
            tree.links.new(n.outputs[0], tsock)
        log.append(f"E{en:<4d} TIML <- {len(tgt['tims'])} nodes restored")

print("")
print("=== restored ===")
for s in log:
    print(s)

# ---------- pass 3: 终检, 全表对比快照 ----------
bad = []
for en_s, e in snap.items():
    en = int(en_s)
    node = entry_node(en)
    if node is None:
        bad.append(f"E{en} missing"); continue
    cur, _ = lmt_link_of(node)
    want = e["lmt"][0]["action"] if e["lmt"] else None
    got = node_action(cur) if cur else None
    if got != want:
        bad.append(f"E{en} LMT want={want} got={got}")
    want_t = [t["action"] for t in (e["timl"][0]["tims"] if e["timl"] else [])]
    if not want_t:
        continue  # 0.0.1 无 TIML 数据线的 entry (E433/435/437)
    d, _ = data_node_of(node)
    if d is None:
        bad.append(f"E{en} no data node"); continue
    cur_t, _ = timl_links_of(d)
    got_t = [node_action(n) for n in cur_t]
    if got_t != want_t:
        bad.append(f"E{en} TIML want={len(want_t)} got={len(got_t)}: {got_t[:4]}...")
if bad:
    print("VERIFY FAILED:")
    for b in bad:
        print("  " + b)
    sys.exit(4)
print("VERIFY OK: all", len(snap), "entries match 0.0.1 wiring")

if SAVE:
    bpy.ops.wm.save_mainfile()
    print("SAVED", bpy.data.filepath)
else:
    print("DRY-RUN (no save)")
