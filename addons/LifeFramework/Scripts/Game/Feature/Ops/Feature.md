# Ops - Design Requirements

> Server operations a player sees and hears: the restart warning and its siren.

## Intent

The restart siren is a placed prop, not a menu. `LF_RestartSirenComponent` (prefab
`Prefabs/Props/Ops/LF_RestartSiren.et`) registers with `LF_Places` under the kind `restart-siren`.
The warning logic (O7) finds the siren through `LF_Places`, never by entity name or coordinate, so
a pack on any map places its own.

## Status

Slice #1345: the prefab and the registration only. The component makes no sound yet.
