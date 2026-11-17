// Proves the flagship's race course file (#1927): Configs/Racing/LF_RaceCourses_Everon1989.conf holds the
// dirt loop with the decided values, passes the same rules as LF_RaceCourse.IsValid (two or more unique
// checkpoint ids, laps >= 1, fee >= 0, minimum entrants >= 1, payout split adding to 100) and is listed as an
// overlay by the flagship mission header. Core Enforce tests cannot name a pack file (validate-addon-boundary),
// so the pack values are checked here.
// red-proof: a fixture copy below with a duplicate checkpoint, a payout split of 50/30/10 and no header line prints a FAIL line each.
import { readFileSync } from "node:fs";
import { join } from "node:path";
import { fileURLToPath } from "node:url";

const PACK = "addons/LF_Everon1989";
const COURSES = `${PACK}/Configs/Racing/LF_RaceCourses_Everon1989.conf`;
const GUID = /Configs\/Racing\/LF_RaceCourses_Everon1989\.conf/;

const nums = (block) => [...block.matchAll(/^\s*(-?\d+)\s*$/gm)].map((m) => Number(m[1]));
const strs = (block) => [...block.matchAll(/^\s*"([^"]*)"\s*$/gm)].map((m) => m[1]);
const field = (text, name) => text.match(new RegExp(String.raw`^\s*${name}\s+(-?\d+|"[^"]*")\s*$`, "m"))?.[1];
const list = (text, name) => text.match(new RegExp(String.raw`${name}\s*\{([^}]*)\}`))?.[1] ?? "";

export function courseProblems(text, headers = []) {
  const out = [];
  if (!/^LF_RaceCoursesConfig\s*\{/m.test(text)) out.push("the file is not an LF_RaceCoursesConfig");
  const id = field(text, "m_sId")?.replaceAll('"', "");
  if (id !== "dirt_loop") out.push(`the flagship course id must be dirt_loop, got ${id}`);
  const cps = strs(list(text, "m_aCheckpoints"));
  if (cps.length < 2) out.push("a course needs at least two checkpoints");
  if (new Set(cps).size !== cps.length) out.push(`a checkpoint is listed twice: ${cps.join(", ")}`);
  const laps = Number(field(text, "m_iLaps"));
  if (!(laps >= 1)) out.push(`laps ${laps} is below 1`);
  if (!(Number(field(text, "m_iEntryFee")) >= 0)) out.push("entry fee is negative or missing");
  if (!(Number(field(text, "m_iMinEntrants")) >= 1)) out.push("minimum entrants is below 1");
  if (!(Number(field(text, "m_iLobbySeconds")) >= 0)) out.push("lobby seconds is negative or missing");
  const payout = nums(list(text, "m_aPayoutPercent"));
  const sum = payout.reduce((a, b) => a + b, 0);
  if (payout.length < 1 || sum !== 100) out.push(`payout split ${payout.join("/")} adds up to ${sum}, not 100`);
  if (cps.some((c) => /[^a-z0-9_]/.test(c))) out.push("checkpoint ids are lower case letters, digits and underscores");
  for (const [name, h] of headers) if (!GUID.test(h)) out.push(`${name} does not list the course file as a pack overlay`);
  return out;
}

const readText = (p) => readFileSync(p, "utf8");
if (process.argv[1] === fileURLToPath(import.meta.url)) {
  const root = process.argv[2] ?? ".";
  const headers = ["LF_Everon1989"].map((m) => [`${m}.conf`, readText(join(root, PACK, "Missions", `${m}.conf`))]);
  const real = readText(join(root, COURSES));
  let bad = courseProblems(real, headers);
  const broken = real.replace('"dirt_cp_2"', '"dirt_cp_1"').replace(/^( *)60$/m, (_, a) => a + "50");
  const red = courseProblems(broken, [["fixture header", ""]]);
  if (red.length < 3) bad.push(`the red-proof fixture only raised ${red.length} problem(s): ${red.join("; ")}`);
  for (const p of bad) console.log(`FAIL ${p}`);
  console.log(`SUMMARY pack-race-course-selftest ${bad.length ? "fail" : "ok"}`);
  process.exit(bad.length ? 1 : 0);
}
