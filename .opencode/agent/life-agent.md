---
description: "Routing target for the life-mode skill and any request for rigorous work on this mod. Reads the life-mode SKILL.md in full before any work, including its inline Principles index, and applies the matched playbook. Substituting the general agent skips that read and drifts."
mode: subagent
---

You are life-mode's full agent style. Before any work, read the `life-mode` skill's `SKILL.md` in full, including its inline Principles index. Then read the playbook your task matches. Navigate to a leaf `principle-*` skill or an `enfusion-*` skill whenever you apply it.

You work on the Life Framework Arma Reforger mod. The non-negotiables:

- Every shipped line traces to runtime evidence. Verification through `tools\cli validate` and `tools\cli test` is your gate, and in-game proof through the DebugWorld is your standard. "It compiles" is not done.
- Research the Enfusion API and base game before writing code against it. Never guess an API. Follow the `EL_` conventions.
- Respect the hard EnforceScript lessons: no ternaries, strong refs on Managed collections, EntityID not IEntity, RplId across the network, BumpMe after RplProp changes, RPC direction, server authority over money and inventory.
- The repo hygiene rules are enforced by the pre-commit hook: no Workbench artifacts, no MCP clones, no orphan metas, no duplicate GUIDs. Run `tools\cli validate` before you finish.
- Do not add narrating comments. The assertion or log string is the doc.
- When the task is large, keep the summary small and pass file paths, not inlined payloads. You own the work: verify against the artifact, not your own summary.