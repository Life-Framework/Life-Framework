# mcp-patches/enfusion-workbench-mcp — local `texture_recolor` tool

This directory is the **tracked source of the `texture_recolor` MCP tool**. The
tool itself lives in the git-ignored clone at `tools/mcp/enfusion-workbench-mcp/`
(AGENTS.md rule #1: never commit MCP clones). `tools\cli mcp install/update`
clones/pulls the upstream repo, which would wipe the local tool — so the CLI
re-applies this patch bundle after every clone/update (`syncLocalPatches`).

## What it adds

A `texture_recolor` MCP tool that decodes `.edds` BC7 pixels with the engine's
own `Compressonator_MD_DLL.dll` (via `koffi` FFI), applies color ops (hue /
saturation / lightness / tint / whiten), re-encodes, and writes a new `.edds` +
`.meta` with a fresh GUID into the addon. See the tool's own doc header in
`src/tools/texture-recolor.ts` for the full pipeline.

## Files

Copied verbatim into `tools/mcp/enfusion-workbench-mcp/`:

- `src/textures/edds.ts` — parse/rebuild the `.edds` DDS+DX10+COPY container
- `src/textures/compressonator.ts` — FFI wrapper (koffi) for BC7 decode/encode
- `src/textures/recolor.ts` — pure HSL/tint/whiten pixel math (unit-tested)
- `src/textures/__tests__/edds.test.ts` — container parse/rebuild tests
- `src/textures/__tests__/recolor.test.ts` — color-op tests
- `src/tools/texture-recolor.ts` — the MCP tool registration

## Wiring also applied

- **`src/server.ts`** — add the import near the other tool imports and register
  the tool (see the `registerTextureRecolor` call site):
  ```js
  import { registerTextureRecolor } from "./tools/texture-recolor.js";
  // ...
  registerTextureRecolor(server, config, wbClient);
  ```
- **`package.json`** — add the runtime dependency:
  ```json
  "koffi": "^3.1.6",
  ```
  (then `npm install` — the CLI's `npm ci` will pick it up from the lockfile)

## Re-apply

`tools\cli mcp install enfusion-workbench` / `tools\cli mcp update enfusion-workbench`
copies every file under `src/` here back into the clone and runs `npm ci && npm run build`.
If the upstream clone ever renames a file this bundle touches, the copy still
overwrites it (the tool is ours, not upstream's); fix any `server.ts` merge
conflict by re-applying the two lines above.

## Keeping it in sync

Edit the source here, then re-copy into the clone to test:
```sh
# from the repo root, after editing tools/mcp-patches/enfusion-workbench-mcp/src/...
xcopy /Y tools\mcp-patches\enfusion-workbench-mcp\src tools\mcp\enfusion-workbench-mcp\src
cd tools\mcp\enfusion-workbench-mcp && npm run build
```
The patch bundle is the source of truth; the clone is a build artifact.