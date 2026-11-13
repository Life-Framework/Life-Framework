# Writing the prompt and the spec

The generator is paid per attempt and cannot be corrected afterwards, so the spec is where the
thinking goes. Write `tools/assets/<slug>/spec.json` (start it with `tools\cli asset new <slug> ...
--no-run`), then run the `concept` stage alone: `tools\cli asset run <slug> --only concept`.

## From the roadmap item

1. Read the issue and the feature row. Write `concept` in one paragraph: what the object is in
   1989 Everon, who carries it, what it does in the game. If you cannot say what it does, the
   asset is not ready to be made.
2. Find the real object from the period and note three things a player would recognise it by.
   Those three things are the prompt.
3. Measure it. `size_m` is length (front to back), height and width in metres. A weapon is scaled
   on its height, so the height has to be right; the tool then checks the other two within 25%
   and 40%. For a pistol-skeleton weapon keep the height at the M9's 0.14 m: the hand pose
   assumes that grip.
4. Set the budget. Triangle caps per kind are in `kinds.mjs` (a handgun is 8,000 at LOD0, at most
   15,000). 1024 px textures are enough for anything hand-sized.

## The prompt

Meshy allows 800 characters. The concept stage enforces that and "no text, no logos".

- **Name the object first, in the plainest words.** "1980s police taser stun gun" came out as a
  pocket pistol twice: the generator knows pistols much better than it knows a 1980s taser, and
  the word "pistol" in the prompt pulled it there. Describe the shape you want instead of the
  category it resembles.
- **Describe geometry in the preview prompt and colour in `texture_prompt`.** The preview builds
  the shape with no colour. A colour word in the shape prompt does nothing for the shape, and a
  part that only exists as a colour ("a yellow block at the front") is not built.
- **Say where each part is, relative to the others.** "At the front, in place of a barrel" was
  ignored; the yellow ports landed on the side of the grip. One part per sentence, with its
  position, works better than a list.
- **Say what is absent, and do not count on it.** "No slide, no hammer" removed the hammer; the
  top of the body still has a slide line and a rear sight.
- **One object.** "Single object, no hands, no stand, no text, no logos." Generated lettering is
  always garbled.
- **Ask for hard surfaces** for manufactured things: "clean hard-surface industrial design".
- Do not ask for a pose, a scene, or lighting in the shape prompt.

`texture_prompt` goes to the refine call: materials, colours, wear, "flat neutral lighting"
(baked highlights look wrong in the game's own light).

## Model choice

`meshy.ai_model` is `meshy-6-lite` (5 credits a preview) or `latest` (20). The lite model gave a
detailed modern pistol; `latest` gave a simpler, boxier shape closer to the prompt. Start on
`latest` when the silhouette matters and the budget allows two attempts.

`target_polycount` is what Meshy remeshes to before the clean stage decimates. 20,000 leaves
enough to decimate to 8,000 without holes.

## When the concept stage refuses

It prints every problem at once. Fix the spec; nothing has been paid yet.
