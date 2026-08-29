# Refactoring

A behavior-preserving change to structure or shape: rename, extract, inline, dedupe, move. The game must behave identically before and after.

1. Establish the baseline behavior. What is the observable contract this refactor must preserve? Name it. For scripts it is the runtime behavior in the DebugWorld. For prefabs it is the component set and resource references. For configs it is the entries the game reads.
2. Measure blast radius first per **enfusion-blast-radius**. A rename touches every reference. A move touches every `{GUID}path` reference and every resource database entry. An inheritance change touches every child prefab. Map it before editing.
3. Prefer the tool over the hand per **principle-build-the-lever**. The Workbench MCP offers `refactor-rename`, `refactor-replace-guid`, `refactor-move-resource`, and `refactor-remove-unused`. Use them. If no tool fits, use `rg` over the repo and Workbench resource paths. Never hand-edit a GUID across a tree.
4. Keep each unit small and verifiable. One rename, one move, one extraction per commit. Verify after each unit per **principle-sequence-verifiable-units**. A failed refactor bisects cleanly when the units are small.
5. Enforce behavior preservation. Run `tools\cli validate`. Boot the DebugWorld and confirm no new script or resource errors in the logs. Where the refactor touched logic, run the affected `EL_Test` suite and the smoke test (`tools\cli test`).
6. Migrate callers and delete the legacy shape in the same wave per **principle-migrate-callers-then-delete-legacy-apis** (as encoded here): do not leave a compatibility shim behind. If the old name must survive for save data or saved-world references, make the migration explicit and delete the alias once no live reference remains.
7. Watch the diff. If the refactor reads as more than it removed, stop and reconsider per **principle-laziness-protocol**. Refactoring is paid for in simplicity, not in churn.

**Reply:** the contract preserved, the blast radius you mapped, the tooling used, and the in-game verification that behavior is identical.