// docs/player-manual/build/maps.mjs - renders every map of the player manual into
// website/public/docs-images/player-manual/ and returns the legends the pages and the HTML print.
//
//   node docs/player-manual/build/maps.mjs            (or: node docs/player-manual/build.mjs, which also builds the pages)
//
// Needs the offline map cache ("tools\cli map status"); no game and no Workbench.
import { mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { parseConf, findBlocks, str, num } from "../../../tools/map/lib/conf.mjs";
import { drawMap as realDraw } from "./draw.mjs";
import { ROOT, townPlaces, crimePlaces, islandPlaces, homes, plots, P, E, T } from "./places.mjs";

export const IMG_DIR = join(ROOT, "website/public/docs-images/player-manual");

function readProperty() {
  const houses = findBlocks(parseConf(readFileSync(join(ROOT, "addons/LF_Everon1989/Configs/Property/LF_Property_Everon1989/LF_Property_Homes_SaintPhilippe.conf"), "utf8")), "LF_PropertyHouseDef")
    .map((b) => ({ id: str(b.props.m_sHouseId), tier: str(b.props.m_sTierLabel), listed: num(b.props.m_iPrice) }));
  const plotRows = findBlocks(parseConf(readFileSync(join(ROOT, "addons/LF_Everon1989/Configs/Property/LF_Property_Everon1989.conf"), "utf8")), "LF_PropertyPlotDef")
    .map((b) => ({ id: str(b.props.m_sPlotId), tier: str(b.props.m_sTierId), listed: num(b.props.m_iPrice) }));
  const flagship = JSON.parse(readFileSync(join(ROOT, "server/flagship/config.json"), "utf8"));
  const percent = flagship.houses?.salePercent ?? 100;
  const alpha = (v) => Math.floor((v * percent) / 100);
  return { houses: houses.map((h) => ({ ...h, price: alpha(h.listed) })), plots: plotRows.map((p) => ({ ...p, price: alpha(p.listed) })), percent };
}

// The short text beside a marker; the legend keeps the full in-game name.
function shortName(name) {
  if (name.startsWith("Registry clerk")) return "Town hall";
  if (name.startsWith("Parcel depot")) return "Post office";
  if (name.startsWith("Clinic")) return "Clinic";
  if (name.startsWith("ATM")) return "ATM";
  if (name.startsWith("Timber yard")) return "Timber yard";
  if (name.startsWith("Sawmill")) return "Sawmill";
  if (name.startsWith("Scrapyard")) return "Scrapyard";
  if (name.startsWith("Spring")) return "Spring Pond";
  const m = name.match(/^Fishing spot \((.+)\)$/);
  if (m) return m[1];
  if (name === "Organization office") return "Org office";
  if (name.startsWith("Contested ground (farm")) return "Farm hideout";
  if (name === "Contested ground") return "Mast hideout";
  if (name.startsWith("Fuel station (roadside")) return "Roadside fuel";
  return name;
}
const numbered = (rows, start = 1) => { return rows.map((r, i) => ({ ...r, n: r.n ?? start + i, label: r.label || shortName(r.name), noLabel: r.group === "town" && r.name !== "Saint-Philippe" })); };

/** Everything the pages need: the rendered map files and the legends. */
export async function buildMaps({ log = () => {}, render = true } = {}) {
  // render: false rebuilds only the legends and leaves the PNGs on disk as they are.
  const drawMap = render ? realDraw : async () => ({ png: null });
  if (render) mkdirSync(IMG_DIR, { recursive: true });
  const out = {};
  const save = (name, png) => { if (!png) return; writeFileSync(join(IMG_DIR, name), png); log(`  ${name} ${Math.round(png.length / 1024)} KB`); };

  // ---- Saint-Philippe: one numbering for the overview and the centre map.
  const town = numbered(townPlaces());
  const townBox = [4270, 10520, 4780, 11260];
  const overview = await drawMap({ bbox: townBox, scale: 2, title: "Saint-Philippe", markers: town, radius: 8, textScale: 1, labelAll: true, maxLabelChars: 22 });
  save("saint-philippe.png", overview.png);
  const centreBox = [4300, 10560, 4660, 10880];
  const centre = await drawMap({ bbox: centreBox, scale: 3, title: "Saint-Philippe centre", markers: town, radius: 10, textScale: 1, labelAll: true, maxLabelChars: 24 });
  save("saint-philippe-centre.png", centre.png);
  out.town = town;

  // ---- Homes and plots for sale.
  const prop = readProperty();
  const homeRows = homes(prop.houses);
  const plotRows = plots(prop.plots);
  const homeMarkers = [
    ...homeRows.map((h, i) => ({ ...h, n: i + 1, group: "home", name: `${h.code} ${h.tier}`, label: `${h.code} ${(h.price / 1000).toFixed(h.price % 1000 ? 1 : 0)}K` })),
    ...plotRows.map((p, i) => ({ ...p, n: homeRows.length + i + 1, group: "home", name: `Plot ${p.id.split(".").pop().toUpperCase()}`, label: `PLOT ${p.id.split(".").pop().toUpperCase()} ${(p.price / 1000).toFixed(p.price % 1000 ? 1 : 0)}K` })),
  ];
  const homesMap = await drawMap({ bbox: [4290, 10520, 4780, 11000], scale: 2.1, title: "Homes and plots for sale", markers: homeMarkers, radius: 9, textScale: 1, labelAll: true, maxLabelChars: 16 });
  save("homes.png", homesMap.png);
  out.homes = homeMarkers;
  out.propertyPercent = prop.percent;

  // ---- Crime and police.
  const crime = numbered(crimePlaces());
  const crimeMap = await drawMap({ bbox: [4290, 10540, 4780, 11040], scale: 2, title: "Crime and police: Saint-Philippe", markers: crime, radius: 9, textScale: 1, labelAll: true, maxLabelChars: 24 });
  save("crime-town.png", crimeMap.png);
  out.crime = crime;

  // ---- Island overview.
  const island = numbered(islandPlaces());
  const islandMap = await drawMap({ bbox: [400, 800, 11000, 11600], scale: 0.13, title: "Everon island", markers: island, radius: 10, textScale: 1, labelAll: true, maxLabelChars: 22, quant: 3 });
  save("island.png", islandMap.png);
  out.island = island;

  // ---- Job-area insets: the same places the pages name, closer in.
  const insets = {};
  const inset = async (key, title, bbox, scale, markers, opts = {}) => {
    const rows = numbered(markers, 60);
    const m = await drawMap({ bbox, scale, title, markers: rows, radius: opts.radius || 9, textScale: opts.textScale || 1, labelAll: true, maxLabelChars: 26, key: opts.key });
    save(`${key}.png`, m.png);
    insets[key] = { title, rows };
  };
  const pick = (names) => town.filter((t) => names.some((n) => t.name.startsWith(n)));
  const g = (name, what, xz, group = "work") => ({ group, name, x: xz[0], z: xz[1], what });
  await inset("farm-timber", "Fields, timber yard and the range", [4290, 10450, 4590, 10700], 3, [
    ...pick([T("LF-MapPoi_WheatField"), T("LF-MapPoi_Farm"), T("LF-MapPoi_FarmSupply"), T("LF-MapPoi_TimberYard"), T("LF-MapPoi_Hardware"), T("LF-MapPoi_TimberMerchant"), T("LF-MapPoi_SeedFeed"), T("LF-MapPoi_Range")]),
    g("Wheat field (cutting area)", "The ripe wheat you cut with the reaper.", E("saint_philippe_wheatfield_wheat_field_field")),
    g("Weighbridge", "Unload the reaper hopper here: Sell grain or Bag grain.", E("saint_philippe_wheatfield_wheat_field_weighbridge")),
    g("Harvest contracts board", "Post, take and return harvest contracts.", E("saint_philippe_wheatfield_wheat_field_board")),
  ]);
  await inset("sawmill", "Everon sawmill", [2960, 5140, 3220, 5340], 2.4, [
    g(T("LF-MapPoi_Sawmill"), "Lumberjack desk and two saws inside the hall.", P("Workplace", "everon-sawmill.yard")),
    g("Lumberjack desk", "Sign on as a lumberjack.", E("everon_sawmill_yard_source_sawmill_desk")),
  ], { key: false });
  await inset("pond", "Spring Pond", [4580, 10460, 4740, 10600], 4, [
    ...pick([T("LF-MapPoi_FishingSpot") + " (Spring Pond)"]),
  ], { key: false });
  await inset("bay", "Fisherman's Bay", [3930, 10630, 4110, 10770], 4, [
    g(T("LF-MapPoi_FishingSpot") + " (Fisherman's Bay)", "Sheltered cove: mackerel, bass and more.", P("FishingSpot", "fishermansbay.cove")),
  ], { key: false });
  await inset("lake", "Black Lake", [9430, 2930, 9610, 3070], 4, [
    g(T("LF-MapPoi_FishingSpot") + " (Black Lake)", "Quiet lake shore with a bench and a lantern.", P("FishingSpot", "blacklake.shore")),
  ], { key: false });
  await inset("harbour", "Harbour and dock", [4370, 11000, 4570, 11250], 3.5, [
    ...pick([T("LF-MapPoi_Smokehouse"), T("LF-MapPoi_Wholesale"), T("LF-Sign_HarbourOffice_Title"), T("LF-MapPoi_Atm") + " (harbour)"]),
  ]);
  await inset("scrapyard", "Scrapyard", [4930, 10790, 5110, 10930], 4, [
    g(T("LF-MapPoi_Scrapyard"), "Search the scrap piles; the recycler bench is in the yard.", P("ScrapRecycler", "scrapyard.recycler")),
    g("Recycler bench", "Break car parts and scrap into metal.", E("scrapyard_recycler_recycler_bench")),
  ]);
  await inset("barn", "Barn compound", [5290, 9830, 5440, 9970], 4.5, [
    g("Chop yard", "Chop stolen cars (a back-country bench).", E("chopyard_yard_chop_shop"), "crime"),
    g("Still barn: mash barrels", "Mash plums and cabbage.", E("stillbarn_1_source_still_barn_mash_1"), "crime"),
    g("Still barn: the still", "Distill the mash into moonshine.", E("stillbarn_1_source_still_barn_still"), "crime"),
    g("Still barn: packing bench", "Pack bottles into crates.", E("stillbarn_1_source_still_barn_pack"), "crime"),
    g("Still barn: laundering", "Wash marked cash, keep 65 percent.", E("stillbarn_1_source_still_barn_launder"), "crime"),
  ]);
  out.insets = insets;
  return out;
}

if (import.meta.url === `file:///${process.argv[1].replace(/\\/g, "/")}`) {
  const r = await buildMaps({ log: (m) => console.log(m) });
  console.log(`maps ok: ${r.town.length} town places, ${r.homes.length} homes and plots, ${Object.keys(r.insets).length} insets`);
}
