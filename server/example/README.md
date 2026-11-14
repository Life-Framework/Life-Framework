# Example server: the 1989 Everon flagship

A ready folder. Copy it, point it at your Reforger server, run one command, and the core
(`LifeFramework`) and the 1989 Everon pack (`LF_Everon1989`) boot as a dedicated server.

| File | What it is |
|---|---|
| `server.json` | The base game's dedicated-server config: ports, name, admin password, the scenario `LF_Everon1989.conf` and the mod list (core first, then the pack) |
| `config.json` | The owner config from the flagship preset: every value of the shipped economy, written out so you can change it |
| `start.cmd`, `start.sh` | The one start command (Windows, Linux) |
| `profile/` | Created on first start: logs, saves, and the copy of `config.json` the server reads |

## Use it

1. Install **Arma Reforger Server** (Steam app 1874900).
2. Copy this folder anywhere (`C:\LFServer`).
3. In `server.json` set `game.name`, and **`game.passwordAdmin`** (one word, no spaces: log in in game with
   `#login <password>`). Ports are `bindPort` 2001 and `a2s.port` 17777 (UDP): change them when taken, forward them.
4. Optional: edit `config.json` (prices, wages, autosave). `tools\cli config-check config.json` or the
   [config page](https://life-framework.org/config) tells you about a mistake before the restart.
5. Start it:
   - Windows: `set LF_REFORGER_SERVER=C:\Program Files (x86)\Steam\steamapps\common\Arma Reforger Server` (only when the server is
     elsewhere), then `start.cmd`
   - Linux: `LF_REFORGER_SERVER=~/reforger-server ./start.sh`

   Both copy `config.json` to `profile/profile/LifeFramework/config.json` (where the mod reads it) and run
   `ArmaReforgerServer -config server.json -profile profile -maxFPS 60`. Extra arguments pass through:
   `start.cmd -loadSessionSave` continues the saved town after a restart. Stop it with Ctrl+C so it writes the shutdown save.
6. The first start downloads both mods from the Workshop; players get them on join.

After a game update, start once, watch `profile/logs/*/console.log` for `[LFDebug:Persistence] save loaded:` and
copy `profile/` away first (a save written by an older build; `docs/ops/game-updates.md`).

## Engine settings worth a look in `server.json`

`game.gameProperties`: `disableThirdPerson` (true for first person only), `serverMaxViewDistance`,
`networkViewDistance`, `battlEye`. Survival values (hydration, repacking) are decided by the mod's feature config, not here.

## For maintainers

- `tools\cli server-template check` is the static check (also in `validate`): server.json names the core then the pack
  and boots the pack scenario, config.json is the flagship preset and passes `config-check`, the start scripts and this README
  say what a host needs. `--engine` lets the real dedicated server read `server.json` and prints its schema verdict.
- Boot proof from a checkout: `tools\cli session up --mission template` starts a dedicated server on this folder's
  scenario and mods (with `config.json` in its profile) and joins a client. The engine refuses `-config` together with
  `-addons`, so a checkout boots the same files with `-server` and `-addons`; only a published mod loads through `-config`.
- Not in this folder yet: the release download (#296) and swapping the pack out (#297). The download needs the
  Workshop ids above to be live and a zip built from this folder (`server.json` ids, scenario ids stay in step through
  `validate`). Swapping the pack means replacing the second `mods` entry and `scenarioId` with the other pack's ids;
  `templateProblems` pins the flagship today.
