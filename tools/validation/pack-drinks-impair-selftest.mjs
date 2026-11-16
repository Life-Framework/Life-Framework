// Proves the flagship's drink impairment overlay (#2003): Configs/Survival/LF_DrinkImpairment_Everon1989.conf
// holds a row for beer and one for moonshine, keyed by each prefab's real GUID (read from the core's
// .meta), moonshine is the strongest of the rows (strength, duration and cap above beer's), and the
// flagship mission header lists the file as a pack overlay. The overlay mechanics are covered in game by
// survival/drink-impairment-table; core Enforce tests cannot name a pack file (validate-addon-boundary),
// so the pack values are checked here.
// red-proof: a fixture copy below where beer is stronger than moonshine, moonshine names a wrong GUID and the header lacks the line prints a FAIL line each.
import { readFileSync } from "node:fs";
import { join } from "node:path";
import { fileURLToPath } from "node:url";

const PACK = "addons/LF_Everon1989";
const CORE = "addons/LifeFramework";
const FILE = `${PACK}/Configs/Survival/LF_DrinkImpairment_Everon1989.conf`;
const LISTED = /Configs\/Survival\/LF_DrinkImpairment_Everon1989\.conf/;
const DRINKS = { beer: "Prefabs/Items/Drinks/BeerBottle.et", moonshine: "Prefabs/Items/Drinks/Moonshine.et" };

export function rowsOf(text) {
  return text.split(/LF_DrinkImpairmentRow\s+"\{[0-9A-F]{16}\}"\s*\{/).slice(1).map((body) => {
    const get = (k) => body.match(new RegExp(String.raw`^\s*${k}\s+(\S+)\s*$`, "m"))?.[1];
    const prefab = get("m_Prefab")?.replaceAll('"', "") ?? "";
    return {
      guid: prefab.match(/^\{([0-9A-F]{16})\}/)?.[1],
      path: prefab.replace(/^\{[0-9A-F]{16}\}/, ""),
      op: get("m_sOp")?.replaceAll('"', ""),
      strength: Number(get("m_fStrength")),
      duration: Number(get("m_fDurationSeconds") ?? 600), // the row class default
      cap: Number(get("m_fCap") ?? 60),
    };
  });
}

// guids: { path: GUID } read from the core's .meta files.
export function drinkProblems(text, guids, headers = []) {
  const out = [];
  if (!/^LF_DrinkImpairmentConfig\s*\{/m.test(text)) out.push("the file is not an LF_DrinkImpairmentConfig");
  const rows = rowsOf(text);
  const byPath = (p) => rows.find((r) => r.path === p);
  const beer = byPath(DRINKS.beer);
  const moon = byPath(DRINKS.moonshine);
  if (!beer) out.push("beer has no row");
  if (!moon) out.push("moonshine has no row");
  for (const r of rows) {
    if (guids[r.path] && r.guid !== guids[r.path]) out.push(`${r.path} is keyed by ${r.guid}, its .meta says ${guids[r.path]}`);
    if (r.op !== "add" && r.op !== "patch" && r.op !== "replace") out.push(`${r.path} has override verb ${r.op}`);
    if (!(r.strength > 0 && r.strength <= 100)) out.push(`${r.path} strength ${r.strength} is outside 1-100`);
    if (!(r.duration > 0)) out.push(`${r.path} duration is not positive`);
    if (!(r.cap > 0 && r.cap <= 100)) out.push(`${r.path} cap ${r.cap} is outside 1-100`);
    if (r.cap < r.strength) out.push(`${r.path} cap ${r.cap} is below one dose (${r.strength})`);
  }
  if (new Set(rows.map((r) => r.guid)).size !== rows.length) out.push("two rows name the same drink");
  if (beer && moon) {
    if (!(moon.strength > beer.strength)) out.push(`moonshine (${moon.strength}) is not stronger than beer (${beer.strength})`);
    if (!(moon.duration > beer.duration)) out.push("moonshine does not last longer than beer");
    if (!(moon.cap > beer.cap)) out.push("moonshine does not stack higher than beer");
    for (const r of rows) if (r.strength > moon.strength) out.push(`${r.path} (${r.strength}) is stronger than moonshine`);
  }
  for (const [name, h] of headers) if (!LISTED.test(h)) out.push(`${name} does not list the drink overlay as a pack overlay`);
  return out;
}

const readText = (p) => readFileSync(p, "utf8");
if (process.argv[1] === fileURLToPath(import.meta.url)) {
  const root = process.argv[2] ?? ".";
  const guids = {};
  for (const p of Object.values(DRINKS)) guids[p] = readText(join(root, CORE, `${p}.meta`)).match(/\{([0-9A-F]{16})\}/)?.[1];
  const headers = ["LF_Everon1989"].map((m) => [`${m}.conf`, readText(join(root, PACK, "Missions", `${m}.conf`))]);
  const real = readText(join(root, FILE));
  const bad = drinkProblems(real, guids, headers);
  // Red proof: beer made stronger than moonshine, moonshine keyed by a wrong GUID, no header line.
  const broken = real.replace("m_fStrength 12", "m_fStrength 50").replace("{5280F09CEB005042}", "{5280F09CEB005043}");
  const red = drinkProblems(broken, guids, [["fixture header", ""]]);
  if (red.length < 3) bad.push(`the red-proof fixture only raised ${red.length} problem(s): ${red.join("; ")}`);
  for (const p of bad) console.log(`FAIL ${p}`);
  console.log(`SUMMARY pack-drinks-impair-selftest ${bad.length ? "fail" : "ok"}`);
  process.exit(bad.length ? 1 : 0);
}
