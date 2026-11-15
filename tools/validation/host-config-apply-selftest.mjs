// tools/validation/host-config-apply-selftest.mjs
// inputs: tools/host/**, tools/commands/staff.mjs, server/flagship/**
//
// Proves `tools\cli host config apply [<host dir>]` (#1322) on a fixture host dir: the written
// server.json has the host's secrets and the roster's administrators, the written config.json has
// the webhook URL, the printed summary holds no secret, a secret found in a repo file refuses the
// run, a missing secrets.json leaves the password empty and says so, a running host refuses, and
// nothing is written on a refusal. `--roster <file>` (#1672) swaps in a roster for a scratch host: a filled uid
// applies while the committed roster is #pending, the repo roster alone still refuses, and the real host folder
// refuses `--roster`.
// red-proof (#1672): before `rosterFile` existed, with the repo roster #pending:
// "FAIL --roster with a filled uid was refused: the staff roster cannot give game.admins: ..." (2026-11-15).
// red-proof: before tools/host/config-apply.mjs existed this file died on the import:
// "Cannot find module '.../tools/host/config-apply.mjs'" (2026-11-15).

import { cpSync, existsSync, mkdirSync, mkdtempSync, readFileSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import { spawnSync } from "node:child_process";
import { safeRemoveTree } from "../link-safety.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const { applyConfig } = await import(pathToFileURL(join(ROOT, "tools", "host", "config-apply.mjs")).href);
const { hostDir, hostPaths, writeHostState } = await import(pathToFileURL(join(ROOT, "tools", "host", "host.mjs")).href);

const failures = [];
const fail = (m) => failures.push(m);

const PASSWORD = "pw-Alpha-7731";
const ADMIN_PASSWORD = "adm-Beta-9942";
const WEBHOOK = "https://discord.com/api/webhooks/123456/SECRETTOKEN-xyz";
const ADMIN_UID = "a1b2c3d4-0000-4000-8000-000000000001";
const MOD_UID = "modmodmod-2";
const ANNOUNCE = "https://discord.com/api/webhooks/777/ANNOUNCE-TOKEN-qq";
const SECRETS = [PASSWORD, ADMIN_PASSWORD, WEBHOOK, "SECRETTOKEN-xyz", ANNOUNCE, "ANNOUNCE-TOKEN-qq"];

const base = mkdtempSync(join(tmpdir(), "lf-host-config-apply-"));
const repo = join(base, "repo");
mkdirSync(repo, { recursive: true });
for (const f of ["config.json", "server.json"]) cpSync(join(ROOT, "server", "flagship", f), join(repo, f));
writeFileSync(join(repo, "staff.txt"), `# fixture roster\n${ADMIN_UID} administrator # Fixture Admin\n${MOD_UID} moderator # Fixture Mod\n`);

const read = (p) => JSON.parse(readFileSync(p, "utf8"));
const repoServer = () => read(join(repo, "server.json"));
const repoConfig = () => read(join(repo, "config.json"));
const writeRepo = (name, obj) => writeFileSync(join(repo, name), `${JSON.stringify(obj, null, 2)}\n`);

let n = 0;
function freshHost(secrets) {
  const dir = join(base, `host${++n}`);
  mkdirSync(dir, { recursive: true });
  if (secrets !== undefined) writeFileSync(join(dir, "secrets.json"), typeof secrets === "string" ? secrets : JSON.stringify(secrets, null, 2));
  return dir;
}
// The two paths `host drift` (#1462) reads; apply must write exactly these.
const outServer = (dir) => join(hostPaths(dir).profile, "server.json");
const outConfig = (dir) => join(hostPaths(dir).profile, "profile", "LifeFramework", "config.json");
const leaks = (text) => SECRETS.filter((s) => text.includes(s));

try {
  const goodSecrets = { game: { password: PASSWORD, passwordAdmin: ADMIN_PASSWORD }, staffWebhookUrl: WEBHOOK };

  // 1. The happy path.
  const dir = freshHost(goodSecrets);
  const r = applyConfig(dir, { repoDir: repo });
  if (!r.ok) fail(`apply was refused: ${r.reason}`);
  else {
    const s = read(outServer(dir));
    if (s.game?.password !== PASSWORD) fail("server.json has no game.password");
    if (s.game?.passwordAdmin !== ADMIN_PASSWORD) fail("server.json has no game.passwordAdmin");
    if (JSON.stringify(s.game?.admins) !== JSON.stringify([ADMIN_UID])) fail(`server.json game.admins is not the roster's administrators: ${JSON.stringify(s.game?.admins)}`);
    const base0 = repoServer();
    if (s.game?.name !== base0.game.name || s.game?.scenarioId !== base0.game.scenarioId || JSON.stringify(s.game?.mods) !== JSON.stringify(base0.game.mods)) fail("server.json lost the repo's name, scenario or mods");
    const c = read(outConfig(dir));
    if (c.staff?.webhookUrl !== WEBHOOK) fail("config.json has no staff.webhookUrl");
    if (c.dayCycle?.dayLengthMinutes !== repoConfig().dayCycle.dayLengthMinutes) fail("config.json lost the repo's dayCycle");
    const out = [...(r.lines ?? []), ...(r.warnings ?? [])].join("\n");
    if (!out.trim()) fail("the summary is empty");
    for (const l of leaks(out)) fail(`the printed summary holds a secret: ${l.slice(0, 4)}...`);
    if (!/game\.password/.test(out)) fail("the summary does not mention game.password (masked)");
    // The repo files stay secret-free.
    for (const f of ["config.json", "server.json"]) for (const l of leaks(readFileSync(join(repo, f), "utf8"))) fail(`a secret was written into the repo file ${f}: ${l.slice(0, 4)}...`);
  }

  // 2. A second apply with nothing changed says so and still holds the secrets.
  const again = applyConfig(dir, { repoDir: repo });
  if (!again.ok) fail(`the second apply was refused: ${again.reason}`);
  else if (!/unchanged|no change/i.test((again.lines ?? []).join("\n"))) fail("the second apply does not say nothing changed");

  // 3. A secret key in a repo file refuses the run and writes nothing.
  const keepServer = readFileSync(join(repo, "server.json"), "utf8");
  const keepConfig = readFileSync(join(repo, "config.json"), "utf8");
  const leaky = [
    ["server.json", (o) => { o.game.password = "hunter2"; }, /game\.password/],
    ["server.json", (o) => { o.game.passwordAdmin = "x"; }, /passwordAdmin/],
    ["server.json", (o) => { o.game.admins = ["someone"]; }, /game\.admins/],
    ["config.json", (o) => { o.staff = { webhookUrl: "https://discord.com/api/webhooks/1/x" }; }, /webhook|URL/i],
  ];
  for (const [file, edit, want] of leaky) {
    const o = file === "server.json" ? repoServer() : repoConfig();
    edit(o);
    writeRepo(file, o);
    const d = freshHost(goodSecrets);
    const res = applyConfig(d, { repoDir: repo });
    if (res.ok) fail(`a secret in ${file} did not refuse the run`);
    else if (!want.test(res.reason ?? "")) fail(`the refusal for ${file} does not name the key: ${res.reason}`);
    if (existsSync(outServer(d)) || existsSync(outConfig(d))) fail(`a refused run (secret in ${file}) wrote a file`);
    for (const l of leaks(res.reason ?? "")) fail(`the refusal holds a secret: ${l.slice(0, 4)}...`);
    writeFileSync(join(repo, "server.json"), keepServer);
    writeFileSync(join(repo, "config.json"), keepConfig);
  }

  // 4. No secrets.json: the password stays empty and the run says so.
  const bare = freshHost();
  const nr = applyConfig(bare, { repoDir: repo });
  if (!nr.ok) fail(`apply without secrets.json was refused: ${nr.reason}`);
  else {
    const s = read(outServer(bare));
    if (s.game?.password !== "") fail("game.password is not empty without secrets.json");
    const txt = [...(nr.lines ?? []), ...(nr.warnings ?? [])].join("\n");
    if (!/secrets\.json/.test(txt) || !/password/i.test(txt) || !/empty/i.test(txt)) fail(`the run does not say the password is empty: ${txt}`);
    if (read(outConfig(bare)).staff?.webhookUrl) fail("config.json has a webhook URL without secrets.json");
  }

  // 5. A damaged secrets.json refuses and prints no content.
  for (const bad of ["{ not json", JSON.stringify({ game: { password: 12345 } }), JSON.stringify({ staffWebhookUrl: "ftp://x/y" })]) {
    const d = freshHost(bad);
    const res = applyConfig(d, { repoDir: repo });
    if (res.ok) fail(`a damaged secrets.json was accepted: ${bad.slice(0, 20)}`);
    if (existsSync(outServer(d))) fail("a damaged secrets.json still wrote server.json");
  }

  // 6. The host runs: refuse and write nothing.
  const live = freshHost(goodSecrets);
  writeHostState(live, { pid: process.pid });
  const lr = applyConfig(live, { repoDir: repo });
  if (lr.ok || !/running/i.test(lr.reason ?? "")) fail(`a running host was written to or the reason is missing: ${JSON.stringify(lr.reason)}`);
  if (existsSync(outServer(live)) || existsSync(outConfig(live))) fail("a run on a running host wrote a file");

  // 7. A pending administrator (no uid yet) refuses: game.admins cannot be written.
  writeFileSync(join(repo, "staff.txt"), "#pending administrator Jasper\n");
  const pd = freshHost(goodSecrets);
  const pr = applyConfig(pd, { repoDir: repo });
  if (pr.ok || !/admin/i.test(pr.reason ?? "")) fail(`a roster with a pending administrator was applied: ${JSON.stringify(pr.reason)}`);
  // 7b. `--roster <file>` (#1672) replaces <repo>/staff.txt for a scratch host: the repo roster stays #pending, the
  // given file has a uid, and the apply goes through with that uid. Without it the same repo still refuses (7).
  const rosterFile = join(base, "scratch-roster.txt");
  writeFileSync(rosterFile, `${ADMIN_UID} administrator # Rehearsal Admin\n`);
  const rd = freshHost(goodSecrets);
  const rr = applyConfig(rd, { repoDir: repo, rosterFile });
  if (!rr.ok) fail(`--roster with a filled uid was refused: ${rr.reason}`);
  else if (JSON.stringify(read(outServer(rd)).game?.admins) !== JSON.stringify([ADMIN_UID])) fail("--roster did not put its administrator into game.admins");
  const rm = applyConfig(freshHost(goodSecrets), { repoDir: repo, rosterFile: join(base, "no-such-roster.txt") });
  if (rm.ok || !/roster/i.test(rm.reason ?? "")) fail(`a missing --roster file was accepted or has no reason: ${JSON.stringify(rm.reason)}`);
  const rp = applyConfig(freshHost(goodSecrets), { repoDir: repo });
  if (rp.ok || !/admin/i.test(rp.reason ?? "")) fail(`the repo roster (#pending) must still refuse without --roster: ${JSON.stringify(rp.reason)}`);
  // --roster on the real host folder refuses, as the beat scripts refuse it. dryRun: if this ever passed, nothing is written.
  const realHost = hostDir({ ...process.env, LF_HOST_DIR: "" });
  const rh = applyConfig(realHost, { repoDir: repo, rosterFile, dryRun: true });
  if (rh.ok || !/real host/i.test(rh.reason ?? "")) fail(`--roster on the real host folder was accepted or the reason is missing: ${JSON.stringify(rh.reason)}`);
  // through the CLI: the flag is parsed, and the real host folder refuses there too
  const cliRoster = spawnSync(process.execPath, [join(ROOT, "tools", "cli.mjs"), "host", "config", "apply", freshHost(goodSecrets), "--repo", repo, "--roster", rosterFile], {
    cwd: ROOT, encoding: "utf8", timeout: 60000, windowsHide: true, env: { ...process.env, LF_HOST_DIR: join(base, "unused") },
  });
  if (cliRoster.status !== 0) fail(`host config apply --roster exited ${cliRoster.status}: ${`${cliRoster.stdout}${cliRoster.stderr}`.slice(0, 300)}`);
  const cliReal = spawnSync(process.execPath, [join(ROOT, "tools", "cli.mjs"), "host", "config", "apply", realHost, "--repo", repo, "--roster", rosterFile, "--dry-run"], {
    cwd: ROOT, encoding: "utf8", timeout: 60000, windowsHide: true, env: { ...process.env, LF_HOST_DIR: "" },
  });
  if (cliReal.status === 0 || !/real host/i.test(`${cliReal.stdout}${cliReal.stderr}`)) fail(`the CLI accepted --roster on the real host folder: ${`${cliReal.stdout}${cliReal.stderr}`.slice(0, 300)}`);
  writeFileSync(join(repo, "staff.txt"), `${ADMIN_UID} administrator # Fixture Admin\n`);

  // 8. The host dir may not sit inside the repo checkout (a secret there could be committed).
  const inRepo = join(ROOT, "tmp", `lf-config-apply-selftest-${process.pid}`);
  const ir = applyConfig(inRepo, { repoDir: repo });
  if (ir.ok || !/repo/i.test(ir.reason ?? "")) fail(`a host dir under the repo was accepted: ${JSON.stringify(ir.reason)}`);
  if (existsSync(inRepo)) fail("a host dir under the repo was created");

  // 9. The CLI subcommand loads, applies with a host dir argument and prints no secret.
  const cliDir = freshHost(goodSecrets);
  const cli = spawnSync(process.execPath, [join(ROOT, "tools", "cli.mjs"), "host", "config", "apply", cliDir, "--repo", repo], {
    cwd: ROOT, encoding: "utf8", timeout: 60000, windowsHide: true, env: { ...process.env, LF_HOST_DIR: join(base, "unused") },
  });
  const cliOut = `${cli.stdout}${cli.stderr}`;
  if (cli.status !== 0) fail(`host config apply exited ${cli.status}: ${cliOut.slice(0, 300)}`);
  for (const l of leaks(cliOut)) fail(`the CLI output holds a secret: ${l.slice(0, 4)}...`);
  if (!existsSync(outServer(cliDir))) fail("the CLI did not write server.json");
  const cliBad = spawnSync(process.execPath, [join(ROOT, "tools", "cli.mjs"), "host", "config"], {
    cwd: ROOT, encoding: "utf8", timeout: 60000, windowsHide: true, env: { ...process.env, LF_HOST_DIR: join(base, "unused") },
  });
  if (cliBad.status === 0 || !/apply/.test(`${cliBad.stdout}${cliBad.stderr}`)) fail("host config without a subcommand did not print the usage");

  // 10. announceWebhookUrl (read by `host post`, #1414) is a known host secret: no "not used" warning,
  // and its value never prints.
  const announce = freshHost({ game: { password: PASSWORD, passwordAdmin: ADMIN_PASSWORD }, announceWebhookUrl: ANNOUNCE });
  const ar = applyConfig(announce, { repoDir: repo });
  if (!ar.ok) fail(`apply with announceWebhookUrl was refused: ${ar.reason}`);
  else {
    const aw = [...(ar.lines ?? []), ...(ar.warnings ?? [])].join("\n");
    if (/announceWebhookUrl/.test(aw)) fail(`a secrets.json with announceWebhookUrl was warned about: ${aw}`);
    for (const l of leaks(aw)) fail(`the summary for announceWebhookUrl holds a secret: ${l.slice(0, 4)}...`);
  }
} finally {
  safeRemoveTree(base);
}

if (failures.length) {
  for (const f of failures) console.log(`FAIL  ${f}`);
  process.exit(1);
}
console.log("host-config-apply-selftest ok");
