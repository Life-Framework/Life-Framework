# Visual boards: the wanted board's face

How the police wanted board shows its posters on the prop, for every player
near it, without a menu. The design comes from
`docs/architecture/visual-boards-spike.md`; the limits from
`docs/architecture/visual-boards-budget.md`. Refs #758.

![The face on a game client](visual-boards/face-close.jpg)

*A game client on a dedicated server, just after it robbed the fuel till: the
robber's poster, unnamed until the police identify them.*

## Pieces

| Piece | Where | Does |
|---|---|---|
| `LF_WantedPosters` | `Feature/Police/LF_WantedPosters.c` | Server: builds the public list from the crime reports and wanted levels, one poster per suspect, newest first, no place, the name only once identified. Rebuilds on `LF_OnCrimeCommitted`, `LF_OnWantedChanged`, `LF_OnSuspectIdentified` and after a load, one rebuild per 1.5 s burst; sends only a changed list |
| `LF_WantedPosterBoardComponent` | `Feature/Police/LF_WantedPosterBoard.c`, on `Prefabs/Police/LF_WantedBoard.et` | Holds the list on every machine: `RplSave`/`RplLoad` for a late joiner or a board streaming in, `RpcDo_SetPosters` (reliable broadcast) on a change |
| `LF_WantedPosterFace` | same file | Client: one render target (`LF_WantedBoardFace.layout`, 1920 x 1080 units) on a local `LF_BoardFace.et` quad over the cork; the vanilla cork texture under the posters; one `LF_WantedPoster.layout` per entry |
| `LF_WantedPosterFaces` | same file | Client: once a second, builds the faces within `drawRadius` of the camera and releases those beyond it plus 5 m |
| `LF_WantedPostersConfig` | `Configs/Police/LF_WantedPosters.conf`, owner section `wantedBoard` | `posters` (4, at most 8 in two rows), `drawRadius` (30 m). The radius reaches clients inside the board's own list |

An entry is a short string of ids: `P;name;offence id;level;clothing keys;vehicle key`,
the offence being the crime catalog id (#685a). Police-only detail (the place,
the dispatch log, stolen cars, witness lines) stays on the Read card.

## Portraits

![A portrait on the face](visual-boards/portrait-close.jpg)

The server captures the suspect's look when the report is made (`LF_WantedLook`):
the character prefab and each loadout slot's item, as prefab GUIDs, about 120
characters. `LF_WantedPortrait` spawns that character in the preview world,
dresses it slot by slot and hands it to the poster's `ItemPreviewWidget`. The
widget is three photo frames wide and the frame clips it to the head and
shoulders, for every suspect. The dummy is deleted once the face has frozen for
the last time (33 s after a change); the frozen face keeps the picture. The
look is saved in its own payload, `wantedlooks` (save version 29), matched to
the reports by sequence after `wanted` loads, so the portraits come back after a
restart. A version 28 or older save has no looks: its posters show an empty
photo frame until the suspect's next crime.

Measured on the session (one portrait, RTX 3080, 1280x720, uncapped): 6.08 ms
mean, 8.03 ms p95 while live, 5.91 / 7.57 frozen.

## The live window

A face renders live for 3 s after it is drawn or changed, then hides its widget
root: the mesh keeps the last frame and costs nearly nothing (the budget's
"frozen" rows). It renders again for 2 s at 10 s and at 30 s after it was
built (and after each change), so textures that streamed in late are on the
frozen frame; the cork texture is loaded again without the cache at each
refresh. Before that reload, a late joiner's face kept the cork at its lowest
mip even after the refreshes (left); with it, the late joiner's frozen face has
the full cork (right):

| Before | After |
|---|---|
| ![Late joiner, cached low mip](visual-boards/face-late-joiner-flat.jpg) | ![Late joiner, reloaded](visual-boards/face-late-joiner.jpg) |

## From the street

![The board from 6 m](visual-boards/face-street.jpg)

## Not here yet

Stolen cars (spike issue 5), the look on the saved report (issue 2), and the
night and distance checks (issue 7). The dispatch
boards inherit the wanted board and keep their painted face
(`m_bDrawPosters 0`).
