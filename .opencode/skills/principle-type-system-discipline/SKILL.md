---
name: principle-type-system-discipline
description: "Use ONLY when explicitly directed by the life-mode skill or another skill, or when you are designing signatures or scripted component classes in Enforce Script. Make illegal states unrepresentable, brand semantic primitives, parse external data at boundaries. Never auto-load for routine work."
---

# Type System Discipline

Make illegal states unrepresentable, brand semantic primitives, parse external data at boundaries, refuse to lie to the compiler, exhaust variants, derive from authoritative schemas.

**Why:** Enforce Script is statically typed enough to carry your intent. A bare `string` used for a job id and a bare `int` used for money invite silent mistakes: concatenating the wrong string, negating the wrong int. A typed wrapper or a class with named fields refuses those mistakes at compile time.

**Rule:**
- Brand semantic primitives. Money is not `float`, it is a value you never conflate with a count. When the engine gives you a weak type (a `ResourceName`, a raw `EntityId`), wrap the meaning at the boundary where it arrives.
- Prefer a `class` with named fields over parallel arrays of primitives. A job with `m_sName`, `m_iPay`, `m_fCooldown` reads and enforces itself.
- Exhaust variants with enum-style classes or typed switches instead of string matching that silently accepts a typo.
- Derive from authoritative schemas. When a config class mirrors a config file, keep the field names and units identical to the file.

**In this repo:**
- `EL_Component<Class T>` is a generic class; use the strong type parameter rather than casting to base types.
- Config classes (`EL_JobConfig`, `EL_VehicleSettings`) are the mod's typed boundary: write them to mirror the config file one field for one key.
- `EL_Test` subclasses override `GetName()` and `Run(EL_TestContext)`. The framework's contract is the type; a test that cannot run is a compile-time failure of the shape, not a runtime surprise.
- Localization keys are branded strings: they must resolve through the string table, so treat them as a distinct kind from user input.