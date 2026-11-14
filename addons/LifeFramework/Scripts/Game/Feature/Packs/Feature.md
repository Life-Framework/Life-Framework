# Packs

## Intent

A content pack says which core versions it was built for, and the core refuses a pack outside
that range instead of loading overlays that may name fields that moved. Packs may rely only on
the declared public surface (`docs/architecture/public-surface.md`).

## Pack manifest (#963)

- **Player sees:** nothing. The server owner sees one `[LFDebug:Packs]` error line at boot naming the pack, the range it needs and the core it found.
- **Owner and side:** `LF_PackManifest` decides on the server. The mission header names the manifest (`m_LF_PackManifest`); `LF_MissionConfigPackSource` asks it before it returns any overlay. A header without a manifest loads as before (the core's own test missions).
- **Data:** a `.conf` of class `LF_PackManifest`: `m_sPackId`, `m_sCoreMin` (inclusive), `m_sCoreBelow` (exclusive). The core version is `LF_PackManifest.CORE_VERSION`. Nothing persists. The flagship's is `addons/LF_Everon1989/Configs/LF_PackManifest.conf`.
- **Checks:** `validate-pack-manifest` (range parses and holds the current core; the pack's `.conf` files use only classes listed in `docs/architecture/public-surface.json`; no baseline name was removed without a deprecation entry).
- **Proof:** `packs/manifest` (LOGIC).

## Pack config sections (#553)

- **Player sees:** nothing. The server owner sets `everon1989`-style sections in `config.json`.
- **Owner and side:** server. The pack lists sections in its manifest `.conf` (`m_aConfigSections`); `LF_PackConfigSections` validates them (namespace, no core name, field types, defaults, ranges), `LF_ConfigService.IsKnownTopLevelKey` accepts the valid ones, and `LF_PackConfigSections.Read(section)` returns the values over the declared defaults.
- **Data:** `LF_PackConfigSection` / `LF_PackConfigField` in `.conf`; nothing persists. The core never names a pack.
- **Offline:** `tools/config/pack-sections.mjs` feeds `config.schema.json`, `pack check` and `pack explain`.
- **Proof:** `config/pack-section` (LOGIC); `pack-check-selftest.mjs` (schema merge, `config-check`, `pack check`, `pack explain`).
