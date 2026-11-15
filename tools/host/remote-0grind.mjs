// The flagship on a rented 0grind server, driven from this PC through 0grind's API (there is no shell on
// the box). `tools\cli host 0grind <cmd>` is the command (tools/host/commands/0grind.mjs); this is the logic.
//
// API (app.0grind.io/kb/api, read from the page bundle 2026-11-15): base https://api.0grind.io/v2,
// `Authorization: Bearer <token>` AND the source IP: the token is the one on the panel's Account page,
// "API Access" (not a service's Integrations token, which only feeds Server Admin Tools events), and the
// calling IPv4 must be in that page's "Allowed IPv4 Addresses" (at most 10). Either wrong gives
// 401 {"success":false,"reason":"Unauthorized"}. Every JSON reply is {success, reason, data}.
// Global rate limit: 1 request per second.
//
// The token lives in ~/.life-framework/0grind-token (LF_0GRIND_TOKEN_FILE overrides). It is never
// printed: every message and error passes through redact().
import { existsSync, mkdirSync, readFileSync, renameSync, statSync, writeFileSync } from "node:fs";
import { homedir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { applyConfig, ownerConfigPath, serverJsonPath } from "./config-apply.mjs";
import { freshDir, hostDir, stamp } from "./host.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");

export const FLAGSHIP_0GRIND = {
  serviceId: "298627",
  address: "144.76.90.77",
  gamePort: 2023,
  a2sPort: 3023,
  rconPort: 5023,
};
export const API_BASE = "https://api.0grind.io/v2";
export const MIN_INTERVAL_MS = 1100; // the 1 request per second limit, with a margin
export const JSON_TIMEOUT_MS = 20000;
export const FILE_TIMEOUT_MS = 120000;

// RCON commands that only read. Everything else needs --yes.
export const READ_ONLY_RCON = new Set(["#players"]);

// Keys whose values never print (server_settings, system_settings, ServerConfig.json paths).
const SECRET_KEY = /password|passwd|secret|token|webhook|apikey|api_key|credential/i;

export function tokenPath(env = process.env) {
  return env.LF_0GRIND_TOKEN_FILE || join(homedir(), ".life-framework", "0grind-token");
}

export function readToken(env = process.env) {
  const p = tokenPath(env);
  if (!existsSync(p)) return { error: `no 0grind API token at ${p}: copy it from app.0grind.io, Account, API Access` };
  const t = readFileSync(p, "utf8").trim();
  if (!t) return { error: `the 0grind token file ${p} is empty` };
  return { token: t };
}

// The local folder that mirrors the box: profile/logs/logs_<stamp>/console.log (so host triage,
// summary and attendance read it with LF_HOST_DIR=<it> or a path), backups/<stamp>-remote/, the staged
// profile/server.json `config push` builds, and secrets.json (the host-only values, never in the repo).
export function remoteDir(env = process.env) {
  if (env.LF_0GRIND_DIR) return env.LF_0GRIND_DIR;
  return join(dirname(hostDir(env)), "host-0grind");
}

export function makeRedactor(secrets) {
  const vals = secrets.filter((s) => typeof s === "string" && s.length >= 3);
  return (text) => vals.reduce((acc, v) => acc.split(v).join("***"), String(text ?? ""));
}

// Masks every secret-looking key of a settings object (deep copy).
export function maskSecrets(obj) {
  if (Array.isArray(obj)) return obj.map(maskSecrets);
  if (obj && typeof obj === "object") {
    const out = {};
    for (const [k, v] of Object.entries(obj)) {
      if (SECRET_KEY.test(k) && typeof v === "string") out[k] = v ? "(set, masked)" : "(empty)";
      else out[k] = maskSecrets(v);
    }
    return out;
  }
  return obj;
}

// Every secret value in an object (for the redactor), keyed by secret-looking names.
export function secretValues(obj, out = []) {
  if (Array.isArray(obj)) obj.forEach((x) => secretValues(x, out));
  else if (obj && typeof obj === "object") {
    for (const [k, v] of Object.entries(obj)) {
      if (SECRET_KEY.test(k) && typeof v === "string" && v) out.push(v);
      else secretValues(v, out);
    }
  }
  return out;
}

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

// The API client. `fetchImpl`, `base`, `minIntervalMs` and `lastCallFile` are for the selftest.
// The last call time is kept in a file so two commands run back to back still respect the limit.
export function createClient({ token, serviceId = FLAGSHIP_0GRIND.serviceId, base = API_BASE, fetchImpl = fetch, minIntervalMs = MIN_INTERVAL_MS, lastCallFile = null, now = () => Date.now(), log = () => {} } = {}) {
  const redact = makeRedactor([token]);
  let last = 0;
  const readLast = () => {
    if (!lastCallFile) return last;
    try {
      return Math.max(last, Number(readFileSync(lastCallFile, "utf8")) || 0);
    } catch {
      return last;
    }
  };
  const markLast = () => {
    last = now();
    if (!lastCallFile) return;
    try {
      mkdirSync(dirname(lastCallFile), { recursive: true });
      writeFileSync(lastCallFile, String(last));
    } catch {
      // the in-process limit still holds
    }
  };

  async function request(method, path, { query, json, body, raw = false, timeoutMs } = {}) {
    const url = new URL(base.replace(/\/$/, "") + path);
    for (const [k, v] of Object.entries(query ?? {})) url.searchParams.set(k, v);
    const headers = { Authorization: `Bearer ${token}` };
    let payload;
    if (json !== undefined) {
      headers["Content-Type"] = "application/json";
      payload = JSON.stringify(json);
    } else if (body !== undefined) {
      headers["Content-Type"] = "application/octet-stream";
      payload = body;
    }
    for (let attempt = 0; attempt < 2; attempt++) {
      const wait = readLast() + minIntervalMs - now();
      if (wait > 0) await sleep(wait);
      markLast();
      log(`${method} ${url.pathname}${url.search}`);
      let r;
      try {
        r = await fetchImpl(url, { method, headers, body: payload, signal: AbortSignal.timeout(timeoutMs ?? (raw ? FILE_TIMEOUT_MS : JSON_TIMEOUT_MS)) });
      } catch (e) {
        const why = e?.name === "TimeoutError" ? `timed out after ${timeoutMs ?? (raw ? FILE_TIMEOUT_MS : JSON_TIMEOUT_MS)} ms` : e?.message ?? String(e);
        return { ok: false, status: 0, reason: redact(`${method} ${path}: ${why}`) };
      }
      // Count the interval from the reply too: 0grind counts arrivals, and the send-time mark alone lets
      // a slow request and a fast one land closer than the limit.
      markLast();
      if (r.status === 429 && attempt === 0) {
        await sleep(Math.max(minIntervalMs * 2, 50));
        continue;
      }
      if (raw && r.ok && !/json/i.test(r.headers.get("content-type") ?? "")) {
        return { ok: true, status: r.status, bytes: Buffer.from(await r.arrayBuffer()) };
      }
      const text = await r.text();
      let j = null;
      try {
        j = JSON.parse(text);
      } catch {
        // not JSON
      }
      if (raw && r.ok && !(j && j.success === false)) return { ok: true, status: r.status, bytes: Buffer.from(text, "utf8") };
      if (!r.ok || !j || j.success === false) {
        const reason = j?.reason ?? (text.slice(0, 200) || r.statusText);
        return { ok: false, status: r.status, reason: redact(`${method} ${path}: HTTP ${r.status} ${reason}`) };
      }
      return { ok: true, status: r.status, data: j.data };
    }
    return { ok: false, status: 429, reason: `${method} ${path}: rate limited twice` };
  }

  const svc = encodeURIComponent(serviceId);
  const fs = (p) => `/service/${svc}/filesystem${p}`;
  return {
    redact,
    request,
    // The panel's service layout polls GET /service/{id} every 5 s (status, alias, ...).
    service: () => request("GET", `/service/${svc}`),
    action: (name) => request("POST", `/armareforger/${svc}/${name}`),
    systemSettings: () => request("GET", `/armareforger/${svc}/system_settings`),
    serverSettings: () => request("GET", `/armareforger/${svc}/server_settings`),
    patchSystemSettings: (changes) => request("PATCH", `/armareforger/${svc}/system_settings`, { json: changes }),
    patchServerSettings: (changes) => request("PATCH", `/armareforger/${svc}/server_settings`, { json: changes }),
    rcon: (command) => request("POST", `/armareforger/${svc}/rcon`, { json: { command } }),
    list: (path) => request("GET", fs("/files"), { query: { path } }),
    get: (path) => request("GET", fs("/file"), { query: { path }, raw: true }),
    put: (path, bytes) => request("PUT", fs("/file"), { query: { path }, body: bytes }),
  };
}

// The hint a 401 prints: what the maintainer clicks on the panel.
export function authHint(ip) {
  return [
    "0grind refused the token (401 Unauthorized). The API checks the token AND the calling IP:",
    "  1. app.0grind.io, Account (main account only), API Access: copy that API Token into " + tokenPath() + " (a service's Integrations token is not an API token)",
    `  2. same section, Allowed IPv4 Addresses: add this PC's public IPv4${ip ? ` ${ip}` : " (curl -s https://api.ipify.org)"} and save`,
  ];
}

// ---------------------------------------------------------------- config push

// server.json (the base game's dedicated config) field -> 0grind server_settings key.
const GAME_MAP = [
  ["name", "server_name"],
  ["password", "password"],
  ["passwordAdmin", "admin_password"],
  ["admins", "admins"],
  ["scenarioId", "scenario_id"],
  ["visible", "visible"],
  ["crossPlatform", "cross_platform"],
  ["supportedPlatforms", "supported_platforms"],
  ["modsRequiredByDefault", "mods_required_by_default"],
  ["mods", "mods"],
];
const PROPS_MAP = [
  ["serverMaxViewDistance", "server_max_view_distance"],
  ["serverMinGrassDistance", "server_min_grass_distance"],
  ["networkViewDistance", "network_view_distance"],
  ["disableThirdPerson", "disable_third_person"],
  ["VONDisableUI", "von_disable_ui"],
  ["VONDisableDirectSpeechUI", "von_disable_direct_speech_ui"],
  ["VONCanTransmitCrossFaction", "von_can_transmit_cross_faction"],
  ["battlEye", "enable_battleye"],
  ["missionHeader", "mission_header"],
];
const OPERATING_MAP = [
  ["lobbyPlayerSynchronise", "lobby_player_synchronise"],
  ["disableNavmeshStreaming", "disable_navmesh_streaming"],
  ["disableServerShutdown", "disable_server_shutdown"],
  ["disableCrashReporter", "disable_crash_reporter"],
  ["disableAI", "disable_ai"],
  ["playerSaveTime", "player_save_time"],
  ["aiLimit", "ai_limit"],
  ["slotReservationTimeout", "slot_reservation_timeout"],
];
// The box's own: ports, addresses and the RCON block are 0grind's, never ours.
export const KEPT_TOP_LEVEL = ["bindAddress", "bindPort", "publicAddress", "publicPort", "a2s", "rcon"];

// The server_settings our server.json sets. rcon_* is never in it.
export function toServerSettings(server) {
  const out = {};
  const g = server?.game ?? {};
  for (const [from, to] of GAME_MAP) if (g[from] !== undefined) out[to] = g[from];
  const gp = g.gameProperties ?? {};
  for (const [from, to] of PROPS_MAP) if (gp[from] !== undefined) out[to] = gp[from];
  const op = server?.operating ?? {};
  for (const [from, to] of OPERATING_MAP) if (op[from] !== undefined) out[to] = op[from];
  if (op.joinQueue?.maxSize !== undefined) out.join_queue_max_size = op.joinQueue.maxSize;
  return out;
}

// Their ServerConfig.json with our game and operating blocks; their ports, addresses and RCON kept.
// game.maxPlayers stays theirs: the slot count is what 0grind rents.
export function mergeServerConfig(theirs, ours) {
  const out = structuredClone(theirs ?? {});
  out.game = structuredClone(ours.game ?? {});
  if (theirs?.game?.maxPlayers !== undefined) out.game.maxPlayers = theirs.game.maxPlayers;
  if (ours.operating !== undefined) out.operating = { ...(theirs?.operating ?? {}), ...structuredClone(ours.operating) };
  for (const k of KEPT_TOP_LEVEL) {
    if (theirs && k in theirs) out[k] = structuredClone(theirs[k]);
    else delete out[k];
  }
  return out;
}

const showVal = (k, v) => {
  if (SECRET_KEY.test(k.split(".").pop())) return v === undefined ? "(absent)" : v === "" || v === '""' ? "(empty)" : "(set, masked)";
  const s = JSON.stringify(v);
  if (s === undefined) return "(absent)";
  return s.length > 80 ? `${s.slice(0, 77)}...` : s;
};

function flatten(obj, path = "", out = new Map()) {
  if (obj && typeof obj === "object" && !Array.isArray(obj) && Object.keys(obj).length) {
    for (const [k, v] of Object.entries(obj)) flatten(v, path ? `${path}.${k}` : k, out);
  } else out.set(path, obj);
  return out;
}

// Diff lines (secrets masked) and the server_settings keys that change.
export function diffSettings(current, wanted) {
  const changes = {};
  const lines = [];
  for (const [k, v] of Object.entries(wanted)) {
    if (JSON.stringify(current?.[k]) === JSON.stringify(v)) continue;
    changes[k] = v;
    lines.push(`  ~ ${k}: ${showVal(k, current?.[k])} -> ${showVal(k, v)}`);
  }
  return { changes, lines };
}

export function diffObjects(a, b) {
  const fa = flatten(a ?? {});
  const fb = flatten(b ?? {});
  const lines = [];
  for (const [k, v] of fb) {
    if (!fa.has(k)) lines.push(`  + ${k} = ${showVal(k, v)}`);
    else if (JSON.stringify(fa.get(k)) !== JSON.stringify(v)) lines.push(`  ~ ${k}: ${showVal(k, fa.get(k))} -> ${showVal(k, v)}`);
  }
  for (const k of fa.keys()) if (!fb.has(k)) lines.push(`  - ${k} (removed)`);
  return lines;
}

// Builds our server.json for the box with the existing apply logic (repo files, secrets.json, roster
// admins, every refusal) into the local staging folder, which is outside the repo.
export function stageServerJson(dir, { repoDir, adminsPending = false } = {}) {
  const r = applyConfig(dir, { ...(repoDir ? { repoDir } : {}), adminsPending });
  if (!r.ok) return r;
  return {
    ...r,
    server: JSON.parse(readFileSync(serverJsonPath(dir), "utf8")),
    owner: readFileSync(ownerConfigPath(dir)),
    staff: readFileSync(join(repoDir ?? join(ROOT, "server", "flagship"), "staff.txt")),
  };
}

export const SERVER_CONFIG_PATH = "/ServerConfig.json";
// The API's / is the -profile folder (/home/reforger/profile); the game's $profile: is its profile/
// subfolder, beside .save. The mod reads its owner config and roster there.
export const OWNER_CONFIG_PATH = "/profile/LifeFramework/config.json";
export const STAFF_PATH = "/profile/LifeFramework/staff.txt";

// The plan and, with yes, the write. Returns { ok, reason?, lines, warnings }.
// only: a list of server.json game fields (name, password, passwordAdmin, ...). It limits the push to
// those server_settings keys: ServerConfig.json and the mod's own files are not read or written, so the
// scenario and the mods list stay as the box has them (the vanilla scenario before the mods are published).
export function onlySettings(wanted, only) {
  const keys = new Set();
  for (const f of only) {
    const hit = GAME_MAP.find(([from]) => from === f);
    if (!hit) return { error: `--only ${f}: not a game field config push knows (${GAME_MAP.map(([a]) => a).join(", ")})` };
    keys.add(hit[1]);
  }
  return { wanted: Object.fromEntries(Object.entries(wanted).filter(([k]) => keys.has(k))) };
}

export async function configPush(client, { dir, repoDir, adminsPending = false, yes = false, only = null } = {}) {
  if (only?.length) return configPushOnly(client, { dir, repoDir, adminsPending, yes, only });
  const staged = stageServerJson(dir, { repoDir, adminsPending });
  if (!staged.ok) return { ok: false, reason: staged.reason, lines: [], warnings: [] };
  const ours = staged.server;
  const redact = makeRedactor([...secretValues(ours)]);
  const cur = await client.serverSettings();
  if (!cur.ok) return { ok: false, reason: cur.reason, status: cur.status, lines: [], warnings: [] };
  const current = cur.data?.settings ?? {};
  const theirsFile = await client.get(SERVER_CONFIG_PATH);
  if (!theirsFile.ok) return { ok: false, reason: theirsFile.reason, status: theirsFile.status, lines: [], warnings: [] };
  let theirs;
  try {
    theirs = JSON.parse(theirsFile.bytes.toString("utf8"));
  } catch {
    return { ok: false, reason: `${SERVER_CONFIG_PATH} on the box is not valid JSON; fix it on the panel first`, lines: [], warnings: [] };
  }
  // The mod's own files: compared byte for byte (absent on a fresh box).
  const extra = [];
  for (const [path, bytes] of [[OWNER_CONFIG_PATH, staged.owner], [STAFF_PATH, staged.staff]]) {
    const g = await client.get(path);
    if (!g.ok && g.status !== 404) return { ok: false, reason: g.reason, status: g.status, lines: [], warnings: [] };
    if (!g.ok) extra.push({ path, bytes, line: `${path}: absent on the box, to upload (${bytes.length} bytes)` });
    else if (!g.bytes.equals(bytes)) extra.push({ path, bytes, line: `${path}: differs, to upload (${g.bytes.length} -> ${bytes.length} bytes)` });
  }
  const ownerSecrets = (() => {
    try {
      return secretValues(JSON.parse(staged.owner.toString("utf8")));
    } catch {
      return [];
    }
  })();
  const allSecrets = makeRedactor([...secretValues(ours), ...secretValues(current), ...secretValues(theirs), ...ownerSecrets]);
  const wanted = toServerSettings(ours);
  const { changes, lines: settingLines } = diffSettings(current, wanted);
  const merged = mergeServerConfig(theirs, ours);
  const fileLines = diffObjects(theirs, merged);
  const lines = [
    settingLines.length ? `server_settings: ${settingLines.length} to change` : "server_settings: unchanged",
    ...settingLines,
    fileLines.length ? `${SERVER_CONFIG_PATH}: ${fileLines.length} to change (ports, addresses and rcon kept)` : `${SERVER_CONFIG_PATH}: unchanged`,
    ...fileLines,
    ...(extra.length ? extra.map((e) => e.line) : [`${OWNER_CONFIG_PATH} and ${STAFF_PATH}: unchanged`]),
  ];
  if (!yes) {
    lines.push("dry run: nothing written; add --yes to write (the change applies on the next start or restart)");
    return { ok: true, lines: lines.map(allSecrets), warnings: staged.warnings.map(redact), written: [] };
  }
  const written = [];
  if (Object.keys(changes).length) {
    const p = await client.patchServerSettings(changes);
    if (!p.ok) return { ok: false, reason: allSecrets(p.reason), lines: lines.map(allSecrets), warnings: [] };
    written.push(`server_settings (${Object.keys(changes).length})`);
  }
  if (fileLines.length) {
    const u = await client.put(SERVER_CONFIG_PATH, Buffer.from(`${JSON.stringify(merged, null, 2)}\n`, "utf8"));
    if (!u.ok) return { ok: false, reason: allSecrets(u.reason), lines: lines.map(allSecrets), warnings: [] };
    written.push(SERVER_CONFIG_PATH);
  }
  for (const e of extra) {
    const u = await client.put(e.path, e.bytes);
    if (!u.ok) return { ok: false, reason: allSecrets(u.reason), lines: lines.map(allSecrets), warnings: [] };
    written.push(e.path);
  }
  lines.push(written.length ? `written: ${written.join(", ")}; restart to apply (host 0grind restart --yes)` : "nothing to write");
  return { ok: true, lines: lines.map(allSecrets), warnings: staged.warnings.map(redact), written };
}

async function configPushOnly(client, { dir, repoDir, adminsPending, yes, only }) {
  // The roster's admins matter only when admins is one of the pushed fields; a pending administrator uid
  // must not block a name or password push.
  const staged = stageServerJson(dir, { repoDir, adminsPending: adminsPending || !only.includes("admins") });
  if (!staged.ok) return { ok: false, reason: staged.reason, lines: [], warnings: [] };
  const ours = staged.server;
  const sel = onlySettings(toServerSettings(ours), only);
  if (sel.error) return { ok: false, reason: sel.error, lines: [], warnings: [] };
  const cur = await client.serverSettings();
  if (!cur.ok) return { ok: false, reason: cur.reason, status: cur.status, lines: [], warnings: [] };
  const current = cur.data?.settings ?? {};
  const allSecrets = makeRedactor([...secretValues(ours), ...secretValues(current)]);
  const { changes, lines: settingLines } = diffSettings(current, sel.wanted);
  const lines = [settingLines.length ? `server_settings (only ${only.join(", ")}): ${settingLines.length} to change` : `server_settings (only ${only.join(", ")}): unchanged`, ...settingLines];
  if (!yes) {
    lines.push("dry run: nothing written; add --yes to write (the change applies on the next restart)");
    return { ok: true, lines: lines.map(allSecrets), warnings: staged.warnings.map(allSecrets), written: [] };
  }
  const written = [];
  if (Object.keys(changes).length) {
    const p = await client.patchServerSettings(changes);
    if (!p.ok) return { ok: false, reason: allSecrets(p.reason), lines: lines.map(allSecrets), warnings: [] };
    written.push(`server_settings (${Object.keys(changes).length})`);
  }
  lines.push(written.length ? `written: ${written.join(", ")}; restart to apply` : "nothing to write");
  return { ok: true, lines: lines.map(allSecrets), warnings: staged.warnings.map(allSecrets), written };
}

// ---------------------------------------------------------------- logs and saves

const folders = (data) => (data?.files ?? []).filter((f) => f.type === "folder" || (f.type === "link" && f.link_target_type === "folder"));

// The newest /logs/<stamp> folder that holds a console.log, copied to
// <dir>/profile/logs/logs_<stamp>/console.log. Returns { ok, file?, reason? }.
export async function logsPull(client, { dir, all = false } = {}) {
  const l = await client.list("/logs");
  if (!l.ok) return l;
  const stamps = folders(l.data).map((f) => f.name).sort();
  for (let i = stamps.length - 1; i >= 0; i--) {
    const remote = `/logs/${stamps[i]}`;
    const inner = await client.list(remote);
    if (!inner.ok) return inner;
    const files = (inner.data?.files ?? []).filter((f) => f.type === "file");
    if (!files.some((f) => f.name === "console.log")) continue;
    const local = join(dir, "profile", "logs", `logs_${stamps[i].replace(/[^A-Za-z0-9._-]/g, "_")}`);
    mkdirSync(local, { recursive: true });
    const pulled = [];
    for (const f of all ? files : files.filter((x) => x.name === "console.log")) {
      const g = await client.get(`${remote}/${f.name}`);
      if (!g.ok) return g;
      writeFileSync(join(local, f.name), g.bytes);
      pulled.push(f.name);
    }
    return { ok: true, remote, file: join(local, "console.log"), pulled };
  }
  return { ok: false, reason: `no /logs/<stamp>/console.log on the box (${stamps.length} log folder(s))` };
}

// Every file under a remote folder: [{ path, size }]. Follows the listing's 256-entry cap honestly.
async function walk(client, root, out, warnings, depth = 0) {
  if (depth > 12) {
    warnings.push(`${root}: deeper than 12 levels, skipped`);
    return { ok: true };
  }
  const l = await client.list(root);
  if (!l.ok) return l;
  if (l.data?.full === false) warnings.push(`${root}: the listing hit 0grind's 256-entry cap; the copy is incomplete (use SFTP for this folder)`);
  for (const f of l.data?.files ?? []) {
    const p = `${root}/${f.name}`;
    const kind = f.type === "link" ? f.link_target_type : f.type;
    if (kind === "folder") {
      const r = await walk(client, p, out, warnings, depth + 1);
      if (!r.ok) return r;
    } else if (kind === "file") out.push({ path: p, size: f.size_bytes });
  }
  return { ok: true };
}

export const SAVE_ROOT = "/profile/.save";

// /profile/.save into <dir>/backups/<stamp>-remote/ (0grind's own backups skip world saves). Files land
// in a .partial folder first and it is renamed only when every size matches the listing.
export async function savesPull(client, { dir, now = new Date() } = {}) {
  const files = [];
  const warnings = [];
  const w = await walk(client, SAVE_ROOT, files, warnings);
  if (!w.ok) return w;
  if (!files.length) return { ok: false, reason: `no file under ${SAVE_ROOT} on the box (no save yet?)` };
  const dest = freshDir(join(dir, "backups", `${stamp(now)}-remote`));
  const partial = `${dest}.partial`;
  let bytes = 0;
  for (const f of files) {
    const g = await client.get(f.path);
    if (!g.ok) return { ...g, reason: `${g.reason} (the partial copy is in ${partial})` };
    if (typeof f.size === "number" && g.bytes.length !== f.size) {
      return { ok: false, reason: `${f.path}: got ${g.bytes.length} bytes, the listing says ${f.size} (the save may have been written meanwhile; pull again). Partial copy: ${partial}` };
    }
    const local = join(partial, ...f.path.slice(SAVE_ROOT.length + 1).split("/"));
    mkdirSync(dirname(local), { recursive: true });
    writeFileSync(local, g.bytes);
    bytes += g.bytes.length;
  }
  renameSync(partial, dest);
  return { ok: true, dest, files: files.length, bytes, warnings };
}

// The System-page settings the flagship needs (labels as the panel shows them). Each wrong one is a line.
export function systemSettingsWarnings(settings) {
  const s = settings ?? {};
  const out = [];
  if (s.restart_on_failure === false) out.push("System: Restart on Failure is off; a crash leaves the server down");
  if (s.armareforger_load_session_save === false) out.push("System: Enable Loading Session Save is off; every restart starts a fresh world");
  if (s.armareforger_hack_postgame_session_clear === true) out.push("System: PostGame Restart - Clear Saved Sessions is on; it deletes ALL saves");
  if (s.armareforger_hack_autorecover_sessions_crash === true) out.push("System: Attempt Auto-Recovering from Max Sessions Bug is on; it deletes ALL saves on that crash");
  if (s.armareforger_a2s_enabled === false) out.push("System: Enable Public A2S Access is off; the Server Browser and host players cannot query it");
  return out;
}

// The System-page values the flagship needs. armareforger_load_session_save is the real "Enable Loading
// Session Save" (the launch flag); server_settings.load_session_save is the Server Settings page's own
// toggle (2026-11-15: that one was already on while this was off, and the saves were not loaded).
export const FLAGSHIP_SYSTEM_SETTINGS = {
  restart_on_failure: true,
  armareforger_load_session_save: true,
  armareforger_hack_postgame_session_clear: false,
  armareforger_hack_autorecover_sessions_crash: false,
  armareforger_a2s_enabled: true,
};

// The keys of FLAGSHIP_SYSTEM_SETTINGS that differ from the box, and the diff lines.
export function systemSettingsPlan(current) {
  const changes = {};
  const lines = [];
  for (const [k, v] of Object.entries(FLAGSHIP_SYSTEM_SETTINGS)) {
    if (current?.[k] === v) continue;
    changes[k] = v;
    lines.push(`  ~ ${k}: ${JSON.stringify(current?.[k])} -> ${JSON.stringify(v)}`);
  }
  return { changes, lines };
}

export function isReadOnlyRcon(command) {
  const c = String(command ?? "").trim().split(/\s+/)[0].toLowerCase();
  return READ_ONLY_RCON.has(c.startsWith("#") ? c : `#${c}`);
}

export function fileSize(p) {
  try {
    return statSync(p).size;
  } catch {
    return 0;
  }
}
