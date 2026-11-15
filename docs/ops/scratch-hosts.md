# Scratch hosts

A scratch host is a throwaway server folder an agent makes to prove a host step: a first boot, a wipe, a
restore, an install guide dry run. It is not the live server and it is not a worktree. This page says where one
goes, who removes it, and how to find the ones nobody removed.

## Where it goes

`C:\LF\scratch\<slug>-<stamp>` on the pool machine that runs it.

- `<slug>` is the slice or task, for example `slice-1669` or `flagstart`.
- `<stamp>` is the start time as `yyyyMMdd-HHmm`, for example `20261115-0720`.

One folder per run. Never put a scratch host under `C:\LF\b2-hosts`, beside a worktree, or inside
`server/profile`. `C:\LF\b2-hosts` was where #1647's live run left its folders; it is gone and nothing writes
there any more.

## Who removes it

The agent that made it, at the end of its own run, whether the run passed or failed. Use the repo's remover,
never `rm -rf`, `Remove-Item -Recurse` or `git worktree remove --force`: a scratch host can hold junctions into
the Steam installs, and a plain recursive delete follows them and empties the base game
(`AGENTS.md`, "Worktrees").

```powershell
ssh lf-cloud 'node C:\LF\Life-Framework\tools\safe-rm.mjs C:\LF\scratch\<slug>-<stamp>'
```

Stop the host's server process first. A folder a process still holds cannot be removed, and `safe-rm` prints
`safe-rm: FAIL <dir>` for it. If the process belongs to another agent, say so in a comment on that agent's issue
instead of stopping it.

## List the leftovers

A leftover is a folder in `C:\LF\scratch` whose agent has finished. List them on a pool machine:

```powershell
ssh lf-cloud 'Get-ChildItem C:\LF\scratch -Force -ErrorAction SilentlyContinue | Select-Object Name,LastWriteTime'
ssh lf-testbox 'Get-ChildItem C:\LF\scratch -Force -ErrorAction SilentlyContinue | Select-Object Name,LastWriteTime'
```

An empty answer, or a missing `C:\LF\scratch`, means nothing is left. Before removing a listed folder, check that
no server holds it:

```powershell
ssh lf-cloud 'Get-CimInstance Win32_Process | Where-Object { $_.CommandLine -like "*C:\LF\scratch*" } | Select-Object ProcessId,Name'
```

A folder with a live process belongs to a running agent; leave it. A folder with none and a `LastWriteTime` older
than a day is a leftover: remove it with `safe-rm` as above.

Whoever finds a leftover removes it and names the folder in the PR or comment that mentions it.
