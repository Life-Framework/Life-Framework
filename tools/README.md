# Development Tools

Utilities, scripts and AI-assistant tooling for Life Framework development.

## Unified CLI

`tools\cli` (Windows shim; underlying script `tools/cli.mjs`, zero-dependency
Node.js) is the entry point for everything in this directory:

```
tools\cli status                        # toolchain + MCP server state
tools\cli mcp install [name]            # clone + build MCP servers
tools\cli mcp update [name]             # update MCP servers from GitHub
tools\cli mcp verify [name]             # prove a server starts
tools\cli mcp enable|disable <name>     # toggle a server in opencode.json
tools\cli build                         # headless Workbench build (worktree only)
tools\cli serve                         # boot the headless test server (blocks, worktree only)
tools\cli test [--no-build] [--tier fast|all|persistence]
                                        # build + boot server + parse ELTEST results (worktree only)
tools\cli ci                            # validate + build + test (full gate, worktree only)
tools\cli wt new <feature>              # create worktree + branch + port allocation
tools\cli wt list                       # worktrees, ports, dirty/merged state
tools\cli wt test|build|dev|gate <slug> # run a heavy command in a specific worktree
tools\cli wt ship <slug>                # sync -> gate -> push -> PR -> auto-merge into main
tools\cli wt prune <slug>               # remove a merged worktree + branch
tools\cli call <tool> '<json>'|@file    # call an MCP tool directly (see mcp-call.mjs)
tools\cli validate                      # run tools/validation/* checks
tools\cli lint                          # run tools/lint/* checks
tools\cli run test                      # run tools/test/* checks
```

`build / test / dev / serve / ci` refuse to run in the main checkout (the
world-editor copy). Create a worktree with `cli wt new` — see
`docs/worktrees.md` for the full parallel-agent workflow.

Adding a new check = dropping a runnable script into the matching folder.
Runs on Windows (PowerShell), and works from any terminal.

## Contents

- `cli.mjs` / `cli.cmd` - the unified CLI (install/update/verify/toggle MCPs, build, serve, test, dispatch checks)
- `wt.mjs` - parallel-worktree library (hub state, port allocation, locks, gh/REST PR + merge)
- `mcp-call.mjs` - call an MCP tool from the shell (API research without opencode's MCP plumbing)
- `mcp/` - Enfusion/Reforger MCP servers for AI assistants (git-ignored clones; see `mcp/README.md`)
- `build/` - Build and packaging scripts
- `validation/` - Repo validator (also wired in as a git pre-commit hook; see below)
- `lint/` - Lint checks (drop a script, `tools\cli lint` runs it)
- `test/` - Test scripts (drop a script, `tools\cli run test` runs it)
- `utilities/` - Helper scripts for common tasks

## MCP tool calls without opencode

`tools\cli call <tool> '<json args>'` (or `node tools/mcp-call.mjs ...`) speaks
JSON-RPC to the configured MCP server over stdio and prints the tool result.
Use it for API research in any session:

```
tools\cli call list
tools\cli call api_search '{"query":"SCR_SpawnLogic","format":"tree"}'   # or @path/to/args.json
tools\cli call game_read @tmp/args.json        # file-based args (safer than inline)
```

The server command + env come from `opencode.json`, so it works from any checkout
that has the server installed under `tools/mcp/`. See `tools/mcp/README.md` for
the servers this can reach and `tools/mcp-call.mjs` for the `--env` override.

## Build / test pipeline

The mod's automated lifecycle (see `../AGENTS.md`):

```
tools\cli build    # headless Workbench build: addons/LifeFramework -> server/build/ (PC)
tools\cli test     # boot Arma Reforger Server on Worlds/DebugWorld (Missions/EL_DebugTest.conf),
                   #   run the ELTEST suite, parse [ELTEST] SUMMARY, exit nonzero on failure
tools\cli ci       # validate + build + test
```

- Test scenario: `addons/LifeFramework/Missions/EL_DebugTest.conf` (DebugWorld + `EL_TestGameMode`)
- Server config: `server/configs/test-server.json`; wrappers: `server/scripts/launch-test.{ps1,sh}`
- Logs land in `server/logs/`; build output and profile are git-ignored.

## Pre-commit hook

The repo validator (`tools/validation/validate-repo.ps1`) is installed as a git
pre-commit hook via `.githooks/` (`git config core.hooksPath .githooks`). It
blocks commits that would introduce Workbench artifacts (`*.rdb`, `log`,
`*.gproj.user`), MCP clones, orphan `.meta` files, or duplicate resource GUIDs,
and warns on lowercase `data/` path references.

## Available Tools

### Build Tools
*Coming soon - scripts for packaging and releasing the mod*

### Validation Tools
- `validation/validate-repo.ps1` - artifact/GUID/meta consistency checks (pre-commit + manual)

### Utilities
*Coming soon - helper scripts for development tasks*

## Contributing Tools

If you create useful development tools or scripts, please contribute them here!