---
name: el-tdd
description: "Use for a Life Framework bug or logic change with a cheap LF_Test path. Write a red test, make the smallest fix, and leave the regression registered."
---

# EL Test First

- Create `addons/LifeFramework/Scripts/Game/Tests/LF_Test_<Area>.c` extending
  `LF_Test` with `GetName()` and `Run(ctx)`. Add a `// tier: LOGIC|WORLD|PERSISTENCE`
  comment above the class and a `// red-proof:` comment.
- Express the defect as an assertion and run it red before the fix. Prove the
  assertion can fail once, then revert the perturbation and record the observed
  failure line in `// red-proof:`.
- Fix the smallest cause, run the test green, and run `tools\cli regen-tests`.
  On a registry merge conflict, regenerate; never hand-merge it.
- Never edit `LF_TestManager.c` or `LF_TestRegistrations.generated.c`. Prefer
  deterministic LOGIC tests; use WORLD/PERSISTENCE only when the world or save
  path matters. Persistence tests go through public mutators and accessors.
- In a worktree, loop on `tools\cli test --no-build --only <LF_Test_Class>`
  (one test, any tier, ~10 s) for the red and green runs. Then run
  `--tier fast` for logic and `--tier all` once for world/runtime coverage. A prefab-only fix usually needs
  runtime proof, not a new test.
