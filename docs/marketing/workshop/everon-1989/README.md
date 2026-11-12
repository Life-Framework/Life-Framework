# Workshop kit: Life Framework: Everon 1989 (DRAFT)

**DRAFT.** Drafted for #375; the maintainer signs it off. Nothing here is published. It follows the
core kit ([`../README.md`](../README.md)) and is checked by `tools/validation/validate-release-ready.mjs`.

| Field | Value or file |
|---|---|
| Project Name (30 max) | `Life Framework: Everon 1989` |
| Mod ID (the `.gproj` GUID, also the Workshop item ID) | `B1E84D5687E11724` |
| Requires | Life Framework (Core), `79636E668EA37AC9`, version 0.1.0 or later |
| Preview Image | [`preview.jpg`](preview.jpg): a DRAFT PLACEHOLDER, replaced before publish (spec below) |
| Summary (1024 max) | [`summary.txt`](summary.txt) |
| Description (5000 max) | [`description.bbcode`](description.bbcode) |
| Version | `0.1.0` |
| Licence | Arma Public License (APL) |

The Workshop build is APL because the Workshop Terms of Use require an APL-family licence
(decision D2). The source on GitHub stays MIT. The pack publishes in the same release as move
batch 1 (decision D3).

The version is semver, set by the pack. Its description names the core range it needs. A pack release
that needs a newer core raises the "Requires" line.

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
- The narrative premise and period art direction (#375) are not written; the description does not
  claim one.
- Tags, category and change notes, once the pack loads and has content to describe.
