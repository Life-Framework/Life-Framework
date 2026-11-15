// tools/validation/release-chapter-selftest.mjs
// inputs: tools/release/**, tools/host/**, tools/validation/release-chapter-selftest.mjs
//
// Proves `tools\cli release chapter` (tools/release/chapter.mjs) offline: git, gh, the CLI, Workbench, A2S,
// the Workshop page and Discord are all stubs, and the build worktree is a temp folder. Covers the dry run
// touching nothing, the real run's order (core published before the pack, push, restart, verify, post, tag),
// the deferred restart with players on, --now, and each refusal stopping before anything later runs.
// red-proof: the --published cases were written first and failed against the unchanged command (7 failures). Earlier, three mutations made it fail, then were restored (PR body): publishing the pack before the
// core ("core published before the pack"), dropping the players check ("no restart with players on"), and
// accepting a Workshop version equal to the release ("refuses a version not newer than the Workshop").

import { mkdirSync, mkdtempSync, readFileSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import {
  CORE_VERSION_FILE,
  PACK_MANIFEST_FILE,
  STAMPED_FILES,
  cmpSemver,
  discordPost,
  inRange,
  judgeChecks,
  judgePublishLog,
  packRange,
  parseWorkshopVersion,
  publishFlags,
  runChapter,
} from "../release/chapter.mjs";
import { safeRemoveTree } from "../link-safety.mjs";

const failures = [];
const ok = (cond, msg) => {
  if (!cond) failures.push(msg);
};

const SHA = "a".repeat(40);
const GPROJ = (id, title) => `GameProject {\n ID "${id}"\n GUID "0000"\n TITLE "${title}"\n}\n`;
const CORE_C = 'class LF_PackManifest\n{\n\tstatic const string CORE_VERSION = "0.1.0";\n}\n';
const MANIFEST = 'LF_PackManifest {\n m_sPackId "LF_Everon1989"\n m_sCoreMin "0.1.0"\n m_sCoreBelow "0.2.0"\n}\n';
const PAGE = (v) => `<script>self.__next_f.push([1,"{\\"currentVersionNumber\\":\\"${v}\\",\\"x\\":1}"])</script>`;
const MERGES = ["Merge pull request #1701 from Life-Framework/ws/a\x1fTaxi job pays per km\x1e", "Merge pull request #1702 from Life-Framework/ws/b\x1fBank vault drill sound\x1e"].join("\n");

const temps = [];
function tempDir(tag) {
  const d = mkdtempSync(join(tmpdir(), `lf-relchap-${tag}-`));
  temps.push(d);
  return d;
}

// A world of stubs. `over` changes one thing per scenario.
function world(over = {}) {
  const root = tempDir("root");
  const build = tempDir("wt");
  for (const [f, t] of [
    ["addons/LifeFramework/LifeFramework.gproj", GPROJ("LifeFramework", "Life Framework (Core)")],
    ["addons/LF_Everon1989/LF_Everon1989.gproj", GPROJ("LF_Everon1989", "Life Framework: Everon 1989")],
    [CORE_VERSION_FILE, CORE_C],
  ]) {
    mkdirSync(dirname(join(build, f)), { recursive: true });
    writeFileSync(join(build, f), t);
  }
  const logFile = join(build, "console.log");
  writeFileSync(logFile, over.serverLog ?? "x\n[LFDebug:Packs] core 0.1.1 accepted by pack LF_Everon1989\n");
  const calls = [];
  const t = { now: 0 };
  const deps = {
    log: (s) => calls.push(`log ${s}`),
    git: (a, cwd) => {
      calls.push(`git ${a.join(" ")}`);
      const k = a.filter((x) => !x.startsWith("-c") && !x.startsWith("user."));
      if (k[0] === "status") return { code: 0, out: over.dirty ? " M addons/x.c" : "" };
      if (k[0] === "fetch") return { code: 0, out: "" };
      if (k[0] === "rev-parse" && String(k[3] ?? k[2]).startsWith("refs/tags/")) return { code: over.tagExists ? 0 : 1, out: "" };
      if (k[0] === "rev-parse") return { code: 0, out: SHA };
      if (k[0] === "merge-base") return { code: 0, out: "" };
      if (k[0] === "ls-remote") return { code: 0, out: "" };
      if (k[0] === "tag" && k[1] === "--list") return { code: 0, out: over.tags ?? "" };
      if (k[0] === "show") return { code: 0, out: k[1].endsWith(PACK_MANIFEST_FILE) ? over.manifest ?? MANIFEST : CORE_C };
      if (k[0] === "log") return { code: 0, out: MERGES };
      if (k[0] === "write-tree") return { code: 0, out: "t".repeat(40) };
      if (k[0] === "commit-tree") return { code: 0, out: "c".repeat(40) };
      return { code: 0, out: "" };
    },
    gh: (a) => {
      calls.push(`gh ${a[1]}`);
      if (a[1].endsWith("/check-runs")) return { code: 0, out: JSON.stringify(over.runs ?? [{ name: "validate", status: "completed", conclusion: "success" }, { name: "claim", status: "completed", conclusion: "skipped" }]) };
      return { code: 0, out: "[]" };
    },
    cli: (a, opts = {}) => {
      calls.push(`cli ${a.join(" ")}${opts.env?.LF_WB_LOCAL ? " [LF_WB_LOCAL]" : ""}`);
      if (a[0] === "wt") return { code: 0, out: `created\n  path:   ${build}\n` };
      if (a[0] === "pack") return { code: 0, out: "pack OK in 50s: 1 .pak file(s)" };
      if (a[0] === "host" && a[3] === "config") return { code: over.pushFails ? 1 : 0, out: "[LFDebug:Host0grind] server_settings: unchanged" };
      if (a[0] === "host" && a[3] === "restart") return { code: 0, out: "restart: sent" };
      if (a[0] === "host" && a[3] === "logs") return { code: 0, out: `[LFDebug:Host0grind] logs: /logs/x -> ${logFile} (10 bytes)` };
      if (a[0] === "server-doctor") return { code: over.doctorFails ? 1 : 0, out: over.doctorFails ? "SUMMARY server-doctor fail stage=scripts" : "SUMMARY server-doctor ok" };
      return { code: 0, out: "" };
    },
    fetchText: async (url) => {
      calls.push(`fetch ${url.slice(-16)}`);
      const core = url.includes("4550A2B6");
      if (over.pages) return PAGE(over.pages[core ? "core" : "pack"]);
      const published = calls.some((c) => c.startsWith(`publish ${core ? "LifeFramework" : "LF_Everon1989"}`));
      return PAGE(published ? "0.1.1" : over.published ?? "0.1.0");
    },
    publish: async (o) => {
      calls.push(`publish ${o.addon} ${o.version}`);
      if (over.publishFails === o.addon) return { ok: false, line: "Publish Addon: Game Identity is not linked with Bohemia Account!" };
      return { ok: true, line: "Publishing successful" };
    },
    a2s: async () => {
      calls.push("a2s");
      if (over.a2sDown) throw new Error("timeout");
      return { players: over.players ?? 0 };
    },
    webhook: () => (over.noHook ? { error: 'secrets.json has no "announceWebhookUrl"' } : { url: "https://discord.com/api/webhooks/1/x" }),
    send: async (url, content) => {
      calls.push(`send ${content.length}`);
      deps.posted = content;
      return { ok: true, status: 204 };
    },
    readText: (f) => readFileSync(f, "utf8"),
    isMain: () => !!over.isMain,
    now: () => t.now,
    sleep: async (ms) => {
      t.now += ms;
    },
  };
  return { root, build, deps, calls };
}

const has = (calls, prefix) => calls.some((c) => c.startsWith(prefix));
const idx = (calls, prefix) => calls.findIndex((c) => c.startsWith(prefix));
const summary = (calls) => calls.filter((c) => c.startsWith("log SUMMARY")).pop() ?? "";

// ---- pure helpers
ok(cmpSemver("0.1.10", "0.1.9") === 1 && cmpSemver("0.2.0", "0.2.0") === 0 && cmpSemver("0.1.0", "1.0.0") === -1, "cmpSemver orders numerically");
ok(parseWorkshopVersion(PAGE("0.3.4")) === "0.3.4", "reads the version from the escaped page payload");
ok(parseWorkshopVersion('{"currentVersionNumber":"1.2.3"}') === "1.2.3", "reads the version from plain JSON");
ok(parseWorkshopVersion("<html></html>") === null, "no version on a page without one");
ok(judgeChecks([{ name: "validate", status: "completed", conclusion: "success" }], []).length === 0, "a passing validate is green");
ok(judgeChecks([{ name: "validate", status: "completed", conclusion: "failure" }], []).length > 0, "a failed check is red");
ok(judgeChecks([{ name: "validate", status: "in_progress", conclusion: null }], []).length > 0, "a running check is not green");
ok(judgeChecks([], []).length > 0, "no checks at all is not green");
ok(judgeChecks([{ name: "validate", status: "completed", conclusion: "success" }], [{ context: "ci", state: "failure" }]).length > 0, "a failed commit status is red");
ok(inRange("0.1.1", packRange(MANIFEST)) && !inRange("0.2.0", packRange(MANIFEST)) && !inRange("0.0.9", packRange(MANIFEST)), "pack range is [min, below)");
const flags = publishFlags({ gproj: "g", dir: "d", version: "0.1.1", notesFile: "n" });
ok(flags.join(" ").includes("-wbModule=ResourceManager -publishAddon -publishAddonDir d -publishAddonVersion 0.1.1 -publishAddonChangeNoteFile n"), "publish flags as the wiki names them");
ok(!flags.some((f) => /wbBackendLogin|^-profile$|password/i.test(f)), "publish never passes a login, a password or -profile");
ok(judgePublishLog("a\nPublishing successful\n").ok, "Publishing successful passes");
ok(!judgePublishLog("Publishing successful\nPublish Addon: Backend authentication timeout!\n").ok, "a Publish Addon error fails even beside a success line");
ok(!judgePublishLog("Failed to publish bundle. Exiting").ok && !judgePublishLog("").ok, "a failed or empty log fails");
const many = Array.from({ length: 300 }, (_, i) => `- A merged pull request with a long-ish title number ${i} (#${i})`).join("\n");
const post = discordPost({ version: "0.1.1", notes: many, restartNow: true });
ok(post.length < 2000 && /more\.$/.test(post), "a long changelog is cut under Discord's 2000 with 'more'");

// ---- dry run: reads everything, changes nothing
{
  const w = world();
  const code = await runChapter(w.root, ["0.1.1", "--dry-run"], w.deps);
  ok(code === 0, `dry run passes (${summary(w.calls)})`);
  for (const p of ["cli wt", "cli pack", "publish", "cli host remote 0grind restart", "send", "git push", "git -c"]) ok(!has(w.calls, p), `dry run never runs "${p}"`);
  ok(has(w.calls, "cli host remote 0grind config push") && !w.calls.some((c) => c.startsWith("cli host remote 0grind config push --yes")), "dry run shows the config diff without --yes");
  ok(w.calls.some((c) => /would publish Life Framework - Core .*-publishAddonVersion 0\.1\.1/.test(c)), "dry run prints the core publish command");
  ok(readFileSync(join(w.root, "tmp", "release", "notes-0.1.1.md"), "utf8").includes("Taxi job pays per km (#1701)"), "notes built from merged PRs");
}

// ---- real run, server empty
{
  const w = world();
  const code = await runChapter(w.root, ["0.1.1"], w.deps);
  ok(code === 0, `real run passes (${summary(w.calls)})`);
  const order = ["cli wt new release-0-1-1", "cli pack LifeFramework", "cli pack LF_Everon1989", "publish LifeFramework", "publish LF_Everon1989", "cli host remote 0grind config push --yes", "cli host remote 0grind restart --yes", "cli host remote 0grind logs pull", "cli server-doctor", "send", "git -c user.name=release -c user.email=release@localhost tag -a v0.1.1", "git push origin refs/tags/v0.1.1"];
  const at = order.map((p) => idx(w.calls, p));
  ok(at.every((x) => x >= 0), `every step ran (missing: ${order.filter((_, i) => at[i] < 0).join(", ")})`);
  ok(at.every((x, i) => i === 0 || x > at[i - 1]), "steps ran in order");
  ok(idx(w.calls, "publish LifeFramework") < idx(w.calls, "publish LF_Everon1989"), "core published before the pack");
  ok(w.calls.some((c) => c.startsWith("cli pack LifeFramework") && c.endsWith("[LF_WB_LOCAL]")), "pack runs on this PC's Workbench");
  for (const f of STAMPED_FILES) ok(readFileSync(join(w.build, f), "utf8").includes("0.1.1"), `stamped ${f}`);
  ok(/restarting onto it now/.test(w.deps.posted ?? ""), "the post says the server restarts now");
}

// ---- players on: no restart, verify deferred, still published, posted and tagged
{
  const w = world({ players: 3 });
  const code = await runChapter(w.root, ["0.1.1"], w.deps);
  ok(code === 0, `players-on run passes (${summary(w.calls)})`);
  ok(!has(w.calls, "cli host remote 0grind restart"), "no restart with players on");
  ok(!has(w.calls, "cli host remote 0grind logs pull"), "verify waits for the scheduled restart");
  ok(has(w.calls, "send") && has(w.calls, "git push origin refs/tags/v0.1.1"), "still announced and tagged");
  ok(/next scheduled restart/.test(w.deps.posted ?? ""), "the post says the next scheduled restart");
}
{
  const w = world({ players: 3 });
  await runChapter(w.root, ["0.1.1", "--now"], w.deps);
  ok(has(w.calls, "cli host remote 0grind restart --yes"), "--now restarts with players on");
}

// ---- --published: the maintainer published both items in the Workbench GUI (Visibility Public). No build and
// no publish run; both pages must already show the version; the server, verify, post and tag steps still run,
// and the tag lands on the origin/main commit itself (nothing was stamped here).
{
  const w = world({ pages: { core: "0.1.1", pack: "0.1.1" } });
  const code = await runChapter(w.root, ["0.1.1", "--published"], w.deps);
  ok(code === 0, `--published run passes (${summary(w.calls)})`);
  for (const p of ["cli wt", "cli pack", "publish"]) ok(!has(w.calls, p), `--published never runs "${p}"`);
  const order = ["cli host remote 0grind config push --yes", "cli host remote 0grind restart --yes", "cli host remote 0grind logs pull", "cli server-doctor", "send", `git -c user.name=release -c user.email=release@localhost tag -a v0.1.1 ${SHA}`, "git push origin refs/tags/v0.1.1"];
  const at = order.map((p) => idx(w.calls, p));
  ok(at.every((x) => x >= 0), `--published ran every later step (missing: ${order.filter((_, i) => at[i] < 0).join(", ")})`);
  ok(at.every((x, i) => i === 0 || x > at[i - 1]), "--published steps ran in order");
  ok(has(w.calls, "fetch ") && w.calls.filter((c) => c.startsWith("fetch ")).length >= 2, "--published reads both Workshop pages");
}
{
  const w = world({ pages: { core: "0.1.1", pack: "0.1.1" }, players: 3 });
  const code = await runChapter(w.root, ["0.1.1", "--published"], w.deps);
  ok(code === 0 && !has(w.calls, "cli host remote 0grind restart") && has(w.calls, "git push origin refs/tags/v0.1.1"), "--published with players on: no restart, still announced and tagged");
}
{
  const w = world({ pages: { core: "0.1.1", pack: "0.1.1" } });
  const code = await runChapter(w.root, ["0.1.1", "--published", "--dry-run"], w.deps);
  ok(code === 0, `--published dry run passes (${summary(w.calls)})`);
  for (const p of ["cli wt", "cli pack", "publish", "send", "git push", "git -c"]) ok(!has(w.calls, p), `--published dry run never runs "${p}"`);
  ok(w.calls.some((c) => /skipped: --published/.test(c)), "--published dry run says build and publish are skipped");
}
for (const [name, pages] of [
  ["core page still on the old version", { core: "0.1.0", pack: "0.1.1" }],
  ["pack page still on the old version", { core: "0.1.1", pack: "0.1.0" }],
  ["a page already past the version", { core: "0.1.2", pack: "0.1.1" }],
]) {
  const w = world({ pages });
  const code = await runChapter(w.root, ["0.1.1", "--published"], w.deps);
  ok(code === 1, `--published refuses: ${name} (exit ${code})`);
  ok(!has(w.calls, "cli host") && !has(w.calls, "send") && !has(w.calls, "git push"), `--published ${name}: nothing after preflight runs`);
  ok(/SUMMARY release-chapter fail step=1/.test(summary(w.calls)), `--published ${name}: fails in step 1`);
}
{
  const w = world({ pages: { core: "0.1.1", pack: "0.1.1" }, tagExists: true });
  ok((await runChapter(w.root, ["0.1.1", "--published"], w.deps)) === 1 && !has(w.calls, "cli host"), "--published still refuses an existing tag");
}

// ---- refusals stop before anything later runs
const refusals = [
  ["dirty tree", { dirty: true }, ["0.1.1"], "cli wt"],
  ["main checkout", { isMain: true }, ["0.1.1"], "git status"],
  ["tag exists", { tagExists: true }, ["0.1.1"], "cli wt"],
  ["not newer than the Workshop", { published: "0.1.1" }, ["0.1.1"], "cli wt"],
  ["not newer than the last tag", { tags: "v0.1.2" }, ["0.1.1"], "cli wt"],
  ["outside the pack's core range", {}, ["0.2.0"], "cli wt"],
  ["red gates", { runs: [{ name: "validate", status: "completed", conclusion: "failure" }] }, ["0.1.1"], "cli wt"],
  ["core publish fails", { publishFails: "LifeFramework" }, ["0.1.1"], "publish LF_Everon1989"],
  ["pack publish fails", { publishFails: "LF_Everon1989" }, ["0.1.1"], "cli host"],
  ["config push fails", { pushFails: true }, ["0.1.1"], "cli host remote 0grind restart"],
  ["server-doctor fails", { doctorFails: true }, ["0.1.1"], "send"],
  ["log shows the old core", { serverLog: "[LFDebug:Packs] core 0.1.0 accepted by pack LF_Everon1989\n" }, ["0.1.1"], "send"],
  ["no webhook", { noHook: true }, ["0.1.1"], "git push"],
];
for (const [name, over, args, never] of refusals) {
  const w = world(over);
  const code = await runChapter(w.root, args, w.deps);
  ok(code === 1, `refuses: ${name} (exit ${code})`);
  ok(!has(w.calls, never), `${name}: never reaches "${never}"`);
  ok(/SUMMARY release-chapter fail step=\d/.test(summary(w.calls)), `${name}: one SUMMARY fail line`);
}
{
  const w = world({ published: "0.1.1" });
  await runChapter(w.root, ["0.1.1", "--dry-run"], w.deps);
  ok(w.calls.some((c) => /not newer than the published Life Framework - Core 0\.1\.1/.test(c)), "refuses a version not newer than the Workshop");
}

// ---- --verify alone
{
  const w = world();
  const code = await runChapter(w.root, ["0.1.1", "--verify"], w.deps);
  ok(code === 0 && /ok verify/.test(summary(w.calls)), `--verify passes on a good log (${summary(w.calls)})`);
  ok(!has(w.calls, "publish") && !has(w.calls, "send"), "--verify publishes and posts nothing");
}

for (const d of temps) safeRemoveTree(d);
if (failures.length) {
  for (const f of failures) console.log(`FAIL  ${f}`);
  console.log(`release-chapter-selftest: ${failures.length} failure(s)`);
  process.exit(1);
}
console.log("release-chapter-selftest: OK");
