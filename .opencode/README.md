# opencode layer

The repo root `AGENTS.md` is the contract. Skills live in `.claude/skills/`,
which both Claude Code and opencode discover (opencode searches `.claude/skills`
as a project compatibility path). This folder holds only opencode-specific
entry points:

- `command/`: `/life-mode`, `/prove` (the `enfusion-verify` skill), and
  `/interrogate`.
- `agent/`: `life-agent` (routing target) and `el-reviewer` (read-only
  adversarial reviewer used by the `interrogate` skill).

Restart opencode after changing skills, agents, commands, or `opencode.json`.
