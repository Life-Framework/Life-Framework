// tools/validation/host-public-selftest.mjs
// inputs: tools/host/**, tools/lib/addons.mjs, server/flagship/**
//
// Proves `tools\cli host start --public` without a live run: the launch is a backend-registered
// `-config <profile>/server.json -profile <profile>` boot with no -noBackend, no -addons and no
// -server; and the pre-flight refuses a missing server.json, placeholder or unconfirmed Workshop
// IDs, an empty password, the wrong scenario and a drifted config, and passes the real flagship.
// Also `host config apply --mods` (the publish confirmation) and the watch/autostart pass-through.
// red-proof: with publicHostArgs pushing "-noBackend" this printed
// "a public launch passes -noBackend (not in the server browser, no join password)" and exited 1 (2026-11-15).
// red-proof: with publicPreflight skipping the password check this printed
// "pre-flight passes an empty game.password" and exited 1 (2026-11-15).
import { mkdirSync, mkdtempSync, readFileSync, writeFileSync, existsSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import { safeRemoveTree } from "../link-safety.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const imp = (p) => import(pathToFileURL(join(ROOT, ...p.split("/"))).href);
const { publicHostArgs, publicPreflight, isPlaceholderId, readWorkshopConfirm, workshopConfirmPath, parseModsFlag } = await imp("tools/host/public.mjs");
const { applyConfig, serverJsonPath } = await imp("tools/host/config-apply.mjs");
const { PACK_MISSION } = await imp("tools/lib/addons.mjs");
const { runAtLogon, taskCommand } = await imp("tools/host/autostart.mjs");

const failures = [];
const fail = (m) => failures.push(m);
const CORE = "79636E668EA37AC9";
const PACKID = "B1E84D5687E11724";

// ---- launch arguments
const args = publicHostArgs({ profile: "P", serverJson: "P/server.json", workshopDir: "W", loadSave: false });
const reload = publicHostArgs({ profile: "P", serverJson: "P/server.json", workshopDir: "W", loadSave: true });
const val = (a, f) => a[a.indexOf(f) + 1];
if (args.includes("-noBackend")) fail("a public launch passes -noBackend (not in the server browser, no join password)");
if (args.includes("-addons")) fail("a public launch passes -addons (the -config boot drops it; the mods come from game.mods)");
if (args.includes("-server")) fail("a public launch passes -server (the scenario comes from game.scenarioId)");
if (val(args, "-config") !== "P/server.json") fail("a public launch does not boot -config <profile>/server.json");
if (val(args, "-profile") !== "P") fail("a public launch does not use the host profile");
if (val(args, "-addonsDir") !== "W") fail("a public launch does not keep Workshop downloads in the host's workshop folder");
if (args.some((a) => /LF_AUTOTEST|LF_TEST_TIER|LF_E2E|LF_HARNESS|LF_SESSION_IDENTITY|-scrDefine/.test(a))) fail("a public launch carries a test define");
if (args.includes("-loadSessionSave")) fail("a public boot with no save passes -loadSessionSave");
if (!reload.includes("-loadSessionSave")) fail("a public boot with a save does not pass -loadSessionSave");
if (args.includes("-addr")) fail("a public launch binds -addr (bindPort/publicPort come from server.json)");

// ---- placeholder ids
for (const id of ["0000000000000000", "FFFFFFFFFFFFFFFF", "0123456789ABCDEF", "PLACEHOLDER00000", "79636E668EA37AC", "", undefined]) {
  if (!isPlaceholderId(id)) fail(`"${id}" is not seen as a placeholder`);
}
for (const id of [CORE, PACKID]) if (isPlaceholderId(id)) fail(`${id} is seen as a placeholder`);

// ---- --mods parsing
if (JSON.stringify(parseModsFlag(`${CORE.toLowerCase()}, ${PACKID}`).ids) !== JSON.stringify([CORE, PACKID])) fail("--mods does not take two ids, any case, comma and spaces");
if (!parseModsFlag(CORE).error) fail("--mods takes a single id (it needs the core and the pack)");
if (!parseModsFlag(`${CORE},0000000000000000`).error) fail("--mods takes a placeholder id");

// ---- pre-flight on a parsed server.json
const flagship = JSON.parse(readFileSync(join(ROOT, "server", "flagship", "server.json"), "utf8"));
const good = structuredClone(flagship);
good.game.password = "alpha-pass";
good.game.passwordAdmin = "admin-pass";
good.game.admins = ["0f0e0d0c-0000-4000-8000-000000000001"];
const confirm = { mods: [CORE, PACKID] };
const pf = (sj, extra = {}) => publicPreflight({ serverJson: sj, confirm, drift: [], scenario: PACK_MISSION, ...extra });
const ok = pf(good);
if (!ok.ok) fail(`pre-flight refuses the real flagship config: ${ok.refusals.join("; ")}`);
if (ok.warnings.length) fail(`pre-flight warns on a complete config: ${ok.warnings.join("; ")}`);
const refusedFor = (label, sj, extra, re) => {
  const r = pf(sj, extra);
  if (r.ok) fail(`pre-flight passes ${label}`);
  else if (!r.refusals.some((m) => re.test(m))) fail(`pre-flight refuses ${label} without saying why (${r.refusals.join("; ")})`);
};
refusedFor("a missing server.json", null, {}, /host config apply/);
const noPw = structuredClone(good); noPw.game.password = "";
refusedFor("an empty game.password", noPw, {}, /game\.password/);
const blankPw = structuredClone(good); blankPw.game.password = "   ";
refusedFor("a blank game.password", blankPw, {}, /game\.password/);
const placeholder = structuredClone(good); placeholder.game.mods[1].modId = "0000000000000000";
refusedFor("a placeholder Workshop id", placeholder, {}, /placeholder/);
const oneMod = structuredClone(good); oneMod.game.mods = oneMod.game.mods.slice(0, 1);
refusedFor("a mod list without the pack", oneMod, { confirm: { mods: [CORE] } }, /core and the pack|2 mods/);
const noMods = structuredClone(good); delete noMods.game.mods;
refusedFor("no game.mods", noMods, {}, /game\.mods/);
refusedFor("unconfirmed Workshop ids", good, { confirm: null }, /--mods/);
refusedFor("ids confirmed for other items", good, { confirm: { mods: [CORE, "C1E84D5687E11724"] } }, /--mods/);
const wrongScenario = structuredClone(good); wrongScenario.game.scenarioId = "{1054CC8646357708}Missions/LF_PlaytestTown.conf";
refusedFor("the playtest scenario", wrongScenario, {}, /scenarioId/);
refusedFor("a drifted config", good, { drift: [{ file: "server.json", field: "game.name", repo: "a", host: "b" }] }, /drift/);
const hidden = structuredClone(good); hidden.game.visible = false;
refusedFor("game.visible false", hidden, {}, /visible/);
const noAdmins = structuredClone(good); noAdmins.game.admins = [];
const na = pf(noAdmins);
if (!na.ok || !na.warnings.some((w) => /admins/.test(w))) fail("an empty game.admins is not a warning (the maintainer learns his id on the first join)");
const versioned = structuredClone(good); versioned.game.mods[0].version = "not a version";
refusedFor("a mod version that is not x.y.z", versioned, {}, /version/);
const pinned = structuredClone(good); pinned.game.mods[0].version = "0.1.0";
if (!pf(pinned).ok) fail("pre-flight refuses a mod pinned to 0.1.0");
// The summary never carries the password.
if (JSON.stringify(pf(good)).includes("alpha-pass")) fail("the pre-flight result carries the password");

// ---- config apply --mods: the confirmation file, refusals, and a real apply on a fixture host
const base = mkdtempSync(join(tmpdir(), "lf-host-public-"));
try {
  const host = join(base, "host");
  mkdirSync(host, { recursive: true });
  writeFileSync(join(host, "secrets.json"), JSON.stringify({ game: { password: "alpha-pass", passwordAdmin: "admin-pass" } }));
  const repoDir = join(ROOT, "server", "flagship");
  const r0 = applyConfig(host, { adminsPending: true, repoDir });
  if (!r0.ok) fail(`config apply refused the flagship: ${r0.reason}`);
  if (readWorkshopConfirm(host)) fail("config apply without --mods wrote the Workshop confirmation");
  const r1 = applyConfig(host, { adminsPending: true, repoDir, mods: [CORE, "C1E84D5687E11724"] });
  if (r1.ok) fail("config apply --mods takes ids that differ from server/flagship/server.json");
  else if (!/server\/flagship\/server\.json|PR/.test(r1.reason)) fail(`a --mods mismatch does not say where to change the ids (${r1.reason})`);
  if (existsSync(workshopConfirmPath(host))) fail("a refused --mods wrote the confirmation");
  const r2 = applyConfig(host, { adminsPending: true, repoDir, mods: [CORE, PACKID] });
  if (!r2.ok) fail(`config apply --mods refused the published ids: ${r2.reason}`);
  const c = readWorkshopConfirm(host);
  if (!c || JSON.stringify(c.mods) !== JSON.stringify([CORE, PACKID])) fail("config apply --mods did not record the confirmed ids");
  const written = JSON.parse(readFileSync(serverJsonPath(host), "utf8"));
  const real = publicPreflight({ serverJson: written, confirm: c, drift: [], scenario: PACK_MISSION });
  if (!real.ok) fail(`the applied flagship fails the pre-flight: ${real.refusals.join("; ")}`);
  if (applyConfig(host, { adminsPending: true, repoDir, mods: [CORE, PACKID], dryRun: true }).ok !== true) fail("a --mods dry run fails");

  // A roster whose administrator is still #pending: refused with the --admins-pending hint, applied with it.
  const pendRepo = join(base, "pending-repo");
  mkdirSync(pendRepo, { recursive: true });
  for (const f of ["server.json", "config.json"]) writeFileSync(join(pendRepo, f), readFileSync(join(repoDir, f)));
  writeFileSync(join(pendRepo, "staff.txt"), "#pending administrator Jasper\n");
  const p0 = applyConfig(host, { repoDir: pendRepo });
  if (p0.ok) fail("config apply takes a #pending administrator without --admins-pending");
  else if (!/--admins-pending/.test(p0.reason)) fail(`a #pending refusal does not name --admins-pending (${p0.reason})`);
  const p1 = applyConfig(host, { repoDir: pendRepo, adminsPending: true });
  if (!p1.ok) fail(`--admins-pending refused a #pending administrator: ${p1.reason}`);
  else if (JSON.parse(readFileSync(serverJsonPath(host), "utf8")).game.admins.length !== 0) fail("--admins-pending wrote a pending administrator into game.admins");
} finally {
  safeRemoveTree(base);
}

// ---- watch and autostart keep the public mode
const spawned = [];
runAtLogon({ root: "C:\\R", nodePath: "node", spawn: (n, a) => (spawned.push(a), 1), public: true });
if (!spawned[0]?.includes("--public")) fail("autostart --public does not run `host watch --public`");
spawned.length = 0;
runAtLogon({ root: "C:\\R", nodePath: "node", spawn: (n, a) => (spawned.push(a), 1) });
if (spawned[0]?.includes("--public")) fail("plain autostart runs the public host");
if (!taskCommand({ root: "C:\\R", nodePath: "node", public: true }).endsWith("--run --public")) fail("the public task command does not pass --public");
if (taskCommand({ root: "C:\\R", nodePath: "node" }).includes("--public")) fail("the plain task command passes --public");
const watchSrc = readFileSync(join(ROOT, "tools", "host", "commands", "watch.mjs"), "utf8");
if (!/\["--flagship", "--public", "--force"\]/.test(watchSrc)) fail("host watch does not pass --public through to host start");

if (failures.length) {
  console.log("host-public-selftest: FAIL");
  for (const f of failures) console.log(`  ${f}`);
  process.exit(1);
}
console.log("host-public-selftest: OK (public launch args, pre-flight refusals, --mods confirmation, watch/autostart)");
