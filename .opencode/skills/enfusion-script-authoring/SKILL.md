---
name: enfusion-script-authoring
description: "Use when editing Enforce Script under addons/LifeFramework/Scripts. Covers EL_ naming, component structure, replication/RPC, persistence, and test wiring without duplicating AGENTS.md."
---

# Enforce Script

Read `AGENTS.md` for language, memory, replication, RPC, persistence, and test
constraints. This skill only adds the authoring workflow:

- Use `EL_` for new classes. Put shared code in `Core`, feature code in
  `Feature/<area>`, UI in `UI`, and tests in `Tests`.
- Before coding, identify the owning component/manager, config source, and
  server/client/persisted state. Keep one class focused on one concern.
- Research unfamiliar base classes and methods with `api_search` or
  `game_read`; do not guess signatures.
- For components, ensure the prefab carries the component and any required
  `XxxClass` editor pairing. Null-check world lookups at the boundary.
- For RPC/replication/persistence, validate server authority, JIP/restart
  behavior, and serializer/config registration.
- For pure logic, add an `EL_Test` with `// tier:` and `// red-proof:`; run
  `tools\cli regen-tests` rather than editing generated registration.
- Match local `//---`/`//!` style, avoid narrating comments, then run
  `tools\cli validate`.
