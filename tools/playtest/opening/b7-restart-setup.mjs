// BEAT B7 setup: put the next server restart N minutes ahead, in a scratch or test host profile.
//
//   node tools/playtest/opening/b7-restart-setup.mjs --profile <dir> --in 16
//   node tools/playtest/opening/b7-restart-setup.mjs --profile <dir> --undo
//
// Writes the profile's own <dir>/config.json "restarts" section (the M4 owner section,
// Feature/Ops/LF_RestartConfig.c): "times" becomes the one test time, "warningMinutes" the
// shipped 15, 5 and 1, so the beat sees the three warnings on the way to the restart. The
// restart lands on a whole minute at or after now + N, so the 15 minute warning is never
// short. Every other key in the file is kept. The original bytes are saved beside it as
// config.json.b7-backup (first setup only; a re-run keeps the first backup) and `--undo`
// puts them back and removes the backup.
//
// Never touches server/flagship/: the real restarts are 06:00 and 18:00 and live there.
// A profile whose path (or resolved path) has a server/flagship part is refused.
//
// `--now <ISO time>` stands in for the clock (the selftest uses it).
// Proof: tools/validation/opening-b7-selftest.mjs.
import { copyFileSync, existsSync, readFileSync, realpathSync, unlinkSync, writeFileSync } from "node:fs";
import { join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

export const WARNINGS = [15, 5, 1];
export const BACKUP_SUFFIX = ".b7-backup";

const val = (args, name) => {
  const i = args.indexOf(name);
  return i === -1 || i + 1 >= args.length ? null : args[i + 1];
};

/** True when a path has a `server/flagship` pair of segments, either slash, any case. */
export function isFlagshipPath(p) {
  const parts = String(p).split(/[\\/]+/).map((s) => s.toLowerCase());
  for (let i = 0; i + 1 < parts.length; i++) if (parts[i] === "server" && parts[i + 1] === "flagship") return true;
  return false;
}

/** The restart time for `now` plus `minutes`: the next whole minute at or after it, as HH:MM. */
export function restartTime(now, minutes) {
  const t = new Date(now.getTime() + minutes * 60000);
  if (t.getSeconds() > 0 || t.getMilliseconds() > 0) t.setTime(t.getTime() + 60000 - (t.getSeconds() * 1000 + t.getMilliseconds()));
  const pad = (n) => String(n).padStart(2, "0");
  return `${pad(t.getHours())}:${pad(t.getMinutes())}`;
}

function fail(message) {
  console.log(`BEAT B7 setup FAIL ${message}`);
  process.exit(1);
}

/** Resolve the profile's config.json and refuse anything under server/flagship (also through links). */
function configPath(profile) {
  if (!profile) fail("--profile <dir> is required");
  const dir = resolve(profile);
  if (isFlagshipPath(profile) || isFlagshipPath(dir)) fail(`refused: ${dir} is under server/flagship, which is never touched`);
  const cfg = join(dir, "config.json");
  if (!existsSync(cfg)) fail(`no config.json in ${dir}`);
  let real;
  try {
    real = realpathSync(cfg);
  } catch (e) {
    fail(`cannot resolve ${cfg}: ${e.message}`);
  }
  if (isFlagshipPath(real)) fail(`refused: ${cfg} resolves into server/flagship, which is never touched`);
  return cfg;
}

function main(args) {
  const cfg = configPath(val(args, "--profile"));
  const backup = cfg + BACKUP_SUFFIX;

  if (args.includes("--undo")) {
    if (!existsSync(backup)) fail(`nothing to undo: no ${backup}`);
    copyFileSync(backup, cfg);
    unlinkSync(backup);
    console.log(`BEAT B7 setup UNDONE ${cfg}`);
    return;
  }

  const raw = val(args, "--in");
  const minutes = raw !== null && /^\d+$/.test(raw) ? Number(raw) : NaN;
  if (!(minutes >= 1 && minutes <= 1440)) fail(`--in must be whole minutes, 1 to 1440 (got ${raw})`);

  const nowArg = val(args, "--now");
  const now = nowArg ? new Date(nowArg) : new Date();
  if (Number.isNaN(now.getTime())) fail(`--now is not a time: ${nowArg}`);

  let config;
  try {
    config = JSON.parse(readFileSync(cfg, "utf8"));
  } catch (e) {
    fail(`${cfg} is not valid JSON: ${e.message}`);
  }
  if (!config || typeof config !== "object" || Array.isArray(config)) fail(`${cfg} is not a JSON object`);

  const time = restartTime(now, minutes);
  const previous = config.restarts && typeof config.restarts === "object" && !Array.isArray(config.restarts) ? config.restarts : {};
  config.restarts = { ...previous, times: [time], warningMinutes: [...WARNINGS] };

  if (!existsSync(backup)) copyFileSync(cfg, backup);
  writeFileSync(cfg, JSON.stringify(config, null, 2) + "\n");
  console.log(`BEAT B7 setup PASS ${time}`);
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) main(process.argv.slice(2));
