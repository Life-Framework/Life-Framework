# Public surface

What a content pack may rely on. Everything else in the core is internal and may change
in any release. The machine list is [`public-surface.json`](public-surface.json); the check is
`tools/validation/validate-pack-manifest.mjs` (#963). Rule: [`core-vs-pack.md`](core-vs-pack.md).

## Pack manifest

A pack ships a `.conf` of class `LF_PackManifest` (flagship:
`addons/LF_Everon1989/Configs/LF_PackManifest.conf`) and its mission header names it in
`m_LF_PackManifest`.

| Field | Meaning |
|---|---|
| `m_sPackId` | the pack's addon id, for log lines |
| `m_sCoreMin` | lowest core version supported, `x.y.z`, inclusive; empty is no lower bound |
| `m_sCoreBelow` | first core version not supported, `x.y.z`, exclusive; empty is no upper bound |

When the running core (`LF_PackManifest.CORE_VERSION`) is outside the range, the core logs one
line and loads none of that mission's config overlays:

`[LFDebug:Packs] pack 'LF_Everon1989' refused: it needs core >= 0.1.0, < 0.2.0, this core is 0.3.0; its config overlays are not loaded`

A header with no manifest loads as before (the core's own test missions). A manifest that is
named but does not load as an `LF_PackManifest` also refuses.

## What is public

| Surface | Public names | Status |
|---|---|---|
| Config classes | the root `.conf` classes in `public-surface.json` `configClasses`, with their field names, plus the pack manifest and mission header | listed |
| Bus events | `Feature/<Area>/LF_<Area>Events.c` declarations | to be listed |
| Dialogue hooks and params | the dialogue config and action params | to be listed |
| Prefab slots | the slots a pack prefab fills | to be listed (#964) |
| Terms list | the vocabulary a pack renames | to be listed (#958) |
| Systems list | the system ids the owner switches on and off | to be listed (#960) |

A surface not yet listed is internal until it is. Adding a name is free; removing or renaming
one is covered below.

## Changing the surface

- A pack's `.conf` files may only use root classes that are listed. `validate` fails on a pack
  `.conf` whose root class is a core `LF_` class that is not.
- `public-surface-baseline.json` holds every name released so far. A name that is in the
  baseline and in neither `configClasses` nor `deprecated` is a removed public name, and
  `validate` fails on it.
- To retire a name, move it from `configClasses` to `deprecated` as
  `{ "name": "LF_OldConfig", "replacement": "LF_NewConfig", "since": "0.2.0", "removeIn": "0.3.0" }`
  and keep it working for one minor version, logged once at boot. Add a line to the release
  notes. Once `removeIn` has shipped, the entry and its baseline name go together.
- Every listed class must still be declared in core script, so a rename that forgets the list
  fails too.
