---
name: enfusion-script-authoring
description: "Use when editing Enforce Script under addons/LifeFramework/Scripts. Covers LF_ naming, component structure, replication/RPC, persistence, logging, and test wiring without duplicating AGENTS.md."
---

# Enforce Script

Read `AGENTS.md` for the language, memory, replication, RPC, persistence, and
test constraints. This skill adds the authoring workflow:

- Every class is `LF_`, tests are `LF_Test_*`; the real EveronLife mod (`EL_`) can
  load on the same server and collide, so never add an `EL_` name
  (`validate-no-el-prefix.mjs` fails it). Shared infrastructure is
  `LF_Debug`, `LF_Utils`, `LF_Component`.
- Put shared code in `Core`, feature code in `Feature/<Area>` (with its
  `Feature.md`), UI in `UI`, and tests in `Tests`.
- Before coding, identify the owning component or manager, the config source,
  and which state is server-only, replicated, or persisted. One class, one
  concern.
- Research unfamiliar base classes and methods with `tools\cli call
  api_search` or `game_read`; do not guess signatures. Reading a sibling
  feature for structure is encouraged.
- A script component referenced by prefabs needs the `XxxClass : YyyClass`
  pairing where its base declares one, and the prefab must carry it.
  Null-check world lookups at the boundary.
- Log player-visible state transitions and rejections through
  `LF_Debug.Info/Warn/Error("<Feature>", ...)`; a broken input logs
  `LF_Debug.Error` and degrades only that feature.
- For RPC, replication, or persistence, validate server authority, JIP and
  restart behavior, and serializer registration.
- For logic, add an `LF_Test` with `// tier:`, `// red-proof:` and the `RedProof()` hook (AGENTS.md "Tests") and run
  `tools\cli regen-tests`; never edit the generated registration.
- Match the local `//---` / `//!` comment style, skip narrating comments, then
  run `tools\cli validate`.
