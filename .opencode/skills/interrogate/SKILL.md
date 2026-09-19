---
name: interrogate
description: "Use before shipping a risky or contested Life Framework diff. Find real failures in replication, boundaries, resources, persistence, input, layouts, and GUIDs, then fix and verify them."
---

# Interrogate

1. Read the diff and state the behavior it claims to change.
2. Spawn `el-reviewer` with edit denied and ask for defects, not style notes.
3. Run one independent adversarial pass focused on a different failure mode.
4. Fix real findings; record why dismissed findings are noise.
5. Re-run the relevant verification gate before shipping.

Do not invoke this for routine, low-risk edits. The final report lists findings,
dismissals, fixes, and evidence.
