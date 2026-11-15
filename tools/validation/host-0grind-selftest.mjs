// tools/validation/host-0grind-selftest.mjs
// inputs: tools/host/**, server/flagship/**, tools/commands/staff.mjs
//
// Proves `tools\cli host remote 0grind ...` against a fake 0grind API on 127.0.0.1 (no live call):
// the bearer header, the token never printed (not even when the API echoes it), the 401 panel hint,
// the 1 request per second spacing, --yes before start/stop/restart/push/rcon writes, config push as a
// masked diff that writes nothing without --yes and keeps the box's ports, bind address and RCON block,
// logs pull into a host-shaped folder, saves pull with size checks, and the read-only RCON allowlist.
// red-proof: with the start/stop/restart --yes check removed this printed
// "start without --yes reached the API (POST /armareforger/298627/start)" and exited 1 (2026-11-15).
// red-proof: with createClient's redact() dropped from the error reason this printed
// "the token was printed" and exited 1 (2026-11-15).
import { createServer } from "node:http";
import { cpSync, existsSync, mkdirSync, mkdtempSync, readFileSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import { safeRemoveTree } from "../link-safety.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const imp = (p) => import(pathToFileURL(join(ROOT, ...p.split("/"))).href);
const cmd = await imp("tools/host/commands/remote.mjs");
const { toServerSettings, mergeServerConfig, isReadOnlyRcon, MIN_INTERVAL_MS } = await imp("tools/host/remote-0grind.mjs");
const { newestConsoleLog, findSavePoints } = await imp("tools/host/host.mjs");

const failures = [];
const fail = (m) => failures.push(m);
const TOKEN = "tok_9f8e7d6c5b4a39281706f5e4d3c2b1a0";
const PASSWORD = "alphaGate-71";
const ADMIN_PW = "adminPw-5523";
const RCON_PW = "rconBoxPw-9";
const SID = "298627";

// ---- fake box state
const theirsConfig = {
  bindAddress: "0.0.0.0",
  bindPort: 2023,
  publicAddress: "144.76.90.77",
  publicPort: 2023,
  a2s: { address: "0.0.0.0", port: 3023 },
  rcon: { address: "0.0.0.0", port: 5023, password: RCON_PW, permission: "admin" },
  game: { name: "0grind default", password: "", passwordAdmin: "old", admins: [], scenarioId: "{X}Missions/Default.conf", maxPlayers: 16, mods: [] },
};
const state = {
  settings: { password: "", admin_password: "old", scenario_id: "{X}Missions/Default.conf", server_name: "0grind default", visible: false, mods: [], rcon_port: 5023, rcon_password: RCON_PW, rcon_permission: "admin", admins: [] },
  files: new Map([
    ["/ServerConfig.json", Buffer.from(JSON.stringify(theirsConfig))],
    ["/logs/2026-11-14_06-00-00/console.log", Buffer.from("old run\n")],
    ["/logs/2026-11-15_06-00-01/console.log", Buffer.from("12:00 DEFAULT : Entered online game state.\n")],
    ["/logs/2026-11-15_06-00-01/script.log", Buffer.from("s\n")],
    ["/logs/2026-11-15_18-00-00/error.log", Buffer.from("no console here\n")],
    ["/profile/.save/game/C638C03B775C31BB-LF_Everon1989/playthrough000/savepoint000/meta-info.json", Buffer.from('{"m_sGameVersion":"1.9.0.37"}')],
    ["/profile/.save/game/C638C03B775C31BB-LF_Everon1989/playthrough000/savepoint000/data.bin", Buffer.from([1, 2, 3, 4, 5])],
  ]),
};
state.system = { armareforger_battleye_rcon_password: "beRcon-77", armareforger_a2s_enabled: true, restart_on_failure: false, armareforger_load_session_save: false, armareforger_hack_postgame_session_clear: true, armareforger_hack_autorecover_sessions_crash: true };
const calls = [];

function listing(path) {
  const names = new Map();
  for (const [p, b] of state.files) {
    if (!p.startsWith(`${path}/`)) continue;
    const rest = p.slice(path.length + 1).split("/");
    if (rest.length === 1) names.set(rest[0], { name: rest[0], type: "file", size_bytes: b.length, modified: 0, link_target: null, link_target_type: null });
    else if (!names.has(rest[0])) names.set(rest[0], { name: rest[0], type: "folder", size_bytes: 0, modified: 0, link_target: null, link_target_type: null });
  }
  return { path, files: [...names.values()], full: true };
}

const server = createServer((req, res) => {
  const chunks = [];
  req.on("data", (c) => chunks.push(c));
  req.on("end", () => {
    const url = new URL(req.url, "http://x");
    const body = Buffer.concat(chunks);
    calls.push({ method: req.method, path: url.pathname, query: url.searchParams.get("path"), auth: req.headers.authorization, body, t: Date.now() });
    const json = (status, obj) => {
      res.writeHead(status, { "Content-Type": "application/json" });
      res.end(JSON.stringify(obj));
    };
    if (req.headers.authorization !== `Bearer ${TOKEN}`) return json(401, { success: false, reason: "Unauthorized" });
    const p = url.pathname.replace(/^\/v2/, "");
    if (p === "/service/leak") return json(500, { success: false, reason: `internal error for token ${TOKEN}` });
    if (p === `/service/${SID}` && req.method === "GET") return json(200, { success: true, data: { status: "running", alias: "LF flagship" } });
    if (p === `/armareforger/${SID}/system_settings` && req.method === "PATCH") {
      const c = JSON.parse(body.toString("utf8"));
      Object.assign(state.system, c);
      return json(200, { success: true, data: { changed: Object.keys(c) } });
    }
    if (/^\/armareforger\/298627\/(start|stop|restart)$/.test(p)) return json(200, { success: true, data: null });
    if (p === `/armareforger/${SID}/server_settings` && req.method === "GET") return json(200, { success: true, data: { settings: state.settings } });
    if (p === `/armareforger/${SID}/server_settings` && req.method === "PATCH") {
      const c = JSON.parse(body.toString("utf8"));
      Object.assign(state.settings, c);
      return json(200, { success: true, data: { changed: Object.keys(c) } });
    }
    if (p === `/armareforger/${SID}/system_settings`) return json(200, { success: true, data: { settings: state.system } });
    if (p === `/armareforger/${SID}/rcon`) return json(200, { success: true, data: `Processing Command: ${JSON.parse(body.toString("utf8")).command}` });
    if (p === `/service/${SID}/filesystem/files`) return json(200, { success: true, data: listing(url.searchParams.get("path")) });
    if (p === `/service/${SID}/filesystem/file` && req.method === "GET") {
      const f = state.files.get(url.searchParams.get("path"));
      if (!f) return json(404, { success: false, reason: "File not found" });
      res.writeHead(200, { "Content-Type": "application/octet-stream" });
      return res.end(f);
    }
    if (p === `/service/${SID}/filesystem/file` && req.method === "PUT") {
      state.files.set(url.searchParams.get("path"), body);
      return json(200, { success: true, data: url.searchParams.get("path") });
    }
    return json(404, { success: false, reason: `Requested URL ${url.pathname} not found` });
  });
});
await new Promise((r) => server.listen(0, "127.0.0.1", r));
const base = `http://127.0.0.1:${server.address().port}/v2`;

const work = mkdtempSync(join(tmpdir(), "lf-0grind-"));
const remote = join(work, "host-0grind");
mkdirSync(remote, { recursive: true });
const tokenFile = join(work, "token");
writeFileSync(tokenFile, `${TOKEN}\n`);
const repoDir = join(work, "flagship");
cpSync(join(ROOT, "server", "flagship"), repoDir, { recursive: true });
writeFileSync(join(remote, "secrets.json"), JSON.stringify({ game: { password: PASSWORD, passwordAdmin: ADMIN_PW } }));

const env = { LF_0GRIND_API: base, LF_0GRIND_TOKEN_FILE: tokenFile, LF_0GRIND_DIR: remote, LF_0GRIND_A2S_HOST: "127.0.0.1", LF_0GRIND_A2S_PORT: "9", LF_0GRIND_A2S_TIMEOUT_MS: "200" };
let printed = "";
async function run(args, extra = {}) {
  const orig = console.log;
  let out = "";
  console.log = (...a) => {
    out += `${a.join(" ")}\n`;
  };
  let code;
  try {
    code = await cmd.main(args, { root: ROOT, hostDir: join(work, "host"), env: { ...env, ...(extra.env ?? {}) }, minIntervalMs: 60, ipLookup: () => "203.0.113.7", repoDir });
  } finally {
    console.log = orig;
  }
  printed += out;
  return { code, out };
}
const writes = () => calls.filter((c) => c.method !== "GET");

try {
  // ---- status, auth header
  let r = await run(["0grind", "status"]);
  if (r.code !== 0 || !/status="running"/.test(r.out)) fail(`status did not read the service (${r.code}): ${r.out}`);
  if (calls[0]?.auth !== `Bearer ${TOKEN}`) fail("the API call does not send Authorization: Bearer <token>");
  if (!/a2s: 127\.0\.0\.1:9 no answer/.test(r.out)) fail("status does not report the A2S query");
  if (!/WARNING System: Restart on Failure is off/.test(r.out) || !/deletes ALL saves/.test(r.out)) fail(`status does not warn about Restart on Failure off and a save-wiping toggle: ${r.out}`);
  if (!/Loading Session Save is off/.test(r.out)) fail("status does not warn about session save loading while it is off");
  if (!/Auto-Recovering from Max Sessions Bug is on/.test(r.out)) fail("status does not warn about the save-deleting auto-recover option");

  // ---- 401 prints the panel clicks
  const badToken = join(work, "bad");
  writeFileSync(badToken, "wrong-token-123");
  r = await run(["0grind", "config", "show"], { env: { LF_0GRIND_TOKEN_FILE: badToken } });
  if (r.code === 0) fail("a 401 did not fail the command");
  if (!/API Access/.test(r.out) || !/Allowed IPv4 Addresses/.test(r.out) || !/203\.0\.113\.7/.test(r.out)) fail(`a 401 does not say what to click on the panel: ${r.out}`);
  if (r.out.includes("wrong-token-123")) fail("a rejected token was printed");

  // ---- the API echoing the token in an error
  r = await run(["0grind", "status"], { env: { LF_0GRIND_SERVICE: "leak" } });
  if (!/\*\*\*/.test(r.out)) fail(`an error that carries the token is not redacted: ${r.out}`);

  // ---- --yes gates
  const before = writes().length;
  for (const a of ["start", "stop", "restart"]) {
    r = await run(["0grind", a]);
    if (r.code === 0 || !/--yes/.test(r.out)) fail(`${a} without --yes was not refused`);
  }
  if (writes().length !== before) fail(`start without --yes reached the API (${writes().slice(before).map((c) => `${c.method} ${c.path.replace(/^\/v2/, "")}`).join(", ")})`);
  r = await run(["0grind", "restart", "--yes"]);
  if (r.code !== 0 || !writes().some((c) => c.path.endsWith(`/armareforger/${SID}/restart`) && c.method === "POST")) fail("restart --yes did not POST /restart");

  // ---- config show masks every secret
  r = await run(["0grind", "config", "show"]);
  if (r.code !== 0 || !/server_settings/.test(r.out) || !/ServerConfig\.json/.test(r.out)) fail(`config show failed: ${r.out}`);
  if (r.out.includes(RCON_PW) || r.out.includes("beRcon-77")) fail("config show printed an RCON password");

  // ---- config push: dry run writes nothing
  const w0 = writes().length;
  r = await run(["0grind", "config", "push", "--admins-pending"]);
  if (r.code !== 0) fail(`config push dry run failed: ${r.out}`);
  if (writes().length !== w0) fail("config push without --yes wrote to the box");
  if (!/dry run/.test(r.out) || !/server_name/.test(r.out) || !/password: \(empty\) -> \(set, masked\)/.test(r.out)) fail(`config push dry run does not show a masked diff: ${r.out}`);
  // ---- config push --yes
  r = await run(["0grind", "config", "push", "--admins-pending", "--yes"]);
  if (r.code !== 0) fail(`config push --yes failed: ${r.out}`);
  const patch = writes().find((c) => c.method === "PATCH");
  const sent = patch ? JSON.parse(patch.body.toString("utf8")) : {};
  if (sent.password !== PASSWORD || sent.admin_password !== ADMIN_PW) fail("config push did not send the password and admin password from secrets.json");
  if (sent.server_name !== "Life Framework | Everon 1989 (Alpha)" || sent.scenario_id !== "{C638C03B775C31BB}Missions/LF_Everon1989.conf") fail("config push did not map the name and scenario");
  if (Object.keys(sent).some((k) => k.startsWith("rcon"))) fail("config push changed an rcon_* setting");
  const pushed = JSON.parse(state.files.get("/ServerConfig.json").toString("utf8"));
  if (pushed.bindPort !== 2023 || pushed.publicPort !== 2023 || pushed.a2s?.port !== 3023 || pushed.bindAddress !== "0.0.0.0") fail(`config push replaced the box's ports or bind address (${JSON.stringify({ b: pushed.bindPort, p: pushed.publicPort, a: pushed.a2s })})`);
  if (pushed.rcon?.password !== RCON_PW || pushed.rcon?.port !== 5023) fail("config push replaced the box's RCON block");
  if (pushed.game?.name !== "Life Framework | Everon 1989 (Alpha)" || pushed.game?.password !== PASSWORD || pushed.game?.maxPlayers !== 16) fail("config push did not write our game block (with their slot count)");
  const owner = state.files.get("/profile/LifeFramework/config.json");
  if (!owner || JSON.parse(owner.toString("utf8")).dayCycle === undefined) fail("config push did not upload the owner config.json to $profile:LifeFramework/");
  if (!state.files.get("/profile/LifeFramework/staff.txt")?.toString("utf8").includes("#pending administrator")) fail("config push did not upload staff.txt");
  r = await run(["0grind", "config", "push", "--admins-pending"]);
  if (!/LifeFramework\/staff\.txt: unchanged/.test(r.out)) fail(`a second push re-uploads the owner config or roster: ${r.out}`);
  if (!/server_settings: unchanged/.test(r.out) || !/ServerConfig\.json: unchanged/.test(r.out)) fail(`a second push is not a no-op: ${r.out}`);

  // ---- config push --only: just those server_settings keys, scenario, mods and files untouched
  Object.assign(state.settings, { server_name: "0grind default", password: "", scenario_id: "{X}Missions/Default.conf", mods: ["keep"] });
  const putsBefore = writes().filter((c) => c.method === "PUT").length;
  const w2 = writes().length;
  r = await run(["0grind", "config", "push", "--admins-pending", "--only", "name,password"]);
  if (r.code !== 0 || writes().length !== w2 || !/only name, password/.test(r.out)) fail(`config push --only dry run wrote or failed: ${r.out}`);
  r = await run(["0grind", "config", "push", "--admins-pending", "--only", "name,password", "--yes"]);
  const onlyPatch = writes().filter((c) => c.method === "PATCH").pop();
  const onlySent = onlyPatch ? JSON.parse(onlyPatch.body.toString("utf8")) : {};
  if (r.code !== 0 || Object.keys(onlySent).sort().join() !== "password,server_name") fail(`config push --only sent ${Object.keys(onlySent).join(",")} instead of server_name and password: ${r.out}`);
  if (state.settings.scenario_id !== "{X}Missions/Default.conf" || state.settings.mods[0] !== "keep") fail("config push --only changed the scenario or the mods");
  if (writes().filter((c) => c.method === "PUT").length !== putsBefore) fail("config push --only wrote a file to the box");
  r = await run(["0grind", "config", "push", "--only", "name,password"]);
  if (r.code !== 0 || !/only name, password/.test(r.out)) fail(`config push --only needs --admins-pending for fields that are not admins: ${r.out}`);
  r = await run(["0grind", "config", "push", "--admins-pending", "--only", "bogus"]);
  if (r.code === 0 || !/not a game field/.test(r.out)) fail("config push --only accepted an unknown field");

  // ---- system fix: the flagship's System-page settings
  const w3 = writes().length;
  r = await run(["0grind", "system", "fix"]);
  if (r.code !== 0 || writes().length !== w3 || !/armareforger_load_session_save: false -> true/.test(r.out) || !/dry run/.test(r.out)) fail(`system fix without --yes wrote or hid the diff: ${r.out}`);
  r = await run(["0grind", "system", "fix", "--yes"]);
  const sysPatch = writes().filter((c) => c.method === "PATCH" && c.path.endsWith("/system_settings")).pop();
  const sysSent = sysPatch ? JSON.parse(sysPatch.body.toString("utf8")) : {};
  if (r.code !== 0 || sysSent.restart_on_failure !== true || sysSent.armareforger_load_session_save !== true || sysSent.armareforger_hack_autorecover_sessions_crash !== false || sysSent.armareforger_hack_postgame_session_clear !== false) fail(`system fix --yes sent ${JSON.stringify(sysSent)}`);
  if ("armareforger_a2s_enabled" in sysSent) fail("system fix re-sent a setting that was already right");
  r = await run(["0grind", "status"]);
  if (/WARNING/.test(r.out)) fail(`status still warns after system fix: ${r.out}`);
  r = await run(["0grind", "system", "fix"]);
  if (!/already as the flagship needs/.test(r.out)) fail("a second system fix is not a no-op");

  // ---- logs pull: the newest folder with a console.log, host-shaped
  r = await run(["0grind", "logs", "pull"]);
  const log = newestConsoleLog(join(remote, "profile"));
  if (r.code !== 0 || !log || !/logs_2026-11-15_06-00-01/.test(log) || !readFileSync(log, "utf8").includes("Entered online game state")) fail(`logs pull did not land the newest console.log where host triage looks (${log}): ${r.out}`);

  // ---- saves pull
  r = await run(["0grind", "saves", "pull"]);
  const m = /-> (\S+-remote)/.exec(r.out);
  if (r.code !== 0 || !m) fail(`saves pull failed: ${r.out}`);
  else {
    if (!existsSync(join(m[1], "game", "C638C03B775C31BB-LF_Everon1989", "playthrough000", "savepoint000", "data.bin"))) fail("saves pull did not mirror the save tree");
    if (findSavePoints(m[1]).length !== 1) fail("the pulled save is not a backup host restore can read");
  }
  state.files.set("/profile/.save/game/x/broken.bin", Buffer.from("abc"));
  const realGet = state.files.get.bind(state.files);
  state.files.get = (k) => (k === "/profile/.save/game/x/broken.bin" ? Buffer.from("a") : realGet(k));
  r = await run(["0grind", "saves", "pull"]);
  state.files.get = realGet;
  if (r.code === 0 || !/listing says 3/.test(r.out)) fail("saves pull accepted a file whose size does not match the listing");

  // ---- rcon
  r = await run(["0grind", "rcon", "#players"]);
  if (r.code !== 0 || !/Processing Command: #players/.test(r.out)) fail("rcon #players did not run");
  const w1 = writes().length;
  r = await run(["0grind", "rcon", "#shutdown"]);
  if (r.code === 0 || writes().length !== w1) fail("rcon #shutdown ran without --yes");
  if (!isReadOnlyRcon("players") || isReadOnlyRcon("#kick 3") || isReadOnlyRcon("#restart")) fail("the read-only RCON allowlist is wrong");

  // ---- the rate limit: no two calls closer than the interval
  const gaps = calls.slice(1).map((c, i) => c.t - calls[i].t);
  if (Math.min(...gaps) < 50) fail(`two API calls were ${Math.min(...gaps)} ms apart (interval 60 ms)`);
  if (MIN_INTERVAL_MS < 1000) fail(`the default interval is ${MIN_INTERVAL_MS} ms; 0grind allows 1 request per second`);

  // ---- pure mapping
  const s = toServerSettings({ game: { gameProperties: { battlEye: true, VONDisableUI: false } }, operating: { lobbyPlayerSynchronise: true, joinQueue: { maxSize: 4 } } });
  if (s.enable_battleye !== true || s.von_disable_ui !== false || s.lobby_player_synchronise !== true || s.join_queue_max_size !== 4) fail("toServerSettings does not map gameProperties and operating");
  const mm = mergeServerConfig({ game: { maxPlayers: 16 } }, { bindPort: 2001, a2s: { port: 17777 }, game: { maxPlayers: 40 } });
  if ("bindPort" in mm || "a2s" in mm || mm.game.maxPlayers !== 16) fail("mergeServerConfig pushes our ports or slot count onto a box that has none");

  if (printed.includes(TOKEN)) fail("the token was printed");
  for (const v of [PASSWORD, ADMIN_PW, RCON_PW]) if (printed.includes(v)) fail("a password was printed");
} finally {
  server.close();
  safeRemoveTree(work);
}

if (failures.length) {
  for (const f of failures) console.log(`FAIL ${f}`);
  console.log(`host-0grind-selftest: ${failures.length} failure(s)`);
  process.exit(1);
}
console.log("host-0grind-selftest: OK (fake 0grind API: bearer auth, token redaction, 401 hint, --yes gates, masked config push keeping ports and rcon, logs and saves pull, rcon allowlist, 1 req/s)");
