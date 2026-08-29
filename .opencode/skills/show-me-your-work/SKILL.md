---
name: show-me-your-work
description: "Use when starting a long, autonomous, or multi-phase task on this mod, or any task the user steps away from to review later. Keep a decision trail the human can audit: one record per decision, why it was made, what evidence drove it, what was ruled out. Trigger on 'keep a decision trail', 'I'm going to bed', 'trust it when I'm back', 'record your reasoning', 'run until done'."
---

# Show me your work

A reviewable decision trail. When the human is away, the trail is the accountability. Commit it when the stakes need an auditable record; keep it local otherwise.

## The trail

A plain-text log, one record per decision, appended as you go:

- **Decision.** What was chosen.
- **Why.** The evidence or principle that drove it.
- **Ruled out.** What was considered and rejected, and why. The rejected options are half the value; they are what stops a re-litigation.
- **Status.** Decided, in progress, verified, reverted.

Keep it tight: a line or two per record, not prose. The trail is for reconstruction, not storytelling.

## When to commit it

- A task that spans sessions or phases. The next session reads the trail to pick up (see the **session-pickup** playbook).
- An overnight or "trust it when I'm back" run. The human audits the trail, not the transcript.
- A big refactor or migration where the diff alone does not say why.

For short work, keep it local in the session. Committing a trail for every task buries the signal.

## Rules

- Write the record at decision time, not at the end. A trail written after the fact is a summary with hindsight.
- Every claim that a thing works points at the evidence, per **principle-prove-it-works**: a log line, a test result, an in-game observation. No bare "verified".
- When the trail contradicts the working tree, the tree wins. Note the discrepancy in the trail.
- End the task with a current-state brief: what is done, what is proven, what is next.