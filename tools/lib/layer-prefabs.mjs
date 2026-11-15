// Which prefabs a .layer places as world entities, and what a missing bounds row means on a branch.
//
// A layer names a prefab in three ways: an entity declaration (`Class Name : "{GUID}path.et" {`, a child entity
// too), a property inside a component (`Prefab "{GUID}path.et"` in a loadout slot, an inventory entry, a spawner's
// resource) and so on. Only the first is a prop in the world that `place` and the placement audit measure. The
// NPC Dresser writes clothing prefabs into a character's loadout slots; those are not props and need no bounds
// in tools/place/prefab-bounds.json.
import { MAINTAINER_BRANCH } from "./world-checks.mjs";

// The prefab paths (`Prefabs/...et`, no GUID) entities in this layer text are declared from.
export function layerEntityPrefabs(text) {
  const out = new Set();
  for (const m of String(text).matchAll(/:\s*"\{[0-9A-Fa-f]{16}\}([^"]+\.et)"\s*\{/g)) out.add(m[1]);
  return out;
}

// "fail", "warn" or "ok" for `missing` vanilla prefabs without bounds. Missing bounds never block a
// town-maintainer-* branch: the maintainer placed the prop by hand and looked at it, so it is a warning.
export function boundsGapVerdict(missing, branch) {
  if (!missing) return "ok";
  return MAINTAINER_BRANCH.test(branch || "") ? "warn" : "fail";
}
