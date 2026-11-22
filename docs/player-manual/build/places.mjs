// docs/player-manual/build/places.mjs - the places the player manual's maps and legends are built from.
//
// Every position is read from the MainWorld layers (the way the in-game map finds it), so the maps follow the
// world when a layer moves. Every name is the in-game map label from the pack string table
// (LF-MapPoi_*), or the sign over the door when the map has no label for that place.
import { dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { loadPlaces } from "../../../tools/map/lib/places.mjs";
import { layerFiles, readLayers } from "../../../tools/map/lib/layers.mjs";
import { loadStrings } from "../../../tools/map/lib/overlay.mjs";

export const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "../../..");
const text = loadStrings(ROOT);
const placeRows = loadPlaces(ROOT, { world: "main" }).places;
const entities = readLayers(ROOT, layerFiles(resolve(ROOT, "addons/LF_Everon1989/Worlds/MainWorld/MainWorld_Layers")));

/** In-game text of a string key (without the #); throws when the key is unknown, so a rename fails the build. */
export function T(key) {
  const v = text(key);
  if (!v) throw new Error(`manual: unknown string key ${key}`);
  return v;
}

/** [x, z] of a registered place by kind and id. */
export function P(kind, id) {
  const p = placeRows.find((r) => r.kind === kind && r.id === id);
  if (!p) throw new Error(`manual: no place ${kind} ${id}`);
  return [p.x, p.z];
}

/** [x, z] of a layer entity by (unique) name suffix. */
export function E(suffix) {
  const hits = entities.filter((e) => e.name === suffix || e.name.endsWith(suffix));
  if (hits.length !== 1) throw new Error(`manual: ${hits.length} entities match ${suffix}`);
  return [hits[0].xz[0], hits[0].xz[1]];
}

/** Every entity whose name starts with a prefix, as { name, xz }. */
export function entitiesNamed(prefix) {
  return entities.filter((e) => e.name.startsWith(prefix)).map((e) => ({ name: e.name, xz: [e.xz[0], e.xz[1]] }));
}

export const GROUPS = {
  civic: { name: "Services", color: [36, 84, 160] },
  law: { name: "Police and jail", color: [110, 50, 150] },
  shop: { name: "Shops", color: [205, 115, 20] },
  work: { name: "Work sites", color: [40, 130, 60] },
  crime: { name: "Off the public map (crime)", color: [175, 40, 40] },
  home: { name: "Homes and plots for sale", color: [140, 95, 40] },
  town: { name: "Towns", color: [60, 60, 60] },
};

// One row per numbered place on the Saint-Philippe maps. `what` is the one line the legend shows.
// ATM rows and the three cash van stops repeat a name on purpose: each is its own spot.
export function townPlaces() {
  const rows = [
    // services
    ["civic", T("LF-MapPoi_Hostel"), P("HostelBed", "saint-philippe.hostel"), "Where a new character wakes. Safe zone around it."],
    ["civic", T("LF-MapPoi_Clerk"), P("Clerk", "saint-philippe.arrival"), "Town hall: ID card, driving licence, the arrival quest."],
    ["civic", T("LF-MapPoi_NoticeBoard"), P("TownBoard", "saint-philippe.board"), "The town board: every place in town, with a waypoint button."],
    ["civic", T("LF-MapPoi_ParcelDepot") + " (post office)", P("ParcelDepot", "saint-philippe.parcel"), "Sign up for courier work, take parcels."],
    ["civic", T("LF-MapPoi_Bank"), P("Bank", "saint-philippe.bank"), "Teller for deposits and withdrawals."],
    ["civic", T("LF-MapPoi_Atm") + " (by the bank)", P("ATM", "atm.saint-philippe.b79"), "Cash machine on the bank wall."],
    ["civic", T("LF-MapPoi_Atm") + " (east side)", P("ATM", "atm.saint-philippe.b97"), "Cash machine near the filling station."],
    ["civic", T("LF-MapPoi_Atm") + " (harbour)", P("ATM", "atm.saint-philippe-dock.b08"), "Cash machine on the dock."],
    ["civic", T("LF-MapPoi_Hospital") + " (hospital)", P("HospitalSite", "saint-philippe.clinic"), "Medic duty desk, hospital till, where you wake after dying."],
    ["civic", T("LF-MapPoi_TaxiRank"), E("saint_philippe_taxi_job_taxi_pad"), "Start a taxi run."],
    ["civic", T("LF-MapPoi_CarLot"), P("VehicleLot", "saint-philippe.carlot"), "Buy a car. Impound desk beside it."],
    ["civic", T("LF-MapPoi_FillingStation"), P("FuelTill", "saint-philippe.fuel"), "Pumps and the fuel kiosk."],
    ["civic", T("LF-MapPoi_Garage"), P("Garage", "saint-philippe-districts.garage"), "Mechanic: car parts and repair kits."],
    ["civic", T("LF-MapPoi_Church"), P("Church", "saint-philippe.church"), "The village church."],
    ["civic", T("LF-Sign_OrgRegistry_Title"), P("OrgDesk", "saint-philippe.org"), "Register or join an organization."],
    ["civic", T("LF-MapPoi_Range"), P("RangeLane", "saint-philippe.range"), "Three target lanes."],
    ["civic", T("LF-MapPoi_Market"), P("MarketTrader", "saint-philippe.market"), "Market stall trader."],
    ["civic", T("LF-Sign_HarbourOffice_Title"), P("HarbourMaster", "saint-philippe.harbour"), "The harbour master."],
    // police
    ["law", T("LF-MapPoi_PoliceStation").replace("{Police}", "Police"), P("PoliceStation", "saint-philippe.police"), "Duty desk, armoury, wanted board, jail cell."],
    // shops
    ["shop", T("LF-MapPoi_Supermarket"), P("StoreCounter", "saint-philippe.supermarket"), "Food and drink."],
    ["shop", T("LF-MapPoi_GeneralStore"), P("StoreCounter", "saint-philippe.general-store"), "Tinned food, seeds, drinks, miner's till."],
    ["shop", T("LF-MapPoi_Bakery"), P("StoreCounter", "saint-philippe.bakery"), "Bread, baguettes, croissants."],
    ["shop", T("LF-MapPoi_Butcher"), P("StoreCounter", "saint-philippe.butcher"), "Ham, saucisson, canned and dried meat."],
    ["shop", T("LF-MapPoi_Cafe"), P("CafeTable", "saint-philippe.cafe"), "Drinks and snacks. Chess table next door."],
    ["shop", T("LF-MapPoi_Newsagent"), P("StoreCounter", "saint-philippe.newsagent"), "Tourist map, newspaper, postcards."],
    ["shop", T("LF-MapPoi_Pharmacy"), P("StoreCounter", "saint-philippe.pharmacy"), "Aspirin, gauze, tourniquets."],
    ["shop", T("LF-MapPoi_ClothingStore"), P("ClothingCounter", "saint-philippe.clothing"), "Clothes, hats, jackets."],
    ["shop", T("LF-MapPoi_Hardware"), P("StoreCounter", "saint-philippe.hardware"), "Axe, shovel, planks, cement, radio, repair kit."],
    ["shop", T("LF-MapPoi_TimberMerchant"), P("TimberCounter", "saint-philippe.merchant-timber"), "Sells your planks and hardwood boards."],
    ["shop", T("LF-MapPoi_SeedFeed"), P("StoreCounter", "saint-philippe.seed-feed"), "Fishing rod, bait, seeds."],
    ["shop", T("LF-MapPoi_FarmSupply"), P("StoreCounter", "saint-philippe.farm-supply"), "The reaper and beehive kit."],
    ["shop", T("LF-MapPoi_Furniture"), P("StoreCounter", "saint-philippe.furniture"), "Furniture for your home."],
    ["shop", T("LF-MapPoi_GunStore"), P("GunCounter", "saint-philippe.gun-counter"), "Firearm permit, pistols, rifles, ammo."],
    ["shop", T("LF-MapPoi_Barber"), P("BarberChair", "saint-philippe.barber"), "Haircut."],
    // work
    ["work", T("LF-MapPoi_WheatField"), P("WheatField", "saint-philippe.wheatfield"), "Harvest contracts and the weighbridge."],
    ["work", T("LF-MapPoi_Farm"), P("FarmPlot", "saint-philippe.farm"), "Vegetable plots, beehives, honey kitchen."],
    ["work", T("LF-MapPoi_Orchard"), P("Orchard", "saint-philippe.orchard"), "Farmer job desk and apple trees."],
    ["work", T("LF-MapPoi_TimberYard"), P("Workplace", "saint-philippe.woodlot"), "Lumberjack job desk and sawmill."],
    ["work", T("LF-MapPoi_Smokehouse"), P("Smokehouse", "saint-philippe-dock.smokehouse"), "Smoke your fish."],
    ["work", T("LF-MapPoi_Wholesale"), P("WholesaleCounter", "saint-philippe-dock.wholesale"), "Buys bulk goods at a falling price."],
    ["work", T("LF-MapPoi_FishingSpot") + " (Spring Pond)", P("FishingSpot", "saint-philippe.pond"), "Fish from the jetty."],
  ];
  return rows.map(([group, name, xz, what]) => ({ group, name, x: xz[0], z: xz[1], what }));
}

/** The crime markers: not on the public map, found by asking around. */
export function crimePlaces() {
  const rows = [
    ["law", "Police station", P("PoliceStation", "saint-philippe.police"), "Robbers want it far away. Officers want it close."],
    ["crime", T("LF-MapPoi_Fence") + " (chop bench)", P("FenceYard", "saint-philippe.fence"), "Fence, lockpicks, drills. Chop stolen cars."],
    ["crime", T("LF-MapPoi_Bank") + " (vault)", P("Bank", "saint-philippe.bank"), "Bank heist: staff door, vault, getaway."],
    ["crime", "Cash van stop 1", P("CashVanStop", "saint-philippe.cashvan#0"), "The cash van parks here for 5 minutes at a time."],
    ["crime", "Cash van stop 2", P("CashVanStop", "saint-philippe.cashvan#1"), "Next stop on the van's schedule."],
    ["crime", "Cash van stop 3", P("CashVanStop", "saint-philippe.cashvan#2"), "Next stop on the van's schedule."],
    ["crime", T("LF-MapPoi_FillingStation") + " (till)", P("FuelTill", "saint-philippe.fuel"), "Hold up the attendant for 150."],
    ["crime", T("LF-MapPoi_Supermarket") + " (till)", P("StoreCounter", "saint-philippe.supermarket"), "Till robbery, 120 to 180."],
    ["crime", "Buyer spot (harbour)", E("saint_philippe_dock_buyer_spots_drop_b22"), "Where a buyer takes goods that cannot be sold in a shop."],
    ["crime", "Buyer spot (east)", E("saint_philippe_districts_buyer_spots_drop_b21"), "Same, east side."],
    ["crime", "Buyer spot (north-west)", E("saint_philippe_districts_buyer_spots_drop_b64"), "Same, west side."],
  ];
  return rows.map(([group, name, xz, what]) => ({ group, name, x: xz[0], z: xz[1], what }));
}

const HOUSE_TIERS = {
  "Cottage": [4000, "Cottage"],
  "Village house": [6000, "Village house"],
  "Family house": [8000, "Family house"],
  "Town house": [10000, "Town house"],
  "Farmhouse": [13000, "Farmhouse"],
  "Villa": [20000, "Villa"],
};

/** Homes for sale: id, tier, listed price (property overlay) and position (the For sale sign). */
export function homes(listed) {
  const signs = entitiesNamed("HomeSign_saint_philippe_");
  return listed.map((h) => {
    const code = h.id.split(".").pop().replace("-", "_");
    const sign = signs.find((s) => s.name === `HomeSign_saint_philippe_${code}`);
    if (!sign) throw new Error(`manual: no sign for ${h.id}`);
    return { ...h, code: h.id.split(".").pop().toUpperCase(), x: sign.xz[0], z: sign.xz[1], tier: HOUSE_TIERS[h.tier] ? h.tier : h.tier };
  });
}

/** Plots for sale (build sites). */
export function plots(listed) {
  return listed.map((p) => {
    const xz = P("PlotForSale", p.id);
    return { ...p, x: xz[0], z: xz[1] };
  });
}

// The island: towns come from the world's catalogue, sites from the layers.
export function islandPlaces() {
  const rows = [
    ["town", "Saint-Philippe", P("Clerk", "saint-philippe.arrival"), "Spawn, shops, police, clinic, bank, homes. Everything starts here."],
    ["town", "Meaux", [4520, 9468], "Village on the road south. Nothing built for players yet."],
    ["town", "Montignac", [4773, 7095], "Town with a notice board at the fuel depot. No shops yet."],
    ["town", "Lamentin", [1280, 5941], "Western town. Scenery only."],
    ["town", "Morton", [5135, 4012], "Northern town. Scenery only."],
    ["town", "Saint Pierre", [9689, 1558], "Far north-east town. Scenery only."],
    ["work", T("LF-MapPoi_Sawmill"), P("Workplace", "everon-sawmill.yard"), "The island sawmill: lumberjack desk, two saws, boards."],
    ["work", T("LF-MapPoi_FishingSpot") + " (Fisherman's Bay)", P("FishingSpot", "fishermansbay.cove"), "Sheltered cove with a bench and lantern. Gaspard tells you about it."],
    ["work", T("LF-MapPoi_FishingSpot") + " (Black Lake)", P("FishingSpot", "blacklake.shore"), "A lake shore far to the north-east."],
    ["work", T("LF-MapPoi_Scrapyard"), P("ScrapRecycler", "scrapyard.recycler"), "Scrap piles and the recycler bench."],
    ["work", T("LF-MapPoi_FuelDepot"), P("FuelDepot", "montignac.depot"), "Collective-farm fuel depot. Closed to players for now."],
    ["work", "Silo Farm", P("RoadFarm", "roadfarm.a"), "Roadside farm with a payphone."],
    ["work", "Fuel station (roadside)", P("RoadFuel", "roadfuel.fuel"), "Roadside fuel stop with a payphone."],
    ["crime", "Barn compound", P("ChopYard", "chopyard.yard"), "Chop yard and the moonshine still barn."],
    ["crime", T("LF-MapPoi_ContestedSite"), E("hilltopmast_a_site"), "Hilltop mast: a hideout organizations fight over."],
    ["crime", T("LF-MapPoi_ContestedSite") + " (farm)", E("farmcompound_compound_site_centre"), "Farm compound: a second hideout."],
  ];
  return rows.map(([group, name, xz, what]) => ({ group, name, x: xz[0], z: xz[1], what }));
}
