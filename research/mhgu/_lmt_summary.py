"""Quick v67 LMT entry summary: entry count, non-empty tracks, frames."""
import struct
import sys
from pathlib import Path


def summarize(path: Path) -> None:
    data = path.read_bytes()
    magic, version, count = struct.unpack_from("<4sHH", data)
    if magic != b"LMT\0":
        print(f"{path.name}: not LMT (magic={magic!r})")
        return
    offsets = struct.unpack_from(f"<{count}I", data, 12)
    nonempty = 0
    frames = []
    tracks = []
    for off in offsets:
        if off == 0:
            frames.append(None)
            tracks.append(0)
            continue
        nonempty += 1
        # action header: +0x04 numTracks u32, +0x08 numFrames u32
        num_tracks, num_frames = struct.unpack_from("<II", data, off + 4)
        frames.append(num_frames)
        tracks.append(num_tracks)
    valid_frames = [f for f in frames if f]
    print(f"{path.name}: v{version}, {count} slots, {nonempty} non-empty")
    if valid_frames:
        print(f"  frames: min={min(valid_frames)} max={max(valid_frames)}")
    nonzero_tracks = sum(1 for t in tracks if t)
    print(f"  entries with tracks>0: {nonzero_tracks}")
    # first 40 non-empty entries: idx (frames, tracks)
    shown = [(i, frames[i], tracks[i]) for i in range(count) if offsets[i]]
    sample = ", ".join(f"{i}({f}f/{t}t)" for i, f, t in shown[:40])
    print(f"  first entries: {sample}")


if __name__ == "__main__":
    for arg in sys.argv[1:]:
        summarize(Path(arg))
