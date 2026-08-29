---
name: principle-model-the-domain
description: "Use ONLY when explicitly directed by the life-mode skill or another skill, or when you are writing stateful Enfusion logic or code that branches a lot. Encode the domain in a structure instead of scattered conditionals. Never auto-load for routine work."
---

# Model the Domain

Encode the domain in a structure instead of scattered conditionals.

**Why:** A Life mod is a long list of states: a player is alive, dead, in jail, on duty, wanted, broke, banned. When each state lives as an `if` in a different method, the behavior is scattered and every change touches five files. When the state lives in a structure, the code reads like the domain.

**Rule:**
- A state machine, an enum-style class, a typed registry, a config table, or a per-concern component beats repeated conditionals.
- Put behavior where the state is. An item's consume behavior belongs on the item or its effect component, not in a switch inside a manager.
- The right collection beats a search loop. `EL_TestManager` uses a registered array; a lookup by key deserves a map or a config-driven table, not a linear scan.
- If the same shape assumption (a set of fields, a set of valid values) repeats across files, that is a missing structure, not a coincidence.

**In this repo:**
- `EL_Component<Class T>` is the mod's model for "find a component of a type". Reuse it; do not write your own cast-and-check.
- Managers own one domain each (`EL_ATMManager`, `EL_JobManager`, `EL_WhitelistManager`). New state belongs in the manager that owns its domain, or a new manager, never in a grab-bag.
- `EL_JobConfig` and `EL_VehicleSettings` are the config-table pattern this mod already uses. Encode lists as data there, not as logic.
- Localization keys are domain data. User-facing strings belong in the string table, not inlined in script.