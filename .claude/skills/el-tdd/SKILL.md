---
name: el-tdd
description: "Use for a Life Framework bug or logic change with a cheap LF_Test path. Write a red test, make the smallest fix, and leave the regression registered."
---

# EL Test First

- Create `addons/LifeFramework/Scripts/Game/Tests/LF_Test_<Area>.c` extending
  `LF_Test` with `GetName()` and `Run(ctx)`. Add a `// tier: LOGIC|WORLD|PERSISTENCE`
  comment above the class and a `// red-proof:` comment.
- Express the defect as an assertion and run it red before the fix. For a bug
  fix that run, red on the unfixed code and then green on the fixed code, IS the
  red proof: record the observed failure line in `// red-proof:`, no extra
  break-and-restore run.
- A new test with no failing state to start from: put the hook in `Run()`
  (`if (!RedProof()) <the act>;`, skipping what the assertions guard) and prove
  red in one batched run for every new test of the change,
  `tools\cli test --no-build --red LF_Test_A,LF_Test_B` (passes only if all fail,
  names any hollow one). Record the `RED-OK` line. Node selftests keep their own
  instant red proofs.
- Fix the smallest cause, run the test green, and run `tools\cli regen-tests`.
  On a registry merge conflict, regenerate; never hand-merge it.
- Never edit `LF_TestManager.c` or `LF_TestRegistrations.generated.c`. Prefer
  deterministic LOGIC tests; use WORLD/PERSISTENCE only when the world or save
  path matters. Persistence tests go through public mutators and accessors.
- In a worktree, loop on `tools\cli test --no-build --only <LF_Test_Class>`
  (one test, any tier, ~10 s) for the red and green runs. Then run
  `--tier fast` for logic and `--tier all` once for world/runtime coverage. A prefab-only fix usually needs
  runtime proof, not a new test.
