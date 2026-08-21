# A doc explain bones of MHW armature

## Axis

In blender, z is height, x,y is front/back and left/right.
While in MHW, x,z is front/back and left/right, y is height.

The explaination is in blender axis.

## Special bones

- 000: the root bone, decide the y(height) axis of player
- 249: decide position of left ankle, should bind to 016.
- 250: decide position of right ankle, should bind to 020.
- 248: decide position of left wrist, should bind to 008, its rotation decide the sword position if it bind to left hand.
- 247: decide position of right wrist, should bind to 012, its rotation decide the sword position if it bind to right hand.
- 253: decide position of camara, bind to gound, I mean, (x,y) of bone 000.
- 251, 252: Unknow
- 030: Related to sheath, not very clear.

## Weapon holding

Only y scale of 030 and 047 change the binding state. x,z scale defaults to 1, they have no effect.

| 030 | 047 | Left   | Right  | Back         |
|-----|-----|--------|--------|--------------|
| 1   | 1   | None   | None   | Sheath&Sword |
| 1   | 2   | None   | Sword  | Sheath       |
| 2   | 1   | Sword  | None   | Sheath       |
| 2   | 2   | None   | Sword  | Sheath       |
| 3   | 1   | None   | Sheath | Sword        |
| 3   | 2   | Sword  | Sheath | None         |

XYZ location, XYZW rotation have some effect, not very clear.
