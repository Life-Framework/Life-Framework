# Install a server from zero

This page goes from a machine with the dedicated server installed to a player joining. It needs no
Workbench, no checkout and no script.

The life server ships as Workshop items. Today the playtest town is in the core item
(`79636E668EA37AC9`). The 1989 pack becomes a second item (#223) and is not published yet; when it
is, add its ID to `game.mods` beside the core's.

## 1. Install the server

Install **Arma Reforger Server** (Steam app `1874900`): Steam's **Tools** library on Windows, or:

```
steamcmd +login anonymous +app_update 1874900 validate +quit
```

## 2. Copy the server config

Copy [`docs/playtest/server-config.example.json`](../playtest/server-config.example.json) next to
`ArmaReforgerServer.exe` as `server.json`. Change the values you care about:

| Key | Set it to |
|---|---|
| `game.name` | your server's name in the browser |
| `game.passwordAdmin` | an admin password with no spaces; log in in game with `#login <password>` |
| `game.password` | a join password, or leave it empty |
| `bindPort`, `publicPort`, `a2s.port` | other ports when 2001 and 17777 are taken |
| `game.maxPlayers` | up to 16 on the playtest town |
| `game.visible` | `true` to list the server in the browser |

Leave `game.scenarioId` and `game.mods` alone for this first boot:

```json
"scenarioId": "{1054CC8646357708}Missions/LF_PlaytestTown.conf",
"mods": [
  { "modId": "79636E668EA37AC9", "name": "Life Framework" }
]
```

The server downloads that mod from the Workshop on first boot. Players get it on join; they do not
install anything.

## 3. Open the ports

Forward UDP `2001` (game) and UDP `17777` (the A2S query the browser uses) on your router and
firewall. Change the numbers in `server.json` if they are taken, and forward the new ones.

## 4. Start it

```
ArmaReforgerServer.exe -config server.json -profile C:\LFServer\profile -maxFPS 60
```

Linux: `./ArmaReforgerServer -config server.json -profile ~/lfserver/profile -maxFPS 60`.

The first boot downloads the mod and logs `[LFDebug:...]` lines as the town comes up. Wait for:

```
[LFDebug:...] ...
Entered online game state.
```

Logs land under `<profile>\logs\`.

## 5. Know the two folders

The `-profile` argument names a folder. The game puts its own profile one level down:

| What | Where |
|---|---|
| Engine profile (`$profile:`) | `C:\LFServer\profile\profile\` |
| Logs | `C:\LFServer\profile\logs\` |
| Saves | inside the engine profile, under a `.save` folder |
| Owner config | `C:\LFServer\profile\profile\LifeFramework\config.json` |
| Police roster | `C:\LFServer\profile\profile\LifeFramework\whitelist.txt` |

Back up the `C:\LFServer\profile` folder. It holds the saves, the roster and the config.

## 6. Where config.json goes

The server runs on the shipped values until you create
`$profile:LifeFramework/config.json`, which is the path in the table above. On the first boot it
writes `config.example.json` beside where that file would be; it never writes `config.json` itself.

```json
{
  "schemaVersion": 1,
  "shops": {
    "defaultBuyPrice": 50,
    "defaultSellPrice": 25
  }
}
```

A missing file is not an error. A bad field is refused and the shipped value stays, so the server
still boots; the log line names the field. If you have a checkout of the repository,
`tools\cli config-check <file>` reports the same problems offline. The field list, types and
defaults are in the [config reference](../config/owner-config.md) and on the website's config page.

Restart the server after changing the file.

## 7. Join

1. In Arma Reforger, open the **Workshop** and subscribe to **Life Framework**.
2. Open the server browser and join your server by its `game.name`.
3. On the first join, fill in registration form R-89. Your character is keyed to your Steam name.

The first player to join an empty police roster is put on it.

## 8. Keep the save across restarts

Stop the server normally (Ctrl+C in its console, or `tools\cli host stop` if you run it that way),
then add `-loadSessionSave` to the command:

```
ArmaReforgerServer.exe -config server.json -profile C:\LFServer\profile -maxFPS 60 -loadSessionSave
```

Without the flag the town starts fresh. Do not delete the profile folder between runs.

## Problems

Read the newest `console.log` under `<profile>\logs\` before guessing. These rows cover the common
first boots; the full list is in [debug log triage](../debug-log-triage.md).

| Line or symptom | What it means |
|---|---|
| The browser does not list the server | UDP `2001` or `17777` is not forwarded, or `game.visible` is `false` |
| `Wrong GUID` / `INVENTORY (W): Item ... not found in replication` | The server's mod download is incomplete; restart it and let the Workshop download finish |
| `[LFDebug:Config] no owner config at $profile:LifeFramework/config.json` | Expected on a first boot; the server uses the shipped values |
| `[LFDebug:Config] <path>.<key> is not <kind>` | The named field in `config.json` is the wrong type; that value falls back |
| `[LFDebug:Whitelist] whitelist file missing, police faction closed` | Expected on a fresh server; the roster file is created on the first join |
| `[LFDebug:Whitelist] no whitelist file yet: ...` | Expected before anyone has joined |
| `Unable to start replication` | Another server already holds the port; end `ArmaReforgerServer.exe` and start again |
| A save from another game build | After a game update, back up and test the save before keeping it; a different build may refuse it |

The server's save, roster and config live in the profile folder: copy it somewhere safe before a
game update or a mod update.
