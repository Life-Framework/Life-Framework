# Reviewing a generation

After `generate`, open `tmp/assets/<slug>/raw/preview.png` and `raw/refined.png` and answer these
before running `clean`. The clean stage costs nothing, but everything after it is wasted on a
model that fails here.

## Pass or regenerate

| Question | Fail means |
|---|---|
| Would a player name the object from this picture without being told? | Regenerate with a new prompt idea, or stop |
| Are the three recognisable features from the concept there, and in the right place? | Regenerate; say where each part is |
| Is it one object, with nothing fused to it (a hand, a stand, a second copy)? | Regenerate |
| Is there lettering or a logo? | Regenerate with "no text, no logos" first in the texture prompt, or paint it out in the texture |
| Does it fit the period? A 1989 object has no picatinny rail, no polymer-pistol look, no LEDs | Regenerate |
| For a weapon on a vanilla skeleton: is there a grip a hand can close around, about as deep front to back as the vanilla one? | Regenerate; a block the hand cannot wrap shows fingers lying on the side |
| Are the proportions within the spec's `size_m` ratios? | Correct `size_m` if the spec was wrong, else regenerate |

Things that are **not** a reason to regenerate, because the clean stage or a texture edit fixes
them: the wrong orientation, the wrong scale, too many triangles, small floating fragments, a
colour that is slightly off.

## How many attempts

- Attempt 1 fails on shape: change the prompt, not just the seed. Write down what you changed
  and why in the spec's `concept` or the PR.
- Attempt 2 fails: stop. Report what the generator keeps doing. The options are a different
  source (image-to-3D from a reference drawing, a CC0 model, a hand-modelled Blender script) or a
  human modeller.
- Never go past the spec's credit cap; the tool refuses anyway.

## Record it

`state.json` keeps each attempt's task ids and prompt, and `raw-attempt-<n>/` keeps its download.
Put both thumbnails in the PR when the asset ships, with one sentence on why the chosen attempt
won. A reviewer cannot tell a good generation from the fifth lucky one without that.

## The taser, as the worked example

- Attempt 1 (`meshy-6-lite`): a modern compact pistol with slide serrations and a rail. Failed
  "names the object" and "fits the period".
- Attempt 2 (`latest`, prompt rewritten to stress the boxy body and the absent slide): a stubby
  box pistol. The yellow dart block the prompt put at the front came out as a yellow panel with
  two ports on one side of the grip. It was accepted as a placeholder to prove the rest of the
  pipeline, and it fails "names the object" for a real release.
