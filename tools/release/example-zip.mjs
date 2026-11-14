// The example server download (#296): server/example/ packed into LifeFramework-ExampleServer.zip,
// the file a release attaches beside the Workshop items. `tools\cli example-zip [--final] [--out <dir>]`.
//
// The zip holds the five files of the template and nothing else (plus DRAFT.txt on a draft). Its
// server.json gets the Workshop ids and names from tools/release/example-zip.json, the one config
// point. An id is the addon's .gproj GUID; until the maintainer's first Workshop upload (#370)
// confirms one, its value starts with PLACEHOLDER and the build is a draft
// (LifeFramework-ExampleServer-DRAFT.zip). `--final` refuses to build while any placeholder remains,
// so a release never carries one. The finished zip is read back and checked again: the five files,
// server.json valid with the core first, config.json clean under config-check.
// red-proof: tools/validation/example-zip-selftest.mjs.
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { checkText } from "../config/check.mjs";
import { ADDONS } from "../lib/addons.mjs";
import { FILES, templateProblems } from "../server-template/template.mjs";
import { makeZip, readZip } from "./pack-playtest.mjs";

const REPO = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
export const IDS_FILE = "tools/release/example-zip.json";
export const ZIP_NAME = "LifeFramework-ExampleServer.zip";
export const DRAFT_NAME = "LifeFramework-ExampleServer-DRAFT.zip";
export const SLOTS = ["core", "pack"];
export const PLACEHOLDER = "PLACEHOLDER";
const HEX_ID = /^[0-9A-F]{16}$/;
const MODES = { "start.sh": 0o755 };

export const isPlaceholder = (id) => !HEX_ID.test(String(id ?? ""));

export function loadIds(root = REPO) {
  return JSON.parse(readFileSync(join(root, IDS_FILE), "utf8")).mods;
}

// Problems with the ids file itself: both slots present, a real id is the addon's GUID, anything else is a marked placeholder.
export function idsProblems(mods) {
  const out = [];
  SLOTS.forEach((slot, i) => {
    const m = mods?.[slot];
    if (!m || !m.name) return out.push(`${IDS_FILE}: mods.${slot} needs a modId and a name`);
    if (HEX_ID.test(m.modId)) {
      if (m.modId !== ADDONS[i].guid) out.push(`${IDS_FILE}: mods.${slot}.modId ${m.modId} is not ${ADDONS[i].name}'s GUID ${ADDONS[i].guid}`);
    } else if (!String(m.modId ?? "").startsWith(PLACEHOLDER)) {
      out.push(`${IDS_FILE}: mods.${slot}.modId "${m.modId}" is neither a 16-character hex id nor a ${PLACEHOLDER}* marker`);
    }
  });
  return out;
}

export const remainingPlaceholders = (mods) => SLOTS.filter((s) => isPlaceholder(mods?.[s]?.modId));

// The template's server.json with the config's ids and names in its two mod slots.
export function serverJsonFor(root, mods) {
  const sj = JSON.parse(readFileSync(join(root, "server", "example", "server.json"), "utf8"));
  if (!Array.isArray(sj.game?.mods) || sj.game.mods.length !== SLOTS.length) throw new Error(`server/example/server.json game.mods must list the core and the pack (${SLOTS.length} entries)`);
  sj.game.mods = SLOTS.map((s) => ({ modId: mods[s].modId, name: mods[s].name }));
  return JSON.stringify(sj, null, 2) + "\n";
}

// What a finished zip must hold; run on the bytes that were written. `final` also forbids any placeholder text.
export function verifyZip(buf, { final = false } = {}) {
  const out = [];
  const files = new Map(readZip(buf).map((e) => [e.name, e.data]));
  for (const f of FILES) if (!files.has(f)) out.push(`the zip has no ${f}`);
  if (out.length) return out;
  let sj = null;
  try {
    sj = JSON.parse(files.get("server.json").toString("utf8"));
  } catch (e) {
    out.push(`server.json in the zip is not valid JSON: ${e.message}`);
  }
  if (sj) {
    const mods = sj.game?.mods ?? [];
    if (mods[0]?.modId !== ADDONS[0].guid) out.push(`server.json lists "${mods[0]?.modId}" first; the core ${ADDONS[0].guid} must be first`);
    if (mods.length < 2) out.push("server.json lists no pack after the core");
    if (!sj.game?.scenarioId) out.push("server.json has no game.scenarioId");
  }
  for (const p of checkText(files.get("config.json").toString("utf8"), { source: "config.json" }).problems ?? []) {
    if (p.level === "error") out.push(`config.json ${p.line}:${p.col} ${p.msg}`);
  }
  if (final) for (const [name, data] of files) if (data.toString("utf8").includes(PLACEHOLDER)) out.push(`${name} still holds a ${PLACEHOLDER} marker`);
  return out;
}

// Builds the zip bytes. { ok, name, buf, problems, drafts } ; ok is false when anything blocks it.
export function buildExampleZip(root = REPO, { final = false } = {}) {
  const problems = [];
  if (!existsSync(join(root, IDS_FILE))) return { ok: false, problems: [`${IDS_FILE} is missing`], drafts: [] };
  const mods = loadIds(root);
  problems.push(...idsProblems(mods), ...templateProblems(root));
  const drafts = remainingPlaceholders(mods);
  if (final && drafts.length) {
    problems.push(`--final: ${drafts.map((s) => `mods.${s}.modId`).join(", ")} in ${IDS_FILE} is still a ${PLACEHOLDER}; set it to the confirmed Workshop id (#370)`);
  }
  if (problems.length) return { ok: false, problems, drafts };

  const entries = FILES.map((f) => {
    const data = f === "server.json" ? Buffer.from(serverJsonFor(root, mods)) : readFileSync(join(root, "server", "example", f));
    return { name: f, data, ...(MODES[f] ? { mode: MODES[f] } : {}) };
  });
  if (drafts.length) {
    entries.push({
      name: "DRAFT.txt",
      data: Buffer.from(`This is a draft build. ${drafts.map((s) => `mods.${s}.modId`).join(" and ")} in server.json is a ${PLACEHOLDER} until the Workshop item exists (#370).\nThe server will not start until you replace it with the real Workshop id.\n`),
    });
  }
  const buf = makeZip(entries);
  const bad = verifyZip(buf, { final });
  return { ok: bad.length === 0, name: final || !drafts.length ? ZIP_NAME : DRAFT_NAME, buf, problems: bad, drafts };
}

export function main(args, { root = REPO } = {}) {
  const final = args.includes("--final");
  const i = args.indexOf("--out");
  const outDir = i >= 0 && args[i + 1] ? args[i + 1] : join(root, "tmp", "release");
  const r = buildExampleZip(root, { final });
  if (!r.ok) {
    for (const p of r.problems) console.log(`FAIL  ${p}`);
    console.log(`SUMMARY example-zip fail ${final ? "final" : "draft"} problems=${r.problems.length}`);
    return 1;
  }
  mkdirSync(outDir, { recursive: true });
  const file = join(outDir, r.name);
  writeFileSync(file, r.buf);
  console.log(`example-zip: ${file} (${r.buf.length} bytes)`);
  if (r.drafts.length) console.log(`note  draft: ${r.drafts.map((s) => `mods.${s}.modId`).join(", ")} is a ${PLACEHOLDER}; waits on #370, \`example-zip --final\` refuses it`);
  console.log(`SUMMARY example-zip ok ${r.drafts.length ? "draft" : "final"} file=${file}`);
  return 0;
}
