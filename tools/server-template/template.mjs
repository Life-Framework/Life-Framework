// The example server template (#295): server/example/{server.json,config.json,start.cmd,start.sh,README.md}.
// A host copies the folder, points start.cmd at their Reforger server, and boots the core and the pack.
//
// This module is the static half: templateProblems() checks the folder with no game, and
// templateLaunch() reads server.json into the -server / -addons a boot needs (the session's
// `--mission template` uses it, so the proof boots what the file says). The engine half is
// `tools\cli server-template check --engine`, which has the dedicated server read server.json itself.
import { existsSync, readdirSync, readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { checkText } from "../config/check.mjs";
import { ADDONS, CORE } from "../lib/addons.mjs";

const REPO = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
export const TEMPLATE_DIR = join(REPO, "server", "example");
// A variant is the same server.json with only game.mods and game.scenarioId swapped (#297): the core
// alone, or the core and another pack. Fixtures for the harness; the owner swaps the same two entries.
export const VARIANT_DIR = join(REPO, "tools", "server-template", "variants");
export const FILES = ["server.json", "config.json", "start.cmd", "start.sh", "README.md"];

const read = (root, f) => readFileSync(join(root, "server", "example", f), "utf8");

function jsonOf(text, label, out) {
  try {
    return JSON.parse(text);
  } catch (e) {
    out.push(`${label} is not valid JSON: ${e.message}`);
    return null;
  }
}

// "template" boots server/example; "template-<variant>" boots tools/server-template/variants/<variant>/server.json.
export const isTemplateMission = (m) => m === "template" || /^template-[a-z0-9-]+$/.test(m ?? "");
export const templateVariant = (m) => (m === "template" ? "" : String(m).replace(/^template-/, ""));
export function variants(root = REPO) {
  const dir = join(root, "tools", "server-template", "variants");
  return existsSync(dir) ? readdirSync(dir, { withFileTypes: true }).filter((e) => e.isDirectory() && existsSync(join(dir, e.name, "server.json"))).map((e) => e.name) : [];
}
const jsonPath = (root, variant) => (variant ? join(root, "tools", "server-template", "variants", variant, "server.json") : join(root, "server", "example", "server.json"));

// The addon a modId names: a repo addon with that project GUID (the core, the flagship, any `pack new` scaffold).
export function addonByGuid(root, guid) {
  const known = ADDONS.find((x) => x.guid === guid);
  if (known) return known;
  const dir = join(root, "addons");
  if (!existsSync(dir)) return null;
  for (const e of readdirSync(dir, { withFileTypes: true })) {
    const gp = join(dir, e.name, `${e.name}.gproj`);
    if (e.isDirectory() && existsSync(gp) && readFileSync(gp, "utf8").includes(`GUID "${guid}"`)) return { name: e.name, guid };
  }
  return null;
}

// The scenario and the addon names a server.json asks for, in its order. Throws when it names an addon the repo has not got.
export function templateLaunch(root = REPO, variant = "") {
  const sj = JSON.parse(readFileSync(jsonPath(root, variant), "utf8"));
  const addons = sj.game.mods.map((m) => {
    const a = addonByGuid(root, m.modId);
    if (!a) throw new Error(`${variant ? `variant ${variant}` : "server/example"} server.json lists mod ${m.modId}, which is not an addon in this checkout`);
    return a.name;
  });
  return { scenario: sj.game.scenarioId, addons };
}

// What any pack swap must keep true: the core first, every other mod an addon of this checkout (when
// it is there), and the scenario a header inside one of the listed addons. Pack-agnostic.
function launchProblems(root, sj, label, out) {
  const g = sj.game ?? {};
  const mods = g.mods ?? [];
  const core = ADDONS[0];
  if (mods[0]?.modId !== core.guid) out.push(`${label} game.mods must start with the core (${core.guid}); it lists ${mods.map((m) => m.modId).join(",")}`);
  if (new Set(mods.map((m) => m.modId)).size !== mods.length) out.push(`${label} game.mods lists a mod twice`);
  const m = /^\{([0-9A-F]{16})\}(Missions\/[^/]+\.conf)$/.exec(g.scenarioId ?? "");
  if (!m) {
    out.push(`${label} game.scenarioId ${g.scenarioId} is not {GUID}Missions/<name>.conf`);
    return;
  }
  const names = [];
  for (const mod of mods) {
    const a = addonByGuid(root, mod.modId);
    if (a) names.push(a.name);
    else if (existsSync(join(root, "addons"))) out.push(`${label} game.mods lists ${mod.modId}, which no addon in this checkout carries`);
  }
  for (const a of ADDONS) {
    const gp = join(root, "addons", a.name, `${a.name}.gproj`);
    if (mods.some((x) => x.modId === a.guid) && existsSync(gp) && !readFileSync(gp, "utf8").includes(`GUID "${a.guid}"`)) out.push(`${a.name}.gproj does not carry the GUID ${a.guid} the template names`);
  }
  const meta = names.map((n) => join(root, "addons", n, `${m[2]}.meta`)).find((f) => existsSync(f));
  if (!meta) out.push(`${label} game.scenarioId ${g.scenarioId} is in none of the listed addons (${names.join(",")})`);
  else if (!readFileSync(meta, "utf8").includes(`{${m[1]}}`)) out.push(`${label} game.scenarioId GUID ${m[1]} does not match ${m[2]}.meta`);
}

// A variant differs from server.json in game.mods and game.scenarioId only: same ports, engine settings, name.
function variantProblems(root, variant, base, out) {
  const label = `variant ${variant}`;
  const sj = jsonOf(readFileSync(jsonPath(root, variant), "utf8"), `${label} server.json`, out);
  if (!sj || !base) return;
  const strip = (x) => JSON.stringify({ ...x, game: { ...x.game, mods: undefined, scenarioId: undefined } });
  if (strip(sj) !== strip(base)) out.push(`${label} server.json differs from server/example/server.json in more than game.mods and game.scenarioId`);
  launchProblems(root, sj, `${label} server.json`, out);
}

const isPort = (n) => Number.isInteger(n) && n >= 1 && n <= 65535;

// Strings for everything the template gets wrong; empty means it is a ready folder. No game needed.
export function templateProblems(root = REPO) {
  const out = [];
  for (const f of FILES) if (!existsSync(join(root, "server", "example", f))) out.push(`server/example/${f} is missing`);
  if (out.length) return out;

  const sj = jsonOf(read(root, "server.json"), "server.json", out);
  if (sj) {
    const g = sj.game ?? {};
    // The engine's schema: an empty a2s address is refused at boot ("does not match the required pattern").
    for (const k of ["bindAddress", "publicAddress"]) if (sj[k] !== "" && !/^\d{1,3}(\.\d{1,3}){3}$/.test(sj[k] ?? "")) out.push(`server.json ${k} must be empty or an IPv4 address`);
    if (!/^\d{1,3}(\.\d{1,3}){3}$/.test(sj.a2s?.address ?? "")) out.push(`server.json a2s.address "${sj.a2s?.address}" is not an IPv4 address; the engine refuses the file`);
    for (const [k, v] of [["bindPort", sj.bindPort], ["publicPort", sj.publicPort], ["a2s.port", sj.a2s?.port]]) if (!isPort(v)) out.push(`server.json ${k} is not a port`);
    if (sj.bindPort === sj.a2s?.port) out.push("server.json bindPort and a2s.port are the same");
    if (!g.name) out.push("server.json game.name is empty");
    if (!g.passwordAdmin || /\s/.test(g.passwordAdmin)) out.push("server.json game.passwordAdmin must be one word (CHANGE-ME until the host sets it)");
    launchProblems(root, sj, "server.json", out);
  }
  for (const v of variants(root)) variantProblems(root, v, sj, out);

  const cfgText = read(root, "config.json");
  const preset = join(root, "tools", "test", "config-fixtures", "presets", "flagship-1989.json");
  if (existsSync(preset) && cfgText !== readFileSync(preset, "utf8")) out.push("config.json differs from the flagship-1989 preset (tools/test/config-fixtures/presets); copy the preset back");
  try {
    for (const p of checkText(cfgText, { source: "config.json" }).problems ?? []) if (p.level === "error") out.push(`config.json ${p.line}:${p.col} ${p.msg}`);
  } catch (e) {
    out.push(`config.json could not be checked: ${e.message}`);
  }

  for (const f of ["start.cmd", "start.sh"]) {
    const t = read(root, f);
    if (!t.includes("-config") || !t.includes("server.json")) out.push(`${f} does not start the server with -config server.json`);
    if (!t.includes("LifeFramework") || !t.includes("config.json")) out.push(`${f} does not copy config.json into the profile's LifeFramework folder`);
  }
  const readme = read(root, "README.md");
  for (const must of ["start.cmd", "start.sh", "server.json", "config.json", "LF_REFORGER_SERVER"]) if (!readme.includes(must)) out.push(`README.md does not mention ${must}`);
  return out;
}
