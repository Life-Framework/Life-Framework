// tools/map/rules/visibility.mjs
//
// The map's POI visibility rule as a pure function (docs/architecture/everon-map.md section 1, card M2).
// Twin of LF_MapPoiRules in addons/LifeFramework/Scripts/Game/Feature/Map/LF_MapPoiRules.c: both run the one
// case table addons/LifeFramework/Configs/Map/Rules/visibility.cases.json (loader: tools/map/rules/index.mjs),
// so they cannot drift.
// red-proof: tools/validation/map-visibility-selftest.mjs breaks a case and the table fails.
//
//   node tools/map/rules/visibility.mjs            run the case table (exit 1 on a mismatch or a stale Enforce copy)
//   node tools/map/rules/visibility.mjs --gen      write the Enforce copy of the table (LF_MapPoiCases.generated.c)
//
// Exports
//   evaluate(input)       the rule the case table runs (index.mjs runCases): input is
//                         { visibility, role, live, known, roles } -> "full" | "faded" | "pencil" | "hidden"
//   visible(poi, persona) what the emulator calls (tools/map/render/render.mjs: rules.visibility.visible):
//                         a POI row plus a persona string (new, known, police, org:<id>) -> the same result
//   visibleList(kinds, places, audience)  the visible list, for the "lists" part of the table
//
// Vocabulary. visibility: PUBLIC | DISCOVER | ROLE | HIDDEN (the POI row). known: none | heard | visited
// (what the player knows of the place). result: hidden (not drawn) | faded (heard of: faded icon, pencil
// "?") | full (ink icon) | pencil (a personal pencil mark of a HIDDEN place the player met).
import { existsSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

export const rule = "visibility";
export const CASES_PATH = "addons/LifeFramework/Configs/Map/Rules/visibility.cases.json";
const here = dirname(fileURLToPath(import.meta.url));
const ROOT = join(here, "..", "..", "..");

const splitList = (v) => (Array.isArray(v) ? v : String(v ?? "").split(",").map((s) => s.trim()).filter(Boolean));

/** The rule. input: { visibility, role, live, known, roles }. */
export function evaluate({ visibility, role = "", live, known = "none", roles = [] }) {
  if (!live) return "hidden";
  switch (visibility) {
    case "PUBLIC":
      return "full";
    case "DISCOVER":
      if (known === "visited") return "full";
      if (known === "heard") return "faded";
      return "hidden";
    case "ROLE":
      return role !== "" && splitList(roles).includes(role) ? "full" : "hidden";
    case "HIDDEN":
      return known === "heard" || known === "visited" ? "pencil" : "hidden";
    default:
      return "hidden";
  }
}

// The emulator's stand-in rows say `access` (PUBLIC, CIVIC, DISCOVER, POLICE, UNDERWORLD, ORG:<id>); the
// game's POI table says `visibility` and `role`. One reading of both.
function rowOf(poi) {
  if (poi.visibility) return { visibility: poi.visibility, role: poi.role ?? "" };
  const acc = poi.access ?? "PUBLIC";
  if (acc === "PUBLIC" || acc === "CIVIC") return { visibility: "PUBLIC", role: "" };
  if (acc === "DISCOVER") return { visibility: "DISCOVER", role: "" };
  if (acc === "POLICE") return { visibility: "ROLE", role: "police" };
  if (acc === "UNDERWORLD") return { visibility: "HIDDEN", role: "" };
  if (acc.startsWith("ORG:")) return { visibility: "ROLE", role: `org:${acc.slice(4)}` };
  return { visibility: "HIDDEN", role: "" };
}

// A persona is what `map render --as` names: the audience the picture is drawn for.
//   new     a fresh player: only places the clerk marked (poi.heardOf) are heard of
//   known   a player who has been everywhere and met the underworld: every place visited
//   police  an officer on duty
//   org:<id> a member of that org; knows the district by hearsay (heard of)
export function audienceOf(persona = "new") {
  const [who, org] = String(persona).split(":");
  if (who === "known") return { roles: [], known: "visited", heardOfFlag: false };
  if (who === "police") return { roles: ["police"], known: "none", heardOfFlag: false };
  if (who === "org") return { roles: [`org:${org}`], known: "heard", heardOfFlag: false };
  return { roles: [], known: "none", heardOfFlag: true };
}

/** What the emulator draws for one POI row and one persona string. */
export function visible(poi, persona = "new") {
  const a = audienceOf(persona);
  const known = a.heardOfFlag ? (poi.heardOf ? "heard" : "none") : a.known;
  return evaluate({ ...rowOf(poi), live: poi.live ?? true, known, roles: a.roles });
}

/**
 * The visible list: places in order whose kind has a row and whose state is not hidden, as `id:state`.
 * kinds: { kind: { visibility, role } }; places: { id: { kind, live } }; audience: { roles, known: { id: state } }.
 */
export function visibleList(kinds, places, audience) {
  const out = [];
  for (const [id, place] of Object.entries(places)) {
    const row = kinds[place.kind];
    if (!row) continue;
    const state = evaluate({
      visibility: row.visibility,
      role: row.role ?? "",
      live: place.live,
      known: audience.known?.[id] ?? "none",
      roles: audience.roles ?? [],
    });
    if (state !== "hidden") out.push(`${id}:${state}`);
  }
  return out;
}

/** Runs a parsed case table. Returns the failure messages (empty when every case passes). */
export function runCases(table) {
  const fails = [];
  for (const c of table.cases ?? []) {
    const got = evaluate(c.input);
    if (got !== c.expect) fails.push(`case "${c.name}": expected ${c.expect}, got ${got}`);
  }
  const lists = table.lists;
  for (const [name, s] of Object.entries(lists?.scenarios ?? {})) {
    const got = visibleList(lists.kinds, lists.places, s).join(",");
    if (got !== s.expect) fails.push(`scenario "${name}": expected [${s.expect}], got [${got}]`);
  }
  return fails;
}

export const GENERATED_PATH = "addons/LifeFramework/Scripts/Game/Feature/Map/LF_MapPoiCases.generated.c";

/**
 * The Enforce copy of the case table. The engine cannot open the loose .json of an addon (the file is not
 * a resource), so LF_Test_MapPois reads these lines instead. `node tools/map/rules/visibility.mjs --gen`
 * writes the file; the default run and the selftest fail when it is out of date, so the json stays the one source.
 */
export function generatedSource(root = ROOT) {
  const NL = String.fromCharCode(10);
  const BS = String.fromCharCode(92);
  const TAB = String.fromCharCode(9);
  const lines = readFileSync(join(root, CASES_PATH), "utf8").split(NL);
  while (lines.length && lines[lines.length - 1] === "") lines.pop();
  const q = (s) => s.split(BS).join(BS + BS).split('"').join(BS + '"');
  return [
    "// GENERATED by tools/map/rules/visibility.mjs --gen from Configs/Map/Rules/visibility.cases.json. Do not edit.",
    "//! The visibility case table as lines, for LF_Test_MapPois (the engine cannot read the loose .json).",
    "class LF_MapPoiCasesData",
    "{",
    TAB + "//------------------------------------------------------------------------------------------------",
    TAB + "static string Json()",
    TAB + "{",
    TAB + TAB + "string text;",
    ...lines.map((l) => TAB + TAB + 'text += "' + q(l) + BS + 'n";'),
    TAB + TAB + "return text;",
    TAB + "}",
    "}",
    "",
  ].join(NL);
}

export function generatedIsCurrent(root = ROOT) {
  const f = join(root, GENERATED_PATH);
  return existsSync(f) && readFileSync(f, "utf8") === generatedSource(root);
}

export function loadCases(root = ROOT) {
  return JSON.parse(readFileSync(join(root, CASES_PATH), "utf8"));
}

if (process.argv[1] && fileURLToPath(import.meta.url) === process.argv[1]) {
  if (!existsSync(join(ROOT, CASES_PATH))) {
    console.error(`visibility: ${CASES_PATH} is missing`);
    process.exit(1);
  }
  if (process.argv.includes("--gen")) {
    writeFileSync(join(ROOT, GENERATED_PATH), generatedSource());
    console.log(`visibility: wrote ${GENERATED_PATH}`);
    process.exit(0);
  }
  const table = loadCases();
  const fails = runCases(table);
  if (!generatedIsCurrent()) fails.push(`${GENERATED_PATH} is out of date: run node tools/map/rules/visibility.mjs --gen`);
  for (const f of fails) console.error(`FAIL ${f}`);
  if (fails.length) process.exit(1);
  console.log(`visibility: ok (${table.cases.length} cases, ${Object.keys(table.lists.scenarios).length} scenarios)`);
}
