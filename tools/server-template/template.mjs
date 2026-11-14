// The example server template (#295): server/example/{server.json,config.json,start.cmd,start.sh,README.md}.
// A host copies the folder, points start.cmd at their Reforger server, and boots the core and the pack.
//
// This module is the static half: templateProblems() checks the folder with no game, and
// templateLaunch() reads server.json into the -server / -addons a boot needs (the session's
// `--mission template` uses it, so the proof boots what the file says). The engine half is
// `tools\cli server-template check --engine`, which has the dedicated server read server.json itself.
import { existsSync, readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { checkText } from "../config/check.mjs";
import { ADDONS, PACK_MISSION } from "../lib/addons.mjs";

const REPO = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
export const TEMPLATE_DIR = join(REPO, "server", "example");
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

// The scenario and the addon names server.json asks for, in its order. Throws when it names an addon the repo has not got.
export function templateLaunch(root = REPO) {
  const sj = JSON.parse(read(root, "server.json"));
  const addons = sj.game.mods.map((m) => {
    const a = ADDONS.find((x) => x.guid === m.modId);
    if (!a) throw new Error(`server/example/server.json lists mod ${m.modId}, which is not a Life Framework addon`);
    return a.name;
  });
  return { scenario: sj.game.scenarioId, addons };
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
    if (g.scenarioId !== PACK_MISSION) out.push(`server.json game.scenarioId is ${g.scenarioId}, the flagship template boots ${PACK_MISSION}`);
    const ids = (g.mods ?? []).map((m) => m.modId);
    if (ids.join() !== ADDONS.map((a) => a.guid).join()) out.push(`server.json game.mods lists ${ids.join(",")}; the template boots the core then the pack (${ADDONS.map((a) => a.guid).join(",")})`);
    for (const a of ADDONS) {
      const gp = join(root, "addons", a.name, `${a.name}.gproj`);
      if (existsSync(gp) && !readFileSync(gp, "utf8").includes(`GUID "${a.guid}"`)) out.push(`${a.name}.gproj does not carry the GUID ${a.guid} the template names`);
    }
  }

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
