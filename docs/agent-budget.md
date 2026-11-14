# Agent budget

The aim is to build M2 and M3 on one Claude Max subscription plus the free
DeepSeek model in opencode. `tools\cli usage` measures where the tokens go
(`--top N`, `--since 7d`, `--type lf-hard`, `--leads`, `--detail`).

## Budgets and handoff

Every turn re-reads the whole context. Context starts near 45k and grows about
1.6k per turn, so a 400-turn run pays for 400k on each late turn: ten times a
fresh agent's turn. In the ten costliest runs, 94 to 99% of the cost came
after the context passed 150k.

| Agent | Hand off at | Typical run (median / p90 turns) |
|---|---|---|
| `lf-scout` | 30 tool calls | 13 / 48 |
| `lf-builder` | 80 tool calls (about 150k context) | 59 / 131 |
| `lf-hard` | 80 tool calls (about 150k context) | 149 / 288 |
| Lead thread | about 200k context: a new thread from a handoff | |

**Ship is inside the budget, not after it.** On 2026-11-14 several agents (#691, the role
uniforms, among them) hit their limit with the work finished and unshipped, and each needed a fresh
agent to reload everything just to run `wt ship`. So the ship is reserved: when you judge you are
near the limit (about 60 of 80 tool calls, or when a remaining step would not fit with the ship
behind it) and the work passes its tests, **ship what you have**. Commit, write the body, run `wt ship`.
List the checks you did not get to under `## Manual testing`, or open follow-up issues for them, and
stop polishing. A merged, in-dev PR with a short manual list beats a clean branch nobody shipped. Hand
off (below) only when the work does not pass its tests yet. This adds no step: it is the existing ship,
done on time.

**Handoff.** At the budget, when the work is not yet shippable, at the next checkpoint (tests green or a commit,
no session up), the agent:

1. commits work in progress to its `ws/<slug>` branch;
2. writes `tmp/handoff-<slug>.md`, at most 15 lines: goal, done (commits and
   evidence lines), the next step as an exact command, open questions, and the
   `path:line` places that matter;
3. ends with the five-line report, last line `RESUME <worktree>\tmp\handoff-<slug>.md`.

The lead starts a fresh agent of the same type with `Resume <slug>: read
<file> and continue from the next step.` Resumes in the logs cost a median
0.4M tokens. If `wt ship` is the only step left, finish instead; if you were already out of
budget and the lead starts a fresh agent for just that, use the short brief below.

**Ship-only brief.** A fresh agent that only ships needs five lines, not the history. It does not
re-read the issue, re-run the checks listed as passed, or polish.

```
Ship <slug>: worktree ..\Life-Framework-ws-<slug>, branch ws/<slug>, issue #<n>.
Body file: tmp\pr-body-<slug>.md (edit it if a line is wrong; do not rewrite it).
Already passed: <checks, e.g. validate, test --only LF_Test_X, look pictures kept with evidence, interrogate file ticked>.
Not done: <checks left; they go under ## Manual testing or become follow-up issues>.
Run: tools\cli wt ship <slug> --refs <n> --body-file tmp\pr-body-<slug>.md
Fix only what ship's body check or the gate reports. End with the five-line report.
```

**Final report: five lines.**

```
Changed: <one line>
Evidence: <the observed red, green or log line>
Manual: <steps the PR lists, or none>
Next: <follow-up or blocker, or none>
PR <url>, head <sha>, checks <state>        (or RESUME <state file>)
```

## Sizing a job

The lead sizes a job before starting an agent; a brief that will clearly pass
80 tool calls is split first.

- **"Town-wide", "all areas", "every sign":** one agent per area in
  `agent-box/areas.json`, or per few areas so each stays under the budget,
  three at a time. The town-wide placement audit ran as one agent:
  573 turns, 28M tokens; split, it would have cost about a quarter.
- **Two verbs, two agents:** spike + build, research + build, QA + polish,
  film + cut. The first leaves its result in `tmp/` or an issue; the second
  builds from that.
- **QA** runs one agent per flow; it files issues and does not fix them.
- **Media:** edit the storyboard or cut file and re-run its one render command
  (`tools\cli film-make`), never one `ffmpeg` call per turn.

## Searching and reading

- A question that needs more than three greps or reads in unfamiliar code goes
  to one `lf-scout` (Haiku, about 60k context). It is the one helper a builder
  or hard agent may start without its brief saying so.
- Put independent reads and greps in one turn. Runs average 1.07 calls per
  turn, and each turn pays for the whole context.
- Grep first, then read the range (`offset`/`limit`, `sed -n a,bp`). Logs:
  `tools\cli logs <pattern>`, never the whole file.

## Which model does what

| Tier | Model | Use it for |
|---|---|---|
| Lookup | Haiku (`lf-scout`) | API and asset lookups, searches across the repo, grepping logs, PR or CI status, summarising one file or thread |
| Default | Sonnet (`lf-builder`) | A scoped issue end to end under `agent-scope`: code, tests, `wt ship`. Visual builds (boards, signs, sheets), asset pipeline items, placement, video and trailer cuts, QA flows, website, tools, splitting an epic into issues |
| Hard | Opus (`lf-hard`, or the session the human is talking to) | Talking with the human and planning. Replication/JIP, persistence and restart bugs. A new system's design (it writes the plan; `lf-builder` builds it). Root-cause debugging after `lf-builder` stalled twice. Reviewing a risky diff (`interrogate`) |
| Background | DeepSeek V4.1 Flash Free (opencode, through `delegate_task`) | Non-blocking work that `tools\cli validate` or CI can check: docs, `features.md` and triage rows, localization strings, website copy and pages, issue grooming, research write-ups |

Never use `general-purpose`, `Explore` or `fork` for repo work; they run on
the parent's model with no budget. In M1, 130 `general-purpose` runs (111 on
Opus) took 30% of all spend.

**Never give DeepSeek:** Enforce Script that needs in-game proof, persistence
payloads, replication, or `.et`/`.layout` GUID edits. A Claude agent does
those.

**When a job moves up to Opus:** after two failed attempts at the same
problem. The Sonnet agent stops and reports what it tried, and the lead
restarts the job on `lf-hard` with that report as the brief. Running out of
budget is a handoff to a fresh `lf-builder`, not an escalation.

## Where the tokens went

**M1 (2026-11-07 to 11-09):** every subagent ran on Opus, one orchestrator
thread grew to 2,400 items, single QA and debug runs reached 450 to 1,100
items fighting shared machines, and up to eight agents ran at once.

**M2 and M3 (measured 2026-11-14, `tools\cli usage`, 478 subagent runs plus
49 lead threads, 1.42B cost-weighted tokens):**

- **Run length, not log dumps.** No result in the ten costliest runs was
  over 8k tokens (the output caps work), and at most 24 of 288 reads were
  repeats. Those runs each made 270 to 570 turns at 270 to 500k average context.
  Thinking and commands fill the context as much as tool results do (26M of
  each over all runs).
- **Routing.** 28 `lf-hard` runs on builds (the trailer, placement audit, case
  board, wanted boards, asset pipelines, videos) cost 200M; the 23 runs on
  replication, persistence, design and review cost 101M. 13 `lf-builder` runs
  resumed or finished another run's job, for 11M in total; the logs show no
  pattern of Sonnet failing and retrying.
- **Waiting.** 30% of `lf-builder` spend was session, test, wait and poll turns.
  The M2 QA pass waited 265 minutes in 42 calls while holding 329k context.
  The placement audit read a background batch's progress file 69 times.
- **Leads** took 18% at 295k average context. One lead thread ran 1,948
  turns (103M); a third of that was turns with no tool call (replies to
  notifications and the human).
- Simulated, the 150k handoff alone cuts the total by about 30%.

## Working habits

1. **One lead thread per milestone**, handed off near 200k context. The
   roadmap issues are the memory, not the thread.
2. **Don't wake up just to report.** Read a child's result once, when it
   finishes.
3. **No more than three Claude agents at once.** Background DeepSeek jobs
   don't count toward that limit.
4. **Subagents don't start helpers** unless their brief says so; `lf-scout` for
   a search is the exception.
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
   - Never use `sleep` or `until` loops, Monitor, ScheduleWakeup,
     `session status` / `pool status` polls, or reads of a background job's
     progress file to wait. Each poll is a turn that pays for the whole
     context and learns nothing.
9. **Prepare before a session, and book one only when nothing else shows it.** Make every edit before
   `session up`; the session's turns are the most expensive ones in a run, and session lanes had
   the longest waits on 2026-11-14 (p90 about 6 min, worst 61 min). PR pictures come from `look`.
10. **Read the summary, not the transcript.** Heavy commands print one `SUMMARY` line and only the failing items; the full output is the `log=` file ("Compact command output" below).

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

The builder takes PR screenshots with `look`, `look item` or `wb_preview` and keeps them with `tools\cli evidence`; a session shot only for what Workbench play cannot capture (UI, a second player, motion, sound). The free DeepSeek model writes up the player docs, the website and the closed issue for each feature once it merges, and a weekly sweep catches anything missed. See `docs/agent-docs-sweep.md`.

## Nothing waits forever

On 2026-11-10, three agents stalled for an hour or more. Each time, a wait loop called something with no timeout, either an `ssh` probe or a log file that never appeared, so the loop never moved on.

**Rules:**
- Every external call has a bounded runtime. The `ssh` defaults are in `~/.ssh/config` (`ConnectTimeout 8`, `ServerAliveInterval 10`, `ServerAliveCountMax 3`). Wrap any other probe in `timeout <s>`.
- No hand-written wait loops. Use a repo command that blocks with its own budget, or run in the background and end the turn.
- The orchestrator's hourly check flags any agent with no activity for 30+ minutes.
