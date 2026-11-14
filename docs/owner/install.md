# Install a server from zero

This page goes from a machine with the dedicated server installed to a player joining. It needs no
Workbench, no checkout and no script.

The life server is two Workshop items: the **core** (`LifeFramework`, `79636E668EA37AC9`) and the
**1989 Everon pack** (`LF_Everon1989`). The pack depends on the core, so `game.mods` lists the core
first. The pack carries the town; the core alone boots only its DebugWorld test map. To run the core with your
own pack ([add a delivery job](add-job.md) builds one), see [swap the pack out](../../server/example/README.md#swap-the-pack-out).
Neither item is on the Workshop until the first upload (`docs/roadmap/m3-plan.md` D3); until then a `-config` server cannot
download them (see Problems).

## 1. Install the server

Install **Arma Reforger Server** (Steam app `1874900`): Steam's **Tools** library on Windows, or:

```
steamcmd +login anonymous +app_update 1874900 validate +quit
```

## 2. Get the server folder

Take one of these; both give you `server.json` (the base game's server file) and `config.json` (this
mod's owner config):

- **The example server download** (`LifeFramework-ExampleServer.zip`, or the folder
  [`server/example/`](../../server/example/README.md)): `server.json`, `config.json`, `start.cmd` and
  `start.sh`. Unzip it anywhere, for example `C:LFServer`.
- **The config generator** at [life-framework.org/config](https://life-framework.org/config): pick a preset,
  change prices, set the server name, ports and admin password, then **Download both files**. Put the two files
  in a folder; the start line (step 4) is on the same page.

Open `server.json` and change the values you care about:

| Key | Set it to |
|---|---|
| `game.name` | your server's name in the browser |
| `game.passwordAdmin` | an admin password with no spaces; in game, log in with `#login <password>` |
| `game.password` | a join password, or leave it empty |
| `bindPort`, `publicPort`, `a2s.port` | other ports when 2001 and 17777 are taken (`publicPort` is `bindPort`) |
| `game.maxPlayers` | up to 16 on the 1989 town |
| `game.visible` | `true` to list the server in the browser |

Leave `game.scenarioId` and `game.mods` alone for this first boot. They name the pack's mission and both
Workshop items, the core first:

```json
"scenarioId": "{C638C03B775C31BB}Missions/LF_Everon1989.conf",
"mods": [
  { "modId": "79636E668EA37AC9", "name": "Life Framework (Core)" },
  { "modId": "B1E84D5687E11724", "name": "Life Framework: Everon 1989" }
]
```

The server downloads both mods from the Workshop on first boot. Players get them on join; they do not
install anything. A `-config` server only loads Workshop items: it cannot load local addon folders, and it
will not start while an id is not on the Workshop (see Problems).

## 3. Open the ports

Forward UDP `2001` (game) and UDP `17777` (the A2S query the browser uses) on your router and
firewall. Change the numbers in `server.json` if they are taken, and forward the new ones.

## 4. Start it

With the example server folder, run `start.cmd` (Windows) or `./start.sh` (Linux). It copies `config.json` to
where the mod reads it (step 6) and runs the line below. Set `LF_REFORGER_SERVER` first when the server is not in
the default Steam folder. With the generator's two files, run the line yourself:

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
What logging out does to a cuffed, downed, wanted or jailed player is the `disconnect` section
([disconnect rules](disconnect-rules.md)).
Which player state survives a restart (cash on hand, carried gear, hunger and thirst, warrants,
sentences, licences) is the `persistence` section ([persistence switches](persistence-switches.md)).

Restart the server after changing the file.

## 7. Join

1. Players need nothing installed: the game downloads both mods on join. (To see them first, subscribe to **Life Framework** and **Life Framework: Everon 1989** in the Workshop.)
2. Open the server browser and join your server by its `game.name`.
3. A new player first sees the rules quiz (the rules are explained on a wrong answer; `rulesQuiz.enabled` in the config turns it off), then registration form R-89. Your character is keyed to your Steam name.

The first player to join while the police roster is empty is put on the roster, so the owner can clock in. That makes them *eligible*, not on duty: they stay a civilian until they clock in at the duty desk in the police station. The server log confirms it with `[LFDebug:Whitelist] first player <uid> put on the empty police roster`. 

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
| `Addon <id> - Addon was not found on workshop`, `Cannot start until they are removed from server config` | A mod id in `game.mods` is not published. Use the ids above; a checkout's local addons cannot stand in for them on a `-config` server |
| `Unable to read the mission header` | `game.scenarioId` is in an addon that is not in `game.mods`: the 1989 mission needs both items, core first |
| `Game addon '58D0FB3206B6F859' not found` | A custom `-addonsDir` hides the base game; list the server's own `addons` folder in it too |
| `Wrong GUID` / `INVENTORY (W): Item ... not found in replication` | The server's mod download is incomplete; restart it and let the Workshop download finish |
| `[LFDebug:Config] no owner config at $profile:LifeFramework/config.json` | Expected on a first boot; the server uses the shipped values |
| `[LFDebug:Config] <path>.<key> is not <kind>` | The named field in `config.json` is the wrong type; that value falls back |
| `[LFDebug:Whitelist] whitelist file missing, police faction closed` | Expected on a fresh server. It is logged as an error at the first join, just before `first player ... put on the empty police roster` creates the file |
| `[LFDebug:Whitelist] no whitelist file yet: ...` | Expected before anyone has joined |
| `Unable to start replication` | Another server already holds the port; end `ArmaReforgerServer.exe` and start again |
| A save from another game build | After a game update, back up and test the save before keeping it; a different build may refuse it |

The server's save, roster and config live in the profile folder: copy it somewhere safe before a
game update or a mod update.
