# A doc explain bones of MHW armature

## Axis

In blender, z is height, x,y is front/back and left/right.
While in MHW, x,z is front/back and left/right, y is height.

The explaination is in blender axis.

## Special bones

- 000: the root bone, decide the y(height) axis of player
- 249: decide position of left ankle, should bind to 016.
- 250: decide position of right ankle, should bind to 020.
- 247: decide position of right wrist, should bind to 012.
- 248: decide position of left wrist, should bind to 008.
- 253: decide position of camara, bind to gound, I mean, (x,y) of bone 000.
- 251, 252: Unclear, related to weapon positon.


