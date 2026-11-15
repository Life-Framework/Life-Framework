// The opening step of `tools\cli host open --confirm <host dir name>` (#1512, #555).
//
// Opening the server to the public is removing the alpha join password (docs/community/opening-gate.md).
// This refuses unless the gate (`host gate`, #1410) is open, then clears game.password in the host's
// secrets.json and nothing else, re-applies the profile config (`host config apply`, config-apply.mjs) so
// server.json carries the empty password, and drafts the opening announcement for the maintainer to post.
// Nothing is posted and no repo file is written. Maintainer-only: agents run it only with --dry-run on a
// scratch host.
//
// The join password is only enforced by Bohemia's backend (#1520), so a backend-registered server takes
// the change after a restart. The host must be stopped, as for `host password rotate` (password.mjs).
import { existsSync, mkdirSync, readFileSync, renameSync, writeFileSync } from "node:fs";
import { basename, dirname, isAbsolute, join, relative, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { DEFAULT_ADDRESS, DEFAULT_RULES, SERVER_NAME } from "./announce.mjs";
import { applyConfig } from "./config-apply.mjs";
import { gateText } from "./gate.mjs";
import { pidAlive, readHostState } from "./host.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const RESTART = "the join password is only enforced by Bohemia's backend (#1520): on a backend-registered server the change takes effect after a restart (tools\\cli host stop, then start)";

const secretsPath = (dir) => join(resolve(dir), "secrets.json");
const refuse = (reason, extra = {}) => ({ ok: false, reason, ...extra });

function writeAtomic(path, text) {
  mkdirSync(dirname(path), { recursive: true });
  const tmp = `${path}.tmp-${process.pid}`;
  writeFileSync(tmp, text);
  renameSync(tmp, path);
}

// The opening post, for the maintainer to paste. No secret goes in it: it says the password is gone,
// not what it was. Progress is kept from the opening day (docs/community/alpha.md, wipe-policy.md).
export function openingAnnouncement({ address = DEFAULT_ADDRESS, rules = DEFAULT_RULES } = {}) {
  return [
    `**${SERVER_NAME}** is open to everyone.`,
    "",
    "The alpha password is gone: you no longer need one to join. Progress is kept from today.",
    "",
    `Join: ${address}`,
    `Rules: ${rules}`,
    "",
  ].join("\n");
}

// What the gate said, one line per check, for a refusal or a dry run.
function gateLines(gate) {
  return gateText({ ...gate, open: gate.open }).split("\n").slice(1, gate.lines.length + 1);
}

function failedGateReason(gate) {
  const failed = gate.lines.filter((l) => !l.ok);
  return `the opening gate is closed (${failed.length} of ${gate.lines.length} lines fail): ${failed.map((l) => `${l.title}: ${l.detail}`).join("; ")}`;
}

// The steps a run would take, in order. Printed by --dry-run and before a real run.
function stepLines(name) {
  return [
    "steps:",
    "  1. the opening gate (host gate) must be open",
    `  2. clear game.password in ${name}/secrets.json (everything else in the file stays)`,
    "  3. re-apply the config (host config apply): server.json gets the empty password",
    "  4. draft the opening announcement (nothing is posted)",
  ];
}

// Opens the host. `gate` is evaluateGate(...)'s result ({ open, lines }), read by the command.
// Returns { ok, lines, announcement, ... } or { ok: false, reason, lines? } with secrets.json as it was.
// `confirm` must equal the host dir's folder name exactly (not needed for a dry run). `repoDir` is for the selftest.
export function openHost(dir, { confirm, gate, repoDir = join(ROOT, "server", "flagship"), dryRun = false } = {}) {
  const host = resolve(dir);
  const name = basename(host);
  if (!gate || !Array.isArray(gate.lines)) return refuse("no gate result: the opening gate was not read");
  const lines = [...stepLines(name), ...gateLines(gate)];
  const announcement = openingAnnouncement();

  if (dryRun) {
    if (!gate.open) return refuse(failedGateReason(gate), { lines: [...lines, "dry run: the gate is closed, so a real run would refuse here"], announcement });
    return { ok: true, dryRun: true, lines: [...lines, "dry run: the gate is open; nothing was written", RESTART], announcement };
  }

  if (confirm !== name) return refuse(`confirm does not match: type the host folder name exactly (--confirm ${name})`);
  if (!gate.open) return refuse(failedGateReason(gate), { lines });
  const rel = relative(ROOT, host);
  if (rel === "" || (!rel.startsWith("..") && !isAbsolute(rel))) {
    return refuse(`the host dir ${host} is inside the repo checkout; a secret there could be committed. Use the host folder outside the repo (LF_HOST_DIR)`);
  }
  const state = readHostState(host);
  if (state && pidAlive(state.pid)) return refuse(`the host is running (pid ${state.pid}): stop it first (tools\\cli host stop)`);

  const path = secretsPath(host);
  if (!existsSync(path)) return refuse(`no secrets.json in ${host}: there is no join password to remove (wrong host folder?)`);
  const previous = readFileSync(path, "utf8");
  let secrets;
  try {
    secrets = JSON.parse(previous);
  } catch {
    return refuse(`secrets.json is not valid JSON (${path})`);
  }
  if (!secrets || typeof secrets !== "object" || Array.isArray(secrets)) return refuse(`secrets.json is not an object (${path})`);
  if (secrets.game !== undefined && (typeof secrets.game !== "object" || secrets.game === null || Array.isArray(secrets.game))) return refuse(`secrets.json game must be an object (${path})`);

  const had = typeof secrets.game?.password === "string" && secrets.game.password.trim() !== "";
  if (had) writeAtomic(path, `${JSON.stringify({ ...secrets, game: { ...secrets.game, password: "" } }, null, 2)}\n`);

  const r = applyConfig(host, { repoDir });
  if (!r.ok) {
    if (had) writeAtomic(path, previous);
    return refuse(`config apply refused, secrets.json left as it was: ${r.reason}`);
  }
  // The apply warns "anyone can join" for an empty password: that is the point of opening, so it is not repeated as a warning.
  const warnings = (r.warnings ?? []).filter((w) => !/game\.password is empty|anyone can join/i.test(w));
  return {
    ok: true,
    changed: had,
    lines: [
      ...lines,
      had ? "cleared game.password in secrets.json" : "game.password was already empty in secrets.json",
      `config applied: ${r.written.length} file(s) written`,
      RESTART,
    ],
    warnings,
    announcement,
    written: [path, ...r.written],
  };
}
