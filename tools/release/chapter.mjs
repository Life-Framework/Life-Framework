// tools/release/chapter.mjs - `tools\cli release chapter <version>`: ship a finished chapter or milestone to
// the live alpha (decision 2026-11-15: the one 0grind server, service 298627, is both the live alpha and the
// playtest server; the lead releases, the maintainer is told afterwards; wipes are allowed).
//
//   tools\cli release chapter 0.1.1 --dry-run            every check against the real environment, writes nothing
//   tools\cli release chapter 0.1.1 [--notes <file>]     the real release (lead only)
//   tools\cli release chapter 0.1.1 --now                restart the server even with players on
//   tools\cli release chapter 0.1.1 --published          both items already published by hand in the Workbench
//                                                        GUI (Visibility Public): skips build and publish after
//                                                        checking both Workshop pages show exactly 0.1.1, then
//                                                        server, verify, announce, tag (on the origin/main commit)
//   tools\cli release chapter 0.1.1 --verify             step 5 alone, after a scheduled restart
//
// Steps, refusing at the first failure: 1 preflight, 2 build (a clean worktree at the commit, versions
// stamped, both addons packed headless), 3 publish (core, then pack; Workbench -publishAddon with the login
// it has saved on this PC, never -wbBackendLogin), 4 server (config push, restart now or at the next
// scheduled restart), 5 verify (logs pull, server-doctor, the core version line), 6 announce (Discord
// webhook) and tag v<version>. Runbook: docs/release/chapter-release.md.
//
// Every outside effect goes through `deps` (git, gh, the CLI, Workbench, A2S, HTTP, Discord), so
// tools/validation/release-chapter-selftest.mjs runs the whole flow with stubs.
// red-proof: tools/validation/release-chapter-selftest.mjs.

import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { join, resolve } from "node:path";
import { spawnSync } from "node:child_process";
import { get as httpsGet } from "node:https";
import { stampGproj, SEMVER } from "./release.mjs";
import { queryInfo } from "../host/a2s.mjs";
import { hostDir } from "../host/host.mjs";
import { FLAGSHIP_0GRIND, remoteDir } from "../host/remote-0grind.mjs";
import { isDiscordWebhook, postToWebhook, readWebhookUrl, scrub } from "../host/discord-post.mjs";

export const ITEMS = [
  { addon: "LifeFramework", id: "4550A2B656E1B569", title: "Life Framework - Core" },
  { addon: "LF_Everon1989", id: "B1E84D5687E11724", title: "Life Framework - Everon 1989" },
];
export const workshopUrl = (id) => `https://reforger.armaplatform.com/workshop/${id}`;
export const CORE_VERSION_FILE = "addons/LifeFramework/Scripts/Game/Feature/Packs/LF_PackManifest.c";
export const PACK_MANIFEST_FILE = "addons/LF_Everon1989/Configs/LF_PackManifest.conf";
export const STAMPED_FILES = [...ITEMS.map((i) => `addons/${i.addon}/${i.addon}.gproj`), CORE_VERSION_FILE];
// The line LF_MissionConfigPackSource logs once the pack accepts the running core.
export const VERSION_LINE = /\[LFDebug:Packs\] core (\d+\.\d+\.\d+) accepted by pack (\S+)/;
const OK_CONCLUSIONS = new Set(["success", "skipped", "neutral"]);
const MAX_DISCORD = 1900; // under Discord's 2000 with room for the header

// ---------------------------------------------------------------- pure helpers

export function cmpSemver(a, b) {
  const x = String(a).split(".").map(Number);
  const y = String(b).split(".").map(Number);
  for (let i = 0; i < 3; i++) if (x[i] !== y[i]) return x[i] < y[i] ? -1 : 1;
  return 0;
}

// The Workshop page embeds its item data as JSON (escaped inside the RSC payload): "currentVersionNumber":"0.1.0".
export function parseWorkshopVersion(html) {
  const m = String(html ?? "").match(/currentVersionNumber\\?"\s*:\s*\\?"(\d+\.\d+\.\d+)/);
  return m ? m[1] : null;
}

// GitHub check runs and commit statuses of the commit: problems, empty when green. At least one check run must
// exist and `validate` must have passed (the required check on main).
export function judgeChecks(runs, statuses) {
  const problems = [];
  if (!runs.length) problems.push("no GitHub check runs on the commit (was it pushed and checked?)");
  for (const r of runs) {
    if (r.status !== "completed") problems.push(`check "${r.name}" is ${r.status}`);
    else if (!OK_CONCLUSIONS.has(r.conclusion)) problems.push(`check "${r.name}" concluded ${r.conclusion}`);
  }
  if (runs.length && !runs.some((r) => r.name === "validate" && r.conclusion === "success")) problems.push('no passing "validate" check on the commit');
  for (const s of statuses) if (s.state !== "success") problems.push(`status "${s.context}" is ${s.state}`);
  return problems;
}

export function stampCoreVersion(text, version) {
  const re = /(static\s+const\s+string\s+CORE_VERSION\s*=\s*")\d+\.\d+\.\d+(")/;
  return re.test(text) ? text.replace(re, `$1${version}$2`) : null;
}

export function packRange(conf) {
  const min = (String(conf).match(/m_sCoreMin\s+"([^"]*)"/) ?? [])[1] ?? "";
  const below = (String(conf).match(/m_sCoreBelow\s+"([^"]*)"/) ?? [])[1] ?? "";
  return { min, below };
}

export function inRange(version, { min, below }) {
  if (min && cmpSemver(version, min) < 0) return false;
  if (below && cmpSemver(version, below) >= 0) return false;
  return true;
}

// Merged PR lines ("- title (#N)") from `git log --first-parent --merges --format=%s%x1f%b%x1e`.
export function prLines(log) {
  const items = [];
  for (const rec of String(log).split(String.fromCharCode(30))) {
    const [subject, body] = rec.trim().split(String.fromCharCode(31));
    if (!subject) continue;
    const pr = (subject.match(/#(\d+)/) ?? [])[1];
    const title = (body ?? "").split("\n").find((l) => l.trim()) ?? subject;
    items.push(`- ${title.trim()}${pr ? ` (#${pr})` : ""}`);
  }
  return items;
}

export function buildNotes({ version, since, items }) {
  const head = `Life Framework ${version}\n\n${since}\n\n`;
  return head + (items.length ? items.join("\n") : "- (no merged PRs)") + "\n";
}

// The Discord post: a header, then as many note lines as fit, then "and N more".
export function discordPost({ version, notes, restartNow }) {
  const lines = String(notes).split("\n").filter((l) => /^- /.test(l));
  const head = [
    `**Life Framework ${version}** is out on the Workshop (core and Everon 1989).`,
    restartNow ? "The server is restarting onto it now." : "The server picks it up at its next scheduled restart.",
    "",
  ];
  const out = [...head];
  let used = head.join("\n").length;
  let shown = 0;
  for (const l of lines) {
    const tail = lines.length - shown - 1 > 0 ? 24 : 0;
    if (used + l.length + 1 + tail > MAX_DISCORD) break;
    out.push(l);
    used += l.length + 1;
    shown++;
  }
  if (shown < lines.length) out.push(`...and ${lines.length - shown} more.`);
  return out.join("\n").trim();
}

// The Workbench flags for one publish. -gproj names the project, -publishAddonDir the folder -packAddon wrote.
// No -profile: the Bohemia login Workbench saved lives in its default profile. Never -wbBackendLogin.
export function publishFlags({ gproj, dir, version, notesFile }) {
  return [
    "-gproj", gproj,
    "-wbModule=ResourceManager",
    "-publishAddon",
    "-publishAddonDir", dir,
    "-publishAddonVersion", version,
    "-publishAddonChangeNoteFile", notesFile,
    "-noSplash",
  ];
}

// The Workbench console.log of a publish: { ok, line }. The strings are Workbench's own (read from the exe).
export function judgePublishLog(text) {
  const t = String(text ?? "");
  const bad = t.match(/.*(Failed to publish bundle|Publish Addon: [^\r\n]*|Validating bundle failed[^\r\n]*|Creating bundle failed[^\r\n]*|is not meant to be published[^\r\n]*|Publishing (was )?canceled[^\r\n]*).*/);
  if (bad) return { ok: false, line: bad[0].trim() };
  if (/Publishing successful/.test(t)) return { ok: true, line: "Publishing successful" };
  return { ok: false, line: "no \"Publishing successful\" line in the Workbench log" };
}

export function slugFor(version) {
  return `release-${version.replace(/\./g, "-")}`;
}

// ---------------------------------------------------------------- the run

function parseArgs(args) {
  const val = (f) => {
    const i = args.indexOf(f);
    return i >= 0 ? args[i + 1] : undefined;
  };
  const valued = new Set(["--notes", "--at", "--since"]);
  const version = args.find((a, i) => !a.startsWith("--") && !valued.has(args[i - 1]));
  return {
    version,
    notes: val("--notes"),
    at: val("--at"),
    since: val("--since"),
    dry: args.includes("--dry-run"),
    now: args.includes("--now"),
    verifyOnly: args.includes("--verify"),
    noStamp: args.includes("--no-gproj-stamp"),
    published: args.includes("--published"),
  };
}

export const USAGE = "usage: tools\\cli release chapter <x.y.z> [--notes <file>] [--dry-run] [--now] [--at <sha>] [--since <ref>] [--published] [--verify]";

export async function runChapter(root, args, deps) {
  const log = deps.log ?? ((s) => console.log(s));
  const o = parseArgs(args);
  const say = (m) => log(`[release-chapter] ${m}`);
  let stepNo = 0;
  const step = (n, name) => {
    stepNo = n;
    say(`=== ${n}/6 ${name}${o.dry ? " (dry run)" : ""} ===`);
  };
  const fail = (msg) => {
    say(`FAIL  ${msg}`);
    log(`SUMMARY release-chapter fail step=${stepNo} version=${o.version ?? ""}${o.dry ? " dry-run" : ""}`);
    return 1;
  };
  if (!o.version || !SEMVER.test(o.version)) {
    log(USAGE);
    return 2;
  }
  const v = o.version;
  const g = (a, cwd = root) => deps.git(a, cwd);
  if (o.verifyOnly) return verify(root, v, deps, { say, step, fail, log });

  // ---- 1. preflight
  step(1, "preflight");
  if (!o.dry && deps.isMain(root)) return fail("this is the main checkout (the world-editor copy); run from a worktree (tools\\cli wt new <slug>)");
  const st = g(["status", "--porcelain"]);
  if (st.code !== 0) return fail(`not a git repository: ${st.err}`);
  if (st.out) return fail(`the tree is not clean (${st.out.split("\n").length} path(s)); commit or stash first`);
  say("tree clean");
  const fetched = g(["fetch", "-q", "origin", "main", "--tags"]);
  if (fetched.code !== 0) return fail(`git fetch origin: ${fetched.err}`);
  const sha = g(["rev-parse", "--verify", "-q", `${o.at ?? "origin/main"}^{commit}`]).out;
  if (!sha) return fail(`no commit ${o.at ?? "origin/main"}`);
  if (g(["merge-base", "--is-ancestor", sha, "origin/main"]).code !== 0) return fail(`${sha.slice(0, 8)} is not on origin/main`);
  say(`commit ${sha.slice(0, 10)}${o.at ? "" : " (origin/main)"}`);
  if (g(["rev-parse", "--verify", "-q", `refs/tags/v${v}`]).code === 0 || g(["ls-remote", "--tags", "origin", `refs/tags/v${v}`]).out) return fail(`tag v${v} already exists`);
  const tags = g(["tag", "--list", "v*", "--sort=-v:refname"]).out.split("\n").filter((t) => /^v\d+\.\d+\.\d+$/.test(t));
  if (tags[0] && cmpSemver(v, tags[0].slice(1)) <= 0) return fail(`${v} is not newer than the last release tag ${tags[0]}`);
  for (const it of ITEMS) {
    let html;
    try {
      html = await deps.fetchText(workshopUrl(it.id));
    } catch (e) {
      return fail(`cannot read the Workshop page of ${it.title} (${workshopUrl(it.id)}): ${e.message}`);
    }
    const pub = parseWorkshopVersion(html);
    if (!pub) return fail(`no version on the Workshop page of ${it.title} (${workshopUrl(it.id)})`);
    if (o.published) {
      // Published by hand (Workbench GUI): the page must already show exactly this version.
      if (pub !== v) return fail(`--published: the Workshop page of ${it.title} (${workshopUrl(it.id)}) shows ${pub}, not ${v}; publish it in Workbench first`);
      say(`${it.title} ${it.id}: Workshop shows ${pub}`);
      continue;
    }
    if (cmpSemver(v, pub) <= 0) return fail(`${v} is not newer than the published ${it.title} ${pub}`);
    say(`${it.title} ${it.id}: published ${pub}, releasing ${v}`);
  }
  const range = packRange(g(["show", `${sha}:${PACK_MANIFEST_FILE}`]).out);
  if (!inRange(v, range)) {
    return fail(`the pack manifest (${PACK_MANIFEST_FILE}) accepts core ${range.min || "any"} to below ${range.below || "any"}; ${v} is outside it, so the pack would refuse the core. Move m_sCoreBelow in a PR first`);
  }
  const coreText = g(["show", `${sha}:${CORE_VERSION_FILE}`]).out;
  if (stampCoreVersion(coreText, v) === null) return fail(`no CORE_VERSION constant in ${CORE_VERSION_FILE}`);
  if (o.published && stampCoreVersion(coreText, v) !== coreText) say(`WARNING: CORE_VERSION at ${sha.slice(0, 10)} is not ${v}; nothing is stamped with --published, so verify only passes if the upload was built from a commit that says ${v}`);
  const checks = deps.gh(["api", `repos/{owner}/{repo}/commits/${sha}/check-runs`, "--jq", "[.check_runs[] | {name, status, conclusion}]"]);
  const statuses = deps.gh(["api", `repos/{owner}/{repo}/commits/${sha}/status`, "--jq", "[.statuses[] | {context, state}]"]);
  if (checks.code !== 0 || statuses.code !== 0) return fail(`gh api: ${checks.err || statuses.err}`);
  let runs;
  let stats;
  try {
    runs = JSON.parse(checks.out || "[]");
    stats = JSON.parse(statuses.out || "[]");
  } catch {
    return fail("gh api returned something that is not JSON");
  }
  const red = judgeChecks(runs, stats);
  if (red.length) return fail(`gates not green on ${sha.slice(0, 8)}: ${red.join("; ")}`);
  say(`gates green: ${runs.map((r) => `${r.name}=${r.conclusion}`).join(", ")}`);

  const relDir = join(root, "tmp", "release");
  mkdirSync(relDir, { recursive: true });
  const notesFile = o.notes ? resolve(o.notes) : join(relDir, `notes-${v}.md`);
  if (o.notes && !existsSync(notesFile)) return fail(`--notes ${o.notes} does not exist`);
  if (!o.notes) {
    const from = o.since ?? tags[0];
    const range2 = from ? [`${from}..${sha}`] : ["-n", "30", sha];
    const lg = g(["log", "--first-parent", "--merges", "--format=%s%x1f%b%x1e", ...range2]);
    if (lg.code !== 0) return fail(`git log for the notes: ${lg.err}`);
    const items = prLines(lg.out);
    const since = from ? `Merged since ${from}:` : "The last 30 merges (no earlier release tag; pass --since <ref> to choose):";
    writeFileSync(notesFile, buildNotes({ version: v, since, items }));
    say(`notes built from ${items.length} merged PR(s): ${notesFile}`);
  } else say(`notes: ${notesFile}`);
  const notes = readFileSync(notesFile, "utf8");
  if (!notes.trim()) return fail(`the notes file ${notesFile} is empty`);
  const changeNote = join(relDir, `changenote-${v}.txt`);
  writeFileSync(changeNote, notes.replace(/^#+\s*/gm, "").trim() + "\n");

  // ---- 2. build
  step(2, "build");
  const slug = slugFor(v);
  if (o.published) say("skipped: --published (both Workshop pages already show the version)");
  else if (o.dry) {
    say(`would run: tools\\cli wt new ${slug}, then git checkout --detach ${sha.slice(0, 10)} in it`);
    say(`would stamp ${v} into ${STAMPED_FILES.join(", ")}${o.noStamp ? " (gproj stamps skipped)" : ""}`);
    for (const it of ITEMS) say(`would pack ${it.addon}: tools\\cli pack ${it.addon} --wait --out <worktree>\\tmp\\release\\pack\\${it.addon} (this PC's Workbench, LF_WB_LOCAL=1)`);
  }
  let build = null;
  if (!o.dry && !o.published) {
    const made = deps.cli(["wt", "new", slug], { cwd: root });
    const path = (made.out.match(/path:\s+(.+)/) ?? [])[1]?.trim();
    if (made.code !== 0 || !path) return fail(`wt new ${slug}: ${made.out.split("\n").slice(-3).join(" ")} (an old one? tools\\cli wt prune ${slug})`);
    const co = g(["checkout", "-q", "--detach", sha], path);
    if (co.code !== 0) return fail(`checkout ${sha.slice(0, 8)} in ${path}: ${co.err}`);
    for (const f of STAMPED_FILES) {
      if (o.noStamp && f.endsWith(".gproj")) continue;
      const p = join(path, f);
      const before = readFileSync(p, "utf8");
      const after = f.endsWith(".gproj") ? stampGproj(before, v) : stampCoreVersion(before, v);
      if (after === null) return fail(`cannot stamp ${f}`);
      writeFileSync(p, after);
    }
    say(`stamped ${v} in ${path}`);
    const add = g(["add", "--", ...STAMPED_FILES], path);
    const tree = add.code === 0 ? g(["write-tree"], path) : add;
    const ident = ["-c", "user.name=release", "-c", "user.email=release@localhost"];
    const commit = tree.code === 0 ? g([...ident, "commit-tree", tree.out, "-p", sha, "-m", `release: ${v}`], path) : tree;
    if (commit.code !== 0) return fail(`release commit: ${commit.err}`);
    build = { path, commit: commit.out };
    for (const it of ITEMS) {
      const out = join(path, "tmp", "release", "pack", it.addon);
      const p = deps.cli(["pack", it.addon, "--wait", "--out", out], { cwd: path, env: { LF_WB_LOCAL: "1" } });
      if (p.code !== 0 || !/pack OK/.test(p.out)) return fail(`pack ${it.addon}: ${p.out.split("\n").filter(Boolean).slice(-2).join(" ")}`);
      it.packed = out;
      say(`packed ${it.addon} -> ${out}`);
    }
  }

  // ---- 3. publish
  step(3, "publish");
  if (o.published) say("skipped: --published (published by hand in Workbench)");
  for (const it of o.published ? [] : ITEMS) {
    const dir = it.packed ?? `<worktree>\\tmp\\release\\pack\\${it.addon}`;
    const flags = publishFlags({ gproj: `${build?.path ?? "<worktree>"}\\addons\\${it.addon}\\${it.addon}.gproj`, dir, version: v, notesFile: changeNote });
    if (o.dry) {
      say(`would publish ${it.title} ${it.id}: Workbench ${flags.join(" ")} -addonsDir <...>`);
      continue;
    }
    const r = await deps.publish({ root: build.path, addon: it.addon, dir, version: v, notesFile: changeNote });
    if (!r.ok) return fail(`publish ${it.title}: ${r.line}${r.log ? ` (log ${r.log})` : ""}`);
    say(`published ${it.title}: ${r.line}`);
    const seen = await waitFor(deps, 600, 20, async () => parseWorkshopVersion(await deps.fetchText(workshopUrl(it.id)).catch(() => "")) === v);
    if (!seen) return fail(`${workshopUrl(it.id)} does not show ${v} after 600 s`);
    say(`${workshopUrl(it.id)} shows ${v}`);
  }

  // ---- 4. server
  step(4, "server");
  const hostCwd = build?.path ?? root;
  const push = deps.cli(["host", "remote", "0grind", "config", "push", ...(o.dry ? [] : ["--yes"])], { cwd: hostCwd });
  for (const l of push.out.split("\n").filter((x) => /~|\+|-|written|unchanged|dry run/.test(x)).slice(0, 12)) say(`  ${l.replace(/^\[LFDebug:Host0grind\]\s*/, "")}`);
  if (push.code !== 0) return fail("host remote 0grind config push failed (see above)");
  let players = null;
  try {
    players = (await deps.a2s()).players;
  } catch {
    players = null;
  }
  const restartNow = o.now || !players;
  say(`A2S: ${players === null ? "no answer (down or booting)" : `${players} player(s) on`}`);
  if (!restartNow) {
    say(`${players} player(s) on and no --now: the update applies at the next scheduled restart. Afterwards run: tools\\cli release chapter ${v} --verify`);
  } else if (o.dry) {
    say("would run: tools\\cli host remote 0grind restart --yes (0 players or --now)");
  } else {
    const rs = deps.cli(["host", "remote", "0grind", "restart", "--yes"], { cwd: hostCwd });
    if (rs.code !== 0) return fail(`restart: ${rs.out.trim().split("\n").pop()}`);
    say("restart sent; the box downloads the new versions while it boots");
  }

  // ---- 5. verify
  if (restartNow && !o.dry) {
    const code = await verify(hostCwd, v, deps, { say, step, fail, log, inline: true });
    if (code !== 0) return code;
  } else {
    step(5, "verify");
    say(o.dry ? "would wait for A2S, then logs pull, server-doctor (PASS) and the line \"[LFDebug:Packs] core <v> accepted by pack LF_Everon1989\"" : `deferred to the scheduled restart: tools\\cli release chapter ${v} --verify`);
  }

  // ---- 6. announce and tag
  step(6, "announce and tag");
  const post = discordPost({ version: v, notes, restartNow });
  const hook = deps.webhook();
  if (hook.error) return fail(`${hook.error}; nothing posted`);
  if (o.dry) {
    say(`would post ${post.length} characters to the announcements webhook (URL found, not printed):`);
    for (const l of post.split("\n")) log(`    ${l}`);
    if (o.published) say(`would tag v${v} on ${sha.slice(0, 10)} (nothing is stamped with --published) and push refs/tags/v${v}`);
    else say(`would tag v${v} on the release commit (${sha.slice(0, 10)} plus the version stamps) and push refs/tags/v${v}`);
    say("dry run OK: nothing was built, published, pushed, restarted, posted or tagged");
    log(`SUMMARY release-chapter ok dry-run version=${v} commit=${sha.slice(0, 10)}`);
    return 0;
  }
  const sent = await deps.send(hook.url, post);
  if (!sent?.ok) return fail(`Discord post: ${sent?.error ?? "failed"}`);
  say(`posted the changelog (${post.length} characters)`);
  const ident = ["-c", "user.name=release", "-c", "user.email=release@localhost"];
  // With --published nothing was stamped here, so the tag goes on the origin/main commit itself.
  const tagAt = build?.commit ?? sha;
  const tagCwd = build?.path ?? root;
  const tag = g([...ident, "tag", "-a", `v${v}`, tagAt, "-m", `Life Framework ${v}`], tagCwd);
  if (tag.code !== 0) return fail(`tag: ${tag.err}`);
  const pushed = g(["push", "origin", `refs/tags/v${v}`], tagCwd);
  if (pushed.code !== 0) return fail(`push tag: ${pushed.err}`);
  say(`tagged v${v} at ${tagAt.slice(0, 10)} and pushed it`);
  say(build ? `tell the maintainer; then tools\\cli wt prune ${slug}` : "tell the maintainer");
  log(`SUMMARY release-chapter ok version=${v} commit=${sha.slice(0, 10)} restart=${restartNow ? "now" : "scheduled"}`);
  return 0;
}

// Polls `test` every `everyS` seconds for at most `budgetS`. The bound is the command's own (AGENTS.md
// "Nothing waits forever").
async function waitFor(deps, budgetS, everyS, test) {
  const end = deps.now() + budgetS * 1000;
  for (;;) {
    const r = await test();
    if (r) return r;
    if (deps.now() + everyS * 1000 > end) return false;
    await deps.sleep(everyS * 1000);
  }
}

// Step 5: wait for the box to answer A2S, pull its newest log, server-doctor PASS, and the version line.
async function verify(cwd, v, deps, { say, step, fail, log, inline = false }) {
  step(5, "verify");
  if (inline) await deps.sleep(60000); // let the old instance stop before A2S is asked
  const up = await waitFor(deps, 900, 15, async () => {
    try {
      await deps.a2s();
      return true;
    } catch {
      return false;
    }
  });
  if (!up) return fail("the server did not answer A2S within 900 s of the restart");
  say("A2S answers");
  let last = "";
  const ok = await waitFor(deps, 300, 30, async () => {
    const lp = deps.cli(["host", "remote", "0grind", "logs", "pull"], { cwd });
    const file = (lp.out.match(/->\s+(.+?console\.log)/) ?? [])[1];
    if (lp.code !== 0 || !file) {
      last = `logs pull: ${lp.out.trim().split("\n").pop()}`;
      return false;
    }
    const m = deps.readText(file).match(VERSION_LINE);
    if (!m || m[1] !== v) {
      last = m ? `the log shows core ${m[1]}, not ${v}` : `no "[LFDebug:Packs] core ... accepted by pack" line in ${file} yet`;
      return false;
    }
    const doc = deps.cli(["server-doctor", file], { cwd });
    if (doc.code !== 0) {
      last = `server-doctor: ${doc.out.trim().split("\n").pop()}`;
      return "doctor";
    }
    say(`log ${file}: core ${m[1]} accepted by pack ${m[2]}; server-doctor PASS`);
    return true;
  });
  if (ok !== true) return fail(last || "verify failed");
  if (!inline) log(`SUMMARY release-chapter ok verify version=${v}`);
  return 0;
}

// ---------------------------------------------------------------- the real environment

// node:https with no agent, not fetch (a fetch origin left open at exit trips a libuv assertion on Windows;
// tools/host/commands/remote.mjs). 20 s bound.
// The Workshop page is CDN-cached: right after a publish it showed the old version for minutes while a
// cache-busted fetch showed the new one. Every request carries a timestamp query and no-cache headers.
export function cacheBustedRequest(url, now = Date.now()) {
  const u = new URL(url);
  u.searchParams.set("t", String(now));
  return {
    url: u.href,
    headers: { "User-Agent": "LifeFramework-release-tool", "Cache-Control": "no-cache", Pragma: "no-cache" },
  };
}

function httpsText(url, timeoutMs = 20000) {
  return new Promise((ok, no) => {
    const req = cacheBustedRequest(url);
    const q = httpsGet(req.url, { agent: false, timeout: timeoutMs, headers: req.headers }, (s) => {
      if (s.statusCode >= 300 && s.statusCode < 400 && s.headers.location) {
        s.resume();
        return httpsText(new URL(s.headers.location, url).href, timeoutMs).then(ok, no);
      }
      if (s.statusCode !== 200) {
        s.resume();
        return no(new Error(`HTTP ${s.statusCode}`));
      }
      let b = "";
      s.setEncoding("utf8");
      s.on("data", (c) => (b += c));
      s.on("end", () => ok(b));
    });
    q.on("error", no);
    q.on("timeout", () => {
      q.destroy();
      no(new Error(`timed out after ${timeoutMs} ms`));
    });
  });
}

// `publish` is cli.mjs's publishAddon (it needs the Workbench path and addon-dir helpers that live there).
export function realDeps(root, { publish, isMain }) {
  const run = (exe, a, opts = {}) => {
    const r = spawnSync(exe, a, { cwd: opts.cwd ?? root, encoding: "utf8", windowsHide: true, maxBuffer: 1 << 26, timeout: opts.timeoutMs ?? 120000, env: { ...process.env, ...(opts.env ?? {}) } });
    return { code: r.status ?? 1, out: `${r.stdout ?? ""}${r.stderr ?? ""}`.trim(), err: (r.stderr ?? r.error?.message ?? "").trim() };
  };
  return {
    git: (a, cwd) => {
      const r = run("git", a, { cwd });
      return { ...r, out: r.code === 0 ? r.out.split("\n").filter((l) => !/^warning:/.test(l)).join("\n") : r.out };
    },
    gh: (a) => run("gh", a, { timeoutMs: 30000 }),
    // The CLI of the checkout at `cwd` (the build worktree runs its own tools). Pack waits for the build lock.
    cli: (a, opts = {}) => run(process.execPath, [join(opts.cwd ?? root, "tools", "cli.mjs"), ...a], { ...opts, timeoutMs: 1800000 }),
    fetchText: (url) => httpsText(url),
    publish,
    a2s: () => queryInfo(FLAGSHIP_0GRIND.address, FLAGSHIP_0GRIND.a2sPort, { timeoutMs: 4000 }),
    webhook: () => {
      const a = readWebhookUrl(remoteDir());
      const found = a.error ? readWebhookUrl(hostDir()) : a;
      if (found.error) return found;
      return isDiscordWebhook(found.url) ? found : { error: "announceWebhookUrl in secrets.json is not an https Discord webhook URL" };
    },
    send: async (url, content) => {
      try {
        return await postToWebhook(url, content);
      } catch (e) {
        return { ok: false, error: scrub(String(e?.message ?? e), url) };
      }
    },
    readText: (f) => readFileSync(f, "utf8"),
    isMain,
    now: () => Date.now(),
    sleep: (ms) => new Promise((r) => setTimeout(r, ms)),
  };
}
