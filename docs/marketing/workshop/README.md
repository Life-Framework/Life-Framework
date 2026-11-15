# Workshop kit: Life Framework (Core)

Everything the Workbench **Publish Project** dialog asks for, ready to paste or pick.
The click-by-click steps are in [`docs/playtest/workshop-publish.md`](../../playtest/workshop-publish.md).

| Field | Value or file |
|---|---|
| Project Name (30 max) | `Life Framework (Core)` |
| Mod ID (the `.gproj` GUID, also the Workshop item ID) | `4550A2B656E1B569` |
| Scenario ID | none: the core has no playable scenario; the flagship scenario ships in the pack |
| Preview Image | [`preview.jpg`](preview.jpg), 1920x1080, 478 KB. A PLACEHOLDER: it shows the village, and the core brief wants neutral art |
| Screenshots | none until neutral art exists; the nine village shots in [`screenshots/`](screenshots/) belong on the pack page |
| Summary (1024 max) | [`summary.txt`](summary.txt) |
| Description (5000 max) | [`description.bbcode`](description.bbcode) |
| Change Notes | the core 0.1.0 block in [`changelog.md`](changelog.md) |
| Version | `0.1.0` |
| Tags | `Roleplay Life RPG Economy Jobs Police Crime Multiplayer Framework` |
| Licence | Arma Public License (APL) |
| Category | Game Mode |

The Workshop build is APL because the Workshop Terms of Use require an APL-family licence
(decision D2, 2026-11-12). The source on GitHub stays MIT. The description's "Licence and credits"
section says both; check it says so before each publish. Pack authors get the same facts, with the
attribution rules, in [pack-licence.md](../../architecture/pack-licence.md).

## Two items, two brands

The core page is neutral and technical and sends players to the pack ("Looking to play?").
The pack page, [`everon-1989/`](everon-1989/README.md), carries the 1989 setting. The briefs are
[`docs/branding/core.md`](../../branding/core.md) and
[`docs/branding/everon-1989.md`](../../branding/everon-1989.md). `validate-release-ready` checks that
each page names the other, that the Project Name equals the `.gproj` TITLE, and that both briefs
exist.

## Version scheme

Semver, `x.y.z`. `tools\cli release <version>` stamps the same number into both `.gproj` files and
both kit READMEs, so the core and the pack publish together at one version while both are 0.x. The
pack's `Requires` row names the core version it needs, and `validate-release-ready` fails when it
is missing. A save-format or config-schema bump is a major version (decision 2026-11-09). Every
Workshop publish takes a higher number than the last, even for a text-only change.

After publish:

- Discord post: [`discord-announcement.md`](discord-announcement.md)
- Website news: [`website-news.md`](website-news.md)
- Server hosts: [`docs/playtest/workshop-host.md`](../../playtest/workshop-host.md) and
  [`docs/playtest/server-config.example.json`](../../playtest/server-config.example.json)

## Images

`make-images.ps1` rebuilds every image from the website gallery and the logo:

```
powershell -ExecutionPolicy Bypass -File docs\marketing\workshop\make-images.ps1
```

It also writes the scenario image `addons/LifeFramework/Missions/LF_PlaytestTown_Preview.jpg`
(icon, preview and loading screen) and the Workbench thumbnail `addons/LifeFramework/thumbnail.png`.
After changing the scenario jpg, rebuild its `.edds` in Workbench (Resource Browser, right-click
`LF_PlaytestTown_Preview.jpg`, Reimport), or the scenario keeps the old picture.

The screenshots, in the order a player meets the places:

1. `01-spawn-square-wide-after.jpg`: the spawn square and the main street
2. `02-store-counter.jpg`: the general store counter
3. `03-bank-detail.jpg`: the bank front and teller windows
4. `04-carlot-detail.jpg`: the car lot price boards
5. `05-fuel-station-wide.jpg`: the fuel station
6. `06-police-wide.jpg`: the police post
7. `07-hospital-wide.jpg`: the hospital and ambulance
8. `08-cash-van-bags.jpg`: money sacks from the cash van
9. `09-hideout-bench.jpg`: the hideout workbench

## Trailer

A short trailer from the video pipeline (#390, in progress) goes in two places once it exists:
the Workshop description, as a YouTube link at the top under the title, and the Discord post,
as its first line. The Workshop gallery takes images only. The 2026-11-08 decision ties the
**Public** release to the showcase video, so the trailer is due before the visibility switches
from Unlisted to Public.
