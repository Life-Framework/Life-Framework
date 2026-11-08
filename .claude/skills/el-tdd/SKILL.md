---
name: el-tdd
description: "Use for a Life Framework bug or logic change with a cheap EL_Test path. Write a red test, make the smallest fix, and leave the regression registered."
---

# EL Test First

- Create `addons/LifeFramework/Scripts/Game/Tests/EL_Test_<Area>.c` extending
  `EL_Test` with `GetName()` and `Run(ctx)`. Add a `// tier: LOGIC|WORLD|PERSISTENCE`
  comment above the class and a `// red-proof:` comment.
- Express the defect as an assertion and run it red before the fix. Prove the
  assertion can fail once, then revert the perturbation and record the observed
  failure line in `// red-proof:`.
- Fix the smallest cause, run the test green, and run `tools\cli regen-tests`.
  On a registry merge conflict, regenerate; never hand-merge it.
- Never edit `EL_TestManager.c` or `EL_TestRegistrations.generated.c`. Prefer
  deterministic LOGIC tests; use WORLD/PERSISTENCE only when the world or save
  path matters. Persistence tests go through public mutators and accessors.
- In a worktree, use `tools\cli test --no-build --tier fast` for logic and
  `--tier all` for world/runtime coverage. A prefab-only fix usually needs
  runtime proof, not a new test.
