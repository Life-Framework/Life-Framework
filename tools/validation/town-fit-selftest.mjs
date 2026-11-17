// tools/validation/town-fit-selftest.mjs
// inputs: tools/town/**, tools/commands/town.mjs
//
// Proves `tools\cli town fit <place> <slot>` (docs/architecture/everon-world.md 3.3, 3.4) in dry mode, on the fixture
// mapping, template, kit and the map fixture's dossier (tools/town/fixtures/fit-*.json, map-dossier.json: an invented
// place, no real coordinate):
//   - the slot's layer text holds one LF_PlaceComponent per promised kind (kind from the template, id `<place>.<slot>`)
//     and every kit row, each at the position `kit apply` gives from the dossier's building, turned to face its entrance;
//   - no coordinate is typed by hand: every `coords` line of the layer is the marker or a kit row, and a
//     coordinate flag is refused;
//   - the claim file names the place, slot, building, layer and kit;
//   - a placed building (D9, a free plot) is written with its destruction switched off (D15); a vanilla building the
//     layer cannot override is recorded in the claim so `town check` and the lead see it;
//   - a building another claim holds on origin/main, in an open PR (#2021 read) or in a sibling worktree is refused,
//     and so is a slot the mapping leaves out or marks `none`;
//   - --dry writes nothing.
// red-proof: before tools/town/fit.mjs existed this printed "FAIL cannot load tools/town/fit.mjs: ERR_MODULE_NOT_FOUND" (2026-11-16).
// red-proof: with the claim conflicts ignored in planFit() this printed "FAIL a building claimed on origin/main was fitted again" (and the open-PR and same-slot cases) (2026-11-16).
// red-proof: with the kit rows' yaw ignored (anchor yaw 0) this printed "FAIL shop row fit-display is not where kit apply puts it" (2026-11-16).

import { existsSync, mkdtempSync, readFileSync, readdirSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const FIX = join(ROOT, "tools", "town", "fixtures");
let failed = 0;
const fail = (msg) => {
  console.log(`FAIL  ${msg}`);
  failed++;
};
const check = (cond, msg) => {
  if (!cond) fail(msg);
};

let fitMod, kitMod;
try {
  fitMod = await import(pathToFileURL(join(ROOT, "tools", "town", "fit.mjs")).href);
  kitMod = await import(pathToFileURL(join(ROOT, "tools", "town", "kit.mjs")).href);
} catch (e) {
  fail(`cannot load tools/town/fit.mjs: ${e.code ?? e.message}`);
  console.log(`SUMMARY town-fit-selftest fail ${failed}`);
  process.exit(1);
}

const dossier = JSON.parse(readFileSync(join(FIX, "map-dossier.json"), "utf8"));
const mapping = JSON.parse(readFileSync(join(FIX, "fit-mapping.json"), "utf8"));
const template = JSON.parse(readFileSync(join(FIX, "fit-template.json"), "utf8"));
const kit = JSON.parse(readFileSync(join(FIX, "fit-kit.json"), "utf8"));
const b03 = dossier.buildings.find((b) => b.id === "b03");

// Every outside read is injected: git and gh answer from tables, nothing touches the network.
const noGit = (cmd, args) => {
  if (cmd === "gh") return { status: 0, stdout: "[]", stderr: "" };
  if (args[0] === "ls-tree" || args[0] === "fetch") return { status: 0, stdout: "", stderr: "" };
  if (args[0] === "worktree") return { status: 0, stdout: "", stderr: "" };
  return { status: 1, stdout: "", stderr: "unexpected call" };
};
const base = { place: "sample-square", slot: "shop", dossier, mapping, template, kits: { "fit-kit": kit }, kitFile: "tools/town/fixtures/fit-kit.json" };
const ctx = (run = noGit) => ({ root: ROOT, run, now: () => new Date("2026-11-16T12:00:00Z") });

// ---- the happy path: a vanilla building
const plan = fitMod.planFit(base, ctx());
check(plan.ok, `a sound slot did not plan: ${(plan.errors || []).join("; ")}`);
const text = plan.layerText ?? "";
check(plan.layerName === "Place_sample-square_shop.layer" || plan.layerName === "Place_sample_square_shop.layer" || /^Place_.*shop\.layer$/.test(plan.layerName || ""), `layer name is ${plan.layerName}`);
const comps = text.match(/LF_PlaceComponent\s+"\{[0-9A-F]{16}\}"/g) || [];
check(comps.length === 1, `the shop layer holds ${comps.length} LF_PlaceComponent, expected 1`);
check(/m_sKind "StoreCounter"/.test(text) && /m_sId "sample-square\.shop"/.test(text), "the place component lacks kind StoreCounter or id sample-square.shop");

// Hand-derived: the entrance is on the south side (z below the centre), the kit's front is -x, so the kit turns by -90.
const YAW = -90;
const expect = kitMod.applyKit(kit, b03.footprint.cx, b03.footprint.cz, YAW, b03.y);
check(Math.abs(plan.anchor.yaw - YAW) < 1e-6 && plan.anchor.x === b03.footprint.cx && plan.anchor.z === b03.footprint.cz, `anchor is ${JSON.stringify(plan.anchor)}`);
for (const row of expect) {
  const name = `Fit_sample_square_shop_${row.id.replace(/\W/g, "_")}`;
  const m = new RegExp(`${name} : "[^"]+" \\{[\\s\\S]*?\\n coords ([^\\n]+)`).exec(text);
  if (!m) {
    fail(`shop row ${row.id} is missing from the layer`);
    continue;
  }
  const got = m[1].trim().split(/\s+/).map(Number);
  if (got.some((n, i) => Math.abs(n - row.coords[i]) > 0.001)) fail(`shop row ${row.id} is not where kit apply puts it (${got.join(" ")} vs ${row.coords.join(" ")})`);
  if (!text.includes(row.prefab)) fail(`row ${row.id} lost its prefab ${row.prefab}`);
}
check(/m_sSignId "fixture:shop"/.test(text), "the sign row lost its m_sSignId override");

// No coordinate is typed: every coords line is the marker or a kit row.
const allowed = new Set([...expect.map((r) => r.coords.join(" ")), plan.anchor.marker.join(" ")]);
for (const m of text.matchAll(/^\s*coords (.+)$/gm)) check(allowed.has(m[1].trim()), `a coords line ${m[1].trim()} is neither the marker nor a kit row`);
const typed = await fitMod.main(["sample-square", "shop", "--x", "1", "--z", "2", "--dry"], { root: ROOT });
check(typed === 2, `a coordinate flag was accepted (exit ${typed})`);

// ---- the claim
const claim = plan.claim;
check(claim && claim.place === "sample-square" && claim.slot === "shop" && claim.building === "b03" && claim.kit === "fit-kit", `claim is ${JSON.stringify(claim)}`);
check(claim && /^addons\/LF_Everon1989\/Worlds\/MainWorld\/MainWorld_Layers\/Place_.*shop\.layer$/.test(claim.layer || ""), `claim layer is ${claim?.layer}`);
check(plan.claimFile === "tools/town/claims/sample-square-shop.json", `claim file is ${plan.claimFile}`);
check(claim?.destruction?.mode === "needs-hook", "a vanilla building's claim does not say its destruction override is still open");

// ---- D9, a placed building: the layer carries it, indestructible
const bar = fitMod.planFit({ ...base, slot: "bar", buildingPrefab: "{0123456789ABCDEF}Prefabs/Structures/Sample/Pub_Sample.et", destructionGuid: "{58F896B4CD371A8B}" }, ctx());
check(bar.ok, `a plot slot did not plan: ${(bar.errors || []).join("; ")}`);
check(/Pub_Sample\.et" \{/.test(bar.layerText || "") && /SCR_DestructionMultiPhaseComponent "\{[0-9A-F]{16}\}" \{\s*Enabled 0/.test(bar.layerText || ""), "a placed building is not written with its destruction off");
check((bar.layerText.match(/LF_PlaceComponent/g) || []).length === 2, "the bar slot promises CafeTable and Radio: two place components");
const noPrefab = fitMod.planFit({ ...base, slot: "bar" }, ctx());
check(!noPrefab.ok && /building-prefab/.test((noPrefab.errors || []).join(" ")), "a plot slot without its resolved prefab was not refused");

// ---- refusals
const held = (where) => (cmd, args) => {
  const claimJson = JSON.stringify({ schema: 1, place: "sample-square", slot: "elsewhere", building: "b03", layer: "x.layer", kit: "k" });
  if (where === "main") {
    if (cmd === "git" && args[0] === "ls-tree") return { status: 0, stdout: "tools/town/claims/sample-square-elsewhere.json\n", stderr: "" };
    if (cmd === "git" && args[0] === "show") return { status: 0, stdout: claimJson, stderr: "" };
  }
  if (where === "pr" && cmd === "gh") {
    if (args[0] === "pr") return { status: 0, stdout: JSON.stringify([{ number: 4242, headRefName: "ws/town-draft-sample-square-elsewhere", headRefOid: "abc", files: [{ path: "tools/town/claims/sample-square-elsewhere.json" }] }]), stderr: "" };
    if (args[0] === "api") return { status: 0, stdout: claimJson, stderr: "" };
  }
  if (where === "tree" && cmd === "git" && args[0] === "worktree") return { status: 0, stdout: "worktree /nowhere/ws-elsewhere\nbranch refs/heads/ws/elsewhere\n\n", stderr: "" };
  return noGit(cmd, args);
};
for (const where of ["main", "pr"]) {
  const r = fitMod.planFit(base, ctx(held(where)));
  check(!r.ok && /already claimed/.test((r.errors || []).join(" ")), `a building claimed on origin/main was fitted again`.replace("origin/main", where === "main" ? "origin/main" : "an open PR"));
}
const none = fitMod.planFit({ ...base, mapping: { ...mapping, slots: { ...mapping.slots, shop: { none: true, reason: "x" } } } }, ctx());
check(!none.ok, "a `none` slot was fitted");
const absent = fitMod.planFit({ ...base, slot: "garage" }, ctx());
check(!absent.ok, "a slot the mapping lacks was fitted");
const self = fitMod.planFit(base, ctx((cmd, args) => (cmd === "git" && args[0] === "ls-tree" ? { status: 0, stdout: "tools/town/claims/sample-square-shop.json\n", stderr: "" } : cmd === "git" && args[0] === "show" ? { status: 0, stdout: JSON.stringify({ schema: 1, place: "sample-square", slot: "shop", building: "b03" }), stderr: "" } : noGit(cmd, args))));
check(!self.ok, "a slot already fitted on origin/main was fitted again");

// ---- dry writes nothing
const out = mkdtempSync(join(tmpdir(), "lf-fit-"));
try {
  const code = await fitMod.main(["sample-square", "shop", "--dry", "--no-fetch", "--dossier", join(FIX, "map-dossier.json"), "--mapping", join(FIX, "fit-mapping.json"), "--template-file", join(FIX, "fit-template.json"), "--kits-dir", FIX, "--out-dir", out], { root: ROOT, run: noGit });
  check(code === 0, `fit --dry exited ${code}`);
  check(readdirSync(out).length === 0, "fit --dry wrote files");
} finally {
  rmSync(out, { recursive: true, force: true });
}

console.log(failed ? `SUMMARY town-fit-selftest fail ${failed}` : "SUMMARY town-fit-selftest ok");
process.exit(failed ? 1 : 0);
