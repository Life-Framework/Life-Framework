---
name: principle-foundational-thinking
description: "Use ONLY when explicitly directed by the life-mode skill or another skill, or when you are about to write Enfusion logic. Apply before writing logic: choose the component, prefab, config, and data structure first so downstream code becomes obvious. Never auto-load for routine work."
---

# Foundational Thinking

Apply before writing logic: choosing core types and data structures, sequencing scaffold-vs-feature work, asking what concurrent actors share.

**Why:** Enfusion is data-driven. A scripted component does nothing until a prefab carries it, a config feeds it, and the world instantiates it. Get the data shape right and the script writes itself. Get it wrong and the script fills with conditionals patching the shape.

**Rule:**
- Name the entity owner first: which prefab carries this behavior, which base does it extend.
- Name the config driver: what values are data (should be in a config or `EL_*Settings` class) and what is logic.
- Name the state: what must replicate, what is local, what is persisted. Ask what concurrent actors share (every player, every server, every RPC).
- Sequence scaffold before feature: prefab + config + component stub first, behavior second.

**In this repo:**
- The `EL_Component<Class T>` helper exists because finding a typed component is a repeated, central shape. Use it.
- Feature folders under `Scripts/Game/Feature/<area>/` group data shape with logic. A new feature gets its own folder with the same shape as its siblings.
- Managers (`EL_*Manager`) are the mod's chosen structure for per-server state. A manager owns one domain; two managers owning the same domain is a design smell to fix.