# Style guide: how Life Framework looks

The look is a Czechoslovak island village in 1989: enamel plates, chalk, cork, carbon paper,
dull metal. This guide is written from what already looks right in the town and from what a
`tools\cli look all` on 2026-11-13 showed looking wrong (`docs/architecture/agent-quality.md` has
the pictures). It is the reference for the polish checklist (`.claude/skills/polish/SKILL.md`);
when a rule here keeps getting broken it becomes a check in `tools/validation/`.

## The one test

Stand where a player first sees the thing. If it reads as "a developer put this here", it is not
finished. That covers a lone crate in a field that is secretly a trader, a board lying on a lawn,
a blank white sign, a prop named `*_Debug`, and a menu in a different font from every other menu.

## Where things go

| Thing | Belongs | Not |
|---|---|---|
| Notice board, wanted board, dispatch board | Hung on a wall at eye height (`tools\cli place against`), bottom edge about 1.0 m up | Standing on the grass in front of the wall, leaning on nothing |
| Desk, filing cabinet, locker, safe, bed | Indoors, or under a roof or awning against a wall | In the open on a lawn. A desk in a field reads as a test fixture |
| Counter, till, scales | On a counter or stall, facing the customer side | On the ground; half off the edge (`validate-prop-on-surface`) |
| Payphone, post box, bin, bench, street lamp | On the verge beside a path or road, facing it | In the road, in a doorway, in front of a sign |
| Sign plate | On the facade it names, clear of roof caps, gutters and windows | Clipped by a roof tile; overlapping another sign |
| Free-standing sign (posts, A-board) | Beside the entrance or at the roadside, facing the approach | In the middle of a yard as the only marker of an interactable |
| Crates, sacks, barrels, pallets | In groups against a wall or under cover, the big ones at the back | Scattered singly across a lawn; a sack pile taller than the barrel beside it |
| Something illegal (still, stash, chop tools) | Out of sight: behind a wall, under trees, in a shed | Under a sign that names it. A hideout has no "HIDEOUT" sign on the street |

An interactable is always a recognisable object of its own kind, at its real size. A trader is a
person at a stall or a counter with goods on it. A machine is a machine. If the only honest prop
is a stand-in, record it in `docs/asset-inventory.md` as a placeholder and say so in the PR.

**Group, then anchor.** A new feature's props go next to the thing they serve and touch something:
a wall, a counter, a kerb, another prop of the group. Three props that touch read as a place;
three props two metres apart read as debris.

**Nothing debug in the village.** Fixtures and debug tools live in the annex layers.
`validate-player-props` fails a new `*Debug*` entity or `/Debug/` prefab in a village layer.

## Signs and lettering

The sign prefabs are in `Prefabs/Props/Signs/`; `Feature/Signs/Feature.md` documents each type.
Pick by job, do not invent a new board:

| Job | Prefab | Looks like |
|---|---|---|
| Name of a public building | `LF_Sign_Fascia` over the door, or `LF_Sign_Plate` beside it | Enamel plate, one flat colour, white rim, white capitals (the blue POLICE STATION plate, the red BANK fascia) |
| Roadside or yard marker | `LF_Sign_PolePlate`, `LF_Sign_Posts` | White plate, coloured capitals and rule |
| Today's offer, a price, anything informal | `LF_Sign_Chalkboard`, `LF_Sign_Pub` | Chalk on black, an A-board |
| Something to read up close | `LF_Sign_Notice`, `LF_Sign_Board` (cork) | Paper pinned to cork: the wanted board |

- **Colour says who runs it.** State blue for police, red for the bank and the hospital cross,
  green for trade, white for municipal notices. One paint colour per sign, plus white or cream
  lettering, plus the rim. No gradients, no teal or neon, no drop shadows.
- **Capitals, two or three words.** `POLICE STATION`, `GENERAL STORE`. Detail goes on the card the
  Read action opens, not on the plate.
- **A sign is never blank.** A plate with no text is a broken sign; `look` close-ups show it.
- **Text must be readable from where the player stands**: check the close-up, and the wide for
  the building name.
- English text; Czech or Russian only on vanilla dressing props (the "ВХОД ЗАПРЕЩЁН" plate).

## Screens

A screen is the last resort (`docs/decisions.md` "In-world first"), and when one is needed it is a
sheet of paper or a terminal that belongs to the object the player walked up to.

- **Build it from UI kit parts.** `LF_UIScreen.Open(parts)` with `LF_UIPart` rows: `TITLE`,
  `HEADING`, `LINE`, `RICH`, `FIELD`, `ROW`, `STATUS`, `QUANTITY`, `KEYS`, `LIST`, `RULE`, `IMAGE`,
  `BUTTON`, `CLOSE`. `tools\cli feature new <Area> <Name> --kind ui` writes one. No new `.layout`
  for a screen; `validate-ui-kit` fails it.
- **The theme is `LF_UITheme` and nothing else.** Warm paper (238, 230, 212), carbon ink
  (28, 26, 22), brass rule (122, 102, 64), blued-steel rim (62, 64, 58), a darker strip for rows
  (220, 208, 182). RobotoCondensed Bold for print, Regular for a notice's body. Title 28, body 18,
  button 20, caption 16, heading 15. A feature never sets a colour, a font or a size; a sheet that
  needs a different mood takes an `LF_UISkin` (the sign card does).
- **One sheet, one job.** A title, what the player needs to read, at most one field group, the
  action, Close. If it needs tabs it is two objects in the world.
- **Wording is printed matter.** Form headings, ledger lines, stamps: `FORM R-89`, `BALANCE ....
  $1,200`, `PAID`. Not `Success!`, not `Click to confirm`. No emoji, no exclamation marks.
- **It closes itself**: on death, on losing the character, and on walking away from the object
  (`CloseOnDeath`, `CloseWhenUncontrolled`, `CloseBeyond`).
- **Faces on world props** (the wanted poster, a sign face) are render-target layouts, not
  screens. They are listed as exceptions in `tools/validation/ui-kit-baseline.json`, use the theme
  fonts, and are judged at the distance a player reads them from, by day and at night.
- **HUD and toasts** stay small and rare. Feedback comes from the world first (a sound, the
  object changing), a toast second.

## Items a player carries

- A real model at real size; hold it and compare with the hand (`docs/asset-pipeline/`).
- The inventory icon and the preview show the item, lit, filling the cell, not a white or black
  square and not a vanilla stand-in. The check is `previews show <prefab>` in a session
  (`docs/asset-pipeline/review-checklist.md`).
- A generated model gets its real material baked in: no glassy chrome, no plastic white. Rubber
  is matt black, steel is dull, wood is worn.

## People

- A townsperson wears civilian clothes; only a soldier or an officer wears a uniform. On
  2026-11-13 every NPC in the village still wore the same olive fatigues: the clerk, the
  traders, the medic. That is listed under "What remains", and new NPCs must not add to it.
- An NPC stands at its post facing the approach: behind the counter, beside the desk.

## Pictures

- `tools\cli look <area>` before the first edit and after the last, same area, same hour.
- Keep both with `tools\cli evidence <issue> before|after <area>`; the PR embeds the pair.
- Read the close-up of every new or moved thing, not only the wide.
- Red fans and lines on the ground in a `look` are a Workbench overlay; players never see them.
