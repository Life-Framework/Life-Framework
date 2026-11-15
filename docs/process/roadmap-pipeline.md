# Roadmap pipeline

How a chapter or epic becomes merged code and gameplay with about 25 agents at
once. The maintainer approved this method; the tool is `tools\cli slice`, the
card format is `docs/process/slice-card.md`, and `AGENTS.md` still wins on any
rule this page repeats.

## 1. Plan: one planner cuts slices

A chapter or epic goes to one **planner** (Opus, `lf-hard`, one run). It reads
the chapter's beat sheet and child issues and writes a plan file of slices
(`tools\cli slice new <parent>` writes the template to `tmp/slices-<parent>.md`).
Each slice is:

- one player-visible capability or one system change;
- 30 to 45 agent-minutes and a small diff;
- file-disjoint from every slice that may run at the same time: it declares the
  files or globs it owns;
- given an acceptance check before any code: a test name, a proof verb or a
  config fixture;
- placed in the DAG (its dependencies), with a gate tier, a model and its
  machine needs.

`slice new <parent> --plan <file> --dry-run` checks the plan and writes the
cards to `tmp/slices-<parent>.cards.md`; without `--dry-run` it creates each
card as a GitHub sub-issue of the parent (label `slice`), in dependency order.
The check refuses a slice over the timebox, a slice with no acceptance check,
risky work below T2 or off Opus, a dependency cycle, and two slices that can
run at once but own the same file. Shared append-only files (`features.md`,
string tables, triage, the test registry) have merge drivers and do not count.

## 2. Fan out

The lead dispatches every ready slice up to account headroom (`tools\cli usage
--pick`, in progress on another branch) and lets the test pool spread tests
and sessions. `tools\cli slice list <parent> --ready` is the dispatch queue:

- **ready** means open, not `agent:working`, not `hold`, and every dependency merged
  (`in-dev`, `needs-playtest`, `verified` or closed: merging is enough, the
  playtest is not waited for);
- the queue is sorted by **critical path**: the longest chain of unmerged
  minutes from the slice to the end of the chapter goes first, so the slices
  that hold up the most work start first. `BLOCKER` marks a release blocker
  and `feeds-BLOCKER` a slice one depends on; on a tie they go first;
- the footer prints `ready N, running M` and asks for more planning when
  ready falls below twice the running count. Keep the planner ahead.

The lead starts each agent with `tools\cli slice brief <issue>` as the whole
brief, on the agent the card's model names.

## 3. The per-slice loop

Red test, implement, targeted tests, tiered gate, ship queue. The brief spells
it out. The agent stays in its lane: it owns only its files, does not
re-verify other slices, and starts no helpers.

**Timebox.** Every card has a hard budget: at most 45 minutes and 60 tool
calls. At the limit the agent ships what passes, writes
`tmp/handoff-<slug>.md`, and reports `SPLIT`. The lead (or the planner) turns
the rest into new cards. A slice that runs over is split, never extended.

## 4. Integration

Ships go through the ship queue; `prs heal` and the merge drivers (sorted
string tables, append-only docs) settle the rest. When every slice of a chapter
is merged, **one** integration agent runs the chapter's beat sheet end to end
on `main`. Each failed beat becomes a new slice card under the chapter, not a
fix inside the integration run.

## 5. Risk-weighted review

| Tier | What | Gate |
|---|---|---|
| T0 | Docs, tooling with a selftest | validate (`wt ship` runs only validate when `addons/` is untouched) |
| T1 | Data: configs, strings, prefab values | light gate: validate plus the targeted test |
| T2 | Code | full gate (`wt ship`: validate --full, build, suite) |
| T2 + risk | Persistence, replication, money, inventory | full gate, and an independent reviewer (`lf-hard`, **interrogate**) reads the diff before merge |

Today `wt ship` still runs the full gate for any `addons/` change, T1
included; a T1 ship mode is a follow-up. Visible work gets one `look` picture
in the PR. Polish nitpicks become
follow-up issues; they never block the merge.

## 6. The human loop

One batched playtest per chapter: `tools\cli playtest checklist` collects the
merged slices' manual steps into one walk. Each finding becomes a slice card
the same day (`slice new <chapter> --plan`).

## 7. Model routing and metrics

| Model | Agent | Takes |
|---|---|---|
| Haiku | `lf-scout` | checks, docs, reconciliation, lookups |
| Sonnet | `lf-builder` | standard slices: capability, system, data, visual, tooling |
| Opus | `lf-hard` | the planner, persistence, replication, the independent reviewer |

The plan check routes any card with a `risk` to Opus. Every finished slice
appends one ledger row:

```
tools\cli slice record <issue> --model sonnet --type capability --outcome merged|handoff|split|failed --gate pass|fail --minutes 38 [--wait 4]
```

`--gate` is the first gate run, `--minutes` the slice's lead time from start
to PR, `--wait` the minutes queued for the pool. The ledger is one JSONL file
in the git common dir (`.git/lf/slice-ledger.jsonl`), so every worktree writes
to the same file. `tools\cli slice stats` reports, overall and per model and
per slice type:

- **lead time**: median minutes;
- **first-pass gate rate**: share of slices whose first gate passed;
- **rework rate**: share that ended in handoff, split or failure;
- **queue wait**: median minutes waiting for the pool.

Tune the routing table from these numbers: a type whose Sonnet first-pass rate
is low moves up, a type Haiku passes cleanly moves down.

## Worked example: #1005 "Make it yours"

The M3 chapter's beat sheet: the same town with a test pack, Police reads
Sheriff in two languages, robbery off, the clerk speaks the pack's lines, the
UI theme changes, the same core on another map. The planner's output is
`tools/validation/fixtures/slice/plan-1005.md` (the selftest uses it):

| Key | Slice | Type, tier, model | Depends | Acceptance |
|---|---|---|---|---|
| S1 | Robbery switches off from config | system, T2, Sonnet | | fixture `robbery-off` |
| S2 | Test pack renames Police to Sheriff (EN, DE) | data, T1, Sonnet | | `LF_Test_TermsPackSheriff` |
| S3 | The clerk speaks the pack's lines | capability, T2, Sonnet | S2 | `LF_Test_DialoguePackPatch` |
| S4 | Packs override the UI theme | visual, T2, Sonnet | | proof `theme-swap` |
| S5 | Saves keep renamed content (alias table) | system, T2, Opus, persistence, blocker | S2 | `LF_Test_ContentAliasLoad` |
| S6 | The same core boots on another map | capability, T2, Sonnet, blocker | S1, S3, S5 | proof `boot-other-map` |

Each slice owns its own files, so S1, S2 and S4 start at once. With S2 merged
and S1 running, `slice list 1005 --ready` gives S5 (90 minutes of chain and a
blocker), then S3 (90 minutes, feeds the S6 blocker), then S4 (45). S5's brief
names `lf-hard` and the independent reviewer. When S6 merges, one integration
agent films the beat sheet on `main`; then the maintainer plays the chapter
from one `playtest checklist`.
