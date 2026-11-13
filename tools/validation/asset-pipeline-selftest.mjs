// Proves the deterministic parts of tools\cli asset (tools/assets/pipeline/) offline: no Meshy call,
// no Blender, no Workbench. The network is replaced by a fetch that fails the test when touched.
// Each block names the failure it guards (docs/asset-pipeline/README.md).
// red-proof: 2026-11-13, one line broken per block, each restored after its FAIL was seen:
//   specProblems without the kind cap        -> FAIL spec: a weapon over the triangle cap is refused
//   meshy.call without scrub()               -> FAIL meshy: an error never carries the key
//   stageGenerate without the credit check   -> FAIL credits: an attempt over the cap is refused before any call
//   run() ignoring a stage's cached ok       -> FAIL resume: a finished run touches nothing
//   freshGuid without the used-set test      -> FAIL guid: a drawn GUID is never one in use
//   checkSockets without the muzzle test     -> FAIL sockets: a muzzle 5 cm behind the front face is flagged
//   checkModel without the material test     -> FAIL xob: a model that bakes another material is flagged
//   logProblems counting lines before start  -> FAIL log: lines before the plugin starts do not count
//   runLog joining console, script and error -> FAIL log: a run's own start-up lines do not count
//   recordProvenance dropping the refine id  -> FAIL provenance: meshy.json keeps both generations and the refine ids
//   buildPrompts dropping the frame          -> FAIL concepts: every prompt ends in the shared product-shot frame
//   sheetLayout using rows for columns       -> FAIL concepts: ten cells make a 5x2 grid, numbered left to right
//   pickByRubric breaking ties on the high n -> FAIL concepts: a tie goes to the lower number
//   conceptRequest ignoring the reference    -> FAIL concepts: a reference sends image-to-image with the picture
//   checkKitModel skipping the bone check    -> FAIL kit: a model missing a material, a collider or the door bone is flagged
//   ammoListWith without the includes test   -> FAIL prefab: listing the same ammo twice changes nothing
import { existsSync, mkdirSync, mkdtempSync, readFileSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import { STAGES, recordProvenance, run, specProblems } from "../assets/pipeline/asset.mjs";
import * as cc from "../assets/pipeline/concepts.mjs";
import { checkReport, checkSockets } from "../assets/pipeline/checks.mjs";
import { logProblems, runLog } from "../assets/pipeline/import.mjs";
import { KINDS } from "../assets/pipeline/kinds.mjs";
import * as meshy from "../assets/pipeline/meshy.mjs";
import { ematText, freshGuid, textureMeta, usedGuids, writeAssetMetas, xobMeta, xobMetaIds } from "../assets/pipeline/meta.mjs";
import { ammoListWith, ammoPrefab, magazinePrefab, weaponPrefab } from "../assets/pipeline/prefab.mjs";
import { checkModel, parse, worldPositions } from "../assets/pipeline/xob.mjs";
import { checkKitModel, kitModelMeta, kitProblems } from "../assets/pipeline/kit.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");

export async function main() {
  let failed = 0;
  const say = console.log; // the stages' own output is muted below; a FAIL still prints
  const ok = (cond, what) => {
    if (cond) return;
    failed++;
    say(`FAIL ${what}`);
  };
  const throws = async (fn, pattern, what) => {
    try {
      await fn();
      ok(false, `${what} (no error)`);
    } catch (e) {
      ok(pattern.test(e.message), `${what} (got: ${e.message})`);
    }
  };
  const tmp = mkdtempSync(join(tmpdir(), "lf-asset-selftest-"));
  const weapon = KINDS.weapon;
  const spec = () => ({
    slug: "stick", name: "Stick", kind: "weapon", prompt: "A police baton, single object, no text, no logos.", texture_prompt: "",
    meshy: { ai_model: "meshy-6-lite", target_polycount: 20000 },
    size_m: { length: 0.2, height: 0.14, width: 0.036 },
    budget: { lod0_tris: 8000, lod_ratios: [0.5, 0.2], texture_px: 1024, credits: 300 },
    fit: { offset: [0, 0, 0] }, parts: [{ name: "trigger", bone: "w_trigger", box: [0, 0, 0, 1, 1, 1] }],
    asset_dir: "Assets/Weapons/Handguns/Stick", prefab_dir: "Prefabs/Weapons/Handguns/Stick",
  });

  // ---- spec: the spec stage refuses what would waste credits or break an import ---------------
  {
    ok(specProblems(spec()).length === 0, `spec: a complete spec passes (${specProblems(spec())[0]})`);
    const bad = (edit, pattern, what) => {
      const s = spec();
      edit(s);
      ok(specProblems(s).some((p) => pattern.test(p)), `spec: ${what}`);
    };
    bad((s) => (s.budget.lod0_tris = weapon.maxTris + 1), /over the weapon cap/, "a weapon over the triangle cap is refused");
    bad((s) => (s.budget.credits = 301), /per-asset cap/, "a credit budget over 300 is refused");
    bad((s) => (s.prompt = "x".repeat(801) + " no text"), /Meshy allows 800/, "a prompt over 800 characters is refused");
    bad((s) => (s.prompt = "A baton."), /no text, no logos/, "a prompt that allows lettering is refused");
    bad((s) => (s.name = "stick gun"), /PascalCase/, "a name that is not a file name is refused");
    bad((s) => (s.parts[0].bone = "w_bolt"), /not in the weapon skeleton/, "a part on a bone the skeleton lacks is refused");
    bad((s) => (s.size_m.height = 0), /size_m\.height/, "a zero size is refused");
    bad((s) => (s.asset_dir = "addons/x"), /asset_dir/, "an asset_dir outside Assets/ is refused");
  }

  // ---- meshy: request bodies, cost, the ledger, and the key never leaving the module -------------
  {
    const s = spec();
    const p = meshy.previewBody(s);
    ok(p.mode === "preview" && p.topology === "triangle" && p.should_remesh === true && p.target_polycount === 20000, "meshy: the preview asks for a remeshed triangle mesh at the spec's polycount");
    const r = meshy.refineBody({ ...s, texture_prompt: "matte black" }, "task-1");
    ok(r.mode === "refine" && r.preview_task_id === "task-1" && r.enable_pbr === true && r.texture_prompt === "matte black", "meshy: the refine names its preview and asks for PBR maps");
    ok(!("texture_prompt" in meshy.refineBody(s, "task-1")), "meshy: an empty texture prompt is not sent");
    ok(meshy.estimate(s) === 15 && meshy.estimate({ meshy: { ai_model: "latest" } }) === 30, "meshy: one attempt is 15 credits on the lite model and 30 on the latest");

    const ledger = join(tmp, "ledger", "credits.jsonl");
    ok(meshy.spent(ledger) === 0, "meshy: no ledger means nothing spent");
    meshy.logCredits(ledger, { call: "preview", credits: 20 });
    meshy.logCredits(ledger, { call: "refine", credits: 10 });
    ok(meshy.spent(ledger) === 30, "meshy: the ledger adds up its rows");

    const keyFile = join(tmp, "key");
    const KEY = "msy_selftest_not_a_real_key";
    writeFileSync(keyFile, KEY + "\n");
    process.env.LF_MESHY_KEY_FILE = keyFile;
    let sent = null;
    const echo = async (url, init) => {
      sent = { url, init };
      return { ok: false, status: 401, text: async () => JSON.stringify({ message: `invalid key ${KEY}` }) };
    };
    let message = "";
    try { await meshy.call("GET", "/openapi/v1/balance", null, { fetchImpl: echo }); } catch (e) { message = e.message; }
    ok(sent?.init.headers.Authorization === `Bearer ${KEY}`, "meshy: the key goes in the Authorization header");
    ok(/HTTP 401 bad or missing key/.test(message), `meshy: a 401 says what it means (got: ${message})`);
    ok(message && !message.includes(KEY) && message.includes("<meshy-key>"), "meshy: an error never carries the key");
    const boom = async () => { throw new Error(`connect failed for Bearer ${KEY}`); };
    try { await meshy.call("GET", "/x", null, { fetchImpl: boom }); } catch (e) { message = e.message; }
    ok(!message.includes(KEY), "meshy: a network error never carries the key");

    const replies = [{ status: "PENDING", progress: 0 }, { status: "IN_PROGRESS", progress: 40 }, { status: "SUCCEEDED", progress: 100, model_urls: { glb: "u" } }];
    let polls = 0;
    const seq = async () => ({ ok: true, status: 200, text: async () => JSON.stringify(replies[Math.min(polls++, replies.length - 1)]) });
    const done = await meshy.waitTask("t", { fetchImpl: seq, sleep: async () => {} });
    ok(done.status === "SUCCEEDED" && polls === 3, "meshy: waitTask polls until the task succeeds");
    const failedTask = async () => ({ ok: true, status: 200, text: async () => JSON.stringify({ status: "FAILED", task_error: { message: "nsfw" } }) });
    await throws(() => meshy.waitTask("t", { fetchImpl: failedTask, sleep: async () => {} }), /FAILED: nsfw/, "meshy: a failed task throws its reason");
    const stuck = async () => ({ ok: true, status: 200, text: async () => JSON.stringify({ status: "IN_PROGRESS", progress: 10 }) });
    await throws(() => meshy.waitTask("t", { fetchImpl: stuck, sleep: async () => {}, budgetS: -1 }), /rerun the stage to resume/, "meshy: a task past its budget stops the wait and keeps the task");
  }

  // ---- resume and credits: a finished stage is never run again, and the cap stops a paid call ----
  {
    const root = join(tmp, "root");
    const specDir = join(root, "tools", "assets", "stick");
    const cache = join(root, "tmp", "assets", "stick");
    mkdirSync(specDir, { recursive: true });
    mkdirSync(cache, { recursive: true });
    writeFileSync(join(specDir, "spec.json"), JSON.stringify(spec()));
    const realFetch = globalThis.fetch;
    let touched = 0;
    globalThis.fetch = async () => { touched++; throw new Error("the selftest touched the network"); };
    const quiet = console.log;
    console.log = () => {};
    try {
      const stamp = { ok: true, at: new Date().toISOString(), secs: 1, result: {} };
      const all = Object.fromEntries(STAGES.map((s) => [s, stamp]));
      writeFileSync(join(cache, "state.json"), JSON.stringify({ stages: all }));
      const before = readFileSync(join(cache, "state.json"), "utf8");
      const code = await run(root, "stick", {}).catch(() => 1);
      ok(code === 0 && touched === 0 && readFileSync(join(cache, "state.json"), "utf8") === before, "resume: a finished run touches nothing");

      await run(root, "stick", { only: "spec" });
      const st = JSON.parse(readFileSync(join(cache, "state.json"), "utf8"));
      ok(st.stages.spec.result.estimate === 15, "resume: --only reruns one stage and records its result");

      writeFileSync(join(cache, "state.json"), JSON.stringify({ stages: {} }));
      writeFileSync(join(cache, "credits.jsonl"), JSON.stringify({ call: "earlier attempts", credits: 290 }) + "\n");
      let message = "";
      try { await run(root, "stick", { only: "generate" }); } catch (e) { message = e.message; }
      ok(/spent 290 credits/.test(message) && /cap is 300/.test(message) && touched === 0, `credits: an attempt over the cap is refused before any call (got: ${message}; calls ${touched})`);
      const failedState = JSON.parse(readFileSync(join(cache, "state.json"), "utf8"));
      ok(failedState.stages.generate?.ok === false, "credits: the refused stage is recorded as failed");

      await throws(() => run(root, "stick", { only: "bake" }), /stages are/, "resume: an unknown stage is refused");
      await throws(() => run(root, "nothing", {}), /no spec at/, "resume: a slug with no spec is refused");
    } finally {
      console.log = quiet;
      globalThis.fetch = realFetch;
    }
  }

  // ---- guid and meta: fresh GUIDs, and a re-import that keeps every id --------------------------
  {
    const used = new Set(["00000000000000AA"]);
    const draws = [Buffer.alloc(8), Buffer.from("00000000000000AA", "hex"), Buffer.from("00000000000000AB", "hex")];
    let i = 0;
    const g = freshGuid(used, () => draws[i++]);
    ok(g === "00000000000000AB" && used.has(g), "guid: a drawn GUID is never one in use");
    ok(/^[0-9A-F]{16}$/.test(freshGuid(new Set())), "guid: a GUID is 16 upper-case hex digits");

    const addon = join(tmp, "addon");
    mkdirSync(join(addon, "Assets", "W", "Stick", "Data"), { recursive: true });
    const args = { assetDir: "Assets/W/Stick", name: "Stick", collider: { name: "UCX_Stick", layerPreset: "WeaponFire", gamemat: "{3B2D2687F56BB4EF}x.gamemat" }, flags: weapon.metaFlags };
    const first = writeAssetMetas(addon, args);
    const metaFile = join(addon, "Assets", "W", "Stick", "Stick.xob.meta");
    const text1 = readFileSync(metaFile, "utf8");
    const second = writeAssetMetas(addon, args);
    ok(JSON.stringify(first) === JSON.stringify(second) && readFileSync(metaFile, "utf8") === text1, "meta: a re-import keeps every GUID and id");
    const guids = [first.bcr, first.nmo, first.emat, first.xob].map((r) => r.slice(1, 17));
    ok(new Set(guids).size === 4, "meta: the four resources get four GUIDs");
    ok(usedGuids(addon).has(guids[3]), "meta: usedGuids sees what was written");
    ok(/ExportSkinning 1/.test(text1) && /ExportSceneHierarchy 1/.test(text1), "meta: a weapon model exports its skeleton and sockets");
    ok(text1.includes(`SourceMaterial "Stick"`) && text1.includes(`AssignedMaterial "${first.emat}"`), "meta: the model's material is assigned to the asset's .emat");
    ok(/GeometryParam UCX_Stick \{\s+LayerPreset "WeaponFire"/.test(text1), "meta: the collider carries its layer preset");
    ok(readFileSync(join(addon, "Assets", "W", "Stick", "Data", "Stick.emat"), "utf8") === ematText(first), "meta: the .emat names both maps");
    ok(/TextureNType\.conf/.test(textureMeta("a_NMO.edds", "TextureNType.conf", guids[0])), "meta: a normal map is a normal-type texture");
    const ids = xobMetaIds(text1, new Set());
    ok(xobMeta("Assets/W/Stick/Stick.xob", guids[3], { materials: [{ source: "Stick", emat: first.emat }], colliders: [args.collider], ids, flags: args.flags }) === text1, "meta: xobMetaIds reads back what xobMeta wrote");
  }

  // ---- clean report: budget, size, UVs, collider -------------------------------------------------
  const report = () => ({
    size: [0.2, 0.14, 0.036],
    bounds: { min: [-0.018, -0.0837, -0.0708], max: [0.018, 0.0563, 0.1292] },
    uv: { min: 0, max: 1 }, textures: { found: ["base_color", "normal"] },
    lods: [{ name: "Stick_LOD0", tris: 8000 }, { name: "Stick_LOD1", tris: 4000 }, { name: "Stick_LOD2", tris: 1600 }],
    collider: { name: "UCX_Stick", usage: "WeaponFire", faces: 18 },
    parts: [{ name: "trigger", bone: "w_trigger", faces: 200 }],
    bones: [{ name: "w_root", pos: [0, 0, 0] }, { name: "w_hammer", pos: [0, 0.02694, -0.04329] }, { name: "w_trigger", pos: [0, 0.0159, 0.02292] }, { name: "w_slide", pos: [0, 0.04321, 0.05125] }, { name: "w_fire_mode", pos: [-0.00755, 0.04639, -0.0331] }, { name: "w_slide_release", pos: [-0.01186, 0.02396, 0.0215] }],
    sockets: [{ name: "eye", pos: [0, 0.05588, -0.10698] }, { name: "barrel_muzzle", pos: [0, 0.03, 0.129] }, { name: "barrel_chamber", pos: [0.00015, 0.04728, 0.01796] }, { name: "slot_magazine", pos: [0.00015, -0.07526, -0.05794] }],
  });
  {
    const s = spec();
    ok(checkReport(s, weapon, report()).length === 0, `report: a good clean passes (${checkReport(s, weapon, report())[0]})`);
    const bad = (edit, pattern, what) => {
      const r = report();
      edit(r);
      ok(checkReport(s, weapon, r).some((p) => pattern.test(p)), `report: ${what}`);
    };
    bad((r) => (r.lods[0].tris = 9000), /over the budget/, "LOD0 over the budget is flagged");
    bad((r) => (r.lods[2].tris = 4000), /not smaller/, "a LOD that does not shrink is flagged");
    bad((r) => (r.lods[1].name = "Stick_lod1"), /Workbench needs Stick_LOD1/, "a LOD Workbench would not recognise is flagged");
    bad((r) => (r.size[0] = 0.3), /length is 0\.300 m/, "a model half as long again as the spec is flagged");
    bad((r) => (r.uv.max = 1.4), /UVs leave the 0\.\.1 square/, "UVs outside the atlas are flagged");
    bad((r) => (r.textures.found = ["normal"]), /no base colour/, "a model with no base colour is flagged");
    bad((r) => (r.collider.usage = "Default"), /collider usage/, "a collider on the wrong layer is flagged");
    bad((r) => (r.parts[0].faces = 0), /moves nothing/, "a moving part with no faces is flagged");

    // ---- sockets: where a weapon needs them ----
    const sock = (edit, pattern, what) => {
      const r = report();
      edit(r);
      ok(checkSockets(weapon, r).some((p) => pattern.test(p)), `sockets: ${what}`);
    };
    const at = (r, name) => r.sockets.find((x) => x.name === name);
    sock((r) => (at(r, "barrel_muzzle").pos[2] -= 0.05), /cm from the front face/, "a muzzle 5 cm behind the front face is flagged");
    sock((r) => (at(r, "barrel_muzzle").pos[0] = 0.02), /off the centre line/, "a muzzle off the centre line is flagged");
    sock((r) => (at(r, "eye").pos[1] = -0.05), /eye is below/, "an eye point inside the grip is flagged");
    sock((r) => (at(r, "eye").pos[2] = 0.2), /eye is in front/, "an eye point in front of the muzzle is flagged");
    sock((r) => (r.sockets = r.sockets.filter((x) => x.name !== "slot_magazine")), /socket slot_magazine is missing/, "a missing socket is flagged");
    sock((r) => (r.bones = r.bones.filter((x) => x.name !== "w_slide")), /bone w_slide is missing/, "a missing bone is flagged");
    sock((r) => (r.bones.find((x) => x.name === "w_trigger").pos[2] += 0.01), /bone w_trigger is at/, "a bone moved off the vanilla rest position is flagged");
  }

  // ---- xob: the reader on the model this pipeline shipped, and the import checks -----------------
  {
    await throws(() => parse(Buffer.from("not a model, just sixteen bytes.")), /not an XOB9 model/, "xob: a file that is not a model is refused");
    const taser = join(ROOT, "addons", "LifeFramework", "Assets", "Weapons", "Handguns", "Taser", "Taser.xob");
    ok(existsSync(taser), "xob: the taser model is in the addon");
    if (existsSync(taser)) {
      const model = parse(readFileSync(taser));
      const pos = worldPositions(model);
      ok([...weapon.requiredBones, ...weapon.requiredSockets].every((n) => pos[n]), "xob: the taser has every bone and socket the pistol prefab names");
      ok(model.colliders.includes("UCX_Taser"), "xob: the taser has its collider");
      const emat = "Assets/Weapons/Handguns/Taser/Data/Taser.emat";
      const problems = checkModel(model, weapon, { emat, colliderName: "UCX_Taser" });
      ok(problems.length === 0, `xob: the taser passes the import checks (${problems[0]})`);
      ok(checkModel(model, weapon, { emat: "Common/Materials/Other.emat" }).some((p) => /would render white/.test(p)), "xob: a model that bakes another material is flagged");
      ok(checkModel(model, weapon, { colliderName: "UCX_Other" }).some((p) => /no collider UCX_Other/.test(p)), "xob: a dropped collider is flagged");
      ok(checkModel(model, weapon, { sockets: [{ name: "eye", pos: [0, 0.2, 0] }] }).some((p) => /socket eye came out at/.test(p)), "xob: a socket that moved in the export is flagged");
      const moved = { ...model, nodes: model.nodes.map((n) => (n.name === "w_trigger" ? { ...n, pos: [0, 0.05, 0.02292] } : n)) };
      ok(checkModel(moved, weapon, {}).some((p) => /bone w_trigger rests at/.test(p)), "xob: a bone off the vanilla rest position is flagged");
      const cut = { ...model, nodes: model.nodes.filter((n) => n.name !== "barrel_muzzle") };
      ok(checkModel(cut, weapon, {}).some((p) => /no node barrel_muzzle/.test(p)), "xob: a missing socket is flagged");
    }
  }

  // ---- provenance: tools/assets/<slug>/meshy.json keeps every generation id --------------------------
  {
    const root = mkdtempSync(join(tmpdir(), "lf-prov-"));
    mkdirSync(join(root, "tools", "assets", "stick"), { recursive: true });
    const spec = { slug: "stick", prompt: "a stick", texture_prompt: "brown", meshy: { ai_model: "latest" } };
    recordProvenance(root, spec, { previewId: "p1", refineId: "r1" }, new Date("2026-11-13T10:00:00Z"));
    recordProvenance(root, spec, { previewId: "p2", refineId: "r2" }, new Date("2026-11-14T10:00:00Z"));
    recordProvenance(root, spec, { previewId: "p2", refineId: "r2" }, new Date("2026-11-14T11:00:00Z"));
    const rec = JSON.parse(readFileSync(join(root, "tools", "assets", "stick", "meshy.json"), "utf8"));
    ok(rec.generations.length === 2 && rec.generations.map((g) => g.refineId).join() === "r1,r2" && rec.generations[0].model === "latest", "provenance: meshy.json keeps both generations and the refine ids, and a rerun adds none");
  }

  // ---- Workbench log: only the plugin's own window counts ----------------------------------------
  {
    const log = [
      "12:00:01.000 RESOURCES (E): Stick.xob not built yet",
      "12:00:02.000 SCRIPT       : LF_ASSET_IMPORT rebuilding C:/x/Stick.fbx",
      "12:00:03.000 RESOURCES (W): Invalid collider UCX_Stick",
      "12:00:03.500 RESOURCES (E): Stick_LOD0 uses DefaultGeneratedMaterial",
      "12:00:04.000 ENGINE       : Game destroyed.",
      "12:00:05.000 RESOURCES (E): Resource leaks: Stick UI texture",
    ].join("\n");
    const problems = logProblems(log, "Stick");
    ok(problems.length === 2 && /Invalid collider/.test(problems[0]) && /DefaultGeneratedMaterial/.test(problems[1]), `log: lines before the plugin starts do not count (got ${JSON.stringify(problems)})`);
    ok(logProblems("x\nLF_ASSET_IMPORT rebuilding a\nall fine\nLF_ASSET_IMPORT done 1\n", "Stick").length === 0, "log: a clean import reports nothing");
    // A run's folder holds console.log and two extracts of it; script.log ends inside the window, so
    // error.log's start-up lines joined after it would count (the handcuffs import, 2026-11-13).
    const wbRun = join(tmp, "wb-run");
    mkdirSync(wbRun, { recursive: true });
    const startup = "12:00:01.000 RESOURCES (E): metafile without corresponding resource: 'Stick.xob.meta'";
    const scriptLines = ["12:00:02.000 SCRIPT       : LF_ASSET_IMPORT rebuilding Stick.fbx", "12:00:02.100 SCRIPT       : LF_ASSET_IMPORT done 1"];
    writeFileSync(join(wbRun, "console.log"), [startup, ...scriptLines, "12:00:03.000 ENGINE       : Game destroyed."].join("\n"));
    writeFileSync(join(wbRun, "script.log"), scriptLines.join("\n") + "\n");
    writeFileSync(join(wbRun, "error.log"), startup + "\n");
    const runProblems = logProblems(runLog(wbRun), "Stick");
    ok(runProblems.length === 0, `log: a run's own start-up lines do not count (got ${JSON.stringify(runProblems)})`);
  }

  // ---- prefab text -------------------------------------------------------------------------------
  {
    const w = { name_key: "#N", description_key: "#D", magazine_name_key: "#MN", magazine_description_key: "#MD", magazine_well: "LF_MagazineWellTaser", max_ammo: 1, damage: 1, init_speed: 100, weight: 0.5, components: [{ class: "LF_StunWeaponComponent", props: { m_fStunSeconds: 8 } }] };
    const text = weaponPrefab({ id: "AAAAAAAAAAAAAAAA", xob: "{1}x.xob", magazine: "{2}m.et", w, report: report(), guids: { well: "BBBBBBBBBBBBBBBB", components: ["CCCCCCCCCCCCCCCC"] } });
    ok(text.startsWith('GenericEntity : "{809488C4984AC087}Prefabs/Weapons/Handguns/M9/Handgun_M9_base.et"'), "prefab: the weapon inherits the vanilla pistol base");
    ok(/MeshObject "\{CFBAA4B701F95D69\}" \{\s+Object "\{1\}x\.xob"/.test(text), "prefab: the mesh overrides the inherited MeshObject by its GUID");
    ok(text.includes('LF_StunWeaponComponent "{CCCCCCCCCCCCCCCC}" {\n   m_fStunSeconds 8\n'), "prefab: the spec's extra component is written with its values");
    ok(text.includes("SightsPointFront") && text.includes("Offset 0 0.0563 0.1092"), "prefab: the sights sit on the mesh's top, 2 cm behind its front");
    ok((text.match(/{/g) || []).length === (text.match(/}/g) || []).length, "prefab: the weapon's braces balance");
    const mag = magazinePrefab({ id: "A", ammoConf: "{3}c.conf", w, guids: { well: "D" } });
    ok(/MaxAmmo 1\n/.test(mag) && /AmmoMapping \{\n    0\n/.test(mag) && mag.includes("MagazineWell LF_MagazineWellTaser"), "prefab: a one-shot magazine maps one round and has its own well");
    ok(/DamageValue 1\n/.test(ammoPrefab({ id: "A", w })) && /InitSpeed 100\n/.test(ammoPrefab({ id: "A", w })), "prefab: the projectile takes the spec's damage and speed");

    const one = ammoListWith("", "{A}a.et");
    ok(one === 'SCR_AmmoList {\n m_aAmmoList + {\n  "{A}a.et"\n }\n}\n', "prefab: the ammunition list starts as an appending override");
    const two = ammoListWith(one, "{B}b.et");
    ok(two.includes('"{A}a.et"\n  "{B}b.et"\n }\n}'), "prefab: a second asset's ammo is appended");
    ok(ammoListWith(two, "{A}a.et") === two, "prefab: listing the same ammo twice changes nothing");
    await throws(() => ammoListWith("SCR_AmmoList {\n m_aAmmoList {\n }\n}\n", "{C}c.et"), /not the append-only override/, "prefab: a hand-written list that would replace vanilla's is refused");
  }

  // ---- concepts: prompt fan-out, sheet layout, scoring and the pick -----------------------------
  {
    const variants = Array.from({ length: 10 }, (_, i) => ({ label: `Variant ${i + 1}`, detail: `detail number ${i + 1}.` }));
    const rec = () => ({ slug: "stick", subject: "A police baton.", variants: variants.map((v) => ({ ...v })) });
    ok(cc.conceptProblems(rec()).length === 0, `concepts: ten distinct variants pass (${cc.conceptProblems(rec())[0]})`);
    const prompts = cc.buildPrompts("A police baton.", variants);
    ok(prompts.length === 10 && prompts.every((p) => p.prompt.endsWith(cc.FRAME) && p.prompt.startsWith("A police baton.")), "concepts: every prompt ends in the shared product-shot frame");
    ok(new Set(prompts.map((p) => p.prompt)).size === 10 && prompts[2].n === 3 && !prompts[0].prompt.includes(".."), "concepts: the ten prompts are distinct, numbered and tidy");
    const plain = cc.conceptRequest("nano-banana-2-1", "p", null);
    ok(plain.path === "/openapi/v1/text-to-image" && !("reference_image_urls" in plain.body), "concepts: without a reference the ten are text-to-image");
    const withRef = cc.conceptRequest("nano-banana-2-1", "p", "data:image/jpeg;base64,AA");
    ok(withRef.path === "/openapi/v1/image-to-image" && withRef.call === "image-to-image" && withRef.body.reference_image_urls[0] === "data:image/jpeg;base64,AA", "concepts: a reference sends image-to-image with the picture");
    ok(cc.conceptProblems({ ...rec(), reference: { file: "tools/assets/stick/ref.gif" } }).some((m) => /reference.file/.test(m)), "concepts: a reference that is not a picture is refused");
    ok(cc.conceptProblems({ ...rec(), reference: { file: "../outside/ref.png" } }).some((m) => /inside the repo/.test(m)), "concepts: a reference outside the repo is refused");
    ok(cc.conceptProblems({ ...rec(), reference: { file: "tools/assets/stick/ref.jpg" } }).length === 0, "concepts: a repo-relative jpg reference passes");
    const nine = rec(); nine.variants.pop();
    ok(cc.conceptProblems(nine).some((m) => /exactly 10/.test(m)), "concepts: nine variants are refused");
    const same = rec(); same.variants[4].detail = same.variants[1].detail;
    ok(cc.conceptProblems(same).some((m) => /details must be distinct/.test(m)), "concepts: two identical details are refused");

    const L = cc.sheetLayout(10, 5, 400, 44);
    ok(L.cols === 5 && L.rows === 2 && L.width === 2000 && L.height === 888, "concepts: ten cells make a 5x2 grid");
    ok(L.cells[0].x === 0 && L.cells[4].x === 1600 && L.cells[4].y === 0 && L.cells[5].x === 0 && L.cells[5].y === 444 && L.cells[9].n === 10, "concepts: cells are numbered left to right, top row first");
    const args = cc.sheetArgs({ images: prompts.map((p) => `${cc.pad(p.n)}.png`), out: "sheet.png" });
    const fc = args[args.indexOf("-filter_complex") + 1];
    ok(args.filter((a) => a === "-i").length === 10 && fc.includes("xstack=inputs=10:layout=0_0|400_0|800_0|1200_0|1600_0|0_444|") && fc.includes("textfile=lab10.txt"), "concepts: ffmpeg gets ten inputs, the grid layout and a label per cell");

    const r = rec();
    ok(!cc.scored(r), "concepts: an unscored set is not scored");
    await throws(async () => cc.pickByRubric(r), /scored first/, "concepts: --pick refuses an unscored set");
    const full = (n) => Array(6).fill(n);
    for (let n = 1; n <= 10; n++) cc.applyScore(r, n, full(1));
    cc.applyScore(r, 7, [2, 2, 2, 2, 2, 1], "reads as a baton");
    cc.applyScore(r, 4, [2, 2, 2, 2, 2, 1]);
    cc.pickByRubric(r, new Date("2026-11-13T10:00:00Z"));
    ok(r.pick.n === 4 && r.pick.by === "rubric" && /11\/12/.test(r.pick.why) && /image_to_3d/.test(r.pick.why), `concepts: a tie goes to the lower number and the why is recorded (got ${JSON.stringify(r.pick)})`);
    ok(cc.ranking(r)[0].n === 4 && cc.ranking(r)[1].n === 7, "concepts: the ranking is by total, then number");
    await throws(async () => cc.applyScore(r, 3, [1, 2, 3, 0, 0, 0]), /integers 0 to 2/, "concepts: a score above 2 is refused");
    await throws(async () => cc.applyScore(r, 11, full(1)), /no concept 11/, "concepts: scoring a missing concept is refused");
    cc.pickConcept(r, 9, { by: "human", why: "the maintainer's pick" });
    ok(r.pick.n === 9 && r.pick.by === "human" && r.pick.why === "the maintainer's pick", "concepts: a human pick replaces the rubric pick and keeps its reason");

    // The stage with no concepts.json does nothing, and generate keeps text-to-3D; with one it stops for a pick.
    const root = join(tmp, "croot");
    const dir = join(root, "tmp", "assets", "stick");
    mkdirSync(join(root, "tools", "assets", "stick"), { recursive: true });
    mkdirSync(dir, { recursive: true });
    const ctx = { root, spec: spec(), dir, state: {}, fullState: {} };
    const noop = { writeState() {}, resolveFfmpeg: () => null };
    const quiet = console.log;
    console.log = () => {};
    try {
      const skipped = await cc.stageConcept(ctx, noop);
      ok(skipped.skipped === true && cc.pickedImage(root, "stick", dir) === null, "concepts: without concepts.json the stage is skipped and no image is picked");
      const picked = rec(); cc.pickConcept(picked, 3, { by: "human" });
      cc.saveConcepts(root, "stick", picked);
      ok(cc.pickedImage(root, "stick", dir) === null, "concepts: a pick whose image is missing does not switch generate to image-to-3D");
      mkdirSync(join(dir, "concepts"), { recursive: true });
      writeFileSync(join(dir, "concepts", "03.png"), "png");
      ok(cc.pickedImage(root, "stick", dir)?.pick.n === 3, "concepts: a picked, downloaded image switches generate to image-to-3D");
    } finally {
      console.log = quiet;
    }
    const body = meshy.imageTo3dBody({ meshy: { ai_model: "latest", target_polycount: 20000 }, texture_prompt: "matte" }, "data:image/png;base64,AA");
    ok(body.image_url.startsWith("data:image/png") && body.should_texture === true && body.enable_pbr === true && body.target_formats[0] === "glb", "concepts: the image-to-3D request asks for a textured PBR glb");
    const prov = join(tmp, "proot"); mkdirSync(join(prov, "tools", "assets", "stick"), { recursive: true });
    recordProvenance(prov, spec(), { imageTaskId: "i1", concept: 3, conceptTaskId: "c3", conceptPrompt: "p" }, new Date("2026-11-13T10:00:00Z"));
    recordProvenance(prov, spec(), { imageTaskId: "i1", concept: 3, conceptTaskId: "c3", conceptPrompt: "p" }, new Date("2026-11-13T11:00:00Z"));
    const pg = JSON.parse(readFileSync(join(prov, "tools", "assets", "stick", "meshy.json"), "utf8")).generations;
    ok(pg.length === 1 && pg[0].tool === "Meshy image-to-3D" && pg[0].imageTaskId === "i1" && pg[0].conceptTaskId === "c3", "concepts: meshy.json records the image-to-3D task and the concept it came from, once");
  }

  // ---- kits: several models, shared materials, a meta per model with every material and collider --
  {
    const kit = JSON.parse(readFileSync(join(ROOT, "tools", "assets", "bank-vault", "kit.json"), "utf8"));
    ok(kitProblems(kit).length === 0, `kit: the bank vault kit.json passes (${kitProblems(kit)[0]})`);
    ok(kitProblems({ ...kit, colliders: { VaultFront: { layerPreset: "BuildingFireView", gamemat: "concrete.gamemat" } } }).some((m) => /gamemat/.test(m)), "kit: a collider game material without a GUID is refused");
    const used = new Set();
    const mats = ["A", "B", "C"].map((n, i) => ({ source: `Kit_${n}`, emat: `{${"1".repeat(15)}${i}}Assets/K/Data/Kit_${n}.emat` }));
    const cols = ["UBX_K_00", "UBX_K_01"].map((name) => ({ name, layerPreset: "BuildingFireView", gamemat: "{1A05231C3FD1627A}Common/Materials/Game/concrete.gamemat" }));
    const text = kitModelMeta("", used, { res: "Assets/K/K.xob", guid: "2222222222222222", materials: mats, colliders: cols, flags: { ExportSkinning: true } });
    const assigns = [...text.matchAll(/MaterialAssignClass "\{([0-9A-F]{16})\}"/g)].map((m) => m[1]);
    ok(assigns.length === 3 && new Set(assigns).size === 3 && (text.match(/GeometryParam UBX_K_0/g) || []).length === 2 && /ExportSkinning 1/.test(text), "kit: a model meta assigns every material once and lists every collider");
    const again = kitModelMeta(text, used, { res: "Assets/K/K.xob", guid: "2222222222222222", materials: mats, colliders: cols, flags: {} });
    ok([...again.matchAll(/MaterialAssignClass "\{([0-9A-F]{16})\}"/g)].map((m) => m[1]).join() === assigns.join(), "kit: a re-import keeps the material assign ids");
    const model = { materials: [{ path: "Assets/K/Data/Kit_A.emat" }], colliders: ["UBX_K_00"], nodes: [] };
    const emats = Object.fromEntries(mats.map((m) => [m.source, m.emat]));
    const found = checkKitModel(model, { fbx: "K.fbx", materials: ["Kit_A", "Kit_B"], colliders: ["UBX_K_00", "UBX_K_01"], bones: [{ name: "door" }] }, emats);
    ok(found.some((m) => /Kit_B/.test(m)) && found.some((m) => /UBX_K_01/.test(m)) && found.some((m) => /bone door/.test(m)) && !found.some((m) => /Kit_A\.emat/.test(m)), `kit: a model missing a material, a collider or the door bone is flagged (${found.join("; ")})`);
  }

  if (failed) {
    console.log(`asset-pipeline-selftest: FAILED (${failed})`);
    return 1;
  }
  console.log("asset-pipeline-selftest: OK");
  return 0;
}

if (process.argv[1] && import.meta.url === pathToFileURL(process.argv[1]).href) process.exit(await main());
