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
| `type` | capability, system, data, visual, tooling, docs, town | One player-visible capability or one system change; `town` is the chapter-close town-placement pass (below) |
| `tier` | T0, T1, T2 | T0 docs and tooling, T1 data (light gate), T2 code (full gate) |
| `model` | haiku, sonnet, opus | Routing table in the pipeline doc; any `risk` means opus |
| `machines` | none, test, session, session-2p, full-profile | What the pool must find free |
| `budget` | `N min / M calls` | At most 45 min and 60 calls. Bigger: split it now |
| `owns` | backticked files or globs | Disjoint from every slice that can run at once; shared append-only files excepted. World props: `Grid_<slug>.layer` and `tools/grid/cells/<slug>.json`, never a town layer (`docs/architecture/world-zones.md`) |
| `depends` | keys in a plan, `#N` on an issue | Ready once each is merged (`in-dev` or later) |
| `acceptance` | `test LF_Test_X`, `proof <verb>`, `fixture <path>` | Written first and seen red |
| `risk` | none, persistence, replication, money, inventory | Needs T2 and an independent reviewer |
| `release-blocker` | yes, no | Flags the slice and every slice it depends on in the queue |

## World props: grid, not town

A slice that adds world props puts them in its own debug-grid cell:
`tools\cli grid place slice-<N> <prefab>...` (`docs/architecture/world-zones.md`). It owns
`addons/LifeFramework/Worlds/DebugWorld/DebugWorld_Layers/Grid_slice_<N>.layer` and
`tools/grid/cells/slice-<N>.json` and never edits a town layer; `validate-world-zones` refuses a
non-`town-` branch that does. Its PR picture is `tools\cli look grid:slice-<N>`, at the grid's low
look bar.

## Town placement card

The planner adds one per chapter, depending on every other slice of it. It moves the chapter's
systems from their grid cells into town and works through the cleanup backlog rows they touch. Its
brief runs in a `town-<N>` worktree, the marker the zone guard reads.

```markdown
# Slice S9: Town placement for <chapter>
## Card
- type: town
- tier: T1
- model: sonnet
- machines: full-profile
- budget: 45 min / 60 calls
- owns: `addons/LifeFramework/Worlds/DebugWorld/DebugWorld_Layers/Street_<Area>.layer`, `agent-box/place-audit-allow.json`
- depends: S1, S2, S3
- acceptance: proof look <area> before and after; audit placement --area <area> --strict clean
- risk: none
- release-blocker: no

## Capability
The chapter's systems stand in town where they belong, to the polish bar: furniture inside its
building, NPCs dressed for their job and inside, boards on walls.

## Acceptance
`tools\cli look <area>` before and after for each area, the polish checklist ticked against the
pictures in the PR, and `tools\cli audit placement --area <area> --strict` clean.

## Notes
Systems and their grid cells: <slug> (cell cXrY), ... Backlog rows: `docs/architecture/world-zones.md` #<n>.
```

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
