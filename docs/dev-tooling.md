# Developer tooling reference

The parts of the dev setup that a first change does not need: test machines,
hidden tool windows, the full MCP details, the player-equivalent agent box, and
video. The path every contributor follows is [setup.md](setup.md). Engineering
rules are in [AGENTS.md](../AGENTS.md); worktrees, the test pool and shipping are
in [worktrees.md](worktrees.md).

## Machine-local settings (`lf.local.json`)

A git-ignored `lf.local.json` in the main checkout holds one machine's settings
for the CLI and every worktree of that checkout (`tools/lib/local-config.mjs`).
Precedence: OS env > `lf.local.json` > `opencode.json` > the Steam defaults.
Without the file nothing changes.

| Key | Meaning |
| --- | --- |
| `ENFUSION_*_PATH` | Same as the env vars in [setup.md](setup.md) |
| `LF_GAME_ADDONS` | Folder with the base-game `core/` and `data/` projects the test server loads through the `game-addons` junction. Default: the server install's `addons`, else the game client's |
| `LF_TESTBOX` | ssh host of a second test machine; added to the pool when `tools/pool.json` lacks it (`docs/worktrees.md` "Test pool") |
| `LF_TESTBOX_ROOT` | Main checkout on that machine |
| `LF_TESTBOX_AUTO` | `0` keeps `test` on this machine by default (`--remote` and `--machine` still work) |
| `pool` | Object: `{ "machines": [ { "name": "my-testbox", "slots": 2 } ] }` merges by name over `tools/pool.json`; `"disabled": true` drops a machine, a new name adds one (`docs/worktrees.md` "Test pool") |

## opencode global config

Machine-specific env for the MCP servers can live in your **global** config at
`~/.config/opencode/opencode.json` (deep-merged over the repo's), which keeps
machine paths out of the repo:

```json
{
  "mcp": {
    "enfusion-mcp": {
      "type": "local",
      "command": ["node", "dist/index.js"],
      "cwd": "tools/mcp/enfusion-mcp-bk",
      "enabled": true,
      "environment": {
        "ENFUSION_WORKBENCH_PATH": "C:/Program Files (x86)/Steam/steamapps/common/Arma Reforger Tools",
        "ENFUSION_GAME_PATH": "C:/Program Files (x86)/Steam/steamapps/common/Arma Reforger",
        "ENFUSION_PROJECT_PATH": "C:/Users/you/Documents/My Games/ArmaReforgerWorkbench/addons"
      }
    }
  }
}
```

## Tool windows stay off the screen

Agents run Workbench and the server many times an hour. Those runs start on a
private Win32 desktop named `LF-Agents` (`tools/lib/quiet-launch.ps1`), so they
never draw a window on your monitor and never take focus:

| Run | Where it goes |
| --- | --- |
| `validate --full` (Workbench `-validate` in `validate-scripts.ps1`), `cli build` | `LF-Agents` desktop; exit code and console.log as before. On a PC whose pool caps lack `workbench` neither starts a Workbench: both run on a pool machine (`docs/worktrees.md` "No Workbench on this PC") |
| `cli test` / `wt e2e` dedicated server | `LF-Agents`, no console window |
| `agent-box/session.mjs up` game clients | `LF-Agents`; the server runs detached with no console |
| `cli call wb_*`, `agent-box/harness.mjs play` | When no Workbench serves NET API port 5775, `tools/lib/quiet-workbench.mjs` starts one on `LF-Agents` first, so the MCP never launches a visible one. Refused on a PC without the `workbench` cap |
| every node-spawned console tool | `windowsHide: true` |

Workbench play mode and a game client both render on `LF-Agents`: harness
screenshots (`LF_ScreenshotCapture`) come back as the real 3D world. Screen
recording (`tools/film`) cannot see that desktop; film a game client you start
yourself on a visible monitor. `film.mjs` no longer brings that window to the
front; on a second monitor it lifts it without activating it.

- `tools\cli hidden` lists processes with a window on `LF-Agents`;
  `tools\cli hidden --close` ends them (a hidden Workbench stays up between agent
  calls).
- `LF_SHOW_WINDOWS=1` turns all of this off for debugging: every run starts on
  your desktop again.
- `wt open --workbench` and `wt open` are for you, so they still open visibly.

### Workbench window capture (`wb-shot`)

`System.MakeScreenshot` grabs only the 3D framebuffer. `tools\cli wb-shot --pid <Workbench pid>
[--title "World Editor"] [--crop x,y,w,h] [--max-px N] [--out f.png]` captures the whole Qt window
(Log Console, Resource Browser, Object Properties, menus) with Win32 `PrintWindow`
(`PW_RENDERFULLCONTENT`, fallback 0), `GetDIBits` 24-bit top-down, BGR to RGB, optional crop and
downscale, PNG. `--list` shows the pid's windows, `--find` the Workbench pids, `--wait <s>` blocks
(bounded) until the pid has a visible window. Code: `tools/lib/wb-window-capture.{ps1,mjs}`.

- **It works on `LF-Agents`**, but only because the capture thread attaches to that desktop first
  (`SetThreadDesktop`, which never switches the visible desktop). Without it `PrintWindow` returns
  false there. A Workbench on `LF-Agents` is a live, rendered session, so the spec's constraint holds.
- It needs the window visible and not minimised, and a pid: only that process's windows are listed
  or captured. It sends no input, never focuses a window and never reads the screen.
- The main window of an ultrawide Workbench is 5136x1456; use `--crop` for one panel at full
  resolution (`--crop 0,0,1000,1456` is the Log Console + Resource Browser column) or `--max-px`
  for an overview. A frame with under 16 colours is reported as blank (exit 1).
- `System.MakeScreenshot` stays the tool for the 3D view; this one is for panels and dialogs.

### Modal dialogs on the hidden desktop

`RegisterResourceFile` on a new `.fbx` opens "Register Resource File(s)" (as Model / as Animation), and the
first restricted script call can open "Script Authorization Required". On
`LF-Agents` nobody can click them, so the NET API call hangs. Set
`LF_WB_DIALOG_WATCHER=1` (or pass `dialogWatcher: true` to
`ensureQuietWorkbench`; `startDialogWatcher({ workbenchPid, log })` for a
Workbench you launched yourself) and `tools/lib/wb-dialog-watcher.ps1` runs
beside that Workbench on `LF-Agents`. It answers only those two titles, only
on windows owned by that one Workbench pid, by `BM_CLICK` on a Button child,
then a posted Alt mnemonic, then a click posted at a fixed offset from the
dialog's bottom-right corner ("as Model" for the FBX prompt). It never moves
the cursor or injects input, and it exits 3 without looking at a window when
its thread is not on `LF-Agents`. Each answer is one JSON line in
`<checkout>/tmp/wb-dialog-watcher.jsonl`, with a PNG of the dialog in
`tmp/wb-dialog-snaps/`; it exits with the Workbench.

## Video post-production (optional, `film-make`)

`tools\cli film-make video/<name>.json` joins footage into a trailer with cards, crossfades, captions, voice-over and a
music bed (`video/README.md`, `docs/film-pipeline.md`). It needs the portable ffmpeg (`node tools/film/film.mjs fetch`) and,
for voice-over, Piper (MIT, offline) in a tools-local folder: `tools\cli film-make --setup` downloads the Windows Piper
release and the `en_US-ljspeech-high` voice (public domain) into `tools/film/.tts/` (git-ignored). Nothing is installed globally and no
Python is needed. Licences of the voice and music: `docs/credits.md`.

## MCP servers in detail

MCP servers are configured in [`opencode.json`](../opencode.json); their clones
live git-ignored under `tools/mcp/`. `enfusion-mcp` (API search, vanilla source,
assets, prefabs) is enabled; `enfusion-workbench` (live Workbench bridge, `wb_*`
tools) is also enabled in the committed `opencode.json` and only works while
Workbench is open. Manage them with:

```sh
node tools/cli.mjs mcp install [name]   # clone if missing, npm ci, build
node tools/cli.mjs mcp update [name]    # git pull --ff-only, npm ci, build
node tools/cli.mjs mcp verify [name]    # boot briefly to prove it starts
node tools/cli.mjs mcp enable <name>    # enabled=true in opencode.json
node tools/cli.mjs mcp disable <name>   # enabled=false in opencode.json
```

`enfusion-workbench` carries a native addon (`better-sqlite3`) compiled for one
Node ABI. After a Node upgrade the server dies at startup with "compiled
against a different Node.js version". `tools\cli status` names the addon and
exits 1; `tools\cli mcp install` (or `update`) runs `npm rebuild` on it and
checks it loads. A new worktree has no `tools/mcp/` clones of its own; the CLI
and `tools\cli call` use the main checkout's clones.

Call any server tool from a plain terminal, no opencode session or restart
needed (see `tools/mcp-call.mjs`):

```sh
node tools/cli.mjs call list
node tools/cli.mjs call api_search '{"query":"SCR_SpawnLogic","format":"tree"}'
node tools/cli.mjs call game_read @tmp/args.json     # file args: PowerShell mangles inline JSON
node tools/cli.mjs call --batch @tmp/wb-session.json # several calls in ONE server process
```

The Workbench bridge caches the editor mode per process, so a fresh process
starts in an "unknown" mode and mode-gated tools refuse. Run `wb_state` and
`wb_play` in one `--batch` session.

Each `call` starts its own server and stops it when the call returns (about
2.5 s and 200 MB for that time), so calls leave no process behind. A shared,
always-on server would hold that memory permanently and mix per-process state
(the Workbench mode cache, `--env` overrides that point at a worktree), so
there is none.

**Memory under many agents.** Claude Code starts every stdio MCP server in
its config once per agent. Register only what most agents use; a rarely used
server costs one Node process (at least 25 MB) per running agent. The Discord
connector (`tools/discord-mcp`) connects to the Discord Gateway on the first
`get_new_events` or `wait_for_events` call rather than at startup
(`DISCORD_GATEWAY=eager` restores the old behaviour). Restart opencode after
changing MCP settings.

## Parallel agents

Many agents can work at once, each in its own worktree
(`node tools/cli.mjs wt new <feature>`). `wt ship` needs the GitHub CLI signed in
once (`gh auth login`), or a `GITHUB_TOKEN` env var. Full detail, the test pool
and the ship queue: [worktrees.md](worktrees.md).

## Agent box (GPU or remote machine)

Clone, then install the Workbench MCP servers and the player harness:

```powershell
powershell -ExecutionPolicy Bypass -File agent-box\setup.ps1
node agent-box/harness.mjs selftest
```

That writes `.cursor/mcp.json` for this checkout (gitignored; it contains
absolute paths). Restart the agent session so the servers load. The three
pillars and the player-key hooks are in [agent-box/README.md](../agent-box/README.md).

On a T3 server, register the clone as a project and use the scripts in
`agent-box/t3-project.json`. Threads should use worktrees so the main checkout
stays the world-editor copy.

## The clean-server check for the Workshop items

`tools\cli workshop boot-check 4550A2B656E1B569 B1E84D5687E11724` boots a clean
dedicated server on a pool machine with no local addons and an empty cache, then
joins a client. It proves that a stranger's server boots from the Workshop alone
(#371). The maintainer's upload steps are
[release-first-upload.md](release-first-upload.md). **Status:** the tool is proven
up to the server's `Addon <id> - Addon was not found on workshop.` line; the
passing run, its log lines and the Workshop page URLs are recorded here after the
first upload (#370).
