---
name: principle-boundary-discipline
description: "Use when wiring replication, RPCs, config parsing, persistence, or world interactions. Validate untrusted data at the boundary and keep internal logic typed."
---
# Boundary Discipline
Validate config, RPC payloads, replicated state, resource loads, and entity/component lookups once where they enter the system. Keep business logic typed and free of repeated boundary guards. Server-authorize client requests and validate save data before applying it.
