# Life Framework brand

The site should look like something you could find on a notice board in a
small Everon town in 1989: printed paper, a rubber stamp, a road sign, a
typed form, a terminal screen in the police station. It should never look like
a startup landing page. Read this before you change a colour, a font, or a line
of copy, and change this file in the same pull request when you change the
rules.

The tokens live in `src/index.css`; Tailwind names them in `tailwind.config.ts`.
Use the names (`text-ink-2`, `bg-card`, `border-rule`), never a raw colour.

## Palette

Two themes with the same roles. Day is printed paper; night is the duty office
after dark. Both are deliberate, neither is an inversion of the other.

| Token | Day | Night | Use |
| --- | --- | --- | --- |
| `--paper` | `#f0ebe1` | `#121410` | Page background |
| `--card` | `#fbf8f2` | `#1a1d17` | A sheet on the board: cards, figures, the footer |
| `--inset` | `#e5ddcd` | `#23271f` | Inline code, hover fills, superseded entries |
| `--ink` | `#1b1d19` | `#ebe5d7` | Text, rules, borders that matter |
| `--ink-2` | `#54564d` | `#aaa696` | Secondary text, form labels |
| `--rule` | `#c9bfac` | `#3a3f33` | Hairlines between rows |
| `--green` | `#2d4a33` | `#a3c596` | Everon forest green: links, the primary button, done |
| `--stamp` | `#a3302a` | `#ee7a6b` | Stamp red: status stamps, "you are here", numbering |
| `--sign` | `#e3b23c` | `#e3b23c` | Road-sign yellow: plates, focus ring, one underline |
| `--terminal` | `#1b1d19` | same | Terminal screens, code blocks (dark in both themes) |

Contrast (WCAG): every text pair above is at least 5.1:1 on `paper`, `card`, and
`inset` in both themes. Yellow is a fill, never a text colour on paper. Check a
new pair before you use it; Lighthouse flags anything under 4.5:1.

Red and yellow are spice. A page has one or two red stamps and at most one
yellow plate per block. If everything is stamped, nothing is.

## Type

Three OFL families, self-hosted from `public/fonts/` (latin subset, woff2;
licences beside them). No Google Fonts request, no other families.

| Role | Family | Weights | Where |
| --- | --- | --- | --- |
| Display | Barlow Condensed | 600, 700 | Headings, uppercase. Road-sign lettering |
| Body | IBM Plex Sans | 400, 400 italic, 600 | Everything you read |
| Labels | IBM Plex Mono | 400, 500 | Form labels, dates, issue numbers, buttons, the terminal |

| Class | Size | Notes |
| --- | --- | --- |
| `.display-1` | clamp(2.75rem, 8.5vw, 5.75rem), line-height 0.95 | One per page, the `<h1>` |
| `.display-2` | clamp(2rem, 5vw, 3.25rem) | Section titles |
| `.display-3` | clamp(1.5rem, 3vw, 1.875rem) | Card and entry titles |
| `.heading` | 1.1875rem Plex Sans 600 | Linked titles in lists |
| `.lede` | clamp(1.125rem, 2vw, 1.3125rem) | The paragraph under a title |
| body | 1.0625rem / 1.6 | |
| `.kicker` | 0.75rem mono 500, uppercase, 0.08em tracking | The label above a title, like a field on a form |

## Components

- **Sheet** (`.sheet`): a card is a sheet of paper. Flat, a 1px rule border, a
  hard 4px offset shadow. No blur, no gradient, no glass, no rounded corners.
- **Stamp** (`<Stamp>`): status only, in mono capitals with a double border.
  Tilt one per view at most. Words: WORK IN PROGRESS, IN DEVELOPMENT, DONE,
  NEEDS A HAND, SUPERSEDED, NOT RELEASED, NOT OPEN YET, FINAL PLAYTEST, PLANNED.
- **Plate** (`.plate`): a yellow road-sign plate for a signpost word (PLAYERS,
  MIT). Never for status.
- **Rule** (`.rule-top`): a 2px ink rule opens every section, like a form.
- **Route map** (`<RouteMap>`): milestones are stops on a line. The current stop
  is yellow and says "you are here".
- **Roadmap tree** (`<RoadmapTree>`): the Tree of Life grows from topic pillars;
  each pillar branches through M1, M2, and later milestones into selectable
  issue nodes. Assignment is the claim mechanism; the site and Discord mirror
  it. Keep branch lines ruled and status marks small. No
  gradients or glows.
- **Milestone list** (`<MilestoneList>`): one sheet per milestone in number
  order, with its stamp, one-line goal, and issues. An epic folds open to its
  sub-issues. Each row carries a small CORE (outlined) or FLAGSHIP (solid ink)
  tag; never red, so the stamps stay the only red on the card.
- **Scope meter** (`<ScopeMeter>`): one cell per issue, filled when closed. No
  percentage bars.
- **Figure** (`<Figure>`): a photo print with a caption and a mono credit line:
  WORK IN PROGRESS when it is, the place, the date, and the pull request it came
  from.
- **Terminal** (`.terminal`): commands and config, dark in both themes.
- **Band** (`.band`): a full-width night strip on the emblem's near-black
  ground, dark in both themes. It sets the night tokens locally, so anything
  inside keeps its contrast. Only the home page uses it, to open (hero, with
  the full emblem) and to close (the community call). Two per page at most.
- **Buttons**: mono capitals. Primary is green with an ink offset shadow that
  presses in; secondary is an ink outline. On a band, the second call to
  action may be a yellow sign button (`.btn-sign`). Two buttons per view at
  most.

## Imagery

- Only captures from the game. No mock-ups, no renders made for the site, no
  stock photos, no AI images.
- Every picture says where it was taken, when, and which pull request it came
  from (`content/media.json`). Anything that is not final says WORK IN
  PROGRESS. A shot from the test world says so; never pass it off as Everon.
- New captures come from the film-evidence release (`tools/film/film.mjs
  publish`, `agent-box/`). Convert stills to webp at 640 and, when the source
  allows, 1280 wide. Clips stay release assets; the build downloads them.
- Share images (`public/og/*.jpg`) come from `scripts/make-og.mjs`.

## Voice

Write like the notice on the board: short, specific, and true. Follow the
repo's `unslop` skill.

- Say what exists and what does not. "It is not out yet" beats "coming soon".
- Name the object. "A police fine is a ticket you carry to the station desk"
  beats "immersive law enforcement".
- Numbers come from the build (scopes done, decisions recorded), never typed in.
- Hype comes from the ambition and the evidence, not adjectives. The ambition is
  the line in `docs/direction.md`; quote it, do not inflate it.

| Do | Don't |
| --- | --- |
| "12 of 29 scopes done" | "Rapid progress!" |
| "Not released. The framework release is M3." | "Launching soon" |
| "Captured in the game, PR #137" | A screenshot with no source |
| One stamp, one plate, plain rules | Gradients, glows, blobs, glass, emoji icons |
| Mono labels, condensed capitals, Plex body | A fourth font, all-caps body text |
| "Revolutionary", "next-generation", "seamless" | (never) |
| Long dashes | (never; use a full stop or a comma) |

## Logo

The logo is a round emblem: a helmeted figure with a rifle in front of a globe
and two pines, inside a double ring, with LIFE-FRAMEWORK on the top arc in
Barlow Condensed 800. The maintainer supplied it. The figure and pines are
vectorised from that image, the globe and rings are redrawn, and the lettering
is outlined, so no file needs a font. The SVGs in `public/brand/` are the
source; the PNGs are renders of them.

The rest of the site keeps the 1989 paperwork look described above. Only the
mark changed. Whether the framework gets its own modern site brand, with the
1989 look kept for the flagship, is a separate decision.

| File | What it is | Use it for |
| --- | --- | --- |
| `logo.svg`, `icon-512.png`, `logo-1024.png` | Full emblem, blue `#3b82b8` on `#0b0e12` | Discord server icon, bot avatar, profile pictures |
| `logo-light.svg` | Full emblem, `#1d5a85`, transparent | Light backgrounds: share images (`scripts/make-og.mjs`), print |
| `logo-mono.svg` | Full emblem, `#0b0e12`, transparent | One-colour use: stickers, stamps, embroidery |
| `social-1024.png` | Full emblem with margin on `#0b0e12` | Square profile pictures on other sites |
| `mark.svg`, `mark-512.png` | Small mark (ring, head and shoulders) on a dark disc | `favicon.svg`, `favicon.ico`, `apple-touch-icon.png` |
| `mark-light.svg`, `mark-dark.svg`, `mark-mono.svg` | Small mark, transparent | The site header (`Mark` in `src/components/icons.tsx`), day and night |
| `wordmark.svg`, `wordmark-dark.svg` | Small mark plus LIFE FRAMEWORK in Barlow Condensed 700 | Places that need the name beside the mark as one image |

- **Full emblem or small mark.** The emblem's lettering stops reading below
  about 96 px. At 64 px and below, and anywhere the name is already written
  beside it, use the small mark.
- **Minimum size.** Full emblem 96 px across; small mark 16 px.
- **Clear space.** Keep a quarter of the emblem's diameter clear on every
  side. The small mark needs its ring's width.
- **Colour.** Emblem blue on near-black, `#1d5a85` on light grounds, or one
  flat colour. The blue belongs to the logo only; it is not a site token, so
  do not use it for text or UI.

| Do | Don't |
| --- | --- |
| Use the files in `public/brand/` as they are | Redraw, re-letter, or trace the emblem again |
| Use the small mark in a browser tab, a header, or the Discord member list | Shrink the full emblem until the lettering turns to texture |
| Use the transparent `logo-light.svg` or `logo-mono.svg` on light grounds | Put the dark square `logo.svg` on paper |
| Keep the emblem upright | Rotate it, add a shadow, a glow, a gradient, or a texture |
| Leave the figure, globe, and pines as drawn | Swap in Bohemia or Arma marks, or anything that suggests an official link |
