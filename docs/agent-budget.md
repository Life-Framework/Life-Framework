# Agent budget

The aim is to build M2 (52 issues) and M3 (40 issues) on one Claude Max
subscription plus the free DeepSeek model in opencode. M1 took 31 issues and
used up the subscription faster than that pace allows. These rules come from
reviewing the M1 threads.

## Where M1's tokens went (review of 2026-11-07 to 2026-11-09)

- **Every subagent ran on Opus.** More than 35 of them, including work a
  smaller model handles fine: website pages, the Discord logo, filing roadmap
  issues and research write-ups.
- **One orchestrator thread for everything.** It grew to about 2,400 items.
  Every wake-up rereads it, including the ones that only said "still running".
- **Long QA and debug runs.** Single agents ran to 450–1,100 items, and much of
  that was fighting the shared machine: a session slot held by another agent,
  a client that exited on launch, retries.
- **Fan-out.** Up to eight agents ran at once, and research agents started
  their own Opus helpers. A server restart lost the helpers, so their work was
  done twice.
- **Free models already work for simple jobs.** DeepSeek V4.1 Flash Free
  finished the account setup and a website article without help.

## Which model does what

| Tier | Model | Use it for |
|---|---|---|
| Lookup | Haiku (`lf-scout`) | API and asset lookups, grepping logs, PR or CI status, summarising one file or thread |
| Default | Sonnet (`lf-builder`) | A scoped issue end to end under `agent-scope`: code, tests, `wt ship`. Website, tools, scripted QA runs, splitting an epic into issues |
| Hard | Opus (`lf-hard`, or the session the human is talking to) | Talking with the human and planning. Designing replication, persistence or a new system. Debugging after Sonnet has stalled. Reviewing a risky diff (`interrogate`) |
| Background | DeepSeek V4.1 Flash Free (opencode, through `delegate_task`) | Non-blocking work that `tools\cli validate` or CI can check: docs, `features.md` and triage rows, localization strings, website copy and pages, issue grooming, research write-ups |

**Never give DeepSeek:** Enforce Script that needs in-game proof, persistence
payloads, replication, or `.et`/`.layout` GUID edits. A Claude agent does
those.

**When a job moves up to Opus:** after two failed attempts, or about 150 tool
calls without progress. The Sonnet agent stops and reports what it tried, and
the orchestrator restarts the job on `lf-hard` with that report as the brief.

## Working habits

1. **One orchestrator thread per milestone.** When it gets long, start a new
   thread from a short handoff. The roadmap issues are the memory, not the
   thread.
2. **Don't wake up just to report.** No status turns that say nothing new.
   Read a child's result once, when it finishes.
3. **No more than three Claude agents at once.** Background DeepSeek jobs
   don't count toward that limit.
4. **Subagents don't start helpers** unless their brief says so. Research runs
   in one agent.
5. **Short briefs.** Give the issue number, the files and the acceptance test.
   Don't paste in docs the agent can read itself. Development-track and spike
   briefs add: run the pool with `--priority low` (or create the worktree with
   `wt new <slug> --priority low`).
6. **Shared machines:** queue for a busy slot instead of debugging it. After
   two environment failures (launch, lock, Steam), stop and report.
7. **Cheap checks while iterating.** Use `test --only` and `validate` while
   working. Run `--tier all` and `validate --full` once, at `wt gate`.
8. **Wait with one call.** Repo tooling blocks until it has a verdict, so a
   wait costs one turn.
   - A step that takes up to 10 minutes (`session up`, `test`, `e2e`,
     `wt gate`) runs as one foreground call with a timeout up to 600000 ms. It
     queues quietly and prints a short verdict. `session.mjs wait` joins a
     session another call started.
   - A step longer than that runs in the background and the agent ends its
     turn. The completion notice wakes it.
   - Never use `sleep` or `until` loops, Monitor, ScheduleWakeup, or
     `session status` / `pool status` polls to wait on repo tooling. Each poll
     is a model turn that reads nothing new.
9. **Read the summary, not the transcript.** Heavy commands print one `SUMMARY` line and only the failing items; the full output is the `log=` file ("Compact command output" below).
10. **Read files in pieces.** Use `grep`, `head` and line ranges rather than
   whole files or full logs. Output is capped at 15k characters for Bash and
   10k tokens for MCP tools in the `.claude-gaming` profile.

## Compact command output

About 40% of the cost-weighted tokens in the last 30 threads went to reading code and logs, and every turn re-reads
what an earlier command printed. So the heavy commands print a summary and keep the transcript in a file.
Do not pipe them through `tail`, `head` or `Select-Object` any more.

A run of `validate`, `test`, `e2e`, `build`, `pack`, `ci`, `dev`, `look`, `shots`, `audit`, `session up|down|restart|wait|join`,
`wt gate|ship|e2e`, `asset run|anim|states|kit|sound|dialogue`, `harness.mjs flow|shot|chains|journey|town|pillar|play`,
and a `session.mjs <proof>` verb prints:

```
SUMMARY <cmd> ok|fail exit=N 43.4s lines=245 failures=0 warnings=39 log=tmp\logs\validate-20261114-114747.log
<each failing item with file:line and the exact error, up to 30 lines>
<warnings that change a decision (a pool cap or reclaim, a stale machine, a lowspec capture), up to 6>
skipped 3 check(s): validate-scripts.ps1, ...
<the command's own verdict lines: validate: 90 check(s), session: ready ..., the PR url, proof: PASS>
```

- **The full transcript is the log**, in `<worktree>\tmp\logs\<cmd>-<time>.log` (kept 3 days, never committed). Read it with
  `grep -n` or a line range when the summary names something you need to see.
- **`--verbose`** (or `LF_VERBOSE=1`) prints the full output as before, nothing filtered. Use it for one run, not by habit.
- **A failure, a timeout and a skipped check are never dropped.** If a failed run has no recognisable error line, the summary
  shows the end of the transcript. Failures past the cap are counted with the log named.
- A passing run of 10 lines or fewer prints as it is.
- `help` prints the command names (`help --verbose` is the full usage), `pool status` prints busy lanes, the queue and the
  budgets (`--verbose` adds the free lanes, last runs and sessions), `logs` clips a line at 300 characters (`--full`).
- A proof step that returns `error` or `timeout` shows as a failing line, and the proof's own `... proof: PASS|FAIL` line is kept.
- Tools that parse another command's output run it with `--verbose` or `LF_QUIET_CHILD=1`; a command run inside another
  one (`wt ship` runs validate and test) writes its transcript to the outer log.
- `tools/validation/output-budget-selftest.mjs` fails when a default output goes over its line budget.

Lines printed by default, before and after (2026-11-14, measured in one worktree):

| Command | Before | After |
|---|---|---|
| `validate` (skip mode) | 267 | 10 |
| `test --only <class>` | 15 | 6 |
| `session up` (lowspec, queued behind two sessions) | 14 | 5 |
| `session down` | 2 | 3 |
| `session.mjs keys-proof` | 13 | 3 |
| `harness.mjs town` | 33 | 2 |
| `look <area>` | 4 | 5 |
| `help` | 100 | 5 |
| `wt help` | 44 | 3 |
| `pool status` | 38 | 12 |

A run of at most 10 lines prints as it is plus the one SUMMARY line, which is why `session down` and `look` gain a line.

## Settings

In the `.claude-gaming` Claude Code profile (`~/.claude-gaming/settings.json`):

- **`CLAUDE_CODE_SUBAGENT_MODEL=sonnet`:** a subagent with no `model` runs on
  Sonnet, not Opus.
- **Output caps:** `bashOutputMaxChars: 15000` and `MAX_MCP_OUTPUT_TOKENS=10000`.
- **Fewer background calls:** spinner tips and the terminal-title call are off.

In T3, start new threads on Sonnet and switch a thread to Opus only for
planning or hard debugging. The agent files in `.claude/agents/` pin each
tier's model, so a delegated job gets the right model whatever the parent
thread uses.

## Docs and screenshots

The builder takes the screenshots during its proof session. The free DeepSeek model writes up the player docs, the website and the closed issue for each feature once it merges, and a weekly sweep catches anything missed. See `docs/agent-docs-sweep.md`.

## Nothing waits forever

On 2026-11-10, three agents stalled for an hour or more. Each time, a wait loop called something with no timeout, either an `ssh` probe or a log file that never appeared, so the loop never moved on.

**Rules:**
- Every external call has a bounded runtime. The `ssh` defaults are in `~/.ssh/config` (`ConnectTimeout 8`, `ServerAliveInterval 10`, `ServerAliveCountMax 3`). Wrap any other probe in `timeout <s>`.
- No hand-written wait loops. Use a repo command that blocks with its own budget, or run in the background and end the turn.
- The orchestrator's hourly check flags any agent with no activity for 30+ minutes.
