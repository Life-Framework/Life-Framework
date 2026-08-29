---
description: Adversarially review the current diff before shipping. Applies the interrogate skill: spawn the read-only el-reviewer subagent, run your own skeptical pass, fix the defects, dismiss the noise with reasons.
---

Apply the `interrogate` skill to the current diff of this repo. Load the diff, spawn the `el-reviewer` subagent against it, run your own adversarial pass on a different angle, classify each finding as a real defect or dismissible noise, and fix the defects. Then verify through the `enfusion-verify` skill.

$ARGUMENTS