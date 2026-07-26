import bpy
import hashlib
import json

bpy.ops.wm.addon_enable(module="mhw_lmt_loader")
from mhw_lmt_loader.lmt.Lmt import LMT

paths = {
    "original": r"E:\MHW-arts\chunk-extracted\chunk\hm\wp\wp03\mot\w03_00\w03_00.lmt",
    "valor": r"E:\MHW-arts\MHW-valor\nativePC\hm\wp\wp03\mot\w03_00\w03_00.lmt",
    "spirit": r"E:\MHW-arts\MHW-MHRS-migrate\spirit-reckoning\nativePC\hm\wp\wp03\mot\w03_00\w03_00.lmt",
}


def canonical(animation):
    if animation is None:
        return None
    paths_out = []
    for path in animation.bone_paths:
        bounds = None
        if path.bounds:
            bounds = [list(path.bounds.mult), list(path.bounds.add)]
        paths_out.append([
            path.buffer_type,
            path.usage,
            path.joint_type,
            path.unkn,
            path.bone_id,
            path.weight,
            list(path.reference_frame),
            bounds,
            path.buffer.hex(),
        ])
    events_out = []
    if hasattr(animation, "events"):
        for event in animation.events.events:
            parameters = []
            for parameter in event.parameters:
                parameters.append([
                    list(parameter.type),
                    [list(data.values) for data in parameter.buffer],
                ])
            events_out.append([list(event.type), parameters])
    value = [
        animation.frame_count,
        animation.loop_frame,
        list(animation.unkn),
        paths_out,
        events_out,
    ]
    encoded = json.dumps(value, separators=(",", ":"), sort_keys=False).encode()
    return hashlib.sha256(encoded).hexdigest()


lmts = {name: LMT(open(path, "rb")) for name, path in paths.items()}
signatures = {
    name: [canonical(lmt.get_animation(i)) for i in range(lmt.entry_count)]
    for name, lmt in lmts.items()
}
base = signatures["original"]
print("ENTRY_COUNTS", {name: lmt.entry_count for name, lmt in lmts.items()})
for name in ("valor", "spirit"):
    values = signatures[name]
    shared = min(len(base), len(values))
    changed = [i for i in range(shared) if values[i] != base[i]]
    added = [i for i in range(shared, len(values)) if values[i] is not None]
    print(name.upper() + "_CHANGED", changed)
    print(name.upper() + "_ADDED", added)
