---
name: el-tdd
description: "Use for a Life Framework bug or logic change with a cheap EL_Test path. Write a red test, make the smallest fix, and leave the regression registered."
---

# EL Test First

- Create `Scripts/Game/Tests/EL_Test_<Area>.c` extending `EL_Test` with
  `GetName()` and `Run(ctx)`. Add `// tier:` and `// red-proof:`.
- Express the defect as an assertion and run it red before the fix. Prove the
  assertion can fail once, then revert the perturbation.
- Fix the smallest cause, run the test green, and run `tools\cli regen-tests`.
- Never edit generated registration or test managers. Prefer deterministic pure
  logic tests; use WORLD/PERSISTENCE only when the world or save path matters.
- Use `tools\cli test --tier fast` for logic and the full suite for world/runtime
  coverage. A prefab-only fix usually needs runtime proof, not a new test.
