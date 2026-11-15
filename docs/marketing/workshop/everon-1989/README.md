# Workshop kit: Life Framework: Everon 1989 (DRAFT)

**DRAFT.** Drafted for #375; the maintainer signs it off. Nothing here is published. It follows the
core kit ([`../README.md`](../README.md)) and is checked by `tools/validation/validate-release-ready.mjs`.

| Field | Value or file |
|---|---|
| Project Name (30 max) | `Life Framework: Everon 1989` |
| Mod ID (the `.gproj` GUID, also the Workshop item ID) | `B1E84D5687E11724` |
| Requires | Life Framework (Core), `79636E668EA37AC9`, version 0.1.0 or later |
| Scenario ID | `{C638C03B775C31BB}Missions/LF_Everon1989.conf` (name in the list: Life Framework: Everon 1989; the only scenario the pack lists) |
| Screenshots | the nine village shots in [`../screenshots/`](../screenshots/), 1280x720, each under 600 KB |
| Change Notes | the pack 0.1.0 block in [`../changelog.md`](../changelog.md) |
| Tags | `Roleplay Life RPG Economy Jobs Police Crime Multiplayer Everon 1989` |
| Category | Game Mode (and Scenario if the dialog offers it) |
| Preview Image | [`preview.jpg`](preview.jpg): a DRAFT PLACEHOLDER, replaced before publish (spec below) |
| Summary (1024 max) | [`summary.txt`](summary.txt) |
| Description (5000 max) | [`description.bbcode`](description.bbcode) |
| Version | `0.1.0` |
| Licence | Arma Public License (APL) |

The Workshop build is APL because the Workshop Terms of Use require an APL-family licence
(decision D2). The source on GitHub stays MIT. The pack publishes after the whole pack move,
batches 1 to 4, in the same release as the core (decision D3).

The version is semver; see "Version scheme" in [`../README.md`](../README.md). The description names
the core version it needs, and a pack release that needs a newer core raises the "Requires" row.

## Preview image spec

The current `preview.jpg` is a flat placeholder so the kit is complete; it is not art.

- 1920x1080 JPEG, under 600 KB.
- Subject: the Everon village in 1989, taken in the game with the in-game camera, not a render.
- Period look: late-afternoon light, period vehicles and signs in frame, no modern UI, no HUD.
- No text, logo or badge baked into the image; the Workshop title sits beside it.
- Distinct from the core item's preview, which shows the framework's town: this one shows a place.
- Gallery screenshots (1280x720, each under 600 KB) come from the same village, after the pack content
  is loaded; taken per `docs/agent-docs-sweep.md`.
- Maintainer signs off the final image.

## Open items

- Final preview image and gallery screenshots.
- Maintainer sign-off on the premise and voice in [`docs/branding/everon-1989.md`](../../../branding/everon-1989.md)
  and on the description copy built from it.
- The scenario lists as "Life Framework: Everon 1989". Its in-game briefing (#374) has no PR yet.
- `discord-announcement.md` and `website-news.md` in the parent folder still announce the single
  playtest item; rewrite them for the two items at the first publish.
