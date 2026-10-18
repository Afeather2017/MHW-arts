# -*- coding: utf-8 -*-
"""Point the custom LS moves' damage references (CTO CollisionTimelineObject ノードID)
at vanilla damage rows. 2026-10-18 user request; re-runnable after every blend export.

Routing (COL_AttackBox_Research): TIML ノードID -> wp03.col names[unk4] -> {cgp=CLGM bank,
move=ATK row}. Changing the node swaps BOTH damage and hitbox geometry (bank side effect).

大居合 4 versions (user 2026-10-18): 308=無/309=白/310=黄/311=赤 -> ATK[46]=17/[47]=19/
[48]=31/[49]=39, nodes 86/87/88/89, all bank9. Each variant is single-node: all its
damage windows share one row.

刚1   156: node 86 (無,17) -> 89 居合気刃1赤 (ATK[49]=39, bank9) = 居合(lmt311, 赤刃版).
一文字 339 (短)  : node 63 (見切り攻撃, ATK[25]=12, bank8) -> 54 気刃斬り2 (ATK[17]=30, bank0)
一文字 340 (長)  : node 63 -> 54   = 气刃2(lmt106)的伤害
一文字 342 (納刀取消): node 56 (斬り下がり, ATK[14]=22, bank0) -> 54 (bank0: geometry unchanged)
无双斩 343: DUAL-node (vanilla LMT155 per-segment pattern, f0=48->f25=60):
      key f0 = 88 居合気刃1黄 (ATK[48]=31), key f58 = 89 赤 (ATK[49]=39) ->
      blade1 window [31,39] deals 31, blade2 window [58,70] deals 39 (2nd > 1st).
      Structural edit: 2-key array appended at align16(EOF), track xoff/xcount repointed.
"""
import shutil
import struct
import sys
import time

sys.path.insert(0, r"E:\MHW-arts")
import dump_timl_frames as D
from patch_timl_defense import iter_tracks

LMT = r"E:\SteamLibrary\steamapps\common\Monster Hunter World\nativePC\hm\wp\wp03\mot\w03_00\w03_00.lmt"
CAT = "CollisionTimelineObject"
NAME = "ノードID[0]"
KEY_SIZE = 20

# entry -> (expect_old_or_current, new)  single-key swaps
PLAN = {156: (86, 89), 339: (63, 54), 340: (63, 54), 342: (56, 54)}
# entry -> (blade1_node, blade2_node, switch_frame)  dual-key rebuild
DUAL_PLAN = {343: (88, 89, 58.0)}


def find_track_header(buf, eid):
    """Return (header_pos, xoff, xcount) of entry's CollisionTimelineObject ノードID[0]."""
    n, actions = D.parse_lmt(buf)
    timl = actions[eid]["timlOffset"]
    typesOffset, typeCount = struct.unpack_from("<QQ", buf, timl)
    for ti in range(typeCount):
        p_types = typesOffset + 24 * ti
        toff, tcount, tlHash, _ = struct.unpack_from("<QQIi", buf, p_types)
        if not toff or CAT not in D.tname(tlHash):
            continue
        for xi in range(tcount):
            p_tr = toff + 24 * xi
            xoff, xcount, dtHash, dt = struct.unpack_from("<QQIi", buf, p_tr)
            if not xoff or not xcount or dt != 1:
                continue
            if D.dname(dtHash)[0] == NAME:
                return p_tr, xoff, xcount
    raise SystemExit("entry %d: %s track not found" % (eid, NAME))


def node_track(buf, eid):
    hits = [t for t in iter_tracks(buf, eid) if CAT in t["cat"] and t["name"] == NAME]
    assert len(hits) == 1, "entry %d: %d tracks named %s" % (eid, len(hits), NAME)
    return hits[0]["keys"]


buf = bytearray(open(LMT, "rb").read())
changed = 0

for eid, (old, new) in sorted(PLAN.items()):
    keys = node_track(buf, eid)
    assert len(keys) == 1, "entry %d: %d keys on %s" % (eid, len(keys), NAME)
    k = keys[0]
    if k["value"] == new:
        print("entry %-3d: node already %d (skip)" % (eid, new))
        continue
    assert k["value"] == old, "entry %d: node %d != expect %d" % (eid, k["value"], old)
    struct.pack_into("<I", buf, k["offset"], new)
    changed += 1
    print("entry %-3d: node %d -> %d" % (eid, old, new))

for eid, (n1, n2, switch) in sorted(DUAL_PLAN.items()):
    keys = node_track(buf, eid)
    if len(keys) == 2 and keys[0]["value"] == n1 and keys[1]["value"] == n2 \
            and abs(keys[1]["frame"] - switch) < 0.01:
        print("entry %-3d: dual nodes (%d@f%.0f, %d@f%.0f) already (skip)"
              % (eid, n1, keys[0]["frame"], n2, keys[1]["frame"]))
        continue
    assert len(keys) == 1, "entry %d: unexpected %d keys" % (eid, len(keys))
    orig = bytes(buf[keys[0]["offset"]:keys[0]["offset"] + KEY_SIZE])
    key1 = bytearray(orig)
    struct.pack_into("<I", key1, 0, n1)                      # frame stays as-is (f0)
    key2 = bytearray(orig)
    struct.pack_into("<I", key2, 0, n2)
    struct.pack_into("<f", key2, 12, switch)
    blob = bytes(key1) + bytes(key2)
    start = (len(buf) + 15) & ~15                            # align16(EOF), apply_valor recipe
    buf.extend(b"\x00" * (start - len(buf)))
    buf += blob
    p_tr, _, _ = find_track_header(buf, eid)
    struct.pack_into("<QQ", buf, p_tr, start, 2)             # repoint xoff, xcount=2
    changed += 1
    print("entry %-3d: dual nodes %d@f%.0f + %d@f%.0f (rebuilt from %d)"
          % (eid, n1, float(struct.unpack_from("<f", key1, 12)[0]), n2, switch, keys[0]["value"]))

if changed:
    backup = LMT + ".bak-" + time.strftime("%Y%m%d-%H%M%S") + "-dmgref"
    shutil.copy2(LMT, backup)
    with open(LMT, "r+b") as h:
        h.write(buf)
    print("backup:", backup)
print("written: %s (%d entries changed)" % (LMT, changed))

# verify from disk
buf2 = open(LMT, "rb").read()
for eid, (old, new) in sorted(PLAN.items()):
    keys = node_track(buf2, eid)
    print("verify entry %-3d: node=%d %s" % (eid, keys[0]["value"],
          "OK" if keys[0]["value"] == new else "FAIL"))
for eid, (n1, n2, switch) in sorted(DUAL_PLAN.items()):
    keys = node_track(buf2, eid)
    ok = (len(keys) == 2 and keys[0]["value"] == n1 and keys[1]["value"] == n2)
    print("verify entry %-3d: keys=%s %s" % (eid,
          " ".join("%.0f:%d" % (k["frame"], k["value"]) for k in keys),
          "OK" if ok else "FAIL"))
