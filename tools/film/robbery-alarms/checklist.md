# Robbery alarms video: reviewer checklist

The reviewer is a second agent or person, not the one who cut the video. `cut.mjs` writes one frame every 0.5 s to
`<out>-frames/` and a contact sheet to `<out>-contact.jpg`. Look at every frame, then at adjacent pairs. A finding
names the frame (`f017.jpg` is 8.0 to 8.5 s) and the rule. The video passes when no rule below is broken.

## Every frame

1. **Clothing.** Every character is dressed for their role, with no military kit on a civilian. The robber wears the
   same 1989 civilian look in every shot: denim jacket, dark trousers, black knitted cap (`costume.mjs`). The clerk
   wears the fuel clerk's outfit. No vest, helmet, backpack or uniform part on anyone. There is no police uniform
   yet, so police show only as the patrol car with its lights, and a driver may only be a silhouette.
2. **World.** No floating or clipping prop, no prop half off its surface, no white or placeholder mesh, no missing
   texture. The fuel kiosk shows one cash register, not two.
3. **Screen.** No HUD, debug text, FPS counter, version string, harness overlay, dialogue subtitle, toast, key hint
   or action prompt. The only text is the end card.
4. **Camera.** Steady framing; moves are slow and smooth; nothing in front of the lens (a leaf, a pole, a wall edge
   filling the frame).
5. **Light.** Golden hour on every outdoor shot, the same time of day across the robbery.

## Across frames

6. **Continuity.** The robber looks the same and stands in the same place across the robbery angles; the alarm light
   is off before the alarm cut and flashing after it.
7. **Motion.** No frozen picture between two frames 0.5 s apart in a shot that is meant to move (a camera move, a
   door swinging, a car driving).
8. **Cuts.** Each cut lands on an action or a sound (the alarm, the bell, the siren) and no shot is shorter than 2 s.

## The file (`<out>.json`)

9. 1920x1080, 30 fps, H.264 + AAC, under 10 MB, 20 to 40 s.
10. Integrated loudness about -14 LUFS, true peak under -1 dBFS, no clipping.
11. Game audio only: no narration, no music, no AI voice. The clerk's voice is the game's own dialogue recording.
12. At most two text cards; the end card reads "LIFE FRAMEWORK", "Playtest build 6", "life-framework.org".
