---
name: interrogate
description: "Use when you have a diff or a design on this mod and want it broken before shipping. Adversarial review: spawn the el-reviewer subagent plus a fresh skeptical pass, try to break the change, and fix what survives. Trigger on 'interrogate this', 'review this diff', 'try to break it', 'is this design sound', 'before I commit'. Use ONLY for changes to this mod."
---

# Interrogate

A diff or design gets broken before it ships. The goal is to find the real defect, not to approve the work.

1. Load the diff or the design. Read it as the adversary would: what is the change claiming, and where could it be wrong.
2. Spawn the `el-reviewer` subagent (read-only, edit denied) with the diff and a strict lens. Ask it to find defects, not nitpicks: broken replication, a missing boundary guard, a prefab reference that cannot resolve, a save that cannot load, an input that collides, a GUID that is not unique.
3. Run your own adversarial pass against the same diff, on a different angle. A second opinion is the same prompt against a different model; agreement is high-signal, disagreement is worth resolving.
4. Classify each finding. Real defect (fix it), noise (dismiss with a concrete reason, not churn). A reviewer that files only nitpicks has not tried hard enough.
5. Fix the defects the review surfaces, then re-verify through **enfusion-verify**. A change that survived interrogation is a change you can ship.
6. When the change is contentious (a design, not just a diff), state the contested decision and the mechanism that makes the safe claim true, per **enfusion-blast-radius**.

The reply names the defects found, the ones dismissed and why, and the fixes made.