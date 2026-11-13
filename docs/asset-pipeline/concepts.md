# Concept pictures before 3D

Text-to-3D cannot be corrected after the fact, and a bad shape costs 15 to 30 credits per try. Pictures
cost 4. The `concept` stage explores ten ideas as ten pictures, compares them, and lets one of them drive
Meshy **image-to-3D**; `generate` falls back to text-to-3D when there is no pick. Pictures-first is for a
shape the generator gets wrong from words (a 1980s taser comes out as a pistol or a jerry can).

## 1. Write `tools/assets/<slug>/concepts.json`

```json
{ "slug": "taser",
  "subject": "what it is and the three things a player recognises it by, ending in a full stop",
  "variants": [ { "label": "22 chars max", "detail": "what differs in this one" }, ... 10 of them ] }
```

- Exactly 10 variants, with distinct labels and details. Vary the period details, the silhouette, the
  materials and the colours; keep the subject the same. The tool adds one shared frame to every prompt:
  an orthographic-style three-quarter product shot on a plain light grey background, no hands, no stand,
  no text, no logos. Image-to-3D needs that framing; do not fight it in a variant.
- Describe the object, not its category (see [prompt-writing.md](prompt-writing.md)): say what the body,
  the front and the grip look like.

## 2. Run the stage

`tools\cli asset run <slug> --only concept` makes the ten images (`nano-banana-2-1`, 4 credits each,
`image_model` in the file overrides), saves each task id before waiting (a dead run resumes and pays
nothing twice), and builds `tmp/assets/<slug>/concepts/sheet.png`: a numbered 5x2 grid with each label,
using ffmpeg from `tools/film/.bin` (`FFMPEG_PATH` overrides). It then stops with `awaiting a pick`.

## 3. Score, then pick

Look at the sheet, then at any image that is close (`tmp/assets/<slug>/concepts/NN.png`). Score each
concept 0 (fails), 1 (so-so) or 2 (good) in this order, with a short note:

| Key | Asks |
|---|---|
| `silhouette` | a clear, readable silhouette |
| `era` | fits the period |
| `scale_grip` | plausible scale and grip |
| `background` | clean plain background, one object |
| `no_text` | no text, lettering, logo or watermark |
| `image_to_3d` | solid form, no thin or hidden parts, even light |

`tools\cli asset score <slug> <n> 2,2,1,2,2,1 "note"` writes it to `concepts.json` (total out of 12).

- **Default: a human picks.** Show the maintainer the sheet and the ranking, then
  `tools\cli asset pick <slug> <n> [--why "..."]`.
- **`run --pick`** lets the agent take the highest total (ties go to the lower number); the reason is
  recorded in `concepts.json` `pick`. Use it only when the maintainer said the agent may choose.

## 4. Generate

`tools\cli asset run <slug> --from concept` (or `--only generate`) sends the picked image to
`POST /openapi/v1/image-to-3d` (PBR on, remesh to `meshy.target_polycount`) and records the image task id, the concept number, and
that concept's task id and prompt in `meshy.json`. Review the result as usual
([review-generation.md](review-generation.md)).
