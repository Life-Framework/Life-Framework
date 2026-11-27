// tools/validation/game-update-selftest.mjs
// inputs: tools/lib/game-update.mjs, tools/commands/game-update.mjs
//
// The game-update guard (docs/worktrees.md "Game update guard"): fixture Steam manifests and fixture exes prove that a
// client one version ahead of the server is reported with both builds, that Steam's own "update wanted / queued"
// manifest states count, that an in-step install is silent, and that `game-update` refuses while a session is held,
// waits for servers without stopping them, gives up at its budget, and finishes when the build matches.
// red-proof: with evaluate() returning { ok: true, problems: [], stale: [] } this printed
// "FAIL a client ahead of the server is pending" and "FAIL game-update refuses while a session is held: expected 1, got 0" (2026-11-27).
// red-proof: with cmpVersion() comparing the version strings as text this printed "FAIL 1.10 is newer than 1.9" (2026-11-27).
import { mkdirSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { scratchDir } from "../lib/scratch.mjs";
import * as gu from "../lib/game-update.mjs";

const fails = [];
const eq = (a, b, what) => {
  if (JSON.stringify(a) !== JSON.stringify(b)) fails.push(`${what}: expected ${JSON.stringify(b)}, got ${JSON.stringify(a)}`);
};
const ok = (c, what) => {
  if (!c) fails.push(what);
};

const acf = (id, name, build, { target = build, flags = 4, dl = 0 } = {}) => `"AppState"
{
\t"appid"\t\t"${id}"
\t"name"\t\t"${name}"
\t"StateFlags"\t\t"${flags}"
\t"LastUpdated"\t\t"1795787707"
\t"buildid"\t\t"${build}"
\t"BytesToDownload"\t\t"${dl}"
\t"BytesDownloaded"\t\t"0"
\t"TargetBuildID"\t\t"${target}"
}
`;

// An exe that is just filler with the two places a version lives, as in the real game exes (1.9.0.42): a
// VS_FIXEDFILEINFO (signature, struct version 1.0, then MS/LS: the real one says only 1.9.0.0) and the string table's
// UTF-16 "FileVersion" key, two NULs, the full version text. withString false leaves only the fixed info.
function exeBytes(ver, padBefore = 100, withString = true) {
  const [a, b, c, d] = ver.split(".").map(Number);
  const info = Buffer.alloc(52);
  info.writeUInt32LE(0xfeef04bd, 0);
  info.writeUInt32LE(0x00010000, 4);
  info.writeUInt32LE(((a << 16) | b) >>> 0, 8);
  info.writeUInt32LE((withString ? 0 : (c << 16) | d) >>> 0, 12);
  const str = withString ? Buffer.concat([Buffer.from("FileVersion", "utf16le"), Buffer.alloc(4), Buffer.from(ver, "utf16le"), Buffer.alloc(4)]) : Buffer.alloc(0);
  return Buffer.concat([Buffer.alloc(padBefore, 0x41), info, Buffer.alloc(64, 0x42), str]);
}

// A fake Steam library: <lib>/steamapps/appmanifest_*.acf and <lib>/steamapps/common/<dir>/<exe>.
function library({ client, server, tools, cBuild = "26639613", sBuild = "26634883", sExtra = {}, cExtra = {} }) {
  const lib = scratchDir("lf-gu-");
  const sa = join(lib, "steamapps");
  const dirs = {};
  for (const [k, app] of Object.entries(gu.APPS)) {
    dirs[k] = join(sa, "common", app.dir);
    mkdirSync(dirs[k], { recursive: true });
  }
  writeFileSync(join(sa, "appmanifest_1874880.acf"), acf(1874880, "Arma Reforger", cBuild, cExtra));
  writeFileSync(join(sa, "appmanifest_1874900.acf"), acf(1874900, "Arma Reforger Server", sBuild, sExtra));
  writeFileSync(join(sa, "appmanifest_1874910.acf"), acf(1874910, "Arma Reforger Tools", "26634956"));
  if (client) writeFileSync(join(dirs.client, "ArmaReforgerSteam.exe"), exeBytes(client));
  if (server) writeFileSync(join(dirs.server, "ArmaReforgerServer.exe"), exeBytes(server, 5 * 1024 * 1024 + 7));
  return dirs;
}

// ---- parsing
const m = gu.parseAcf(acf(1874900, "Arma Reforger Server", "26634883", { target: "26700000", flags: 1026 }));
eq([m.appid, m.buildid, m.target, m.flags], [1874900, "26634883", "26700000", 1026], "an .acf parses its ids and flags");

// ---- exe version, including a resource past the first 4 MB block
const d0 = library({ client: "1.9.0.42", server: "1.8.1.7" });
eq(gu.exeVersion(join(d0.client, "ArmaReforgerSteam.exe")), "1.9.0.42", "the client exe version is read");
eq(gu.exeVersion(join(d0.server, "ArmaReforgerServer.exe")), "1.8.1.7", "a version past the first block is read");
eq(gu.exeVersion(join(d0.tools, "none.exe")), null, "a missing exe is null");
// The real exes carry 1.9.0.0 in the fixed info and 1.9.0.42 in the string table: a build-only difference shows.
writeFileSync(join(d0.client, "OnlyFixed.exe"), exeBytes("1.9.0.42", 100, false));
eq(gu.exeVersion(join(d0.client, "OnlyFixed.exe")), "1.9.0.42", "with only the fixed info its four parts are read");
const dBuild = library({ client: "1.9.0.42", server: "1.9.0.40" });
ok(!gu.check(null, { dirs: dBuild }).verdict.ok, "a client one build ahead of the server (same 1.9.0) is pending");
ok(gu.cmpVersion("1.10.0.0", "1.9.0.0") > 0, "1.10 is newer than 1.9");
eq(gu.cmpVersion("1.9.0.42", "1.9.0.42"), 0, "equal versions compare equal");

// ---- the 2026-11-27 case: client ahead of the server
const stale = gu.check(null, { dirs: d0 });
ok(!stale.verdict.ok, "a client ahead of the server is pending");
eq(stale.verdict.stale, ["server"], "the server is the app to update");
ok(/^GAME UPDATE PENDING: /.test(stale.line ?? ""), "the line starts GAME UPDATE PENDING");
ok((stale.line ?? "").includes("client 1.9.0.42 (build 26639613)") && (stale.line ?? "").includes("server 1.8.1.7 (build 26634883)"), "the line names both versions and builds");
ok((stale.line ?? "").includes("tools\\cli game-update"), "the line names the fix");

// ---- in step: silent (the build ids differ between apps and are not compared)
const d1 = library({ client: "1.9.0.42", server: "1.9.0.42" });
const inStep = gu.check(null, { dirs: d1 });
eq(inStep.line, null, "an in-step install prints no pending line");
ok(gu.okLine(inStep.install).includes("in step"), "the ok line says in step");

// ---- Steam's own states: a newer TargetBuildID, or an update queued in StateFlags
const d2 = library({ client: "1.9.0.42", server: "1.9.0.42", sExtra: { target: "26700000", flags: 6 } });
const wants = gu.check(null, { dirs: d2 });
ok(!wants.verdict.ok && /Steam wants build 26700000/.test(wants.line ?? ""), "buildid behind TargetBuildID is pending");
const d3 = library({ client: "1.9.0.42", server: "1.9.0.42", sExtra: { flags: 1026 } });
ok(/an update is queued/.test(gu.check(null, { dirs: d3 }).line ?? ""), "an UpdateStarted StateFlags is pending");
const d4 = library({ client: "1.9.0.42", server: "1.9.0.42", sExtra: { flags: 4 } });
eq(gu.check(null, { dirs: d4 }).line, null, "FullyInstalled alone is not pending");
const d5 = library({ client: null, server: null });
eq(gu.check(null, { dirs: d5 }).line, null, "missing exes (no version) and clean manifests are not pending");

// ---- game-update: bounded procedure over fakes
function harness({ install, held = [], running = [], updates = true }) {
  const state = { t: 0, lines: [], triggered: [], inst: install, running: [...running], held: [...held] };
  const deps = {
    read: () => state.inst,
    held: () => state.held,
    running: () => state.running,
    trigger: (id) => {
      state.triggered.push(id);
      if (updates) state.inst = gu.readInstall(d1);
    },
    sleep: async (ms) => {
      state.t += ms;
      if (state.running.length && state.t >= 30000) state.running = [];
    },
    now: () => state.t,
    log: (l) => state.lines.push(l),
  };
  return { state, deps };
}
const staleInstall = () => gu.readInstall(d0);

let h = harness({ install: staleInstall(), held: [{ lock: "server-2001", root: "x" }] });
let r = await gu.updateRun({}, h.deps);
eq(r.code, 1, "game-update refuses while a session is held");
eq(h.state.triggered, [], "a refused game-update triggers nothing");

h = harness({ install: staleInstall(), running: [{ pid: 4460, app: "server" }] });
r = await gu.updateRun({ waitMs: 10000, pollMs: 5000 }, h.deps);
eq([r.code, r.why, h.state.triggered], [1, "servers-running", []], "game-update gives up at its wait budget without triggering");

h = harness({ install: staleInstall(), running: [{ pid: 4460, app: "server" }] });
r = await gu.updateRun({ waitMs: 120000, pollMs: 5000 }, h.deps);
eq([r.code, r.why, h.state.triggered], [0, "updated", [1874900]], "game-update waits for the server, triggers the server app and finishes");

h = harness({ install: staleInstall(), updates: false });
r = await gu.updateRun({ updateMs: 20000, pollMs: 5000 }, h.deps);
eq([r.code, r.why], [1, "update-timeout"], "game-update gives up when the build never matches");

h = harness({ install: gu.readInstall(d1) });
r = await gu.updateRun({}, h.deps);
eq([r.code, r.why, h.state.triggered], [0, "in-step", []], "an in-step install needs nothing");

h = harness({ install: staleInstall() });
r = await gu.updateRun({ dryRun: true }, h.deps);
eq([r.code, r.why, h.state.triggered], [0, "dry-run", []], "--dry-run triggers nothing");

for (const f of fails) console.log(`FAIL  ${f}`);
console.log(fails.length ? `SUMMARY game-update-selftest fail ${fails.length}` : "SUMMARY game-update-selftest ok");
process.exit(fails.length ? 1 : 0);
