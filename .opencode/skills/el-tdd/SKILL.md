---
name: el-tdd
description: "Use when fixing a bug or adding logic to this mod and there is a cheap in-game test path through the EL_Test framework. Write the failing test first, then the fix. Trigger on 'write a test', 'test first', 'add an EL_Test', 'make a failing test', 'register a test'. Use ONLY when the test targets this mod's Scripts/Game/Tests framework."
---

# Test first (EL_Test)

The EL_Test framework runs in the DebugWorld and reports through `[ELTEST]` markers and a JUnit XML. A failing test landing before the fix is the canonical **principle-sequence-verifiable-units**: the diff tells the story, and the fix is proven twice.

## The framework

- A test extends `EL_Test` and implements `GetName()` and `Run(EL_TestContext ctx)`. Call `ctx.Fail("reason")` on failure; the context counts failures and collects messages.
- Declare the tier with a `// tier: LOGIC|WORLD|PERSISTENCE` comment above the class, carry a `// red-proof:` comment, then run `tools\cli regen-tests`. The registry (`EL_TestRegistrations.generated.c`) is derived from the test files, so tests register automatically; an unregistered test never runs. Never hand-edit `EL_TestManager.c` or the generated registry.
- The suite executes on server start in the DebugWorld via `EL_TestRunnerComponent` (Tests.layer). Results: `[ELTEST] PASS|FAIL <name>`, a `[ELTEST] SUMMARY`, and a JUnit XML under `$profile:TestResults`.
- `tools\cli test` boots the DebugWorld server, runs the suite, and exits nonzero on any failure. That exit code is the agent's pass/fail signal.

## The cadence

1. **Write the failing test first.** Express the bug or the expected logic behavior as an assertion. Run it; watch it fail for the right reason. A test that passes before the fix proves nothing.
2. **Prove the test can fail, once.** Perturb what it covers (a real input, not the comparison), run, observe failure, then revert the perturbation. A case that cannot go red is a defect, not a test.
3. **Fix the code.** Smallest change per **principle-laziness-protocol**. Re-run; the test now passes.
4. **Leave the test registered.** Run `tools\cli regen-tests` so the registry includes it; the regression is now a permanent gate.

## Discipline

- **Determinism beats breadth.** Three consecutive runs must be identical: same pass count, same names. A flaky test is removed or fixed before the work is done.
- **No retry logic.** A test that needs retries is a bug in the test, a race, or a real defect. Fix the cause.
- **Prefer pure logic tests.** The cheapest test is one with no world at all: calculations, formatting, state transitions, config-driven math. The EL_ framework tests (`EL_Test_MathStringSanity`, `EL_Test_ContextSelfTest`) are the pattern. Only test what needs a world (`EL_Test_WorldLoaded`, `EL_Test_MoneyStackPrefab`) when the assertion genuinely needs one.
- **Never gate a test behind an editor define.** A test that only exists in Workbench builds does not exist where the server runs.
- **Hand-built objects start zeroed.** A test that constructs a config or data object by hand does not get declared attribute defaults. Set every field the assertion depends on explicitly.
- **Keep the assertion string as the doc.** `ctx.Fail("money does not round-trip a save/load")` tells the reader what broke. No narrating comment above it.

## Where tests live

`Scripts/Game/Tests/EL_Test_<Area>.c` with a `// tier:` comment and a `// red-proof:` comment; registration is automatic via `tools\cli regen-tests`. See the existing `EL_EngineSmokeTests.c` and the test framework files for the shape. A bug fix that touches pure logic earns a test; a fix that is pure prefab wiring usually does not, and its proof is the in-game pass from **enfusion-verify**.