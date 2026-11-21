// Entity classes whose layer `coords` height is measured from the terrain under them, not from the world's zero
// (checked on the island's test server: the entity's origin was terrain + the layer's y; #3566). The town fit seats
// a row of one of these at y about 0 (tools/town/seat.mjs); the placement audit looks for it at terrain + y
// (agent-box/place-audit.mjs, LF_PlacementAudit.RunBatch). One list for both.
export const TERRAIN_RELATIVE_CLASSES = ["SCR_DestructibleEntity"];

// A terrain-relative row is legitimately a step or two above 0 (a fence on a plinth); more than this is a world height
// copied into a layer that counts from the ground.
export const TERRAIN_RELATIVE_MAX_Y = 2;

export const isTerrainRelative = (cls) => TERRAIN_RELATIVE_CLASSES.includes(String(cls ?? "")) || String(cls ?? "") === PREFAB_RELATIVE_CLASS;

// A prefab whose own class is not SCR_DestructibleEntity but stands the same way: the layer class says GenericEntity, yet the
// island's test server put the booth's base at terrain + the layer's y (payphone reseat #3878: y 1.63 over a terrain of 1.53
// hung it 1.19 m up). layers.mjs gives such a row this marker class, so the audit looks for it at terrain + y too.
export const TERRAIN_RELATIVE_PREFABS = ["Prefabs/Comms/LF_Payphone.et"];
export const PREFAB_RELATIVE_CLASS = "TerrainRelativePrefab";
export const isTerrainRelativePrefab = (prefab) => TERRAIN_RELATIVE_PREFABS.some((p) => String(prefab ?? "").endsWith(p));
