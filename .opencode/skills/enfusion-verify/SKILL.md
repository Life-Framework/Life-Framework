---
name: enfusion-verify
description: "Use to plan or perform proof for Life Framework changes: validation, compile, tests, runtime behavior, dedicated server, persistence, networking, UI, or late join."
---

# Verify

Use the smallest gate that can prove the changed behavior. `AGENTS.md` defines
the full ladder and proof classes.

1. Inspect the diff and run `tools\cli validate` for every code/resource change.
2. Run the relevant `EL_Test` tier. Use `tools\cli test --tier fast` for logic;
   use the full `tools\cli test` after a phase or for world/runtime changes.
3. Exercise the actual feature path in DebugWorld or the dedicated server.
4. Read the specific log/test result. Error counts alone are not evidence.
5. For persistence, networking, or JIP changes, include restart, dedicated-server,
   or late-join proof as applicable.

If automation cannot cover the path, give exact manual actions and pass criteria.
Do not claim runtime proof from compilation alone.
