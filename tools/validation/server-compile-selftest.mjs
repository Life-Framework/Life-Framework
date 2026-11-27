// tools/validation/server-compile-selftest.mjs
// inputs: tools/lib/server-compile.mjs, tools/lib/main-guard.mjs, tools/cli.mjs
//
// The main guard's compile step also compiles on the dedicated server under the session defines (#5180): a long
// LF_Test_SoldSignFace.Run crashed that compiler alone (access violation, no SCRIPT (E) line) while Workbench validate
// and the suite compiled it, so no harness session booted. This checks the verdict on the real crash log, that the
// FAIL line reads as a compile failure to the guard (not "env", which would never revert), the command line a session
// boots, the runner against a fake server, and that `ci --wb-only` runs the step.
// red-proof: 2026-11-27, with the crash branch removed from judgeServerCompile this printed
// "FAIL the crash log is a crash" and "FAIL the crash verdict quotes the crash line" and exited 1; restored, it passes.
import { mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { EventEmitter } from "node:events";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { SESSION_DEFINES, judgeServerCompile, runServerCompile, serverCompileArgs, verdictLine } from "../lib/server-compile.mjs";
import { COMPILE_ERROR, compileVerdict } from "../lib/main-guard.mjs";
import { releaseScratch, scratchDir } from "../lib/scratch.mjs";

const here = dirname(fileURLToPath(import.meta.url));
let failed = 0;
const ok = (cond, what) => {
  if (!cond) {
    failed++;
    console.log(`FAIL ${what}`);
  }
};

// The server's console.log on main at 19184a788 (session up --mission playtest, 2026-11-27).
const CRASH = [
  "22:55:28.465  SCRIPT       : SCRIPT       : Compiling GameLib scripts",
  "22:55:28.529  SCRIPT       : Module: GameLib; loaded 512x files; 664x classes; used 713K of static memory; defines: \"LF_SESSION_IDENTITY,LF_HARNESS,PLATFORM_WINDOWS\"",
  "22:55:28.529  SCRIPT       : SCRIPT       : Compiling Game scripts",
  "22:55:29.745 SCRIPT       : Compiling Game scripts",
  "22:55:29.745  ENGINE    (F): Crashed",
  "22:55:29.879  ENGINE    (E): Application crashed! Generated memory dump: C:/x/ArmaReforgerServer.mdmp",
].join("\r\n");
const PASS = "22:56:14.133  SCRIPT       : Module: Game; loaded 8566x files; 17756x classes; used 46321K of static memory";
const ERROR = "SCRIPT    (E): @\"Scripts/Game/LF_X.c,5\": Broken expression (missing ';'?)\nSCRIPT    (E): Can't compile \"Game\" script module!";

const crash = judgeServerCompile(CRASH);
ok(crash?.verdict === "crash", "the crash log is a crash");
ok(crash?.lines.some((l) => /Crashed/.test(l)), "the crash verdict quotes the crash line");
ok(COMPILE_ERROR.test(verdictLine(crash)), "the crash FAIL line reads as a compile failure to the main guard");
ok(compileVerdict(1, verdictLine(crash)) === "fail", "the guard calls a server compile crash red (not env)");
ok(judgeServerCompile(PASS)?.verdict === "ok", "a compiled Game module passes");
ok(judgeServerCompile(`${ERROR}`)?.verdict === "error", "SCRIPT (E) with Can't compile is an error");
ok(judgeServerCompile(`${ERROR}\n${PASS}`)?.verdict === "error", "an error line fails even when the module line follows");
ok(judgeServerCompile("22:55:28.529  SCRIPT : Compiling Game scripts") === null, "a compile still running has no verdict");
ok(compileVerdict(4, verdictLine({ verdict: "env", why: "no server" })) === "env", "no server install is env, not red");

const args = serverCompileArgs({ mission: "{1054CC8646357708}Missions/LF_PlaytestTown.conf", addonsDirs: ["D:/w/addons", "", "D:/w/game-addons"], addons: ["LifeFramework", "LF_Everon1989"], profile: "D:/w/p" });
const defs = args.filter((_, i) => args[i - 1] === "-scrDefine");
ok(SESSION_DEFINES.every((d) => defs.includes(d)), "the probe compiles with LF_SESSION_IDENTITY and LF_HARNESS, as session up does");
ok(args[args.indexOf("-addonsDir") + 1] === "D:/w/addons,D:/w/game-addons", "one -addonsDir, empty entries dropped");
ok(args[args.indexOf("-server") + 1].includes("LF_PlaytestTown"), "the probe boots the playtest header");

// The runner against a fake server that writes a console.log then stays up until killed.
const scratch = scratchDir("lf-server-compile-");
const exe = join(scratch, "ArmaReforgerServer.exe");
writeFileSync(exe, "");
const fake = (text) => (_exe, a) => {
  const child = new EventEmitter();
  child.killed = false;
  child.kill = () => { child.killed = true; child.emit("exit", 0); };
  const profile = a[a.indexOf("-profile") + 1];
  const dir = join(profile, "logs", `logs_${Date.now()}`);
  setTimeout(() => { mkdirSync(dir, { recursive: true }); writeFileSync(join(dir, "console.log"), text); }, 20);
  fake.last = child;
  return child;
};
const run = (name, text, extra = {}) => {
  const profile = join(scratch, name);
  return runServerCompile({ exe, args: serverCompileArgs({ mission: "m", addonsDirs: ["a"], addons: ["LifeFramework"], profile }), profile, spawnFn: fake(text), pollMs: 20, ...extra });
};
const r1 = await run("crash", CRASH);
ok(r1.verdict === "crash" && fake.last.killed, "the runner reports the crash and kills the server");
const r2 = await run("pass", PASS);
ok(r2.verdict === "ok" && fake.last.killed && /console\.log$/.test(r2.log ?? ""), "the runner passes a compiled module, kills the server and names the log");
const r3 = await run("hang", "SCRIPT : Compiling Game scripts", { timeoutMs: 200 });
ok(r3.verdict === "env" && /no verdict/.test(r3.why), "a hung compile is env after the budget");
const r4 = await runServerCompile({ exe: join(scratch, "missing.exe"), args: [], profile: join(scratch, "m"), spawnFn: fake(PASS) });
ok(r4.verdict === "env" && /no dedicated server/.test(r4.why), "no server install is env");
releaseScratch(scratch);

// `ci --wb-only` (the main guard's compile step and validate --full) runs it after the Workbench compile.
const cli = readFileSync(join(here, "..", "cli.mjs"), "utf8");
const ci = cli.slice(cli.indexOf("async function ciWorkbench("), cli.indexOf("async function ciWorkbench(") + 6000);
ok(/serverCompileStep\(root\)/.test(ci), "ciWorkbench runs serverCompileStep");
ok(/serverGameAddons\(root, "compile-probe"\)/.test(cli), "the probe has its own game-addons junction, never a live session's");

if (failed) {
  console.log(`server-compile-selftest: ${failed} failed`);
  process.exit(1);
}
console.log("server-compile-selftest: OK");
