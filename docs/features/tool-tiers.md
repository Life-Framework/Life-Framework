# Tool tiers

Design context: `docs/roadmap/next-release-0.3.0.md` (work W10, the three finishes).

### Tool tier prefabs — `LF_Everon1989/Prefabs/Tools/Tiers/`, `tools/assets/tool-tiers/` (#4188)
- 🧪 (2026-11-23, #4188) Nine hand-tool prefabs, `LF_Tool_<Pickaxe|Axe|Rod>_<Worn|Standard|Fine>.et`. Each inherits the shipped tool prefab (`Prefabs/Tools/Pickaxe.et`, `Prefabs/Tools/Axe/Axe.et`, `Prefabs/Items/Fishing/LF_FishingRod.et`), so the mesh, hold pose, damage and tool id are unchanged, and overrides the one material with a tinted copy of its own `.emat`: worn is rust brown, standard is the natural finish, fine is polished cool steel. No new texture, so no credit row. Vanilla has no pickaxe, axe or rod mesh (`asset_search`), so the tools keep their own Blender builds. `tools/assets/tool-tiers/spec.mjs` holds the tools and tints, `build.mjs` writes the prefabs, materials and metas (keeps GUIDs on re-run), `check.mjs` is the acceptance check. Nothing sells or equips these yet: the tier items, wear and the hardware bench are the other W10 cards.
