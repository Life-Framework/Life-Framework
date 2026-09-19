# Life Framework Skills

The repo root `AGENTS.md` is the portable source of engine and repository
constraints. Skills should add routing or workflows, not repeat those facts.

## Use

Run `/life-mode` for non-trivial work. It routes to the smallest relevant skill
and verification gate. `/prove` runs the proof workflow. `/interrogate` reviews
a risky diff.

Restart opencode after adding or changing skills, agents, commands, or config.

## Layout

- `skills/life-mode/`: routing plus playbooks.
- `skills/enfusion-*`: API, script, prefab, config, and verification workflows.
- `skills/principle-*`: short decision heuristics, loaded only when relevant.
- `skills/interrogate`, `el-tdd`, `show-me-your-work`, `unslop`, and
  `parallel-worktrees`: situational workflows.
- `agent/`: delegated Life Framework and review agents.
- `command/`: slash-command entry points.

Keep factual engine rules in `AGENTS.md`. Encode recurring rules in
`tools/validation`, `tools/lint`, or `tools/test` instead of adding prose.
