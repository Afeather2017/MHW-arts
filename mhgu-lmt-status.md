# MHGU LMT import status

Date: 2026-08-26

MHGU v67 read-only import is now wired into the Blender 4.3 FreeKinetics
addon. The signed `loopFrame` fix and MHGU `ordinal:boneId` tether lookup
allow real `w07.lmt` files to import and create actions.

The reference `m_body001_out.glb` confirms that LMT bone IDs correspond to
the numeric part after the colon, for example `BoneFunction.001` maps to
the GLB animation node `7:1`. The `mhgu-to-mhw.bmap` file is reserved for
the later MHGU-to-MHW transfer stage.

Skeleton requirement: use `m_body001_out.glb` as the MHGU import/tether
skeleton.  The plain `m_body001.glb` is missing two bones, including the
visible `reference` bone.  LMT bone ID 0 therefore binds to
`pose.bones["reference"]`; ID 0 must not be mapped to
`BoneFunction.000`.  The special on-disk value `0xFF` remains the synthetic
`Root` track.

Current result: when tethered to `m_body001_out.glb`, the imported animation
plays correctly.  Do not use the incomplete `m_body001.glb` for validation.

The GLB `Motion[1]` channels target nodes such as `7:1` (bone id 1), while
the Blender-converted `_out.glb` armature exposes the corresponding
function bones and the additional `reference` bone.  The importer therefore
uses `pose.bones["reference"]` for LMT bone id 0, `BoneFunction.NNN` for
ordinary ids, and `Root` for the special on-disk `0xFF` track.

v67 export is intentionally out of scope for now; the immediate goal is
reliable animation-list and keyframe import.
