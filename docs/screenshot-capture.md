# Screenshot capture harness (`wb_capture`)

Let an AI (or any agent) take a screenshot of the running world — typically to
verify that an object placed in the world renders correctly, or that a visual
feature looks right. No baselines, no pixel diffing: you get an image back and
eyeball it.

## The one hard rule

The capture runs on a **rendering instance**. A headless dedicated server has
no viewport and cannot produce pixels — `System.MakeScreenshot` captures the
screen. The rendering instance is Workbench play mode (or a game client). So the
flow is: **`wb_play` first, then `wb_capture`.**

## How an AI uses it

The `wb_capture` tool lives on the **`enfusion-workbench`** MCP server (not the
`enfusion-mcp` fork). Use that server's tools for the whole flow so the capture
handler is deployed by its `wb_launch`:

1. `enfusion-workbench_wb_launch` (gprojPath = the project you want; a worktree
   project works too — the class must be compiled in) and `wb_play` to enter
   play mode.
2. Either place the object in edit mode first (`wb_entity_create`) and then
   capture at its position, or pass `prefab` + `position` to `wb_capture` and it
   spawns the fixture at capture time (deleted after the shot).
3. `wb_capture` with e.g.
   `{ "position": "128 1 133", "cameraOffset": "12 4 12", "prefab": "{GUID}Prefabs/Vehicles/Wheeled/M151A2/M151A2.et" }`.
   It aims the camera at the target, waits, captures, converts the BMP to PNG,
   and returns both absolute paths — read the PNG to see the shot.
4. Optional determinism knobs: `timeOfDay` (0-24) and `weather` (e.g. `Clear`)
   pin the environment so repeat captures of the same placement are comparable.

From a plain terminal (no opencode restart needed):

```
tools\cli call wb_capture '{"position":"128 1 133","name":"vehicle_check"}' enfusion-workbench
```

`aimAtTarget: false` captures the current view as-is instead of moving a camera.

## Architecture

- **`addons/LifeFramework/Scripts/Game/Feature/ScreenshotCapture/EL_ScreenshotCapture.c`**
  (committed) — the actual capability. Static `StartCapture(EL_ScreenshotShot)`
  kicks off an async capture: optional prefab spawn, optional camera aim
  (reuses the first registered camera, restores it after), preload, settle,
  `System.MakeScreenshot`, file check. Output lands in
  `$logs:el-captures/<name>_<n>.bmp` (outside the repo). Every failure path
  returns a clean `error:<reason>` via `EL_Debug` and never crashes — proven by
  `screenshot/fail-safe` (LOGIC tier) on the headless test server.
- **`EMCP_WB_Capture.c`** (local MCP clone, git-ignored) — a `NetApiHandler`
  with a start/poll protocol, because `System.MakeScreenshot` writes the file
  asynchronously.
- **`wb_capture` tool** (`src/tools/wb-capture.ts`, local) — calls the handler,
  polls to completion, converts the BMP to PNG (pure Node + `zlib`, no deps)
  and returns absolute paths.

## Output

BMP is the engine's native output format. The tool converts it to a PNG next to
it so the image is readable directly. Files land in the Workbench logs dir
(`Documents/My Games/ArmaReforgerWorkbench/logs/el-captures/`) — never in the
addon, so captures cannot pollute the repo or the resource database.

## Manual proof (GPU machine, once after this ships to main)

Prerequisite: this feature merged to main so the main checkout's mod compiles
`EL_ScreenshotCapture.c`.

1. Launch Workbench on the main checkout project and open `Worlds/DebugWorld`
   (or any world with something to look at).
2. `tools\cli call wb_play '{}' enfusion-workbench` — wait for play mode.
3. `tools\cli call wb_capture '{"position":"128 0 133","cameraOffset":"10 3 10","name":"manual_proof"}' enfusion-workbench`
   → expect `result: ok`, a BMP and a PNG path under `.../ArmaReforgerWorkbench/logs/el-captures/`.
4. Open the PNG: it should show the world terrain/objects from ~10m away looking
   at (128, 0, 133). A black or empty image means the capture ran but no
   renderer was active — check you are in play mode, not edit.
5. Repeat with `prefab` set to any base-game vehicle prefab; the vehicle should
   be visible in the shot and gone from the world afterwards (fixture cleanup).
6. Optional: pin `timeOfDay: 12` and `weather: "Clear"` and capture the same
   spot twice — the shots should look the same.

## Constraints / notes

- Requires a GPU-backed, windowed render instance (Workbench play mode or game
  client). Not for the headless `cli test` server.
- `System.MakeScreenshot` captures at native window resolution; the BMP is
  converted to PNG as-is.
- Camera aim reuses the first registered camera and restores its transform
  after the shot. No camera is registered on renderless instances → clean
  `error:no_camera`.
- The engine writes BMP; the PNG conversion is pure Node and covers BI_RGB 24/32
  bpp. An exotic BMP variant falls back to returning the raw BMP path.