# Hold Tuner

A World Editor plugin for placing a carried item in the hand. You pick the item, see it held by a
character, nudge it until it sits right, and save the values into the item prefab. Ctrl+Z undoes the save.

## What decides how an item is held

Measured in a session with the harness verb `heldxf` (`docs/asset-pipeline/states.md`, "Hand"):

- The item hangs from the character's **`LeftHandProp`** bone. With no offset, the item's origin and axes
  are exactly the bone's. No item has a grip socket or a hand IK target.
- The **anim set** the item borrows (`InventoryItemComponent > Attributes > ItemAnimationAttributes >
  AnimationAttachments`, bound as `Gadget`) poses the arm and moves that bone.
- **`HandAdditiveTranslation`** (metres, in the bone's frame) and **`HandAdditiveRotation`** (degrees
  `a b c`; the item's axes in the bone frame are `Rz(c) * Rx(a) * Ry(b)`) move the item off the bone.
  The tuner edits these two values.

If the anim set is wrong, no offset fixes it. The mod's `player_carry` set raises the arm across the chest,
but it leaves `LeftHandProp` about a metre to the left at shoulder height (bone at `-0.70, 1.45, 0.26` in
the body frame, hand at `0.30, 1.53, 0.22`). That is why the cash sack floated a metre from the body.
The vanilla jerrycan set (`Assets/Items/Fuel/workspaces/Jerrycan.agr` + `Jerrycan_player.asi`) carries
one-handed down by the side, with the hand at 0.93 m. Use it for anything carried by a handle or a neck.

## Use it

1. Open any world in the World Editor and make an unlocked layer active (the preview entities go there).
2. **Plugins > Life Framework > Hold Tuner** (Ctrl+Shift+H).
3. Pick the **item** prefab (for example `Prefabs/Items/Roleplay/LF_MoneyBag.et`). The character defaults to
   a vanilla civilian. Press **Preview**. A character and the item appear in the middle of the view, and the
   camera frames the hand. The item starts at the prefab's current values.
4. Nudge it live with the **LF Hold Tuner** tool (World Editor toolbar, hand icon). Its panel has
   **Offset X/Y/Z +/-** (step in cm) and **Rotate A/B/C +/-** (step in degrees). The view moves at once.
   With the tool active, the number pad nudges too: 4/6 move x, 2/8 move y, 1/7 move z, and with Ctrl held
   the same keys rotate a, b and c.
5. Or drag the preview item with the normal gizmo, then press **Read from gizmo** in the tool panel. It
   turns the item's new place back into offset and rotation values.
6. Press **Save to prefab**, in the tool panel or in the plugin dialog. It writes both values into the item
   prefab's `ItemAnimationAttributes` as one undo step and saves the `.et`. Ctrl+Z restores the old values
   in the editor. Save again after an undo to write them back to the file.
7. **Remove preview** (plugin dialog) deletes the two preview entities. Do not save the layer with them in it.

The plugin dialog also has the two values as fields. Type numbers there and press **Preview** to apply them.
Each step prints one `[LFHoldTuner]` line to the Workbench log, with the values and where the hand frame
came from.

## What the preview shows

The World Editor runs no animation, so the preview character stands in its rest pose. The tuner places the
item where the **hold pose** puts the hand, using the bone transform measured in a session for that anim
set (`PoseFrame` in the plugin: the jerrycan set and `player_carry`). The item lands where it will be in
play relative to the body, so you can judge leg clearance. The character's arm will not be around it. For
an anim set that has not been measured, the tuner falls back to the character's live bone in the rest pose
and says so in the log. That still tunes the grip, but check the result in play.

To measure a new anim set: hold an item that uses it in a session, run `heldxf`, and add its
`LeftHandProp` origin and axes to `PoseFrame`.

## Check it in play

After saving, hold the item in a session: `tools\cli asset states <slug> --shots`, or
`node agent-box/session.mjs asset-states-proof <{GUID}prefab> hand` on a `--clients 2 --profile full`
session. `heldxf` prints the item's origin next to the `LeftHand` bone. A neck or handle should land within
a few centimetres of `LeftHandMiddle1`.

## Head-less

`RunCommandline` reads `$profile:lf-hold-tuner/plan.txt`. Its verbs are listed at the top of
`LF_HoldTunerPlugin.c`: `item`, `offset`, `rotation`, `nudge`, `expect-hand`, `save`, `expect-prefab`,
`undo`, `redo` and `clear`. It is the same plan pattern as the NPC Dresser, but no `tools\cli` command
wraps it yet.

## The cash sack (2026-11-16)

`LF_MoneyBag.et` switched from `player_carry` to the jerrycan set. Its values are
`HandAdditiveTranslation 0.03 0.12 0.05` and `HandAdditiveRotation 0 90 0`. The rotation turns the
sack's thin side (0.25 m) toward the leg. The offset puts the gathered neck (model y 0.42) in the fist and
moves the sack 3 cm out from the thigh. `heldxf` reads the sack's base at 0.45 m and the hand at 0.93 m, on
the left side.
