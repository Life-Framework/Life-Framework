// Selftest for the emulator's overlay reader (map follow-up to M0 #2227 and M2 #2230): the POI table is per
// place kind with no coordinates, so a POI is a live place read from a world's `.layer` files and joined to a kind row.
// A fixture layer holds three real kinds across the personas, a dark place, a kind with no row, a place that
// inherits its component from a prefab, a $grp group and a nested child. Then the real playtest town is read.
// inputs: tools/map/**, addons/LifeFramework/Configs/Map/**, addons/LF_Everon1989/Configs/Map/**, addons/LifeFramework/Worlds/**, addons/LF_Everon1989/Worlds/**, addons/LifeFramework/Prefabs/**
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { loadOverlay, joinPois, loadKinds } from "../map/lib/overlay.mjs";
import { loadPlaces, isLive } from "../map/lib/places.mjs";
import { visible } from "../map/rules/visibility.mjs";
import { renderMap } from "../map/render/render.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const FIX = join(ROOT, "tools/map/render/fixtures/places.fixture.layer");
const fails = [];
const expect = (cond, msg) => { if (!cond) fails.push(msg); };
const sorted = (v) => (v && typeof v === "object" ? Object.fromEntries(Object.entries(v).sort(([x], [y]) => (x < y ? -1 : 1))) : v);
const same = (a, b, msg) => expect(JSON.stringify(sorted(a)) === JSON.stringify(sorted(b)), `${msg}: expected ${JSON.stringify(b)}, got ${JSON.stringify(a)}`);

// What each persona sees of an overlay: { id: state } for every POI that is not hidden.
const seen = (o, persona) => Object.fromEntries(o.pois.map((p) => [p.id, visible(p, persona)]).filter(([, s]) => s !== "hidden"));

// 1. The fixture layer, live states [1]: state 2 is dark.
const o = loadOverlay(ROOT, { world: FIX, liveStates: [1] });
expect(o.source === `places:${FIX}`, `a world with places reports its source (got ${o.source})`);
expect(o.info.places === 11, `the fixture holds 11 places (got ${o.info.places})`);
same(o.info.unjoined, { CashVan: 1 }, "a kind with no POI row is counted, never drawn");
expect(o.info.dark === 1, `one dark place (got ${o.info.dark})`);
const by = Object.fromEntries(o.pois.map((p) => [p.id, p]));
expect(by.Fix_Hospital?.x === 100 && by.Fix_Hospital?.z === 100, "a place's position comes from its layer entry");
expect(by.Fix_Hospital?.name === "Clinic", `the label is the localised kind name (got ${by.Fix_Hospital?.name})`);
expect(by.Fix_Fence?.visibility === "HIDDEN", "the pack's Fence row joins (a pack kind)");
expect(by.Fix_PoliceDesk?.name === "Workplace (POLICE)" && by.Fix_PoliceDesk?.source === "LF_WorkplaceComponent", "a place whose component comes from its prefab's ancestry is found (the duty desk)");
expect(by.Fix_GroupDesk_A && by.Fix_GroupDesk_B && by.Fix_GroupDesk_A.z === 180 && by.Fix_GroupDesk_B.z === 220, "a $grp group gives each member its own place");
expect(Math.abs(by.Fix_Nested?.x - 310) < 1e-6 && Math.abs(by.Fix_Nested?.z - 300) < 1e-6, `a nested child is placed from its parent, turned by its yaw (got ${by.Fix_Nested?.x},${by.Fix_Nested?.z})`);
expect(by.Fix_Clinic_State2?.live === false && by.Fix_Clinic_State1?.live === true, "live state: 2 is dark and 1 is live under [1]");

// 2. Across the personas. The dark clinic never shows; the cash van has no row.
const publicIds = ["Fix_Hospital", "Fix_Clinic_State1", "Fix_Nested"];
const workplaceIds = ["Fix_Desk", "Fix_PoliceDesk", "Fix_GroupDesk_A", "Fix_GroupDesk_B"];
const full = (ids) => Object.fromEntries(ids.map((i) => [i, "full"]));
same(seen(o, "new"), full(publicIds), "new player: public places only (discover, role and hidden kinds stay off)");
same(seen(o, "known"), { ...full(publicIds), ...full(workplaceIds), Fix_Fence: "pencil" }, "known player: discover places in ink, the fence as a pencil mark, no police armoury");
same(seen(o, "police"), { ...full(publicIds), ...full(workplaceIds), Fix_Armoury: "full", Fix_Fence: "pencil" }, "police: also the armoury (role row)");
same(seen(o, "org:crew"), { ...full(publicIds), ...Object.fromEntries(workplaceIds.map((i) => [i, "faded"])), Fix_Fence: "pencil" }, "org member: discover places heard of (faded)");

// 3. Every state live when the header lists none: the dark place shows again.
const all = loadOverlay(ROOT, { world: FIX, liveStates: [] });
expect(all.info.dark === 0 && seen(all, "new").Fix_Clinic_State2 === "full", "an empty live list makes every state live");
expect(isLive(-1, [1]) && !isLive(2, [1]) && isLive(2, []) && isLive(2, null), "IsLive mirrors LF_PlaceLiveState.IsLive");

// 4. No place with a POI kind: the source says none, with no fixture rows slipped in.
const empty = loadOverlay(ROOT, { world: "main" });
expect(empty.source === "none" && empty.pois.length === 0, `the pack island has no places yet: source none (got ${empty.source}, ${empty.pois.length} POIs)`);
const asked = loadOverlay(ROOT, { fixture: true });
expect(asked.source === "fixture" && asked.pois.length > 0, "--fixture draws the stand-in rows");
const unknown = (() => { try { loadOverlay(ROOT, { world: "nowhere-island" }); return false; } catch { return true; } })();
expect(unknown, "an unknown --world fails loudly");

// 5. The join, as a pure function: a place with no row is skipped.
const kinds = loadKinds(ROOT);
expect(kinds.has("Fence") && kinds.has("Bank") && kinds.get("PoliceArmoury")?.visibility === "ROLE", "kind rows: core rows plus the pack's");
const j = joinPois(kinds, [{ kind: "Bank", id: "b1", x: 1, z: 2, live: true, state: -1 }, { kind: "Nope", id: "n", x: 0, z: 0, live: true, state: -1 }]);
expect(j.pois.length === 1 && j.unjoined.Nope === 1, "joinPois keeps rows with a kind and counts the rest");

// 6. The real playtest town has places with POI kinds (the duty desks and the hospital sites).
const town = loadOverlay(ROOT, { world: "town" });
expect(town.source === "places:town" && town.pois.length > 0, `the playtest town yields real POIs (got ${town.source}, ${town.pois.length})`);
expect(town.pois.some((p) => p.kind === "HospitalSite") && town.pois.some((p) => p.kind === "Workplace"), "town: clinic and workplace POIs");
const places = loadPlaces(ROOT, { world: "town" });
expect(places.places.length >= town.pois.length && places.files.length > 10, "town: places are read from its layer files");

// 7. A render of the fixture layer: personas draw different counts, on the plain grid with no game files.
const r = {};
for (const as of ["new", "known", "police"]) r[as] = await renderMap({ root: ROOT, world: "fixture", files: [FIX], liveStates: [1], as, size: [600, 600], extract: false });
expect(r.new.layer === "plain" && r.new.stats.overlay.startsWith("places:"), "a world that is not main draws on the plain grid from its places");
expect(r.new.stats.pois === 3 && r.known.stats.pois === 8 && r.police.stats.pois === 9, `drawn POIs new/known/police = 3/8/9 (got ${r.new.stats.pois}/${r.known.stats.pois}/${r.police.stats.pois})`);

if (fails.length) { for (const f of fails) console.error(`FAIL ${f}`); console.error(`SUMMARY map-places selftest fail ${fails.length}`); process.exit(1); }
console.log("SUMMARY map-places selftest ok");
