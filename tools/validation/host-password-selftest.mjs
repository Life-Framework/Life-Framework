// tools/validation/host-password-selftest.mjs
// inputs: tools/host/**, tools/commands/staff.mjs, server/flagship/**
//
// Proves `tools\cli host password rotate|show` (#1415) on a fixture host dir: rotate writes a new
// readable join password into the host's secrets.json and into the applied server.json (through
// `host config apply`), keeps the other secrets, prints the invite line, and never writes the
// password into a repo file; show prints the current one; a running host or a secret in a repo
// file refuses and leaves secrets.json as it was. Only fixtures and temp host dirs are used, and a
// failure message never holds a password.
// red-proof: before tools/host/password.mjs existed this file died on the import:
// "Cannot find module '.../tools/host/password.mjs'" (2026-11-15).

import { cpSync, existsSync, mkdirSync, mkdtempSync, readFileSync, readdirSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import { spawnSync } from "node:child_process";
import { safeRemoveTree } from "../link-safety.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const { rotatePassword, showPassword, generatePassword } = await import(pathToFileURL(join(ROOT, "tools", "host", "password.mjs")).href);
const { hostPaths, writeHostState } = await import(pathToFileURL(join(ROOT, "tools", "host", "host.mjs")).href);

const failures = [];
const fail = (m) => failures.push(m);

const OLD_PASSWORD = "old-Alpha-1111";
const ADMIN_PASSWORD = "adm-Beta-9942";
const WEBHOOK = "https://discord.com/api/webhooks/123456/SECRETTOKEN-xyz";
const ADMIN_UID = "a1b2c3d4-0000-4000-8000-000000000001";

const base = mkdtempSync(join(tmpdir(), "lf-host-password-"));
const repo = join(base, "repo");
mkdirSync(repo, { recursive: true });
for (const f of ["config.json", "server.json"]) cpSync(join(ROOT, "server", "flagship", f), join(repo, f));
writeFileSync(join(repo, "staff.txt"), `# fixture roster\n${ADMIN_UID} administrator # Fixture Admin\n`);

const read = (p) => JSON.parse(readFileSync(p, "utf8"));
let n = 0;
function freshHost(secrets) {
  const dir = join(base, `host${++n}`);
  mkdirSync(dir, { recursive: true });
  if (secrets !== undefined) writeFileSync(join(dir, "secrets.json"), JSON.stringify(secrets, null, 2));
  return dir;
}
const secretsOf = (dir) => read(join(dir, "secrets.json"));
const serverOf = (dir) => read(join(hostPaths(dir).profile, "server.json"));
const noPw = (text) => text.split(OLD_PASSWORD).join("<old>");

try {
  // 1. Rotate on a host with secrets: new password in secrets.json and server.json, rest kept.
  const dir = freshHost({ game: { password: OLD_PASSWORD, passwordAdmin: ADMIN_PASSWORD }, staffWebhookUrl: WEBHOOK });
  const r = rotatePassword(dir, { repoDir: repo });
  if (!r.ok) fail(`rotate was refused: ${r.reason}`);
  else {
    const pw = r.password;
    if (typeof pw !== "string" || pw.length < 12) fail("rotate returned no readable password of at least 12 characters");
    if (pw === OLD_PASSWORD) fail("rotate kept the old password");
    if (!/^[a-z0-9-]+$/.test(pw)) fail("the password is not lowercase letters, digits and dashes (readable aloud)");
    const s = secretsOf(dir);
    if (s.game?.password !== pw) fail("secrets.json game.password is not the new password");
    if (s.game?.passwordAdmin !== ADMIN_PASSWORD) fail("secrets.json lost game.passwordAdmin");
    if (s.staffWebhookUrl !== WEBHOOK) fail("secrets.json lost staffWebhookUrl");
    const srv = serverOf(dir);
    if (srv.game?.password !== pw) fail("the applied server.json has no new game.password");
    if (srv.game?.passwordAdmin !== ADMIN_PASSWORD) fail("the applied server.json lost game.passwordAdmin");
    if (JSON.stringify(srv.game?.admins) !== JSON.stringify([ADMIN_UID])) fail("the applied server.json lost the roster's administrators");
    const invite = (r.invite ?? "").toString();
    if (!invite.includes(pw)) fail("the invite line does not hold the new password");
    if (!invite.includes(read(join(repo, "server.json")).game.name)) fail("the invite line does not name the server");
    if (!/docs\/community\/alpha\.md/.test(invite)) fail("the invite line does not point at docs/community/alpha.md");
    const shown = showPassword(dir);
    if (!shown.ok || shown.password !== pw) fail("show does not print the rotated password");
    // Never in a repo file: the fixture repo and the real server/flagship stay secret-free.
    for (const root of [repo, join(ROOT, "server", "flagship")]) {
      for (const f of readdirSync(root)) {
        if (readFileSync(join(root, f), "utf8").includes(pw)) fail(`the new password was written into the repo file ${f}`);
      }
    }
    // A second rotate gives a different password.
    const r2 = rotatePassword(dir, { repoDir: repo });
    if (!r2.ok) fail(`the second rotate was refused: ${r2.reason}`);
    else if (r2.password === pw) fail("two rotations gave the same password");
    else if (serverOf(dir).game?.password !== r2.password) fail("the second rotate did not reach server.json");
  }

  // 2. Rotate on a host with no secrets.json creates it.
  const bare = freshHost();
  const rb = rotatePassword(bare, { repoDir: repo });
  if (!rb.ok) fail(`rotate on a bare host was refused: ${rb.reason}`);
  else if (secretsOf(bare).game?.password !== rb.password || serverOf(bare).game?.password !== rb.password) fail("rotate on a bare host did not write both files");

  // 3. Injected generator is used (deterministic fixtures), and generated passwords differ.
  const seen = new Set();
  for (let i = 0; i < 20; i++) seen.add(generatePassword());
  if (seen.size < 19) fail(`generatePassword repeats too often (${seen.size} different in 20)`);
  const fixed = freshHost({ game: { password: OLD_PASSWORD } });
  const rf = rotatePassword(fixed, { repoDir: repo, generate: () => "fixed-fox-1234" });
  if (!rf.ok || secretsOf(fixed).game.password !== "fixed-fox-1234") fail("rotate ignored the injected generator");

  // 4. show on a host with no password says so; show on a bad secrets.json refuses.
  const none = freshHost();
  if (showPassword(none).ok) fail("show on a host with no secrets.json reported ok");
  const bad = freshHost();
  writeFileSync(join(bad, "secrets.json"), "{ not json");
  if (showPassword(bad).ok) fail("show on a corrupt secrets.json reported ok");
  const rbad = rotatePassword(bad, { repoDir: repo });
  if (rbad.ok) fail("rotate on a corrupt secrets.json was not refused");
  else if (readFileSync(join(bad, "secrets.json"), "utf8") !== "{ not json") fail("rotate touched a corrupt secrets.json");

  // 5. A running host refuses and secrets.json is unchanged.
  const live = freshHost({ game: { password: OLD_PASSWORD } });
  const before = readFileSync(join(live, "secrets.json"), "utf8");
  writeHostState(live, { pid: process.pid, startedAt: new Date().toISOString() });
  const rl = rotatePassword(live, { repoDir: repo });
  if (rl.ok) fail("rotate was not refused while the host runs");
  else if (!/running/i.test(rl.reason ?? "")) fail(`the running-host refusal does not say so: ${noPw(rl.reason ?? "")}`);
  if (readFileSync(join(live, "secrets.json"), "utf8") !== before) fail("a refused rotate changed secrets.json");

  // 6. A secret in a repo file refuses (apply refuses) and secrets.json is restored.
  const leakRepo = join(base, "leakrepo");
  cpSync(repo, leakRepo, { recursive: true });
  const ls = read(join(leakRepo, "server.json"));
  ls.game.password = "hunter2";
  writeFileSync(join(leakRepo, "server.json"), JSON.stringify(ls, null, 2));
  const leakHost = freshHost({ game: { password: OLD_PASSWORD, passwordAdmin: ADMIN_PASSWORD } });
  const beforeLeak = readFileSync(join(leakHost, "secrets.json"), "utf8");
  const rk = rotatePassword(leakHost, { repoDir: leakRepo });
  if (rk.ok) fail("rotate went ahead with a secret in a repo file");
  if (readFileSync(join(leakHost, "secrets.json"), "utf8") !== beforeLeak) fail("a rotate refused by apply did not restore secrets.json");
  const rkBare = freshHost();
  rotatePassword(rkBare, { repoDir: leakRepo });
  if (existsSync(join(rkBare, "secrets.json"))) fail("a refused rotate left a new secrets.json behind");

  // 7. A host dir inside the repo refuses (a secret there could be committed).
  const inRepo = rotatePassword(join(ROOT, "tmp", "host-password-fixture"), { repoDir: repo });
  if (inRepo.ok) fail("rotate accepted a host dir inside the repo");
  if (existsSync(join(ROOT, "tmp", "host-password-fixture", "secrets.json"))) fail("rotate wrote secrets.json inside the repo");

  // 8. The CLI: rotate prints the invite line, show prints the password, exit codes.
  const cliHost = freshHost({ game: { password: OLD_PASSWORD, passwordAdmin: ADMIN_PASSWORD } });
  const env = { ...process.env, LF_HOST_DIR: cliHost };
  const cli = (...a) => spawnSync(process.execPath, [join(ROOT, "tools", "cli.mjs"), "host", "password", ...a], { env, encoding: "utf8", timeout: 60000 });
  const c1 = cli("rotate", "--repo", repo);
  if (c1.status !== 0) fail(`host password rotate exited ${c1.status}: ${noPw(c1.stderr || c1.stdout)}`);
  else {
    const pw = secretsOf(cliHost).game.password;
    if (pw === OLD_PASSWORD) fail("the CLI rotate did not change the password");
    if (!c1.stdout.includes(pw)) fail("the CLI rotate did not print the invite line with the new password");
    if (!/restart/i.test(c1.stdout)) fail("the CLI rotate does not say the change needs a restart (#1520)");
    const c2 = cli("show");
    if (c2.status !== 0 || !c2.stdout.includes(pw)) fail("the CLI show did not print the current password");
  }
  const c3 = cli("bogus");
  if (c3.status === 0) fail("an unknown subcommand exited 0");
} finally {
  safeRemoveTree(base);
}

if (failures.length) {
  for (const f of failures) console.error(`FAIL ${f}`);
  console.error(`host-password-selftest: ${failures.length} failure(s)`);
  process.exit(1);
}
console.log("host-password-selftest: ok");
