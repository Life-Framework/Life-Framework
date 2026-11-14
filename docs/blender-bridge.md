# Blender bridge

`tools\cli blender` lets an agent drive the user's open Blender window and falls back to a
headless Blender when none is listening. The pieces:

| Piece | Where |
|---|---|
| The Blender extension (the server) | `tools/blender/lf_bridge/` (Blender 4.2+, tested on 5.3) |
| Finding Blender, running scripts, the client | `tools/lib/blender.mjs` (`blenderExe`, `runHeadless`, `runLive`, `bridge.ping/exec/shot`) |
| The command | `tools/commands/blender.mjs` |
| Offline proof | `tools/validation/blender-bridge-selftest.mjs` |

## Setup

1. `tools\cli blender install` builds the extension (`blender --command extension build`) and
   installs it into Blender 5.3's `user_default` repository (`--blender <exe>` picks another).
2. Restart Blender (or Edit > Preferences > Get Extensions > Installed > enable *Life Framework
   Bridge*). The first start creates `~/.life-framework/blender-bridge-token`.
3. In the 3D viewport sidebar (N), the **LF Bridge** tab shows `LF Bridge: listening on
   127.0.0.1:9876` with a Stop button; the status bar carries the same line. Off shows `off` and a
   Start button. Preferences hold the port (default 9876) and *Start when Blender starts*.
4. `tools\cli blender status` shows what it found.

## Security model

The bridge runs Python inside Blender, so it is guarded three ways:

- **Localhost only.** The socket binds `127.0.0.1`; there is no setting for another address, and
  the port is taken exclusively (Windows `SO_EXCLUSIVEADDRUSE`), so no other process can share it.
- **A token on every request.** `~/.life-framework/blender-bridge-token` is 32 random bytes in hex,
  created on first enable and readable only by the user where the OS allows. A request without it
  gets `unauthorized` and reaches no handler; it is compared in constant time and never printed,
  logged or returned. A web page cannot talk to the port: it is not HTTP, and a stray HTTP request
  is answered once and dropped.
- **A visible off switch.** The panel and status bar say whether it is listening and have the
  Start/Stop toggle. A headless Blender (`--background`) never listens unless `LF_BRIDGE_FORCE=1`.

Anything running as the user can read the token file, as with any file in the user's profile; the
bridge keeps other users, other machines and web pages out, not the user's own programs.

## Commands

```
tools\cli blender status
tools\cli blender install [--blender <exe>]
tools\cli blender run <file.py> [--live|--headless] [--needs archbuilder] [--blender <exe>] [--no-factory] [--discard] [-- args]
tools\cli blender exec "<python>" [--live|--headless]        # bpy is imported; set `result` to return JSON
tools\cli blender shot <out.png> [--view front|back|left|right|top|bottom|iso] [--camera <name>] [--size WxH] [--target a,b] [--file x.blend]
tools\cli blender show <model file | asset slug | addons/...fbx> [--clear] [--colliders] [--shot out.png] [--view iso]
```

`run`, `exec` and `shot` use the live bridge when it answers and a headless Blender otherwise (a
wrong token is an error, not a fallback). Each prints one result line, then the script's output.
Scripts written for `blender --python x.py -- args` run unchanged live: `sys.argv` carries the
arguments after `--`, `__file__` is set, a `SystemExit` with a message is a failure.

- **`shot`** with no view renders the active 3D viewport as the user sees it; with a preset it
  renders a temporary camera framing the visible geometry (or `--target` objects) and puts the
  scene back. Headless it needs `--file x.blend` and uses the Workbench engine.
- **`show`** is the "show me X in Blender" command (live only: it errors with *open Blender 5.3
  and turn on LF Bridge* when nothing listens). The target is a model file (`.fbx .glb .gltf .obj
  .blend`; a `.blend` appends its objects), an asset-pipeline slug (the newest
  `tmp/assets/<slug>/clean/*.fbx`, else the raw GLB, else the addon's source `.fbx` under the
  spec's `asset_dir`), or an addon path such as `addons/LifeFramework/Assets/.../X.fbx`. The
  objects go in an `LF_Show` collection, are selected and framed in every 3D view; it prints the
  objects, triangles and dimensions in metres. Collision shapes (`UBX_ UCX_ UTM_`) are hidden unless
  `--colliders`. Several `show`s accumulate; `--clear` empties `LF_Show` first (alone, it only
  clears). `--shot` also writes a picture of what was loaded. **Compiled `.xob` files cannot be
  opened in Blender**; `show` uses the source models, and refuses a `.xob` with that hint.
- **`install`** needs no running Blender.

For other tools: `blenderExe({ needs: 'archbuilder' })` picks the newest install that has
`arch_builder` in any repository under its user `extensions` folder (`user_default` or another) or in
the local repository directory `lf_local` (`LF_ARCHBUILDER_DIR`, default
`C:/Users/jaspe/Documents/Reforger/blender-addons`; Blender keeps custom repositories in
`userpref.blend`, so the directory is named by the variable, not read from the preferences); one that
holds it in its own user folder wins over one that only sees the shared directory.
`runHeadless(script, args, { needs: 'archbuilder', factory: false })` runs it with the user's
add-ons loaded. `LF_BLENDER` overrides the choice. The documented
install paths are `C:/Users/jaspe/Tools/blender-4.5.9-windows-x64/blender.exe` (the default, and
what the asset pipeline's clean stage is proven on) and
`C:/Program Files/Blender Foundation/Blender 5.3/blender.exe`. `LF_BRIDGE_PORT` and
`LF_BRIDGE_TOKEN_FILE` point the client elsewhere (the port must match the extension's preference).

## Asset pipeline

- `tools\cli asset run <slug> --only clean --live [--discard]` runs the clean stage in the open
  Blender. The script starts from an empty scene, so the bridge swaps its
  `bpy.ops.wm.read_factory_settings(use_empty=True)` for an in-place `lf_new_scene()` (that call
  would reset the user's add-ons, the bridge included). It refuses when the open file has unsaved
  changes unless `--discard`; the open file afterwards is `tmp/assets/<slug>/clean/<Name>.blend`.
  Headless stays the default.
- `tools\cli asset shot <slug> [--view v] [--live]` writes `tmp/assets/<slug>/shots/<view>.png`
  (iso, front, right, top) of the cleaned LOD0 model; headless by default.

## How it works

JSON lines over TCP: `{"token", "id", "op": "ping"|"exec"|"shot", ...}` in, `{"id", "ok", "error",
...}` out (`tools/blender/lf_bridge/protocol.py`). The socket threads never touch `bpy`: each
request is queued and run on Blender's main thread by a persistent `bpy.app.timers` callback
(`runner.py`). Every request has a hard timeout (default 5 s ping, 60 s exec, 120 s shot, at most
900 s): a request that never started is dropped; a running Python script gets an exception raised
in the main thread. A long native call (a render) stops when it returns. Blender's window is busy
while a script runs, as with any script.

Known limits: one main thread, so requests run one at a time; `exec` runs arbitrary Python as the
user; the headless fallback cannot render the viewport.
