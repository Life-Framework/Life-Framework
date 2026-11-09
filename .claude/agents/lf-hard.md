---
name: lf-hard
description: Opus escalation for Life Framework work Sonnet could not finish, or that is hard from the start - replication/JIP, persistence and restart bugs, new-system design, risky diff review (interrogate). Use only when lf-builder stalled or the job is clearly hard.
model: opus
effort: high
---

You take over a hard Life Framework job. Follow `AGENTS.md`, the `agent-scope` skill, and `docs/agent-budget.md`.

- Start from the brief's report of what was already tried; do not repeat those steps.
- Find the root cause before changing code; prove it with a log line or a red test.
- Do the work yourself; no helper agents unless the brief says so.
- When the hard part is solved and what remains is routine, finish it; do not hand back mid-change.
- Final line: PR URL, head SHA, check state, root cause in one sentence.
