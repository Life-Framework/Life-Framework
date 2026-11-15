# Slice card

One slice card is one agent job of 30 to 45 minutes
(`docs/process/roadmap-pipeline.md`). A planner writes cards into a plan file;
`tools\cli slice new <parent> --plan <file>` checks them and creates each as a
sub-issue of the chapter or epic. The parser reads the `## Card` bullets, so
keep the field names.

## Plan file

One `# Slice <KEY>: <title>` heading per slice. `depends` names other keys in
the same plan; they become issue numbers on creation.

```markdown
# Slice S3: The clerk speaks the pack's lines
## Card
- type: capability
- tier: T2
- model: sonnet
- machines: test
- budget: 45 min / 60 calls
- owns: `addons/LifeFramework/Scripts/Game/Feature/Dialogue/LF_DialoguePatch*.c`
- depends: S2
- acceptance: test LF_Test_DialoguePackPatch
- risk: none
- release-blocker: no

## Capability
A pack patches the clerk's dialogue by line id and the clerk says the new line.

## Acceptance
LF_Test_DialoguePackPatch loads the test pack, asks the clerk for line
`clerk.greet` and expects the pack's text. Red before the patch code exists.

## Notes
Optional: pointers, `path:line` places, decisions already made.
```

## Fields

| Field | Values | Rule |
|---|---|---|
| `type` | capability, system, data, visual, tooling, docs | One player-visible capability or one system change |
| `tier` | T0, T1, T2 | T0 docs and tooling, T1 data (light gate), T2 code (full gate) |
| `model` | haiku, sonnet, opus | Routing table in the pipeline doc; any `risk` means opus |
| `machines` | none, test, session, session-2p, full-profile | What the pool must find free |
| `budget` | `N min / M calls` | At most 45 min and 60 calls. Bigger: split it now |
| `owns` | backticked files or globs | Disjoint from every slice that can run at once; shared append-only files excepted |
| `depends` | keys in a plan, or `#N` for a sub-issue the parent already has | Ready once each is merged (`in-dev` or later). `slice new` checks a plan against the parent's live sub-issues, owns included |
| `acceptance` | `test LF_Test_X`, `proof <verb>`, `fixture <path>` | Written first and seen red |
| `risk` | none, persistence, replication, money, inventory | Needs T2 and an independent reviewer |
| `release-blocker` | yes, no | Flags the slice and every slice it depends on in the queue |

GitHub caps a parent at 100 sub-issues. A chapter past the cap files new cards under one of its epics with
`slice new <chapter> --plan <file> --attach <epic>`; the card still names the chapter as its parent, and `slice list
<chapter>` reads cards one level down.

A card labelled `hold` (a maintainer decision pending, such as filming before a sign-off) is never ready.

## Agent brief

`tools\cli slice brief <issue>` prints the ready-to-paste brief. Its shape:

```text
Slice #<N>: <title>  (agent lf-builder, model sonnet, tier T2, parent #<chapter>)

Goal: <capability>
Acceptance, written first: <acceptance>
Owns only: <globs>. Shared append-only files (features.md, string tables, triage) are fine. Anything else: stop and report.
Timebox: 45 min / 60 calls, hard. At the limit ship what passes, write tmp/handoff-slice-<N>.md, and say SPLIT with the rest as new cards; never extend.
Machines: test.

Loop:
1. tools\cli wt new slice-<N> --wait; gh issue edit <N> --add-label agent:working.
2. Red: write the acceptance check, run it, see it fail.
3. Implement the smallest change inside owns; run the targeted test until green.
4. Gate T2: full gate (wt ship: validate --full, build, suite). [Risk: an independent reviewer reads the diff before merge.]
5. Ship: tools\cli wt ship slice-<N> --refs <N>. Visible change: one look picture; polish nitpicks become follow-up issues.
6. Record: tools\cli slice record <N> --model sonnet --outcome merged|handoff|split|failed --gate pass|fail --minutes N.
Stay in your lane: do not re-verify other slices, start no helper agents, no wait loops. Report in five lines; say merged, in-dev.
```
