# Host the playtest town on a dedicated server

For community hosts running the Workshop item **Life Framework** (mod ID `79636E668EA37AC9`).
The server downloads the mod from the Workshop; players get it on join.

Playing alone without the Workshop? The zip on the website (see [`docs/playtest.md`](../playtest.md))
has a launcher that starts the same town. This guide is for servers.

## 1. Install the server

Install **Arma Reforger Server** (Steam app 1874900): Steam's Tools library on Windows, or
`steamcmd +login anonymous +app_update 1874900 validate +quit` on Windows or Linux.

## 2. Copy the config

Copy [`server-config.example.json`](server-config.example.json) next to the server as
`server.json` and change:

| Key | Set it to |
|---|---|
| `game.name` | your server's name in the browser |
| `game.passwordAdmin` | a password with no spaces; log in in game with `#login <password>` |
| `game.admins` | your Steam64 ID or Bohemia identity ID (optional) |
| `game.password` | a join password, or leave it empty |
| `bindPort`, `publicPort`, `a2s.port` | other ports when 2001 and 17777 are taken; forward them (UDP) |
| `game.maxPlayers` | up to the town's 16, or more at your own risk |

Leave `game.scenarioId` (`{1054CC8646357708}Missions/LF_PlaytestTown.conf`) and `game.mods` as
they are. The config follows the BIKI page
[Arma_Reforger:Server_Config](https://community.bistudio.com/wiki/Arma_Reforger:Server_Config).

## 3. Start it

```
ArmaReforgerServer.exe -config server.json -profile C:\LFServer\profile -maxFPS 60
```

Linux: `./ArmaReforgerServer -config server.json -profile ~/lfserver/profile -maxFPS 60`.

The first start downloads the mod and logs `[LFDebug:...]` lines as the town comes up.
Players find the server by name in the server browser.

## 4. Keep the save across restarts

The town saves on its own (cash, bank, jobs, cars, organizations). Stop the server normally
(Ctrl+C in its console) so it writes the shutdown save, then start it with
`-loadSessionSave` added to continue that save:

```
ArmaReforgerServer.exe -config server.json -profile C:\LFServer\profile -maxFPS 60 -loadSessionSave
```

Without the flag the town starts fresh. The save lives in the profile folder; back that folder up.

## What is different on this scenario

- Cash and bank top up when they run low, and every new character owns two test cars.
- The first player to join while the police roster is empty is put on it. The roster is
  `<profile>/profile/LifeFramework/whitelist.txt`; back it up, and delete it with the server
  stopped to empty the roster. On a fresh server the log line `whitelist file missing, police faction closed`
  is expected: the file is created when the first player joins.
- The debug menu is closed for everyone except logged-in admins.

## Licence

The Workshop build you run is under the Arma Public License (APL). The source on GitHub is MIT.

## Problems

Send the server's `console.log` (under `<profile>/logs/`) with the problem on the Life Framework
[Discord](https://discord.gg/MjMexJteqz) or as a
[GitHub issue](https://github.com/Life-Framework/Life-Framework/issues).
