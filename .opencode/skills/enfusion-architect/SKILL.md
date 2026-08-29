---
name: enfusion-architect
description: "Use when about to write Enfusion code or prefab work that crosses a boundary (component, prefab inheritance, replication, an RPC, a config consumer). Settle the caller's usage, the data shape, and the module boundary before implementing. Trigger on 'architect this', 'design this component', 'how should this wire together', 'before I write this'. Use ONLY when the work lives in this mod."
---

# Architect

Settle the boundaries before writing. Enfusion punishes guessing a shape: a component wired to the wrong entity, an RPC with the wrong receiver, a prefab that inherits a base it does not match. The data shape is the spec.

1. Name the consumer first. Who calls this, from which side (server or client), on which entity, under which prefab. If you cannot name a concrete caller, the design is a library without a user.
2. Name the data shape per **principle-foundational-thinking**: which prefab carries the component, which base it extends, which config drives it, what state must replicate, what must persist. Write it down in one paragraph.
3. Research the base game via **enfusion-api-research**. Confirm the class to extend exists, what it already provides, and which vanilla system it plugs into. Reuse before building.
4. Draw the boundaries explicitly. The replication boundary (what the server owns, what the client sees). The config boundary (what is data). The persistence boundary (what survives a restart and how). The UI boundary (client-only, server calls). Mark each.
5. Check what could break per **enfusion-blast-radius** before committing to a shape. A component name change, a base prefab change, an RPC signature change, a config key change: map the references now.
6. Write the smallest cross-boundary slice first and verify it before widening, per **principle-sequence-verifiable-units**. A thin vertical slice (one prefab + one component + one config + one test) proves the shape cheaper than a wide design.

The deliverable is the boundary drawing and the data shape, not a file list. Implementation then follows the **feature** or **bug-fix** playbook.