// The logic of `tools\cli host config apply [<host dir>]` (#1322).
//
// Reads server/flagship/config.json and server.json (the repo is public: no secret there), adds the
// host-only values from <host dir>/secrets.json and the roster's administrators, and writes the
// two files the server and `host drift` read:
//   <host>/profile/server.json
//   <host>/profile/profile/LifeFramework/config.json
// It refuses while the host runs, refuses a secret found in a repo file, never writes under the
// repo, and never returns a secret in its summary (every secret value is masked).
//
// secrets.json shape: { "game": { "password": "...", "passwordAdmin": "..." }, "staffWebhookUrl": "https://...", "announceWebhookUrl": "https://..." }
// announceWebhookUrl is read by `host post` (#1414), not written into either file here; it is known so it is not warned about.
import { existsSync, mkdirSync, readFileSync, renameSync, writeFileSync } from "node:fs";
import { dirname, join, relative, resolve, isAbsolute } from "node:path";
import { fileURLToPath } from "node:url";
import { adminIds, parseRoster } from "../commands/staff.mjs";
import { hostDir as realHostDir, hostPaths, pidAlive, readHostState } from "./host.mjs";
import { readWorkshopConfirm, writeWorkshopConfirm } from "./public.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const SECRET_KEY = /password|passwd|secret|token|webhook|apikey|api_key|credential/i;
// Paths whose values never print.
const MASKED = new Set(["game.password", "game.passwordAdmin", "staff.webhookUrl", "announceWebhookUrl"]);
const KNOWN_SECRETS = new Set(["game", "staffWebhookUrl", "announceWebhookUrl"]);
const KNOWN_GAME_SECRETS = new Set(["password", "passwordAdmin"]);

const refuse = (reason) => ({ ok: false, reason, lines: [], warnings: [], written: [] });

export function ownerConfigPath(dir) {
  return join(hostPaths(dir).profile, "profile", "LifeFramework", "config.json");
}
export function serverJsonPath(dir) {
  return join(hostPaths(dir).profile, "server.json");
}

function readJsonFile(path) {
  try {
    return { value: JSON.parse(readFileSync(path, "utf8")) };
  } catch (e) {
    return { error: e.code === "ENOENT" ? "missing" : "not valid JSON" };
  }
}

// Every key in a repo file that holds a secret: a secret-looking key with a value, or a URL.
// Returns paths only, never values.
export function findRepoSecrets(obj, path = "") {
  const out = [];
  if (Array.isArray(obj)) {
    obj.forEach((x, i) => out.push(...findRepoSecrets(x, `${path}[${i}]`)));
  } else if (obj && typeof obj === "object") {
    for (const [k, v] of Object.entries(obj)) {
      const p = path ? `${path}.${k}` : k;
      const empty = v === "" || v === null || (Array.isArray(v) && v.length === 0);
      if (SECRET_KEY.test(k) && !empty) out.push(`${p} holds a value`);
      else if (typeof v === "string" && /https?:\/\//i.test(v)) out.push(`${p} holds a URL`);
      else out.push(...findRepoSecrets(v, p));
    }
  }
  return out;
}

function flatten(obj, path = "", out = new Map()) {
  if (obj && typeof obj === "object" && !Array.isArray(obj)) {
    for (const [k, v] of Object.entries(obj)) flatten(v, path ? `${path}.${k}` : k, out);
  } else out.set(path, JSON.stringify(obj));
  return out;
}

const show = (path, json) => {
  if (json === undefined) return "(absent)";
  if (MASKED.has(path)) return json === '""' ? "(empty)" : "(set, masked)";
  return json.length > 80 ? `${json.slice(0, 77)}...` : json;
};

// What changed between the host's current file and the new one, secrets masked.
function describe(label, oldObj, newObj) {
  const a = flatten(oldObj ?? {});
  const b = flatten(newObj);
  const rows = [];
  let same = 0;
  for (const [k, v] of b) {
    if (!a.has(k)) rows.push(`  + ${k} = ${show(k, v)}`);
    else if (a.get(k) !== v) rows.push(`  ~ ${k}: ${show(k, a.get(k))} -> ${show(k, v)}`);
    else same++;
  }
  for (const k of a.keys()) if (!b.has(k)) rows.push(`  - ${k} (removed)`);
  if (!rows.length) return [`${label}: unchanged (${same} values)`];
  return [`${label}: ${rows.length} changed, ${same} unchanged`, ...rows];
}

function writeAtomic(path, obj) {
  mkdirSync(dirname(path), { recursive: true });
  const tmp = `${path}.tmp-${process.pid}`;
  writeFileSync(tmp, `${JSON.stringify(obj, null, 2)}\n`);
  renameSync(tmp, path);
}

// Applies the flagship config to a host dir. Returns { ok, reason, lines, warnings, written }.
// `repoDir` (default server/flagship) holds config.json, server.json and staff.txt.
// `dryRun` prints the changes and writes nothing.
// `mods` (`--mods <core>,<pack>`, after publishing): the Workshop IDs read off the two item pages. They must
// equal game.mods of the repo server.json (the repo is the live config); a match is recorded in
// <host>/workshop.json, which `host start --public` requires (tools/host/public.mjs).
// `rosterFile` (`--roster <file>`, #1672): read the roster from this file instead of <repoDir>/staff.txt, so a
// rehearsal on a scratch host applies the flagship config while the committed administrator is still #pending.
// Refused on the real host folder (the beat scripts refuse it the same way): the real host takes the committed roster.
// `adminsPending` (`--admins-pending`): an administrator still #pending in the roster does not refuse; game.admins
// gets only the known uids. That is the first public boot, the one where the maintainer learns his uid: a
// -noBackend join logs the all-zero identity, so the real one appears only on a backend-listed server.
export function applyConfig(dir, { repoDir = join(ROOT, "server", "flagship"), dryRun = false, mods = null, adminsPending = false, rosterFile = null } = {}) {
  const host = resolve(dir);
  const rel = relative(ROOT, host);
  if (rel === "" || (!rel.startsWith("..") && !isAbsolute(rel))) {
    return refuse(`the host dir ${host} is inside the repo checkout; a secret there could be committed. Use the host folder outside the repo (LF_HOST_DIR)`);
  }
  if (rosterFile) {
    const real = resolve(realHostDir({ ...process.env, LF_HOST_DIR: "" }));
    if (process.platform === "win32" ? host.toLowerCase() === real.toLowerCase() : host === real) {
      return refuse(`--roster is for a scratch host; ${host} is the real host folder, which takes the committed staff roster`);
    }
  }
  const state = readHostState(host);
  if (state && pidAlive(state.pid)) return refuse(`the host is running (pid ${state.pid}): stop it first (tools\\cli host stop)`);

  const server = readJsonFile(join(repoDir, "server.json"));
  const config = readJsonFile(join(repoDir, "config.json"));
  if (server.error) return refuse(`the repo server.json is ${server.error} (${join(repoDir, "server.json")})`);
  if (config.error) return refuse(`the repo config.json is ${config.error} (${join(repoDir, "config.json")})`);
  const found = [
    ...(Array.isArray(server.value?.game?.admins) && server.value.game.admins.length ? ["server.json game.admins holds a value (admins come from the staff roster)"] : []),
    ...findRepoSecrets(server.value).map((m) => `server.json ${m}`),
    ...findRepoSecrets(config.value).map((m) => `config.json ${m}`),
  ];
  if (found.length) {
    return refuse(`a secret is in a repo file: ${found.join("; ")}. Move it to the host's secrets.json and remove it from server/flagship (treat it as leaked if it was pushed)`);
  }

  // The publish confirmation.
  if (mods) {
    const repoIds = (Array.isArray(server.value?.game?.mods) ? server.value.game.mods : []).map((m) => String(m?.modId ?? "").toUpperCase());
    if (JSON.stringify(repoIds) !== JSON.stringify(mods)) {
      return refuse(`--mods ${mods.join(",")} is not server/flagship/server.json game.mods (${repoIds.join(",") || "none"}). The repo is the live config: change the IDs there in a PR (docs/ops/hotfix.md), core first, then apply again`);
    }
  }

  // The roster's administrators (R1).
  const rosterPath = rosterFile ? resolve(rosterFile) : join(repoDir, "staff.txt");
  if (!existsSync(rosterPath)) return refuse(`the staff roster is missing (${rosterPath})`);
  const roster = adminIds(parseRoster(readFileSync(rosterPath, "utf8")));
  const pendingAdmin = (m) => /has no uid yet/.test(m);
  const rosterErrors = adminsPending ? roster.errors.filter((m) => !pendingAdmin(m)) : roster.errors;
  if (rosterErrors.length) {
    const hint = roster.errors.some(pendingAdmin) && !adminsPending ? ". For the first public boot (to read your uid off the server log) apply with --admins-pending" : "";
    return refuse(`the staff roster cannot give game.admins: ${rosterErrors.join("; ")}${hint}`);
  }

  // The host's secrets.
  const warnings = [];
  const secretsPath = join(host, "secrets.json");
  let password = "";
  let passwordAdmin = "";
  let webhook = "";
  if (!existsSync(secretsPath)) {
    warnings.push(`WARNING no secrets.json in ${host}: game.password is empty, so anyone can join. Write secrets.json and apply again before the host starts`);
  } else {
    const s = readJsonFile(secretsPath);
    if (s.error || !s.value || typeof s.value !== "object" || Array.isArray(s.value)) return refuse(`secrets.json is ${s.error ?? "not an object"} (${secretsPath})`);
    const sv = s.value;
    for (const k of Object.keys(sv)) if (!KNOWN_SECRETS.has(k)) warnings.push(`secrets.json key "${k}" is not used and was ignored`);
    const g = sv.game ?? {};
    if (typeof g !== "object" || Array.isArray(g)) return refuse("secrets.json game must be an object");
    for (const k of Object.keys(g)) if (!KNOWN_GAME_SECRETS.has(k)) warnings.push(`secrets.json key "game.${k}" is not used and was ignored`);
    for (const [name, v] of [["game.password", g.password], ["game.passwordAdmin", g.passwordAdmin], ["staffWebhookUrl", sv.staffWebhookUrl]]) {
      if (v !== undefined && typeof v !== "string") return refuse(`secrets.json ${name} must be a string`);
    }
    password = (g.password ?? "").trim();
    passwordAdmin = (g.passwordAdmin ?? "").trim();
    webhook = (sv.staffWebhookUrl ?? "").trim();
    if (webhook && !/^https:\/\/\S+$/.test(webhook)) return refuse("secrets.json staffWebhookUrl must be an https URL");
    if (!password) warnings.push("WARNING secrets.json has no game.password: game.password is empty, so anyone can join. Fix it before the host starts");
    if (!passwordAdmin) warnings.push("secrets.json has no game.passwordAdmin: game.passwordAdmin is empty");
  }

  // Build the two files.
  const newServer = structuredClone(server.value);
  newServer.game = { ...(newServer.game ?? {}), password, passwordAdmin, admins: roster.ids };
  const newConfig = structuredClone(config.value);
  if (webhook) newConfig.staff = { ...(newConfig.staff ?? {}), webhookUrl: webhook };

  const sPath = serverJsonPath(host);
  const cPath = ownerConfigPath(host);
  const lines = [
    ...describe(`server.json (${sPath})`, readJsonFile(sPath).value, newServer),
    ...describe(`config.json (${cPath})`, readJsonFile(cPath).value, newConfig),
    `game.admins: ${roster.ids.length} from the staff roster`,
  ];
  if (mods) lines.push(`Workshop IDs confirmed for host start --public: ${mods.join(",")}`);
  else if (!readWorkshopConfirm(host)) lines.push("Workshop IDs not confirmed yet: after publishing, apply with --mods <core-id>,<pack-id> before host start --public");
  for (const w of roster.warnings) warnings.push(`staff roster: ${w}`);
  if (adminsPending && roster.errors.some(pendingAdmin)) warnings.push("--admins-pending: an administrator has no uid yet, so they have no admin powers on this boot. After the first join: tools\\cli host attendance, tools\\cli staff roster promote <name> <uid>, then apply again");

  const written = [];
  if (!dryRun) {
    writeAtomic(sPath, newServer);
    writeAtomic(cPath, newConfig);
    written.push(sPath, cPath);
    if (mods) written.push(writeWorkshopConfirm(host, mods));
  } else lines.push("dry run: nothing written");

  // Last line of defence: no secret value survives into the summary.
  const secretValues = [password, passwordAdmin, webhook].filter((v) => v.length >= 3);
  const scrub = (t) => secretValues.reduce((acc, v) => acc.split(v).join("***"), t);
  return { ok: true, lines: lines.map(scrub), warnings: warnings.map(scrub), written };
}
