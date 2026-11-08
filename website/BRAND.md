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
  NEEDS A HAND, SUPERSEDED, NOT RELEASED, NOT OPEN YET.
- **Plate** (`.plate`): a yellow road-sign plate for a signpost word (PLAYERS,
  MIT). Never for status.
- **Rule** (`.rule-top`): a 2px ink rule opens every section, like a form.
- **Route map** (`<RouteMap>`): milestones are stops on a line. The current stop
  is yellow and says "you are here".
- **Scope meter** (`<ScopeMeter>`): one cell per issue, filled when closed. No
  percentage bars.
- **Figure** (`<Figure>`): a photo print with a caption and a mono credit line:
  WORK IN PROGRESS when it is, the place, the date, and the pull request it came
  from.
- **Terminal** (`.terminal`): commands and config, dark in both themes.
- **Buttons**: mono capitals. Primary is green with an ink offset shadow that
  presses in; secondary is an ink outline. Two buttons per view at most.

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

The mark is "LF" built from rectangles inside a square frame, like a stencil
on a crate (`Mark` in `src/components/icons.tsx`, `public/favicon.svg`). The
wordmark is the mark plus "LIFE FRAMEWORK" in Barlow Condensed 700. Keep at
least the frame's width of clear space around it. Do not recolour it except
to `ink` or `paper`.
