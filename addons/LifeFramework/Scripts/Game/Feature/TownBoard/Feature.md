# Town board

A cork board on the spawn square with one pinned card per thing to do in town: what it is, one line,
and where it is from the board. Read a card for its detail on one sheet and set a waypoint to it.

## In-world first

The player walks up to the board (`Prefabs/Props/Signs/LF_TownBoard.et`: the case board's vanilla cork
board on two vanilla sign posts). The cards are on the cork itself. Each card has its own F-key prompt,
`Read: <card title>`, which opens the one sheet. The sheet is only for the detail list and the
waypoint button.

## Shape

- **Player sees:** a header (the board's title) and up to eight index cards, four across and two down,
  each with a red category, a title, one line and "Post office, 30 m north-east". `Read: <title>` over
  a card opens a UI kit notice sheet: title, category, WHERE, the detail list, Set waypoint, Close.
  Set waypoint closes the sheet and shows a small marker on the screen over the place (its name and
  distance; pinned to the screen edge when the place is off screen or behind). Walking within 8 m
  clears it, and so does reading the same card and pressing Clear waypoint. Nothing moves the player:
  there is no teleport or fast travel.
- **Owner and side:** all client side, no RPC and no replicated state: the cards are config.
  `LF_TownBoard` (every machine) reads the config and answers where a card's place is.
  `LF_TownBoardComponent` + `LF_TownBoardFace` (rendering clients) draw the face within 30 m, in the
  wanted board's render target (`LF_WantedPosterFace`: cork, live window, freeze). `LF_TownBoardCardAction`
  (one per slot, local effect) opens `LF_TownBoardSheet` and reports `sign.read:<board sign id>` so the
  arrival quest's "read the town board" step still moves (the board keeps `LF_SignComponent` id `town_map`).
  `LF_Waypoint` and `LF_WaypointHud` hold the one mark and draw it (`UI/Layouts/HUD/LF_Waypoint.layout`,
  a HUD exception in `ui-kit-baseline.json`).
- **Where a card points:** a place kind and id in `LF_Places`, kind `Sign` by default: every
  `LF_SignComponent` claims its sign id there (`LF_Places.Claim`, the first prop of a shared id wins).
  The core never names a place or holds a coordinate; a pack on another map points its cards at its
  own signs. A place not on the map reads "Ask around" and has no waypoint button.
- **Data:** `Configs/TownBoard/LF_TownBoard.conf` (`LF_TownBoardConfig`, public surface) ships the
  title and no cards. The pack adds them: `LF_Everon1989/Configs/TownBoard/LF_TownBoard_Everon1989.conf`
  (an overlay listed in both mission headers), text in the pack table (`LF-TownCard_*`,
  `LF-TownBoard_Title`). Core keys are frame words only (`LF-TownBoard_*`, `LF-Waypoint_Distance`).
  Nothing is persisted. A session with no mission header (no pack) shows the header and no cards.
- **Map:** DebugWorld has no map entity, so the waypoint is a screen marker. A local marker on the
  vanilla map (`SCR_MapMarkerManagerComponent`) is the follow-up for a world that has one.

## Tests

LOGIC `townboard/cards` (slots, where text, compass and distance words, slot geometry, waypoint rules,
HUD edge pinning). WORLD `townboard/board-world` (each card context sits on its face slot and reads its
own slot, empty slots offer nothing, a card finds the post office sign through `LF_Places`). Session
proof `node agent-box/session.mjs town-board-proof` (`up --profile full --mission playtest`).

## Map knowledge (#2242, M14)

Reading a card whose place is a map POI kind (`m_sPlaceKind` has a row in `Configs/Map/LF_MapPois.conf`) marks that
place "heard of" on the reader's map. `LF_TownBoardMapKnown.c` adds one ask beside the read (`LF_AskTownCardHeard`
on the player's own controller, sent only for such a card); the server checks a town board stands within 12 m,
the slot holds a card of a POI kind and the player's ask gap (500 ms), then calls `LF_MapKnown.Hear`. The 1989
cards point at sign ids (kind `Sign`, not a POI), so they mark nothing until a pack points a card at a POI kind.
Test `map/places-known`.
