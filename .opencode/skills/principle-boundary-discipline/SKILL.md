---
name: principle-boundary-discipline
description: "Use ONLY when explicitly directed by the life-mode skill or another skill, or when you are wiring replication, RPCs, config parsing, or world interactions in Enfusion. Concentrate guards at system boundaries; trust internal types. Never auto-load for routine work."
---

# Boundary Discipline

Concentrate guards at system boundaries (network, config, resource load); trust internal types and keep business logic pure.

**Why:** Enfusion is a networked engine. Data crosses boundaries constantly: from config files, over RPCs, through replicated variables, from the world into components. A nil-check or a cast belongs where the untrusted data arrives, not at every use site.

**Rule:**
- Parse and validate external data once, at the boundary. A config value read from disk is untrusted at the read site; after it becomes a typed field, treat it as trusted.
- RPC handlers and replicated-variable setters are boundaries. Guard them there: the sender may be a client, the payload may be forged or stale.
- `IEntity` lookups and `FindComponent` results are world boundaries. A null check at the boundary beats a null check in every consumer.
- Keep business logic pure: give it typed inputs, get a typed result, no boundary noise inside.

**In this repo:**
- `EL_Component<Class T>.Find()` returns null for a missing component. Check it where you cross from the world into the component, once, then use the strong-typed reference.
- Persistence load (`EL_*Persistence` files) is a boundary: player save data is untrusted disk input. Validate at load, then trust the in-memory model.
- RPCs and replicated money/state values are boundaries for a Life economy. A client-authoritative money change is a cheat vector; guard the server side.
- Config classes like `EL_JobConfig` are typed boundaries: keep the raw config read separate from the logic that consumes the typed result.