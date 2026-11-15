// Pins the maintainer's decisions for the flagship (#1405) in server/flagship, so a drift fails one line naming the field:
//   restarts at 06:00 and 18:00, warnings 15/5/1 minutes, toast only (no siren); dayCycle accelerated, 120-minute day;
//   16 player slots (the rented 0grind box, decided 2026-11-15; 40 is the later full launch);
//   the listing name exactly "Life Framework | Everon 1989 (Alpha)"; a server password is the alpha gate, so
//   game.password stays in server.json as an empty key and its value lives only in the host's secrets.json;
//   the maintainer is the only staff (one administrator line or #pending entry, game.admins empty);
//   no secret field (password, admin password, webhook URL) carries a value in either file.
// Fixtures: tools/validation/fixtures/flagship-decisions/*.json, each a patch { file, set | delete | append, expect }
// applied to a copy of the committed file; the check must name `expect` and nothing may be reported on the real files.
// inputs: server/flagship/**, tools/commands/staff.mjs, tools/validation/fixtures/flagship-decisions/**
// red-proof: 2026-11-15, with server/flagship/config.json dayLengthMinutes set to 60 this printed
// "FAIL committed files: dayCycle.dayLengthMinutes: must be 120 (a 2 hour day)" and exit 1; restored, it is green.
import { readdirSync, readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { parseRoster } from "../commands/staff.mjs";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const NAME = "Life Framework | Everon 1989 (Alpha)";
const MAINTAINER = "Jasper";
const SLOTS = 16;
const SECRET_KEY = /password|passwd|secret|token|webhook|apikey|api_key|credential/i;
const same = (a, b) => JSON.stringify(a) === JSON.stringify(b);

const getPath = (o, path) => path.split(".").reduce((x, k) => (x && typeof x === "object" ? x[k] : undefined), o);
function setPath(o, path, value) {
  const keys = path.split(".");
  const last = keys.pop();
  let x = o;
  for (const k of keys) x = x[k] ??= {};
  if (value === undefined) delete x[last];
  else x[last] = value;
}

// { config, server } parsed JSON, staff the staff.txt text -> ["<field>: <what is wrong>"]
export function decisionProblems({ config, server, staff }) {
  const out = [];
  const want = (field, ok, msg) => { if (!ok) out.push(`${field}: ${msg}`); };
  want("dayCycle.mode", getPath(config, "dayCycle.mode") === "accelerated", "must be \"accelerated\"");
  want("dayCycle.dayLengthMinutes", getPath(config, "dayCycle.dayLengthMinutes") === 120, "must be 120 (a 2 hour day)");
  want("restarts.times", same(getPath(config, "restarts.times"), ["06:00", "18:00"]), "must be [\"06:00\", \"18:00\"]");
  want("restarts.warningMinutes", same(getPath(config, "restarts.warningMinutes"), [15, 5, 1]), "must be [15, 5, 1]");
  const extra = Object.keys(config.restarts ?? {}).filter((k) => k !== "times" && k !== "warningMinutes");
  for (const k of extra) out.push(`restarts.${k}: restarts warn with the toast only; only times and warningMinutes are decided`);

  want("game.name", getPath(server, "game.name") === NAME, `must be exactly "${NAME}"`);
  const password = getPath(server, "game.password");
  want("game.maxPlayers", getPath(server, "game.maxPlayers") === SLOTS, `must be ${SLOTS} (the alpha runs on 16 slots)`);
  want("game.password", password === "", password === undefined ? "the key must stay (the alpha gate; the host's secrets.json supplies the value)" : "must be empty (the value is host-only)");
  want("game.passwordAdmin", getPath(server, "game.passwordAdmin") === "", "must be present and empty (host-only)");
  want("game.admins", same(getPath(server, "game.admins"), []), "must be an empty array (host-only; staff.txt lists the maintainer)");

  const scan = (v, path, file) => {
    if (Array.isArray(v)) v.forEach((x, i) => scan(x, `${path}[${i}]`, file));
    else if (v && typeof v === "object") {
      for (const [k, x] of Object.entries(v)) {
        const p = path ? `${path}.${k}` : k;
        const empty = x === "" || x === null || (Array.isArray(x) && x.length === 0);
        if (SECRET_KEY.test(k) && !empty && !(file === "server.json" && p.startsWith("game.pass"))) out.push(`${p}: a secret field holds a value in ${file}; secrets go only in the host's secrets.json`);
        scan(x, p, file);
      }
    } else if (typeof v === "string" && /https?:\/\//i.test(v) && !SECRET_KEY.test(path.split(".").pop())) out.push(`${path}: a URL in ${file}; webhook URLs are host-only`);
  };
  scan(config, "", "config.json");
  scan(server, "", "server.json");

  const roster = parseRoster(staff);
  if (roster.length !== 1) out.push(`staff.txt: the maintainer is the only staff, found ${roster.length} entries`);
  else if (roster[0].level !== "administrator" || roster[0].name !== MAINTAINER) out.push(`staff.txt: the one entry must be "${MAINTAINER}" as administrator`);
  return out;
}

const read = (f) => readFileSync(join(ROOT, "server", "flagship", f), "utf8");
const load = () => ({ config: JSON.parse(read("config.json")), server: JSON.parse(read("server.json")), staff: read("staff.txt") });

function applyPatch(p) {
  const inputs = load();
  if (p.file === "staff.txt") {
    inputs.staff += `${p.append}\n`;
    return inputs;
  }
  const key = p.file === "config.json" ? "config" : "server";
  for (const [path, value] of Object.entries(p.set ?? {})) setPath(inputs[key], path, value);
  for (const path of p.delete ?? []) setPath(inputs[key], path, undefined);
  return inputs;
}

if (process.argv[1]?.endsWith("flagship-decisions-selftest.mjs")) {
  const fails = [];
  try {
    const real = decisionProblems(load());
    if (real.length) fails.push(`committed files: ${real.join(" | ")}`);
  } catch (e) {
    fails.push(`committed files: could not be read: ${e.message}`);
  }

  const dir = join(ROOT, "tools", "validation", "fixtures", "flagship-decisions");
  const names = readdirSync(dir).filter((f) => f.endsWith(".json"));
  if (names.length < 4) fails.push(`fixtures: ${dir} holds ${names.length} cases, expected the wrong name, third restart, 60-minute day and passwordAdmin cases at least`);
  for (const f of names) {
    const p = JSON.parse(readFileSync(join(dir, f), "utf8"));
    const got = decisionProblems(applyPatch(p));
    const named = got.filter((l) => l.startsWith(`${p.expect}:`));
    if (got.length !== 1 || named.length !== 1) fails.push(`fixture ${f}: expected one line naming ${p.expect}, got ${got.length ? got.join(" | ") : "none"}`);
  }

  if (fails.length) {
    for (const f of fails) console.log(`FAIL ${f}`);
    console.log("SUMMARY flagship-decisions-selftest fail " + fails.length);
    process.exit(1);
  }
  console.log(`SUMMARY flagship-decisions-selftest ok ${names.length} drift cases`);
}
