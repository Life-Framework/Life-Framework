// Acceptance for WK-W23 (#4183): the work-site sounds are listed in a manifest with source and licence, every
// file and slot it names exists, and the header loads the slot overlay. `node tools/assets/site-ambience/site-ambience-selftest.mjs`
import { existsSync, readFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const root = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..", "..");
const pack = join(root, "addons", "LF_Everon1989");
const problems = [];
const bad = (m) => problems.push(m);

const manifestFile = join(pack, "Sounds", "Work", "manifest.json");
if (!existsSync(manifestFile)) {
  bad(`${manifestFile} is missing`);
} else {
  const m = JSON.parse(readFileSync(manifestFile, "utf8"));
  const conf = readFileSync(join(pack, "Configs", "SoundSlots", "LF_SoundSlots_Work_Everon1989.conf"), "utf8");
  const header = readFileSync(join(pack, "Missions", "LF_Everon1989.conf"), "utf8");
  const metaGuid = (rel) => /^ Name "\{([0-9A-F]{16})\}/m.exec(readFileSync(join(pack, rel + ".meta"), "utf8"))?.[1];
  if (!/LF_SoundSlots_Work_Everon1989\.conf/.test(header)) bad("the pack header does not list LF_SoundSlots_Work_Everon1989.conf");
  if (!(m.sounds || []).length) bad("manifest has no sounds");
  for (const s of m.sounds || []) {
    const at = `sound ${s.id}`;
    if (!["vanilla", "elevenlabs"].includes(s.source)) bad(`${at}: source must be vanilla or elevenlabs`);
    if (!s.licence) bad(`${at}: licence is empty`);
    if (s.source === "elevenlabs" && !s.prompt) bad(`${at}: a generated sound records its prompt`);
    for (const k of ["project", "sample"]) {
      const m2 = /^\{([0-9A-F]{16})\}(.+)$/.exec(s[k] || "");
      if (!m2) { bad(`${at}: ${k} is not a {GUID}path`); continue; }
      if (!existsSync(join(pack, m2[2]))) bad(`${at}: ${m2[2]} does not exist`);
      else if (metaGuid(m2[2]) !== m2[1]) bad(`${at}: ${k} GUID differs from its .meta`);
    }
    const row = conf.split("LF_SoundSlot ").find((r) => r.includes(`m_sSlot "${s.slot}"`)) || "";
    if (!row.includes(`m_Project "${s.project}"`)) bad(`${at}: slot ${s.slot} does not point at ${s.project} in the conf`);
  }
  const w = m.whistle;
  if (!w || !m.sounds.some((s) => s.slot === w.slot)) bad("manifest.whistle names no listed slot");
  else if (!w.hours.some((h) => h[0] === 6) || !w.hours.some((h) => h[0] === 18)) bad("whistle hours must include 6 and 18");
  else if (!(w.sites || []).length) bad("whistle lists no sites");
}
if (problems.length) {
  console.log("FAIL site-ambience:\n  " + problems.join("\n  "));
  process.exit(1);
}
console.log("OK site-ambience: manifest, files, slots and header agree");
