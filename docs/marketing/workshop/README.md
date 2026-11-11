# Workshop kit: Life Framework playtest town

Everything the Workbench **Publish Project** dialog asks for, ready to paste or pick.
The click-by-click steps are in [`docs/playtest/workshop-publish.md`](../../playtest/workshop-publish.md).

| Field | Value or file |
|---|---|
| Project Name (30 max) | `Life Framework` |
| Mod ID (the `.gproj` GUID, also the Workshop item ID) | `79636E668EA37AC9` |
| Scenario ID | `{1054CC8646357708}Missions/LF_PlaytestTown.conf` |
| Preview Image | [`preview.jpg`](preview.jpg), 1920x1080, 478 KB |
| Screenshots | the nine files in [`screenshots/`](screenshots/), 1280x720, each under 600 KB |
| Summary (1024 max) | [`summary.txt`](summary.txt) |
| Description (5000 max) | [`description.bbcode`](description.bbcode) |
| Change Notes | the 0.1.0 block in [`changelog.md`](changelog.md) |
| Version | `0.1.0` |
| Tags | `Roleplay Life RPG Economy Jobs Police Crime Multiplayer Everon 1989` |
| Licence | Arma Public License (APL) |
| Category | Game Mode (and Scenario if the dialog offers it) |

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
