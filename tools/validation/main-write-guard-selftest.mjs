// Proves the write guard behind #2785 (agents writing into the lead's main checkout), on fixture checkouts:
//   1. an agent whose cwd is a linked worktree is denied Write/Edit/NotebookEdit on addons/, tools/, docs/ ... of the main checkout,
//      by absolute path and by a relative one, and the message names the worktree path to use;
//   2. an agent launched in the main checkout that ran `wt new` (a session claim) is denied the same way;
//   3. never denied: a file in its own worktree, main's tmp/ or lf.local.json, the lead or maintainer session (no
//      claim, cwd main), a claim whose worktree folder is gone, another session's claim, LF_ALLOW_MAIN_WRITE=1,
//      Read and Bash payloads, a malformed payload (the hook script allows it);
//   4. the real hook script prints a PreToolUse deny for a blocked call, nothing for an allowed one, and a call
//      takes well under half a second (measured, printed);
//   5. `wt new` records the claim through recordClaim, and the lead's stray pass finds only untracked files in the
//      guarded folders older than two minutes, leaves tracked edits alone (reports them), and moves with the relative path kept.
//   6. (#3700) a Bash or PowerShell command that redirects, tees, edits in place (sed -i), Set-Content/Out-File/rm/cp-destination into the
//      main checkout is denied from a worktree session (absolute, Git Bash and backslash paths, a cd into main, a relative path with the
//      cwd in main); reads, a cp source, main tmp/, the worktree itself, /dev/null and the lead (no claim) are never denied.
// inputs: tools/lib/main-write-guard.mjs, tools/hooks/main-write-guard.mjs, .claude/settings.json
// red-proof: 2026-11-18, with decideWrite's claim lookup (liveClaim) returning null the "main-launched agent with a claim" check
// failed; 2026-11-19, with the shell branch of decideWrite removed 14 shell checks failed; with the repo.linked branch removed the "cwd is a worktree" checks failed; with GUARDED emptied every deny check failed; restored.
import { mkdirSync, writeFileSync, existsSync, readFileSync, utimesSync, rmSync } from "node:fs";
import { join } from "node:path";
import { spawnSync } from "node:child_process";
import { fileURLToPath } from "node:url";
import { scratchDir } from "../lib/scratch.mjs";
import { decideWrite, findStrays, liveClaim, moveStrays, norm, parseStatus, recordClaim, repoOf, strayRootOf } from "../lib/main-write-guard.mjs";

const failures = [];
const check = (ok, msg) => { if (!ok) failures.push(msg); };

const base = scratchDir("lf-mwg-");
const main = join(base, "agents", "Life-Framework");
const wtDir = join(base, "worktrees", "Life-Framework-ws-demo");
mkdirSync(join(main, ".git", "worktrees", "Life-Framework-ws-demo"), { recursive: true });
mkdirSync(join(main, "addons", "LifeFramework"), { recursive: true });
mkdirSync(join(main, "tmp"), { recursive: true });
mkdirSync(join(wtDir, "addons"), { recursive: true });
writeFileSync(join(wtDir, ".git"), `gitdir: ${join(main, ".git", "worktrees", "Life-Framework-ws-demo").replace(/\\/g, "/")}\n`);

const ev = {}; // an environment with no override
const w = (file_path, over = {}) => ({ tool_name: "Write", tool_input: { file_path }, cwd: main, session_id: "sess-a", ...over });
const inMain = (rel) => join(main, rel);

// repoOf reads both kinds of checkout
check(repoOf(main)?.linked === false && norm(repoOf(main).mainRoot) === norm(main), "repoOf(main) is the main checkout");
check(repoOf(join(wtDir, "addons"))?.linked === true && norm(repoOf(wtDir).mainRoot) === norm(main), "repoOf(worktree) is linked and finds the main root");

// 1. cwd is a linked worktree
for (const tool of ["Write", "Edit", "MultiEdit"]) {
  const d = decideWrite({ tool_name: tool, tool_input: { file_path: inMain("addons/LifeFramework/Language/x.st") }, cwd: wtDir, session_id: "s" }, { env: ev });
  check(d.block === true, `cwd is a worktree: ${tool} into main addons/ is denied`);
  check(d.block && d.reason.includes(norm(wtDir)) && d.reason.includes("addons/LifeFramework/Language/x.st"), `the ${tool} deny names the worktree path and the file`);
}
check(decideWrite({ tool_name: "NotebookEdit", tool_input: { notebook_path: inMain("tools/a.ipynb") }, cwd: wtDir }, { env: ev }).block, "NotebookEdit into main tools/ is denied");
const rel = decideWrite({ tool_name: "Write", tool_input: { file_path: join("..", "..", "..", "agents", "Life-Framework", "tools", "x.mjs") }, cwd: join(wtDir, "addons") }, { env: ev });
check(rel.block === true, "a relative path that lands in main is denied");
for (const dir of ["docs", "website", ".claude", "agent-box"]) {
  check(decideWrite({ tool_name: "Write", tool_input: { file_path: inMain(`${dir}/f.md`) }, cwd: wtDir }, { env: ev }).block, `main ${dir}/ is guarded`);
}

// never denied from a worktree
check(!decideWrite({ tool_name: "Write", tool_input: { file_path: join(wtDir, "addons", "x.c") }, cwd: wtDir }, { env: ev }).block, "own worktree file is allowed");
check(!decideWrite({ tool_name: "Write", tool_input: { file_path: inMain("tmp/pr-body.md") }, cwd: wtDir }, { env: ev }).block, "main tmp/ is allowed");
check(!decideWrite({ tool_name: "Write", tool_input: { file_path: inMain("lf.local.json") }, cwd: wtDir }, { env: ev }).block, "main lf.local.json is allowed");
check(!decideWrite({ tool_name: "Write", tool_input: { file_path: join(base, "elsewhere", "x.c") }, cwd: wtDir }, { env: ev }).block, "a file outside main is allowed");
check(!decideWrite({ tool_name: "Read", tool_input: { file_path: inMain("addons/x.c") }, cwd: wtDir }, { env: ev }).block, "Read is never blocked");
check(!decideWrite({ tool_name: "Bash", tool_input: { command: "echo" }, cwd: wtDir }, { env: ev }).block, "Bash is never blocked");
check(!decideWrite(w(inMain("addons/x.c"), { cwd: wtDir }), { env: { LF_ALLOW_MAIN_WRITE: "1" } }).block, "LF_ALLOW_MAIN_WRITE=1 opens the gate");

// 2. launched in main, claimed by `wt new`
check(!decideWrite(w(inMain("addons/x.c")), { env: ev }).block, "the lead or maintainer in main (no claim) is never blocked");
check(recordClaim(main, { sessionId: "sess-a", slug: "demo", path: wtDir }) === true, "recordClaim writes a claim");
check(liveClaim(main, "sess-a")?.slug === "demo", "liveClaim reads it back");
const claimed = decideWrite(w(inMain("addons/LifeFramework/Scripts/Game/Tests/LF_Test_X.c")), { env: ev });
check(claimed.block === true && claimed.worktree === norm(wtDir), "a main-launched agent with a claim is denied and pointed at its worktree");
check(!decideWrite(w(inMain("addons/x.c"), { session_id: "sess-b" }), { env: ev }).block, "another session's claim does not block this one");
check(decideWrite(w(inMain("addons/x.c"), { session_id: "", transcript_path: join(base, "sess-a.jsonl") }), { env: ev }).block, "the session id falls back to the transcript file name");
check(!decideWrite(w(join(wtDir, "addons", "ok.c")), { env: ev }).block, "a claimed agent writes its own worktree freely");
check(liveClaim(main, "sess-a", Date.now() + 49 * 3600 * 1000) === null, "a claim older than 48 h is dead");
rmSync(wtDir, { recursive: true, force: true });
check(!decideWrite(w(inMain("addons/x.c")), { env: ev }).block, "a claim whose worktree was pruned no longer blocks");
mkdirSync(join(wtDir, "addons"), { recursive: true });
writeFileSync(join(wtDir, ".git"), `gitdir: ${join(main, ".git", "worktrees", "Life-Framework-ws-demo").replace(/\\/g, "/")}\n`);

// 4. the real hook script, timed
const hook = fileURLToPath(new URL("../hooks/main-write-guard.mjs", import.meta.url));
const runHook = (payload, env = {}) => {
  const t = Date.now();
  const r = spawnSync(process.execPath, [hook], { input: typeof payload === "string" ? payload : JSON.stringify(payload), encoding: "utf8", env: { ...process.env, LF_ALLOW_MAIN_WRITE: "", ...env } });
  return { ms: Date.now() - t, code: r.status, out: r.stdout };
};
const blocked = runHook(w(inMain("addons/x.c")));
let parsed = null;
try { parsed = JSON.parse(blocked.out); } catch { /* checked below */ }
check(blocked.code === 0 && parsed?.hookSpecificOutput?.permissionDecision === "deny" && parsed.hookSpecificOutput.hookEventName === "PreToolUse", "the hook script prints a PreToolUse deny");
const allowed = runHook(w(join(wtDir, "addons", "ok.c")));
check(allowed.code === 0 && allowed.out === "", "the hook script prints nothing for an allowed call");
check(runHook("{not json").code === 0 && runHook("").out === "", "a malformed payload is allowed, exit 0");
const times = [blocked.ms, allowed.ms, ...Array.from({ length: 5 }, () => runHook(w(inMain("addons/x.c"))).ms)];
const avg = Math.round(times.reduce((a, b) => a + b, 0) / times.length);
// A node start costs ~100 ms idle. On a PC running ~20 agents one call can take seconds (max 2647 ms on 2026-11-18, which the
// main guard blamed on an unrelated merge, #3283), so the claim is the median, with a loose cap that only a real hang breaks.
const median = [...times].sort((a, b) => a - b)[Math.floor(times.length / 2)];
check(median < 1500 && Math.max(...times) < 20000, `a hook call stays fast (median ${median} ms, max ${Math.max(...times)} ms)`);

// settings.json wires the hook for the write tools
const settings = JSON.parse(readFileSync(fileURLToPath(new URL("../../.claude/settings.json", import.meta.url)), "utf8"));
const pre = settings.hooks?.PreToolUse?.[0];
check(pre && /Write/.test(pre.matcher) && /Edit/.test(pre.matcher) && /NotebookEdit/.test(pre.matcher) && pre.hooks?.[0]?.command?.includes("tools/hooks/main-write-guard.mjs"), ".claude/settings.json runs the hook for Write, Edit and NotebookEdit");

// 6. #3700: a shell write into main (Bash and PowerShell). Twice on 2026-11-19 a sed/script edit that the Write/Edit
// hook cannot see blocked the main checkout's fast-forward.
const sh = (tool, command, over = {}) => ({ tool_name: tool, tool_input: { command }, cwd: wtDir, session_id: "sess-a", ...over });
const M = norm(main); // C:/.../agents/Life-Framework
const gitBash = `/${M[0].toLowerCase()}${M.slice(2)}`; // /c/.../agents/Life-Framework
const denied = [
  ["Bash", `sed -i 's/a/b/' ${M}/docs/features/sites.md`, "sed -i on a main file (absolute path)"],
  ["Bash", `sed -i 's/a/b/' ${gitBash}/tools/town/drops.mjs`, "sed -i on a Git Bash style main path"],
  ["Bash", `echo hi > ${M}/tools/x.mjs`, "a > redirect into main"],
  ["Bash", `echo hi >> "${M}/docs/x.md"`, "a >> redirect into main, quoted"],
  ["Bash", `cat a | tee ${M}/addons/x.st`, "tee into main"],
  ["Bash", `cd ${M} && sed -i 's/a/b/' tools/town/drops.mjs`, "cd into main then a relative sed -i"],
  ["Bash", `cp ./x ${M}/addons/LifeFramework/x.c`, "cp with a main destination"],
  ["Bash", `node -e 1; rm ${M}/docs/old.md`, "rm of a main file after another command"],
  ["PowerShell", `Set-Content -Path ${M.replace(/\//g, "\\")}\\docs\\a.md -Value x`, "Set-Content on a main file (backslashes)"],
  ["PowerShell", `"x" | Out-File ${M}/tools/a.txt`, "Out-File into main"],
  ["PowerShell", `(Get-Content ${M}/docs/a.md) -replace 'a','b' | Set-Content ${M}/docs/a.md`, "a read-modify-write of a main file"],
  ["PowerShell", `Add-Content ${M}/tools/a.txt "x"`, "Add-Content to main"],
];
for (const [tool, command, what] of denied) {
  const d = decideWrite(sh(tool, command), { env: ev });
  check(d.block === true && d.reason.includes(norm(wtDir)), `${tool}: ${what} is denied and names the worktree`);
}
// launched in main with a claim: a relative-path write with no cd lands in main
check(decideWrite(sh("Bash", "sed -i 's/a/b/' tools/town/drops.mjs", { cwd: main }), { env: ev }).block === true, "cwd main + claim: a relative sed -i with no cd is denied");
check(decideWrite(sh("Bash", `cd ${wtDir.replace(/\\/g, "/")} && sed -i 's/a/b/' tools/town/drops.mjs`, { cwd: main }), { env: ev }).block === false, "cwd main + claim: a cd into the worktree first is allowed");
const allowedShell = [
  ["Bash", `cat ${M}/docs/a.md`, "reading a main file"],
  ["Bash", `grep -rn foo ${M}/tools`, "grep in main"],
  ["Bash", `git -C ${M} status --short`, "git status in main"],
  ["Bash", `cp ${M}/docs/a.md ${wtDir.replace(/\\/g, "/")}/docs/a.md`, "copying FROM main into the worktree"],
  ["Bash", `sed -i 's/a/b/' ${wtDir.replace(/\\/g, "/")}/tools/x.mjs`, "sed -i in the worktree"],
  ["Bash", `echo hi > ${M}/tmp/pr-body.md`, "a redirect into main tmp/"],
  ["Bash", `echo hi > ${M}/lf.local.json`, "a redirect into main lf.local.json"],
  ["Bash", `echo hi > /dev/null 2>&1`, "a redirect to /dev/null"],
  ["Bash", `echo "a > ${M}/tools/x" | wc -l`, "a main path inside a quoted string with no write verb outside it"],
  ["PowerShell", `Get-Content ${M}/docs/a.md | Select-Object -First 3`, "PowerShell reading main"],
  ["PowerShell", `Set-Content -Path ${wtDir}/docs/a.md -Value x`, "Set-Content in the worktree"],
  ["Bash", "git status", "an unrelated command"],
];
for (const [tool, command, what] of allowedShell) {
  check(decideWrite(sh(tool, command), { env: ev }).block === false, `${tool}: ${what} is allowed`);
}
check(decideWrite(sh("Bash", `sed -i 's/a/b/' ${M}/docs/a.md`), { env: { LF_ALLOW_MAIN_WRITE: "1" } }).block === false, "LF_ALLOW_MAIN_WRITE=1 opens the gate for shell writes too");
check(decideWrite(sh("Bash", `sed -i 's/a/b/' ${M}/docs/a.md`, { session_id: "sess-b", cwd: main }), { env: ev }).block === false, "the lead or maintainer (no claim, cwd main) shell-edits main freely");
check(decideWrite({ tool_name: "Bash", tool_input: {}, cwd: wtDir }, { env: ev }).block === false && decideWrite({ tool_name: "Bash", tool_input: { command: 5 }, cwd: wtDir }, { env: ev }).block === false, "a Bash payload with no command string is allowed");
const shellHook = runHook(sh("Bash", `sed -i 's/a/b/' ${M}/docs/a.md`));
check(JSON.parse(shellHook.out || "{}")?.hookSpecificOutput?.permissionDecision === "deny", "the hook script denies a blocked shell write");
check(runHook(sh("Bash", "git status")).out === "", "the hook script allows an unrelated shell command");
check(/Bash/.test(pre.matcher) && /PowerShell/.test(pre.matcher), ".claude/settings.json runs the hook for Bash and PowerShell too");

// 5. the lead's stray pass
const st = parseStatus(["?? addons/LifeFramework/Scripts/Game/Tests/LF_Test_Stray.c", "?? addons/LifeFramework/new.st", " M tools/cli.mjs", "?? tmp/x.md", "?? lf.local.json.bak", "M  docs/a.md", ""].join("\0"));
check(st.untracked.length === 2 && st.modified.length === 2, `parseStatus keeps guarded untracked (2) and modified (2): ${JSON.stringify(st)}`);
const old = join(main, "addons/LifeFramework/Scripts/Game/Tests/LF_Test_Stray.c");
mkdirSync(join(main, "addons/LifeFramework/Scripts/Game/Tests"), { recursive: true });
writeFileSync(old, "class LF_Test_Stray {}\n");
writeFileSync(join(main, "addons/LifeFramework/new.st"), "fresh\n");
const longAgo = new Date(Date.now() - 10 * 60 * 1000);
utimesSync(old, longAgo, longAgo);
const found = findStrays(main, st);
check(found.move.length === 1 && found.move[0].endsWith("LF_Test_Stray.c"), "only the untracked file older than 2 min is moved");
check(found.fresh.length === 1 && found.fresh[0].endsWith("new.st"), "a fresh file waits for the next pass");
check(found.modified.length === 2, "tracked edits are reported, not moved");
const dest = join(base, "moved");
const moved = moveStrays(main, found.move, dest, { stamp: "T1" });
check(moved.length === 1 && !existsSync(old) && readFileSync(join(dest, "T1", found.move[0]), "utf8").includes("LF_Test_Stray"), "the file moved with its relative path and content");
check(existsSync(join(main, "addons/LifeFramework/new.st")), "the fresh file was not touched");
check(norm(strayRootOf(main, {})) === norm(join(main, "tmp", "stray")) && norm(strayRootOf(main, { LF_LEAD_STRAY_DIR: dest })) === norm(dest), "stray root: tmp/stray by default, LF_LEAD_STRAY_DIR overrides");
mkdirSync(join(base, "lead"), { recursive: true });
check(norm(strayRootOf(main, {})) === norm(join(base, "lead", "stray")), "stray root is <lf>/lead/stray when that folder exists");

if (failures.length) {
  console.log(`FAIL  main-write-guard-selftest: ${failures.length} check(s) failed`);
  for (const f of failures) console.log(`  ${f}`);
  process.exit(1);
}
console.log(`ok  main-write-guard-selftest: write guard and stray mover behave (hook call ${avg} ms average over ${times.length} runs)`);
