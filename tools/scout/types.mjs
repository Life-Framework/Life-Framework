// tools/scout/types.mjs - the building and prop type names a dossier uses, read off a vanilla prefab's path.
//
// Vanilla Everon prefabs are filed Prefabs/Structures/<Category>/<Sub>/<Name>.et, so the folder says what a thing
// is and the type names below are stable across game builds. The same vocabulary is what `tools\cli scout types`
// prints and what docs/everon/catalog-buildings.md can use, so the two never name one thing two ways.
//
// `civic` marks the public-facing types a town plan wants a picture of (a place a character can walk up to and use),
// `home` the dwellings.

// The building types are the Everon catalog's (docs/everon/README.md, docs/everon/method/classify.mjs, which is where
// these rules come from: scout reads the catalog's own typing when it has the catalog and uses these rules only for a
// building the catalog does not list, so the two never name one thing twice).
// [regex, type, { civic?, home?, ruin? }]. First match wins; the order is the catalog's.
export const BUILDING_RULES = [
  [/Structures\/Houses\/Town\//, "townhouse", { home: true }],
  [/Villa_E_2I01_Police/, "police", { civic: true }],
  [/Structures\/Houses\/Villa\//, "villa", { home: true }],
  [/Structures\/Houses\/Garage\/|HouseAddon_Garage|Industrial\/Garages|Workshop_0|Warehouse_01_Workshop|Repair\//, "garage", {}],
  [/Houses\/Shed\/|HouseAddon_Shed_E|Houses\/Village\/HouseAddon_Workshop/, "shed", {}],
  [/Structures\/Houses\/(Village|Wooden|Prefabricated)\//, "house", { home: true }],
  [/Houses\/Farm\//, "farmhouse", { home: true }],
  [/Commercial\/Shops\//, "shop", { civic: true }],
  [/Commercial\/Pubs\//, "pub", { civic: true }],
  [/Commercial\/Booth\//, "kiosk", {}],
  [/Commercial\/FuelStations\//, "fuel", { civic: true }],
  [/Cultural\/Churches\//, "church", { civic: true }],
  [/Cultural\/Chapels\//, "chapel", { civic: true }],
  [/Cultural\/Castles\//, "castle", {}],
  [/Services\/Hotels\//, "hotel", { civic: true }],
  [/Services\/Government\//, "government", { civic: true }],
  [/Services\/Fire\//, "fire", { civic: true }],
  [/Agriculture\/(Barn|CowShed|Hayloft|WeightStation)/, "farm", {}],
  [/Agriculture\/(Henhouse|Greenhouse)/, "farm-small", {}],
  [/Industrial\/(Houses|Sheds)\//, "industrial", {}],
  [/Industrial\/Containers\//, "industrial-tank", {}],
  [/Industrial\/(Smokestacks|Towers)/, "industrial-tower", {}],
  [/Military\//, "military", {}],
  [/Airport\/(ControlTower|Hangar|Antenna)/, "airport", {}],
  [/Infrastructure\/Naval\//, "port", {}],
  [/Recreation\//, "recreation", { civic: true }],
  [/Ruins\//, "ruin", { ruin: true }],
  [/Infrastructure\/Waterworks/, "waterworks", {}],
];

// Types the dossier counts but does not list one by one (a harbour is hundreds of quay and pier pieces).
export const COUNTED_TYPES = new Set(["port", "waterworks"]);
// Types a town plan wants a picture of.
export const CIVIC_TYPES = new Set(BUILDING_RULES.filter(([, , f]) => f.civic).map(([, t]) => t));
export const HOME_TYPES = new Set(BUILDING_RULES.filter(([, , f]) => f.home).map(([, t]) => t));

// ------------------------------------------------------------------ one vocabulary for typing and templates
// The building types above are THE vocabulary: the catalog, the scout dossier and the town templates
// (tools/town/templates/*.json, `needs.types`) all name a building by one of them. Older names (the first templates
// wrote municipal-office, house-town, shop-*, fuel-station) and the finer names a human reaches for resolve here instead of
// in a second list; a template or a hand-written dossier may use either form and they still meet (tools/town/map.mjs).
// alias -> the canonical types it stands for, in preference order.
export const TYPE_ALIASES = {
  "municipal-office": ["government"],
  "town-hall": ["government"],
  "police-station": ["police"],
  "house-town": ["townhouse"],
  "house-village": ["house"],
  "cottage": ["house", "farmhouse"],
  "shop-general": ["shop"],
  "shop-clothes": ["shop"],
  "shop-drug": ["shop"],
  "shop-modern": ["shop"],
  "fuel-station": ["fuel"],
  "farm-building": ["farm", "farm-small", "farmhouse"],
  "industrial-shed": ["industrial"],
  "house-addon": ["shed", "garage"],
};

export const CANONICAL_TYPES = new Set(BUILDING_RULES.map(([, t]) => t));

// Pure: the canonical building types a name stands for ([] when it is neither canonical nor an alias). A trailing `*`
// ("shop-*") is a family: its base, every alias that starts with that prefix.
export function canonicalTypes(name) {
  const n = String(name || "");
  if (n.endsWith("*")) {
    const prefix = n.slice(0, -1);
    const base = prefix.replace(/-$/, "");
    const out = new Set(canonicalTypes(base));
    for (const [alias, to] of Object.entries(TYPE_ALIASES)) if (alias.startsWith(prefix)) for (const t of to) out.add(t);
    return [...out];
  }
  if (CANONICAL_TYPES.has(n)) return [n];
  return TYPE_ALIASES[n] ? [...TYPE_ALIASES[n]] : [];
}

// A refinement is an alias that names a KIND of one canonical type, told apart by the prefab's stem (the dossier types every
// vanilla shop "shop"; its stem says clothes, drugstore or household goods). A refinement matches a building whose type is the
// refinement itself (a hand-written dossier) or whose type is the canonical one and whose stem says so.
export const TYPE_REFINEMENTS = {
  "shop-clothes": { type: "shop", stem: /clothes|cloth/i },
  "shop-drug": { type: "shop", stem: /drug|pharm/i },
  "shop-general": { type: "shop", stem: /household|general|grocer|ShopModern/i },
};

// Pure: the index of the first wanted type (a template's `types`) that the building satisfies, or -1.
// Both sides resolve to canonical types, so "municipal-office" in a template finds a "government" building and
// "government" finds a hand-written "municipal-office" one; a name this table does not know still matches itself.
// `stem` (optional) lets a refinement such as shop-drug pick the drugstore among the shops.
export function typeIndex(wanted, buildingType, stem = "") {
  const have = new Set([String(buildingType), ...canonicalTypes(buildingType)]);
  for (let i = 0; i < wanted.length; i++) {
    const w = String(wanted[i]);
    if (w === buildingType) return i;
    const ref = TYPE_REFINEMENTS[w];
    if (ref) {
      if (have.has(ref.type) && ref.stem.test(String(stem))) return i;
      continue;
    }
    for (const t of canonicalTypes(w)) if (have.has(t)) return i;
  }
  return -1;
}

// Pure: the names in a list of wanted types that resolve to nothing (a typo in a template).
export const unknownTypes = (wanted) => wanted.filter((w) => canonicalTypes(w).length === 0).map(String);

// A prop worth listing in a dossier, by prefab path. [regex, kind].
export const PROP_RULES = [
  [/\/Naval\/.*(Pier|Jetty|Quay|Dock|Slip)|\/(Pier|Jetty)[A-Za-z]*_\d/i, "pier-or-dock"],
  [/Mooring|Bollard_|\/Bollard/i, "mooring"],
  [/FuelPump|FuelDispenser/i, "fuel-pump"],
  [/\/Bench(Street|Wooden|Stadium)|ForestBench|StreetBench/i, "bench"],
  [/BusShed|BusStop/i, "bus-shelter"],
  [/PhoneBooth/i, "phone-booth"],
  [/Fountain_/i, "fountain"],
  [/\/(Memorial|Monument)|Monument_|MemorialSoviet|MemorialTown/i, "monument"],
  [/\/Statues\//i, "monument"],
  [/\/Cultural\/Calvaries\/|Shrine_/i, "shrine"],
  [/MarketStand/i, "market-stand"],
  [/Mailbox_/i, "mailbox"],
  [/MessageBoard|BoardMap/i, "notice-board"],
  [/WaterWell|\/Well_|Well_\d/i, "well"],
  [/PosterColumn/i, "poster-column"],
  [/ManholeCover|SepticCover/i, "manhole"],
  [/LampStreet/i, "street-lamp"],
];

// Pure: what a prefab path is. { group: building | prop | wall | vegetation | road-part | other, type, civic, home, ruin, stem }.
// `stem` is the prefab's file name without .et: the vanilla name a plan cites.
export function classify(prefab) {
  const path = String(prefab || "").replace(/^\{[0-9A-Fa-f]{16}\}/, "");
  const stem = path.split("/").pop().replace(/\.et$/i, "");
  if (!path) return { group: "other", type: "unknown", stem, civic: false, home: false, ruin: false };
  if (/\/Vegetation\/|\/Forest\//i.test(path)) return { group: "vegetation", type: "vegetation", stem, civic: false, home: false, ruin: false };
  if (/\/Structures\/Walls\/|\/Fence|Fence_/i.test(path)) return { group: "wall", type: "wall-or-fence", stem, civic: false, home: false, ruin: false };
  if (/\/Infrastructure\/(Roads|Pavements)\//i.test(path)) return { group: "road-part", type: "road-part", stem, civic: false, home: false, ruin: false };
  for (const [re, kind] of PROP_RULES) if (re.test(path)) return { group: "prop", type: kind, stem, civic: false, home: false, ruin: false };
  if (/\/Structures\/(BuildingParts|BuildingAddons|Debris)\/|\/Dst\/|\/Furniture\/|Probe_/i.test(path)) return { group: "other", type: "building-part", stem, civic: false, home: false, ruin: false };
  for (const [re, type, f] of BUILDING_RULES) {
    if (re.test(path)) return { group: COUNTED_TYPES.has(type) ? "infrastructure" : "building", type, stem, civic: !!f.civic, home: !!f.home, ruin: !!f.ruin };
  }
  return { group: "other", type: "other", stem, civic: false, home: false, ruin: false };
}

// The rules as a printable table (tools\cli scout types).
export function typeTable() {
  const rows = BUILDING_RULES.map(([re, type, f]) => ({ type, kind: "building", civic: !!f.civic, home: !!f.home, path: re.source.replace(/\\\//g, "/").replace(/\\/g, "") }));
  const aliases = Object.entries(TYPE_ALIASES).map(([alias, to]) => ({ type: alias, kind: "alias", civic: false, home: false, path: `= ${to.join(", ")}` }));
  const props = PROP_RULES.map(([re, kind]) => ({ type: kind, kind: "prop", civic: false, home: false, path: re.source.replace(/\\\//g, "/").replace(/\\/g, "") }));
  return [...rows, ...aliases, ...props];
}
