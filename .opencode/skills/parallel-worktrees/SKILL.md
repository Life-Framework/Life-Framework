---
name: parallel-worktrees
description: "Use before any build, test, dev, serve, or ci command, or when starting mod work as an agent. Keep the main checkout for the world editor; use a registered worktree and ship through the CLI."
---

# Parallel Worktrees

`AGENTS.md` is the contract. This file is the short command reference.

## Start

```text
tools\cli wt new <slug>
cd ../Life-Framework-ws-<slug>
```

Never create the worktree or ports by hand. Work on branch `ws/<slug>`.

## Iterate and gate

```text
tools\cli dev --tier fast
tools\cli wt gate <slug> [--wait]
```

`dev` is the fast loop. `gate` runs validation, build, and the full test tier.
Use `tools\cli wt <command> <slug>` from the main checkout if the worktree's
CLI is stale.

## Ship and clean up

```text
tools\cli wt ship <slug>
tools\cli wt prune <slug>
```

Ship requires a clean committed branch and passes the full gate before pushing
and merging. Use `--pr-only` when review is required. Prune only after merge.

Never run heavy commands in the main checkout or bypass the CLI's worktree
checks. Do not hand-edit `tmp/wt/`.
