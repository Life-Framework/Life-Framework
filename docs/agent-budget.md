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
   Don't paste in docs the agent can read itself.
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
9. **Read files in pieces.** Use `grep`, `head` and line ranges rather than
   whole files or full logs. Output is capped at 15k characters for Bash and
   10k tokens for MCP tools in the `.claude-gaming` profile.

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
