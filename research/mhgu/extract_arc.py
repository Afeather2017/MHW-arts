#!/usr/bin/env python3
"""Extract MHGU MT Framework ARC v17 archives without modifying the source."""

from __future__ import annotations

import argparse
import struct
import zlib
from pathlib import Path


ENTRY = struct.Struct("<64sIIII")


def load_extensions(path: Path) -> dict[int, str]:
    extensions: dict[int, str] = {}
    for line in path.read_text(encoding="utf-8-sig").splitlines():
        if not line.strip() or "," not in line:
            continue
        hash_text, extension = (part.strip() for part in line.split(",", 1))
        extensions[int(hash_text, 16)] = extension
    return extensions


def extract(archive: Path, output: Path, extensions: dict[int, str]) -> None:
    data = archive.read_bytes()
    magic, version, count = struct.unpack_from("<4sHH", data)
    if magic != b"ARC\0" or version != 17:
        raise ValueError(f"Unsupported archive header: {magic!r}, version {version}")

    output.mkdir(parents=True, exist_ok=True)
    unknown: set[int] = set()
    for index in range(count):
        base = 12 + index * ENTRY.size
        raw_path, ext_hash, compressed_size, packed_size, offset = ENTRY.unpack_from(data, base)
        internal = raw_path.split(b"\0", 1)[0].decode("utf-8", errors="replace")
        real_size = packed_size & 0x00FFFFFF
        extension = extensions.get(ext_hash)
        if extension is None:
            extension = f".{ext_hash:08X}"
            unknown.add(ext_hash)

        payload = data[offset : offset + compressed_size]
        decoded = zlib.decompress(payload) if compressed_size != real_size else payload
        if len(decoded) != real_size:
            raise ValueError(
                f"Size mismatch for {internal}: expected {real_size}, got {len(decoded)}"
            )

        destination = output / Path(internal + extension)
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(decoded)

    print(f"{archive.name}: extracted {count} files to {output}")
    if unknown:
        print("Unknown extension hashes: " + ", ".join(f"{value:08X}" for value in sorted(unknown)))


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("archive", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--hashes", type=Path, required=True)
    args = parser.parse_args()
    extract(args.archive, args.output, load_extensions(args.hashes))


if __name__ == "__main__":
    main()
