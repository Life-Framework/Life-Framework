// Acceptance for the level-up sting (#4405, W3 sound half): the core declares the `level_up` slot, the pack
// fills it with a project that exists, the project plays SOUND_LEVEL_UP, the sample is under 2 s, and the
// pack header loads the overlay. `node tools/assets/level-up/level-up-selftest.mjs`
import { existsSync, readFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const root = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..", "..");
const pack = join(root, "addons", "LF_Everon1989");
const SLOT = "level_up";
const EVENT = "SOUND_LEVEL_UP";
const CONF = "Configs/SoundSlots/LF_SoundSlots_Level_Everon1989.conf";
const problems = [];
const bad = (m) => problems.push(m);
const rowOf = (text) => text.split("LF_SoundSlot ").find((r) => r.includes(`m_sSlot "${SLOT}"`)) || "";
const read = (file) => (existsSync(file) ? readFileSync(file, "utf8") : "");
const metaGuid = (rel) => /^ Name "\{([0-9A-F]{16})\}/m.exec(read(join(pack, rel + ".meta")))?.[1];

const core = read(join(root, "addons", "LifeFramework", "Configs", "SoundSlots", "LF_SoundSlots.conf"));
if (!rowOf(core)) bad(`the core slot table does not declare ${SLOT}, so a pack row for it is ignored`);

const conf = read(join(pack, CONF));
const row = rowOf(conf);
if (!row) bad(`${CONF} has no row for ${SLOT}`);
const project = /m_Project "\{([0-9A-F]{16})\}([^"]+)"/.exec(row);
if (row && !project) bad(`the ${SLOT} row names no project`);
if (project) {
  if (!existsSync(join(pack, project[2]))) bad(`${project[2]} does not exist`);
  else {
    if (metaGuid(project[2]) !== project[1]) bad(`${project[2]}: row GUID differs from its .meta`);
    const acp = read(join(pack, project[2]));
    if (!acp.includes(`name "${EVENT}"`)) bad(`${project[2]} has no ${EVENT} event`);
    const sample = /\{([0-9A-F]{16})\}(Sounds\/Level\/Samples\/[^"]+\.wav)/.exec(acp);
    if (!sample) bad(`${project[2]} lists no sample under Sounds/Level/Samples`);
    else if (!existsSync(join(pack, sample[2]))) bad(`sample ${sample[2]} does not exist`);
    else {
      const wav = readFileSync(join(pack, sample[2]));
      const rate = wav.readUInt32LE(24);
      const seconds = (wav.length - 44) / (rate * 2);
      if (seconds >= 2) bad(`the sting is ${seconds.toFixed(2)} s; it must be under 2 s`);
      if (seconds < 0.5) bad(`the sting is ${seconds.toFixed(2)} s; too short to read as a phrase`);
    }
  }
}
if (!/LF_SoundSlots_Level_Everon1989\.conf/.test(read(join(pack, "Missions", "LF_Everon1989.conf")))) bad("the pack header does not list LF_SoundSlots_Level_Everon1989.conf");

if (problems.length) {
  console.log("FAIL level-up:\n  " + problems.join("\n  "));
  process.exit(1);
}
console.log("OK level-up: core slot, pack row, project, event, sample length and header agree");
