// tools/validation/validate-pack-missions.mjs
// inputs: addons/**
//
// The playtest mission headers live in the pack (#323 batch 1), so the core suite can no longer
// read them (the core never names a pack GUID). This check holds what LF_Test_PlaytestMission
// used to assert about them. For each .conf under addons/LF_Everon1989/Missions/:
//   - World names a .ent whose GUID is a real .meta in the core or the pack;
//   - every {GUID} the header names is a real resource in the core or the pack;
//   - a header listed in Scenarios (m_bShowInScenarioMenu not 0) is an LF_MissionHeader with
//     m_bLF_DevToolsOff 1 and m_bLF_HostStopFile 1, a plain-text m_sName and m_sDescription (the
//     Workshop backend copies them as text), one picture for icon, loading screen and preview,
//     and m_iPlayerCount > 1;
//   - a header hidden from Scenarios says m_bShowInScenarioMenu 0 explicitly;
//   - a listed header's m_sDetails #Key is a row in a core or pack string table, and no two
//     listed headers share a World (the scenario list would show the flagship twice, #374).
// red-proof: 2026-11-14, set m_bLF_DevToolsOff 0 in the pack LF_PlaytestTown.conf; observed
// "FAIL  LF_PlaytestTown.conf: listed header does not close dev tools (m_bLF_DevToolsOff 1)".
// red-proof: 2026-11-14 (#374), unhide the pack LF_PlaytestTown.conf; observed
// "FAIL  LF_PlaytestTown.conf: lists the same World as LF_Everon1989.conf (one scenario entry per world)".
// Also: the built-in fixture run below plants each fault and asserts it is reported; it
// runs before the real check, so a check that stops catching a fault fails validate.

import { existsSync, readdirSync, readFileSync, statSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const field = (text, name) => {
  const m = new RegExp(`^\\s*${name}\\s+(.+?)\\s*$`, "m").exec(text);
  if (!m) return null;
  return m[1].replace(/^"(.*)"$/, "$1");
};

// GUID -> file, from every .meta under the given addon folders.
export function metaGuids(dirs) {
  const out = new Map();
  const walk = (d) => {
    if (!existsSync(d)) return;
    for (const n of readdirSync(d)) {
      const p = join(d, n);
      if (statSync(p).isDirectory()) walk(p);
      else if (n.endsWith(".meta")) {
        const m = /Name\s+"\{([0-9A-F]{16})\}/i.exec(readFileSync(p, "utf8"));
        if (m) out.set(m[1].toUpperCase(), p);
      }
    }
  };
  dirs.forEach(walk);
  return out;
}

// Every Id in the .st string tables under the given addon folders.
export function stringIds(dirs) {
  const out = new Set();
  const walk = (d) => {
    if (!existsSync(d)) return;
    for (const n of readdirSync(d)) {
      const p = join(d, n);
      if (statSync(p).isDirectory()) walk(p);
      else if (n.endsWith(".st")) for (const m of readFileSync(p, "utf8").matchAll(/^\s*Id\s+"([^"]+)"/gm)) out.add(m[1]);
    }
  };
  dirs.forEach(walk);
  return out;
}

// One header's text -> list of error strings. `ids` (optional) is the set of string table Ids.
export function checkHeader(name, text, guids, ids) {
  const errors = [];
  const world = field(text, "World");
  if (!world || !/\.ent$/.test(world)) errors.push(`${name}: World is not a .ent (${world})`);
  for (const m of text.matchAll(/\{([0-9A-Fa-f]{16})\}([^"\s]*)/g)) {
    if (!guids.has(m[1].toUpperCase())) errors.push(`${name}: {${m[1]}}${m[2]} is not a resource in the core or the pack`);
  }
  const listed = field(text, "m_bShowInScenarioMenu") !== "0";
  if (!listed) return errors;
  if (!/^\s*LF_MissionHeader\s*\{/.test(text)) errors.push(`${name}: a listed header must be an LF_MissionHeader`);
  if (field(text, "m_bLF_DevToolsOff") !== "1") errors.push(`${name}: listed header does not close dev tools (m_bLF_DevToolsOff 1)`);
  if (field(text, "m_bLF_HostStopFile") !== "1") errors.push(`${name}: listed header does not watch the host stop file (m_bLF_HostStopFile 1)`);
  for (const f of ["m_sName", "m_sDescription"]) {
    const v = field(text, f);
    if (!v || v.startsWith("#")) errors.push(`${name}: ${f} must be plain text for the Workshop (${v})`);
  }
  const pics = ["m_sIcon", "m_sLoadingScreen", "m_sPreviewImage"].map((f) => field(text, f));
  if (!pics[0] || pics.some((p) => p !== pics[0])) errors.push(`${name}: icon, loading screen and preview must be one picture`);
  if (!(Number(field(text, "m_iPlayerCount")) > 1)) errors.push(`${name}: a listed header must be multiplayer (m_iPlayerCount > 1)`);
  const details = field(text, "m_sDetails");
  if (ids && details && details.startsWith("#") && !ids.has(details.slice(1))) errors.push(`${name}: m_sDetails ${details} is not a string table row in the core or the pack`);
  return errors;
}

// A listed header's World may be listed once: a second header on the same world is a duplicate entry.
export function checkWorldOnce(name, text, seen) {
  const world = field(text, "World");
  if (!world) return [];
  if (seen.has(world)) return [`${name}: lists the same World as ${seen.get(world)} (one scenario entry per world)`];
  seen.set(world, name);
  return [];
}

export function checkPackMissions(root) {
  const missions = join(root, "addons", "LF_Everon1989", "Missions");
  if (!existsSync(missions)) return { errors: [], count: 0 };
  const guids = metaGuids([join(root, "addons", "LifeFramework"), join(root, "addons", "LF_Everon1989")]);
  const ids = stringIds([join(root, "addons", "LifeFramework", "Language"), join(root, "addons", "LF_Everon1989", "Language")]);
  const errors = [];
  let count = 0;
  const listedWorld = new Map();
  for (const n of readdirSync(missions).filter((f) => f.endsWith(".conf"))) {
    count++;
    const text = readFileSync(join(missions, n), "utf8");
    errors.push(...checkHeader(n, text, guids, ids));
    if (field(text, "m_bShowInScenarioMenu") !== "0") errors.push(...checkWorldOnce(n, text, listedWorld));
  }
  return { errors, count };
}

function selftest() {
  const g = new Map([["F4889B8DF7CF06CC", "w"], ["BA0EB87AB780FD56", "p"]]);
  const good = `LF_MissionHeader {\n World "{F4889B8DF7CF06CC}Worlds/T/T.ent"\n m_sName "Town"\n m_sDescription "A town"\n m_sIcon "{BA0EB87AB780FD56}M/P.edds"\n m_sLoadingScreen "{BA0EB87AB780FD56}M/P.edds"\n m_sPreviewImage "{BA0EB87AB780FD56}M/P.edds"\n m_iPlayerCount 16\n m_bLF_DevToolsOff 1\n m_bLF_HostStopFile 1\n}\n`;
  const fails = [];
  if (checkHeader("good", good, g).length) fails.push(`clean header reported: ${checkHeader("good", good, g).join("; ")}`);
  const plant = [
    ["dev tools open", good.replace("m_bLF_DevToolsOff 1", "m_bLF_DevToolsOff 0"), /m_bLF_DevToolsOff/],
    ["no stop file", good.replace(" m_bLF_HostStopFile 1\n", ""), /m_bLF_HostStopFile/],
    ["key name", good.replace('"Town"', '"#LF-Name"'), /m_sName must be plain/],
    ["mixed pictures", good.replace('m_sIcon "{BA0EB87AB780FD56}M/P.edds"', 'm_sIcon "{F4889B8DF7CF06CC}M/Q.edds"'), /one picture/],
    ["single player", good.replace("m_iPlayerCount 16", "m_iPlayerCount 1"), /multiplayer/],
    ["missing world GUID", good.replace("F4889B8DF7CF06CC", "0000000000000ABC"), /not a resource/],
    ["SCR header listed", good.replace("LF_MissionHeader {", "SCR_MissionHeader {"), /LF_MissionHeader/],
  ];
  for (const [what, text, re] of plant) {
    if (!checkHeader(what, text, g).some((e) => re.test(e))) fails.push(`planted fault not reported: ${what}`);
  }
  const ids = new Set(["LF-Details"]);
  const withDetails = good.replace("m_iPlayerCount 16\n", 'm_iPlayerCount 16\n m_sDetails "#LF-Details"\n');
  if (checkHeader("details ok", withDetails, g, ids).length) fails.push("a header with a real m_sDetails key was reported");
  if (!checkHeader("details missing", withDetails.replace("#LF-Details", "#LF-Gone"), g, ids).some((e) => /m_sDetails/.test(e))) fails.push("planted fault not reported: m_sDetails key not in a string table");
  const seen = new Map();
  checkWorldOnce("a.conf", good, seen);
  if (!checkWorldOnce("b.conf", good, seen).length) fails.push("planted fault not reported: two listed headers on one world");
  const hidden = good.replace("m_bLF_DevToolsOff 1", "m_bLF_DevToolsOff 0").replace("}\n", " m_bShowInScenarioMenu 0\n}\n");
  if (checkHeader("hidden", hidden, g).length) fails.push("a hidden header was held to the listed rules");
  return fails;
}

if (process.argv[1] && fileURLToPath(import.meta.url) === process.argv[1]) {
  const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
  const self = selftest();
  if (self.length) {
    for (const f of self) console.log(`FAIL  validate-pack-missions selftest: ${f}`);
    process.exit(1);
  }
  const { errors, count } = checkPackMissions(root);
  for (const e of errors) console.log(`FAIL  ${e}`);
  if (errors.length) process.exit(1);
  console.log(`OK    validate-pack-missions: ${count} pack header(s)`);
}
