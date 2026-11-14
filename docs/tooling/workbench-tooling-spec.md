# Life Framework — Clean-Room Workbench Tooling Optimizations Spec

Maintainer-provided spec (2026-11-14). Six clean-room tooling optimizations for `tools/mcp-patches/enfusion-workbench-mcp/` and `tools/`.

Priorities: 1 = live ResourceDatabase.SearchResources + batch RebuildResourceFiles/WaitForFile (items 1-2, low risk). 2 = SetCamera + Resource Manager preview wb_capture (item 3, low risk). 3 = wb_eval via ScriptModule.CompileScript (item 4, medium risk). 4 = LF-Agents-scoped FBX/Auth dialog watcher and PrintWindow capture (items 5-6, medium risk).

## Target Integration Layout
- Workbench Enforce Script handlers: `tools/mcp-patches/enfusion-workbench-mcp/mod/Scripts/WorkbenchGame/EnfusionMCP/`
- MCP server tools (TypeScript + Zod): `tools/mcp-patches/enfusion-workbench-mcp/src/tools/` (registered in `src/server.ts`)
- Process / desktop isolation helpers: `tools/lib/quiet-launch.mjs`, `tools/lib/quiet-workbench.mjs`

The upstream clone lives in `tools/mcp/enfusion-workbench-mcp/` (gitignored). `tools\cli mcp update enfusion-workbench` (or `mcp install`) copies `tools/mcp-patches/` over it. To change an upstream file, copy it into `tools/mcp-patches/...` at the same relative path, edit it there, and re-run the update.

Rules for all Enforce Script additions: no ternary (`?:`); explicit `ref` on Managed collections and elements; `EMCP_WB_*` class/handler naming; every patch lives under `tools/mcp-patches/enfusion-workbench-mcp/` so `tools\cli mcp update enfusion-workbench` reapplies it.

## 1. Live ResourceDatabase search (`wb_resources` `browse`)
Gap: `EMCP_WB_Resources.c` rejects `action="browse"` as unimplemented. Offline `asset_search` reads unpacked/PAK files and misses newly registered or generated mod resources with their live `{GUID}path`.
APIs: `ResourceDatabase.SearchResources(SearchResourcesFilter filter, SearchResourcesCallback callback)`. `SearchResourcesFilter` has `string rootPath` (`"$Addon:Folder"` or empty for all), `ref array<string> fileExtensions` (no dots: "et", "xob", "emat"), `ref array<string> searchStr`, `bool recursive`. Callback: `bool Callback(ResourceName resourceName, string exactPath)`; returning false halts enumeration. (Verify the exact names with api_search before coding.)
Contract: inputs `rootPath?`, `extensions?: string[]`, `filters?: string[]`, `recursive?` (default true), `limit?` (default 200, capped at 1000). Store matches in a static collector during the search, then clear it right after serialising the response so nothing leaks into the next call. Output `{ status, count, limit, entries: [{ name, path, resourceName }] }`. An invalid or empty rootPath returns status ok with count 0, never throws.
Verify: browse `$LifeFramework:Prefabs` filtered to ["et"]; returned `{GUID}path` values match `wb_prefabs` getGuid. Two back-to-back searches with different extensions show zero bleed.

## 2. Batch resource reimport and database acknowledgement (`wb_resources`)
Gap: register/rebuild accept a single path and return before the Resource Database acknowledges the change, so the next step can race the asset.
APIs: `Workbench.GetAbsolutePath(string resourcePath, out string absPath, bool mustExist)`, `ResourceManager.RegisterResourceFile(string absFilePath, bool bBuildRuntimeResource)`, `ResourceManager.RebuildResourceFiles(notnull array<string> filePaths, string configuration)`, `ResourceManager.WaitForFile(string filePath, int maxTimeMs)`.
Contract: accept `paths: string[]` (keep single `path` for compatibility), `configuration?` (default "PC"), `waitMs?` (default 5000; 0 skips waiting). Resolve `$Addon:relative` with GetAbsolutePath; allow verified absolute paths; malformed relative paths go to `failed[]` without aborting the rest. register: RegisterResourceFile per path, successes in `processed[]`, false returns in `failed[]`. rebuild: RebuildResourceFiles once for the batch. When waitMs > 0, WaitForFile per processed path. Output `{ status, processed, failed, configuration }`.
Verify: recolor an `.edds` with `texture_recolor`, batch rebuild with waitMs 5000, then immediately browse / open the asset with no missing-resource errors.

## 3. Resource Manager preview capture and edit-mode SetCamera
Gap: inspecting one `.xob`/`.et`/`.emat` needs play mode (`EMCP_WB_Capture.c`) or a separate headless Workbench via `tools\cli look` (~10 s startup + ~1.9 GB RAM). `EMCP_WB_GetCameraPos.c` only reads the camera; nothing positions the World Editor edit-mode camera.
APIs: `WorldEditor we = Workbench.GetModule(WorldEditor)`, `WorldEditorAPI api = we.GetApi()`, `api.SetCamera(vector pos, vector dir)` (normalise dir; reject `dir.Length() < 0.0001`). `ResourceManager rm = Workbench.GetModule(ResourceManager)`, `rm.SetOpenedResource(string resource)` (`{GUID}path` or `$Addon:path`). `System.MakeScreenshot(string path)`.
Contract (non-blocking, two phases):
1. No `Sleep()` inside a NetApiHandler: on the Workbench main thread it raises a VME ("Cannot stop main thread") and blocks the render thread that writes screenshots.
2. `System.MakeScreenshot(stem + ".png")` returns after queuing; the render thread writes the file asynchronously, often appending its own extension or writing BMP data.
3. TypeScript orchestration: phase 1 calls the handler to SetOpenedResource or SetCamera; wait `settleMs` (default 800) in Node; phase 2 calls the handler to MakeScreenshot to a unique stem in a scratch dir; poll from Node until the file exists, size > 0 and the size is stable across two reads ~150 ms apart, up to `timeoutMs` (default 15000); convert BMP to PNG (reuse `agent-box/bmp.mjs`), optionally downscale to `maxPx`, and return an MCP image block plus the saved path.
Verify: open a `.et` prop in Resource Manager with no world loaded; the PNG is non-uniform (variance above a flat threshold). Move the DebugWorld edit-mode camera with SetCamera, read it back with `EMCP_WB_GetCameraPos`, and the coordinates match.

## 4. Live Enforce Script eval escape hatch (`wb_eval`)
Gap: ad-hoc WorldEditorAPI or ResourceDatabase inspection needs a throwaway WorkbenchPlugin, a script reload, a run and cleanup.
APIs: `ScriptModule parent = Workbench.GetWBGameScriptModule()` (fallback `Workbench.GetWBScriptModule()` if null), `ScriptModule mod = ScriptModule.CompileScript(parent, source, out string errorText, out int errorLine)`, `bool ok = mod.Call(null, entryFunctionName, false, out string returned)`. (Verify exact signatures with api_search.)
Contract: wrap caller `code` in a `string` function with a fallback `return "";` so code without an explicit return still compiles. A compile failure returns `{ status: "compile_error", errorLine, errorText }` without breaking the session. A call failure returns `{ status: "runtime_error", message }` pointing at the Workbench log. The tool description must say world edits wrap WorldEditorAPI mutations in `api.BeginEntityAction(...)` / `api.EndEntityAction(...)` so they enter undo history. The handler stays strictly in tools/mcp-patches, never in addons/LifeFramework/.
Verify: a read-only query (`return api.GetEditorEntityCount().ToString();`) returns the exact integer string. Invalid syntax (a missing semicolon and a ternary `?:`) reports line and error cleanly, and later NET API calls still work.

## 5. Desktop-isolated modal dialog watcher (Win32)
Gap: RegisterResourceFile on `.fbx` (or unsuffixed textures) opens a modal Qt dialog ("Register Resource File(s)") on the Workbench main thread and hangs the NET API socket until answered. First-time restricted script calls (MakeScreenshot, RegisterResourceFile, Save) block on "Script Authorization Required".
Design:
- PID scoping: never scan or click arbitrary windows. Filter candidate HWNDs by the exact Workbench PID (`GetWindowThreadProcessId`) and exclude main-window titles (World Editor, Resource Manager, Enfusion Workbench).
- Hidden desktop: automated Workbenches run on the private Win32 desktop `LF-Agents` (`tools/lib/quiet-launch.ps1`). EnumWindows, SetCursorPos, mouse_event and keybd_event act only on the calling thread's desktop and steal user focus on the primary desktop, so the watcher runs only in a helper attached to `LF-Agents` (or when `LF_SHOW_WINDOWS=1` explicitly).
- Button order: (1) EnumChildWindows for class "Button" with affirmative captions ("Yes to All", "Yes", "OK"); (2) keyboard mnemonic (Alt+A for "Yes to &All"); (3) custom-drawn Qt dialogs with no child HWNDs (the FBX model-vs-animation prompt): anchor relative to the dialog's bottom-right GetWindowRect corner, because the width grows with the file path while the right-aligned button row keeps fixed offsets from (right, bottom).
Verify: register a test `.fbx` in a temporary worktree with no manual mouse input; the call completes and RegisterResourceFile returns true.

## 6. Workbench Qt window capture (Win32 PrintWindow)
Gap: `System.MakeScreenshot` captures only the 3D framebuffer, not the Qt chrome (Log Console, Hierarchy, Object Properties, Resource Browser, menus).
Design: find the visible, non-minimised HWND of the Workbench PID whose title matches a module (World Editor, Resource Manager, Enfusion Workbench). Create a memory DC + bitmap sized to GetWindowRect, call `PrintWindow(hwnd, memDc, PW_RENDERFULLCONTENT)` (0x2, fallback 0), read 24-bit BI_RGB top-down scanlines (`biHeight = -height`) with GetDIBits, convert BGR to RGB, downscale if needed, encode PNG.
Constraint: the window must be on an active, rendered desktop session and not minimised.
Verify: capture the Workbench window with Log Console and Object Properties open; panel text is legible in the PNG.
