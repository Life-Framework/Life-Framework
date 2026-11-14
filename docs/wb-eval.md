# wb_eval: run a snippet of Enforce Script in the live Workbench

`wb_eval` compiles a few lines of Enforce Script inside the running Workbench
and returns the string they return. It is a local patch to the Workbench MCP
(`tools/mcp-patches/enfusion-workbench-mcp/`: the handler
`mod/.../EMCP_WB_Eval.c` and the tool `src/tools/wb-eval.ts`). It never ships
in `addons/`. `tools\cli mcp update enfusion-workbench` reapplies it.

```
tools\cli call wb_eval '{"code":"return api.GetEditorEntityCount().ToString();"}'
```

The snippet is the body of a `string` function. `we` (WorldEditor), `api`
(WorldEditorAPI, null in play mode or in a Resource Manager-only Workbench)
and `rm` (ResourceManager) are ready. If the snippet has no `return`, the result is `""`.

## When to use it

- One-off, read-only questions the existing `wb_*` tools do not answer:
  counting or listing editor entities by some property, reading a
  WorldEditorAPI or ResourceDatabase value, checking that an API behaves the way you expect.
- Probing an API before you write a real handler.

## When not to

- Anything you will run twice, or that another agent needs. That is a
  committed tool: a handler in `tools/mcp-patches/` or a script in `tools/`.
- World edits, if you can avoid them. If you must edit, wrap every
  WorldEditorAPI mutation in `api.BeginEntityAction("<label>");` ...
  `api.EndEntityAction();` so it lands in undo history. Never edit outside that pair.
- Game or mod logic. That belongs in `addons/` with an `LF_Test`.

## Failure modes (measured 2026-11-14)

| What | Result |
|---|---|
| Syntax error (missing `;`, ternary `?:`) | `compile_error` with the snippet line and the compiler text (`Broken expression (missing ';'?)`). The session stays healthy: the next call works |
| VME at run time (null dereference, bad cast) | **Workbench wedges.** The call times out and so does every later NET API call, until Workbench is restarted. Guard every pointer (`if (!api) return "no api";`) |
| Code over 32768 characters | Refused before compiling |
| Memory | Each **new** snippet keeps about 11 MB that the engine never frees, whether the module is deleted or not (50 unique evals added 550 MB; 50 `wb_state` calls added 0). An identical snippet reuses its cached module (50 repeats added 1 MB). After 100 fresh compiles the tool refuses until Workbench restarts |

Keep snippets few and stable: rerunning the same text is free, while every
variation costs another compile.
