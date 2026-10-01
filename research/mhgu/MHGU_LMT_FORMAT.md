# MHGU LMT (v67) Format — Reverse-Engineering Notes for freekinetics Support

Cross-referenced from three sources:
- **RevilLib** (`research/RevilLib-current/src/mtf_lmt/`) — authoritative C++ implementation; MHGULmtTool uses it successfully on these files.
- **MT Framework.mzp** (`research/MTFramework-mzp/`, 3ds Max scripts, UTF-16LE) — the classic MHXX-era LMT importer.
- **Real files** (`research/mhgu/extracted/…`) — `w07.lmt` (256 entries), `co_0.lmt` (171), `w07_sa.lmt` (152).

MHGU = MT Framework **LMT version 67 (0x43)**, **x86 (32-bit offsets)** — "generic MTF v2 format" in RevilLib's version table.
MHW = version **95**, x64. freekinetics (`tools/MHW-FreeKinetics/struct/Lmt.py`) currently parses only v95.

---

## 1. File header

| Field | MHW (v95) | MHGU (v67) |
|---|---|---|
| signature | `LMT\0` | `LMT\0` |
| version `u16` | 95 | **67** |
| entryCount `u16` | ✓ | ✓ |
| filler | `unkn byte[8]` | **none** |
| offsets | `uint64[n]` @ 0x10 | **`uint32[n]` @ 0x0C** |
| alignment | 16 after table | table padded to 16 |

Verified on disk: `12 + 4·n` rounded up to 16 = offset of first action header
(co_0: 12+684=0x2B8→0x2C0 ✓; w07: 0x410 ✓; w07_sa: 0x26C→0x270 ✓).

## 2. Action header ("Animation" in RevilLib, 0x3C bytes, 64-byte aligned)

| Off | Type | Field | MHW equivalent |
|---|---|---|---|
| +0x00 | u32 | `tracks` ptr (KBR/bone data) | `fcurveOffset` (u64 in MHW) |
| +0x04 | u32 | `numTracks` | `fcurveCount` |
| +0x08 | u32 | `numFrames` | `frameCount` |
| +0x0C | i32 | `loopFrame` | `loopFrame` (-1 = no loop, seen on disk) |
| +0x10 | f32×4 | `endFrameAdditiveScenePosition` | `Vec0` (root-motion translation tail) |
| +0x20 | f32×4 | `endFrameAdditiveSceneRotation` | `Vec2` (quaternion, w=1.0 observed) |
| +0x30 | u32 | `flags` (0x80000000 observed) | Flags/Flags2 |
| +0x34 | u32 | `events` ptr → AnimationEvent | `timlOffset` (TIML) |
| +0x38 | u32 | `floats` ptr → FloatTracks | **no MHW equivalent** |

Action headers share the `tracks` pointer between entries exactly like MHW
`fcurveOffset` sharing — the importer's `kbrNodes` dedup logic applies unchanged.

## 3. Bone track header ("BoneTrack", 36 bytes each, numTracks of them at `tracks`)

| Off | Type | Field | MHW equivalent (48-byte header) |
|---|---|---|---|
| +0 | u8 | `bufferType` (codec id) | ✓ same |
| +1 | u8 | `usage` (0-2 local rot/pos/scale, 3-5 absolute) | ✓ same |
| +2 | u8 | `boneType` | `jointType` |
| +3 | u8 | **`boneID` (u8!)** — `0xFF` = root | `boneId` i32 (MHW uses **-1** for root) |
| +4 | f32 | `weight` | ✓ |
| +8 | u32 | `bufferSize` (bytes) | ✓ |
| +0x0C | u32 | `buffer` ptr → keyframes | `bufferOffset` (u64) |
| +0x10 | f32×4 | `referenceData`/basis | `basis` ✓ |
| +0x20 | u32 | `extremes` ptr → TrackMinMax | `lerpOffset` (u64) |

`boneID == 0xff → -1` confirmed in RevilLib `LMTTrackMidInterface::BoneIndex()`.

## 4. TrackMinMax (extremes, 32 bytes) — the "lerp basis"

Disk order (REFLECT min, max):
```
+0x00 f32[4]  min   ← stores (trueMin − trueMax)  [negative extent]
+0x10 f32[4]  max   ← stores trueMax
```
Decode (RevilLib `additiveLerp`): `value = max + min_extent * (raw / mask)`.

freekinetics `Keyframe.lerp`: `vec * mult + add` → bridge as
**`mult = disk.min`, `add = disk.max`** with normalization `raw/mask` (see §5).

## 5. Keyframe buffer codecs — IDs match freekinetics' MHW `typeMapping`

RevilLib `buffRemapRegistry` row 3 (v2 formats, v56+ ⇒ v67):

| ID | RevilLib codec | Layout | freekinetics class |
|---|---|---|---|
| 1 | SingleVector3 | 3×f32, no frame field (implicit +1) | `floatVectorBase` |
| 2 | SingleRotationQuat3 | 3×f32 quat xyz (w=√(1−…)), step +1 | `floatRotKey` |
| 3 | LinearVector3 | 3×f32 + u32 frame delta | `floatVectorKey` |
| 4 | BiLinearVector3_16bit | 3×u16 + u16 frame, extremes | `shortVectorInterpolation` |
| 5 | BiLinearVector3_8bit | 3×u8 + u8 frame, extremes | `byteVectorInterpolation` |
| 6 | LinearRotationQuat4_14bit | 2×u32 bitpacked (w,z,y,x 14-bit + 8-bit frame) | `quaternion14Key` |
| 7 | BiLinearRotationQuat4_7bit | u32 (4×7-bit + 4-bit frame), extremes | `quaternion7Interpolation` |
| 11/12/13 | XW/YW/ZW 14-bit | u32 (var 14-bit + w 14-bit + 4-bit frame), extremes | `XW/YW/ZWQuaternionUnion` |
| 14 | BiLinearRotationQuat4_11bit | 3×u16 (4×11-bit + 4-bit frame), extremes | `quaternion11Interpolation` |
| 15 | BiLinearRotationQuat4_9bit | 5×u8 (4×9-bit + 4-bit frame), extremes | `quaternion9Interpolation` |

- **Frames are relative deltas**, accumulated per key (`importer already does this` at `lmt_importer.py:50-53`).
- Types 1/2 carry no frame field → each key implies **frame += 1** (RevilLib `Buf_SingleVector3::GetFrame`).
- Bit layouts are identical to MHW (freekinetics' bit readers can be reused).

### ⚠ Normalizer divergence (why MHGU must not reuse MHW `marshall` paths)

freekinetics MHW normalizers use empirically-tuned `offset=8, mulmin=7` constants
(e.g. u16: `(raw−8)/65520`), but RevilLib decodes v67 as plain **`raw / mask`**
(u16: `/65535`, u8: `/255`, 7-bit: `/127`, 11-bit: `/2047`, 9-bit: `/511`, 14-bit XW/W: `/16383`).
The one exception is codec 6 (14-bit quat), where both use the same signed
`±raw·(4/16383)`-style math (freekinetics `doubleSign14`, MaxScript `0.000244156`).

⇒ MHGU parsing must decode with `raw/mask` itself, then set `x/y/z/w/frame`
attributes directly on freekinetics codec instances (bypassing their `marshall`),
so the Blender import + MHW export pipeline stays untouched.

## 6. Events & FloatTracks (not needed for bone-animation migration v1)

- `events` → `AnimationEvent { groups[4] of AnimEvents { eventRemaps u16[32], numEvents u32, events ptr → AnimEvent[8] } }` — MHGU's event/TIML analog. Different structure from MHW TIML; skip + log in v1, re-author events in MHW tooling.
- `floats` → `FloatTracks { groups[4] of FloatTrack { componentRemaps u8[4], numFloats u32, frames ptr → FloatFrame[16B] } }` — per-frame float channels. In MHGU these carry weapon grip/sheath states (MHW equivalent = MhBone_030/047 scale-Y integer channels, per project memory). Future work: bridge to MHW scale channels on export.

## 7. Migration architecture decision

Parse MHGU binary → **populate the existing MHW object model** (`LMT`,
`LMTActionHeader`, `LMTBoneDataHeader`, codec instances from `LmtBuffers`), with:
- `boneId` 0xFF → −1 (root) so `"Root"` bind naming works,
- types 1/2 given `frame = 1` per key so relative-frame accumulation works,
- MHGU 2/3 remap to MHW codec ids directly (identical semantics),
- `lerp = LMTBasis(mult=disk.min, add=disk.max)`.

Everything downstream — `lmt_importer.LMTFileLoad`, node tree, Rig Transfer/bmap,
and **export to MHW v95 LMT** — then works without modification.
