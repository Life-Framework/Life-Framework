// tools/validation/host-open-selftest.mjs
// inputs: tools/host/open.mjs, tools/host/commands/open.mjs, tools/host/gate.mjs, tools/host/password.mjs, tools/host/config-apply.mjs
//
// Proves `tools\cli host open --confirm <host dir name> [--dry-run]` (#1512, the opening step of #555) on
// fixture host dirs: a failing gate refuses and touches nothing; a passing gate clears game.password in
// secrets.json and nothing else, re-applies the config (server.json carries an empty password and the
// other secrets), and drafts the opening announcement without the old password; the wrong --confirm, a
// running host, a host dir inside the repo and a refused config apply all leave secrets.json as it was;
// --dry-run prints the steps and writes nothing. Only fixtures and temp host dirs are used.
// red-proof: before tools/host/open.mjs existed this file died on the import:
// "Cannot find module '.../tools/host/open.mjs'" (2026-11-16).

import { cpSync, existsSync, mkdirSync, mkdtempSync, readFileSync, readdirSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve, basename } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import { spawnSync } from "node:child_process";
import { safeRemoveTree } from "../link-safety.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const imp = (p) => import(pathToFileURL(join(ROOT, ...p.split("/"))).href);
const failures = [];
const fail = (m) => failures.push(m);

let open, cmd, host;
try {
  open = await imp("tools/host/open.mjs");
  cmd = await imp("tools/host/commands/open.mjs");
  host = await imp("tools/host/host.mjs");
} catch (e) {
  console.error(`FAIL open.mjs missing (${e.message})`);
  console.error("host-open-selftest: 1 failure(s)");
  process.exit(1);
}
const { hostPaths, writeHostState } = host;

const OLD_PASSWORD = "old-Alpha-1111";
const ADMIN_PASSWORD = "adm-Beta-9942";
const WEBHOOK = "https://discord.com/api/webhooks/123456/SECRETTOKEN-xyz";
const ADMIN_UID = "a1b2c3d4-0000-4000-8000-000000000001";

const base = mkdtempSync(join(tmpdir(), "lf-host-open-"));
const repo = join(base, "repo");
mkdirSync(repo, { recursive: true });
for (const f of ["config.json", "server.json"]) cpSync(join(ROOT, "server", "flagship", f), join(repo, f));
writeFileSync(join(repo, "staff.txt"), `# fixture roster\n${ADMIN_UID} administrator # Fixture Admin\n`);

const read = (p) => JSON.parse(readFileSync(p, "utf8"));
let n = 0;
function freshHost(secrets = { game: { password: OLD_PASSWORD, passwordAdmin: ADMIN_PASSWORD }, staffWebhookUrl: WEBHOOK, announceWebhookUrl: `${WEBHOOK}-announce` }) {
  const dir = join(base, `host${++n}`);
  mkdirSync(dir, { recursive: true });
  if (secrets !== null) writeFileSync(join(dir, "secrets.json"), JSON.stringify(secrets, null, 2));
  return dir;
}
const secretsText = (dir) => readFileSync(join(dir, "secrets.json"), "utf8");
const noPw = (t) => String(t ?? "").split(OLD_PASSWORD).join("<old>");
const serverJson = (dir) => join(hostPaths(dir).profile, "server.json");

const closedGate = { open: false, lines: [
  { id: "sessions", title: "Sessions at the player target", ok: true, detail: "3 sessions" },
  { id: "regression", title: "No open regression", ok: false, detail: "1 open regression issue(s): #99 Saves lost" },
] };
const openGate = { open: true, lines: [{ id: "sessions", title: "Sessions at the player target", ok: true, detail: "3 sessions" }] };

try {
  // 1. A closed gate refuses, names the failing line, and touches nothing.
  const d1 = freshHost();
  const before1 = secretsText(d1);
  const r1 = open.openHost(d1, { confirm: basename(d1), gate: closedGate, repoDir: repo });
  if (r1.ok) fail("a closed gate did not refuse");
  else if (!/regression/i.test(r1.reason ?? "")) fail(`the gate refusal does not name the failing line: ${noPw(r1.reason)}`);
  if (secretsText(d1) !== before1) fail("a refused open (closed gate) changed secrets.json");
  if (existsSync(serverJson(d1))) fail("a refused open (closed gate) applied the config");

  // 2. The wrong or missing --confirm refuses, even with an open gate.
  for (const confirm of [undefined, "", "other-host"]) {
    const d = freshHost();
    const before = secretsText(d);
    const r = open.openHost(d, { confirm, gate: openGate, repoDir: repo });
    if (r.ok) fail(`--confirm ${JSON.stringify(confirm)} did not refuse`);
    if (secretsText(d) !== before) fail(`--confirm ${JSON.stringify(confirm)} changed secrets.json`);
  }

  // 3. An open gate clears the password and nothing else, re-applies the config, drafts the announcement.
  const d3 = freshHost();
  const before3 = read(join(d3, "secrets.json"));
  const r3 = open.openHost(d3, { confirm: basename(d3), gate: openGate, repoDir: repo });
  if (!r3.ok) fail(`an open gate was refused: ${noPw(r3.reason)}`);
  else {
    const after = read(join(d3, "secrets.json"));
    const wantAfter = JSON.parse(JSON.stringify(before3));
    wantAfter.game.password = "";
    if (JSON.stringify(after) !== JSON.stringify(wantAfter)) fail(`secrets.json changed more than game.password: ${noPw(JSON.stringify(after))}`);
    const srv = read(serverJson(d3));
    if ((srv.game?.password ?? "") !== "") fail("the applied server.json still has a join password");
    if (srv.game?.passwordAdmin !== ADMIN_PASSWORD) fail("the applied server.json lost game.passwordAdmin");
    if (JSON.stringify(srv.game?.admins) !== JSON.stringify([ADMIN_UID])) fail("the applied server.json lost the roster's administrators");
    const post = String(r3.announcement ?? "");
    if (!post.includes("Life Framework")) fail("the opening announcement does not name the server");
    if (!/password/i.test(post) || !/kept/i.test(post)) fail(`the announcement does not say the password is gone and progress is kept: ${post}`);
    if (!/Join/.test(post)) fail("the announcement has no join line");
    for (const secret of [OLD_PASSWORD, ADMIN_PASSWORD, WEBHOOK]) if (post.includes(secret)) fail("the opening announcement holds a secret");
    if (!/restart/i.test((r3.lines ?? []).join("\n"))) fail("the result does not say the change needs a restart (#1520)");
    // Never into a repo file: the fixture repo and the real server/flagship stay secret-free.
    for (const root of [repo, join(ROOT, "server", "flagship")]) {
      for (const f of readdirSync(root)) if (readFileSync(join(root, f), "utf8").includes(OLD_PASSWORD)) fail(`the old password is in the repo file ${f}`);
    }
    // A second open on the same host is a no-op for secrets.json (already open) and still ok.
    const again = open.openHost(d3, { confirm: basename(d3), gate: openGate, repoDir: repo });
    if (!again.ok) fail(`a second open was refused: ${noPw(again.reason)}`);
    else if (JSON.stringify(read(join(d3, "secrets.json"))) !== JSON.stringify(wantAfter)) fail("a second open changed secrets.json");
  }

  // 4. A host with no secrets.json has no password to clear: refuses to invent one.
  const d4 = freshHost(null);
  const r4 = open.openHost(d4, { confirm: basename(d4), gate: openGate, repoDir: repo });
  if (!r4.ok && existsSync(join(d4, "secrets.json"))) fail("a refused open left a new secrets.json behind");
  if (r4.ok && existsSync(join(d4, "secrets.json"))) fail("open created a secrets.json on a host that had none");

  // 5. A running host refuses and secrets.json is unchanged.
  const d5 = freshHost();
  const before5 = secretsText(d5);
  writeHostState(d5, { pid: process.pid, startedAt: new Date().toISOString() });
  const r5 = open.openHost(d5, { confirm: basename(d5), gate: openGate, repoDir: repo });
  if (r5.ok) fail("open was not refused while the host runs");
  else if (!/running/i.test(r5.reason ?? "")) fail(`the running-host refusal does not say so: ${noPw(r5.reason)}`);
  if (secretsText(d5) !== before5) fail("a refused open (host running) changed secrets.json");

  // 6. A refused config apply (a secret in a repo file) restores secrets.json.
  const leakRepo = join(base, "leakrepo");
  cpSync(repo, leakRepo, { recursive: true });
  const ls = read(join(leakRepo, "server.json"));
  ls.game.password = "hunter2";
  writeFileSync(join(leakRepo, "server.json"), JSON.stringify(ls, null, 2));
  const d6 = freshHost();
  const before6 = secretsText(d6);
  const r6 = open.openHost(d6, { confirm: basename(d6), gate: openGate, repoDir: leakRepo });
  if (r6.ok) fail("open went ahead with a secret in a repo file");
  if (secretsText(d6) !== before6) fail("an open refused by apply did not restore secrets.json");

  // 7. A host dir inside the repo checkout refuses.
  const inRepo = open.openHost(join(ROOT, "tmp", "host-open-fixture"), { confirm: "host-open-fixture", gate: openGate, repoDir: repo });
  if (inRepo.ok) fail("open accepted a host dir inside the repo");
  if (existsSync(join(ROOT, "tmp", "host-open-fixture", "secrets.json"))) fail("open wrote secrets.json inside the repo");

  // 8. --dry-run lists the steps, writes nothing, and says what a closed gate would do.
  const d8 = freshHost();
  const before8 = secretsText(d8);
  const dry = open.openHost(d8, { confirm: undefined, gate: openGate, repoDir: repo, dryRun: true });
  if (!dry.ok) fail(`a dry run on an open gate failed: ${noPw(dry.reason)}`);
  else {
    const text = (dry.lines ?? []).join("\n");
    for (const word of [/gate/i, /game\.password/, /config apply|apply/i, /announce/i]) if (!word.test(text)) fail(`the dry-run steps do not mention ${word}: ${text}`);
    if (noPw(text).includes(OLD_PASSWORD)) fail("the dry run printed the password");
    if (!dry.announcement) fail("the dry run did not draft the announcement");
  }
  if (secretsText(d8) !== before8) fail("a dry run changed secrets.json");
  if (existsSync(serverJson(d8))) fail("a dry run applied the config");
  const dryClosed = open.openHost(d8, { confirm: undefined, gate: closedGate, repoDir: repo, dryRun: true });
  if (dryClosed.ok) fail("a dry run on a closed gate reported ok");
  else if (!/regression/i.test((dryClosed.lines ?? []).join("\n") + (dryClosed.reason ?? ""))) fail("a dry run on a closed gate does not name the failing line");
  if (secretsText(d8) !== before8) fail("a dry run on a closed gate changed secrets.json");

  // 9. The command: exit codes and output, with an injected gate (ctx.gate) and the real gate over a fixture host.
  const quiet = async (fn) => {
    const log = console.log;
    const err = console.error;
    const lines = [];
    console.log = (...a) => lines.push(a.join(" "));
    console.error = (...a) => lines.push(a.join(" "));
    try {
      return { code: await fn(), out: lines.join("\n") };
    } finally {
      console.log = log;
      console.error = err;
    }
  };
  const d9 = freshHost();
  const ctx9 = { root: ROOT, hostDir: d9, gate: openGate };
  let c = await quiet(() => cmd.main([], ctx9));
  if (c.code === 0) fail("host open with no --confirm exited 0");
  if (!c.out.includes(`--confirm ${basename(d9)}`)) fail(`the no-confirm refusal does not say how to confirm: ${noPw(c.out)}`);
  c = await quiet(() => cmd.main(["--confirm", basename(d9), "--repo", repo], { ...ctx9, gate: closedGate }));
  if (c.code === 0) fail("host open with a closed gate exited 0");
  c = await quiet(() => cmd.main(["--confirm", basename(d9), "--repo", repo], ctx9));
  if (c.code !== 0) fail(`host open with an open gate exited ${c.code}: ${noPw(c.out)}`);
  else {
    if (!/SUMMARY host-open ok/.test(c.out)) fail(`no SUMMARY host-open ok line: ${noPw(c.out)}`);
    if (noPw(c.out).includes("<old>") || c.out.includes(ADMIN_PASSWORD)) fail("the command printed a password");
    if (read(join(d9, "secrets.json")).game.password !== "") fail("the command did not clear game.password");
  }
  const d10 = freshHost();
  c = await quiet(() => cmd.main(["--dry-run", "--repo", repo], { root: ROOT, hostDir: d10, gate: openGate }));
  if (c.code !== 0) fail(`host open --dry-run exited ${c.code}: ${noPw(c.out)}`);
  if (read(join(d10, "secrets.json")).game.password !== OLD_PASSWORD) fail("the command's dry run cleared the password");

  // 10. The real gate over a fixture host dir (no history, no drill) is closed: the CLI refuses and touches nothing.
  const d11 = freshHost();
  const before11 = secretsText(d11);
  const issues = join(base, "issues.json");
  writeFileSync(issues, "[]");
  const cli = spawnSync(process.execPath, [join(ROOT, "tools", "cli.mjs"), "host", "open", "--confirm", basename(d11), "--repo", repo, "--issues-file", issues, "--roster", join(repo, "staff.txt")], {
    env: { ...process.env, LF_HOST_DIR: d11 }, encoding: "utf8", timeout: 60000,
  });
  if (cli.status === 0) fail("the CLI opened a host whose gate is closed (no sessions, no drill)");
  if (!/gate/i.test(`${cli.stdout}${cli.stderr}`)) fail(`the CLI refusal does not mention the gate: ${noPw(cli.stdout + cli.stderr)}`);
  if (secretsText(d11) !== before11) fail("the CLI changed secrets.json behind a closed gate");
} finally {
  safeRemoveTree(base);
}

if (failures.length) {
  for (const f of failures) console.error(`FAIL ${f}`);
  console.error(`host-open-selftest: ${failures.length} failure(s)`);
  process.exit(1);
}
console.log("host-open-selftest: ok");
