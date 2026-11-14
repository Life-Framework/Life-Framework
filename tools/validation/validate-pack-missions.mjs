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
//   - a header hidden from Scenarios says m_bShowInScenarioMenu 0 explicitly.
// red-proof: 2026-11-14, set m_bLF_DevToolsOff 0 in the pack LF_PlaytestTown.conf; observed
// "FAIL  LF_PlaytestTown.conf: listed header does not close dev tools (m_bLF_DevToolsOff 1)".
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

// One header's text -> list of error strings.
export function checkHeader(name, text, guids) {
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
  return errors;
}

export function checkPackMissions(root) {
  const missions = join(root, "addons", "LF_Everon1989", "Missions");
  if (!existsSync(missions)) return { errors: [], count: 0 };
  const guids = metaGuids([join(root, "addons", "LifeFramework"), join(root, "addons", "LF_Everon1989")]);
  const errors = [];
  let count = 0;
  for (const n of readdirSync(missions).filter((f) => f.endsWith(".conf"))) {
    count++;
    errors.push(...checkHeader(n, readFileSync(join(missions, n), "utf8"), guids));
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
