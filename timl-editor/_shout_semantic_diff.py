# 全 entry TIML 语义 diff (dataType=1 轨): python _shout_semantic_diff.py <a.lmt> <b.lmt>
import struct, sys
import dump_timl_frames as D
from patch_timl_defense import iter_tracks

a_path, b_path = sys.argv[1], sys.argv[2]
a_buf = open(a_path, "rb").read()
b_buf = open(b_path, "rb").read()

def snapshot(buf, entry):
    out = []
    for t in iter_tracks(buf, entry):
        out.append((t["cat"], t["name"], tuple((round(k["frame"]), k["value"]) for k in t["keys"])))
    return out

na = len(D.parse_lmt(a_buf)[1])
nb = len(D.parse_lmt(b_buf)[1])
print("entries: %d vs %d" % (na, nb))
keys_a = sorted(D.parse_lmt(a_buf)[1].keys())
keys_b = sorted(D.parse_lmt(b_buf)[1].keys())
if keys_a != keys_b:
    print("!! entry id sets differ: A-only %s, B-only %s" % (
        sorted(set(keys_a) - set(keys_b)), sorted(set(keys_b) - set(keys_a))))
diffs = []
errs = []
for e in sorted(set(keys_a) & set(keys_b)):
    try:
        sa, sb = snapshot(a_buf, e), snapshot(b_buf, e)
    except Exception as ex:
        ea = eb = None
        try: snapshot(a_buf, e)
        except Exception as x: ea = repr(x)[:80]
        try: snapshot(b_buf, e)
        except Exception as x: eb = repr(x)[:80]
        errs.append((e, ea, eb))
        continue
    if sa != sb:
        diffs.append(e)
        print("=== entry %d differs" % e)
        onlya = [t for t in sa if t not in sb]
        onlyb = [t for t in sb if t not in sa]
        for t in onlya:
            print("  A-only [%s] %s: %s" % (t[0], t[1], t[2]))
        for t in onlyb:
            print("  B-only [%s] %s: %s" % (t[0], t[1], t[2]))
if errs:
    print("=== parse errors:")
    for e, ea, eb in errs:
        print("  entry %d: A=%s B=%s" % (e, ea, eb))
if not diffs and not errs:
    print("NO semantic differences (dataType=1 tracks)")
