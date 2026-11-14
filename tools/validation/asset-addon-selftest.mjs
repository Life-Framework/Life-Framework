// Proves offline that every stage of tools\cli asset writes into the addon the spec (or --addon) names,
// and that no stage can make the core name the pack (#971; docs/architecture/core-vs-pack.md). The
// fixture is a temp repo with a core and a pack addon. No Meshy, no Blender, no Workbench.
// red-proof: 2026-11-14, one line broken per block, each restored after its FAIL was seen:
//   addonOf returning the core for every spec          -> FAIL addon: a spec with addon LF_Everon1989 resolves to the pack
//   prefab-gadget looking in the core, not the spec's addon -> the pack prefab block throws "no imported model" (the run dies, red)
//   packReferences returning []                        -> FAIL boundary: a core asset naming a pack GUID, path or the addon name is refused
//   assertBoundary returning at once                   -> FAIL boundary: a core file that names the pack stops the stage
//   guidsInUse scanning only the core                  -> FAIL guid: a fresh GUID is never one the pack or the core holds
//   run() without the state.addon comparison           -> FAIL run: a changed target runs the import again and keeps the generator's result
//   creditPath returning the bare path                 -> FAIL credits: a pack asset's row names its addon
import { existsSync, mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { pathToFileURL } from "node:url";
import { ADDON_STAGES, STAGES, run, specProblems } from "../assets/pipeline/asset.mjs";
import { ADDONS, CORE, addonFlag, addonOf, addonPath, addonProblems, assertBoundary, creditPath, guidsInUse, inventoryRow, plan, pluginDir, slugTarget, stringTable, withAddon } from "../assets/pipeline/addon.mjs";
import { writeAssetMetas } from "../assets/pipeline/meta.mjs";
import { creditRow } from "../assets/pipeline/source.mjs";
import { KINDS } from "../assets/pipeline/kinds.mjs";
import { writePrefabs } from "../assets/pipeline/prefab.mjs";

const PACK = "LF_Everon1989";
const PACK_GUID = "AAAAAAAAAAAAAAAA";

export async function main() {
  let failed = 0;
  const ok = (cond, what) => {
    if (cond) return;
    failed++;
    console.log(`FAIL ${what}`);
  };
  const throws = async (fn, pattern, what) => {
    try {
      await fn();
      ok(false, `${what} (no error)`);
    } catch (e) {
      ok(pattern.test(e.message), `${what} (got: ${e.message})`);
    }
  };
  const quiet = (fn) => async (...a) => {
    const log = console.log;
    console.log = () => {};
    try { return await fn(...a); } finally { console.log = log; }
  };
  const tmp = mkdtempSync(join(tmpdir(), "lf-asset-addon-selftest-"));
  let n = 0;
  // A repo with the core, the pack, and one pack resource the core must never name.
  const fakeRoot = () => {
    const root = join(tmp, `r${n++}`);
    for (const a of ADDONS) {
      mkdirSync(join(root, "addons", a.name), { recursive: true });
      writeFileSync(join(root, "addons", a.name, `${a.name}.gproj`), `GameProject {\n ID "${a.name}"\n GUID "${a.guid}"\n}\n`);
    }
    mkdirSync(join(root, "addons", PACK, "Assets", "P"), { recursive: true });
    writeFileSync(join(root, "addons", PACK, "Assets", "P", "P.xob"), "model");
    writeFileSync(join(root, "addons", PACK, "Assets", "P", "P.xob.meta"), `MetaFileClass {\n Name "{${PACK_GUID}}Assets/P/P.xob"\n}\n`);
    mkdirSync(join(root, "addons", CORE, "Assets", "C"), { recursive: true });
    writeFileSync(join(root, "addons", CORE, "Assets", "C", "C.xob.meta"), 'MetaFileClass {\n Name "{BBBBBBBBBBBBBBBB}Assets/C/C.xob"\n}\n');
    return root;
  };
  const spec = (extra = {}) => ({
    slug: "cuffs", name: "Cuffs", kind: "gadget", size_m: { length: 0.18, height: 0.032, width: 0.1 },
    budget: { lod0_tris: 3000, lod_ratios: [0.5], texture_px: 1024, credits: 30 },
    asset_dir: "Assets/Items/Roleplay/Cuffs", prefab_dir: "Prefabs/Items/Roleplay",
    gadget: { name_key: "#N", description_key: "#D", weight: 0.3, volume: 150, components: [] },
    ...extra,
  });

  // ---- which addon: the spec, then --addon, then the core ----------------------------------------
  {
    ok(addonOf({}) === CORE && addonOf(spec()) === CORE, "addon: a spec with no addon field is the core");
    ok(addonOf({ addon: PACK }) === PACK && addonOf({ addon: PACK.toLowerCase() }) === PACK && addonOf({ addon: "B1E84D5687E11724" }) === PACK, "addon: a spec with addon LF_Everon1989 resolves to the pack, by name in any case or by GUID");
    const s = spec({ addon: PACK });
    ok(withAddon(s, undefined) === s && withAddon(s, CORE).addon === CORE && withAddon(spec(), PACK).addon === PACK, "addon: --addon beats the spec's field, no flag leaves the spec alone");
    try { addonOf({ addon: "Typo" }); ok(false, "addon: an unknown addon throws (no error)"); } catch (e) { ok(/unknown addon "Typo"; known: LifeFramework, LF_Everon1989/.test(e.message), `addon: an unknown addon throws, never falls back to the core (got: ${e.message})`); }
    try { withAddon(spec(), "Typo"); ok(false, "addon: --addon with an unknown name throws (no error)"); } catch (e) { ok(/unknown addon/.test(e.message), "addon: --addon with an unknown name throws"); }
    ok(addonFlag(["cuffs", "--addon", PACK]) === PACK && addonFlag(["cuffs"]) === undefined, "addon: --addon reads its value");
    try { addonFlag(["cuffs", "--addon"]); ok(false, "addon: --addon with no value throws (no error)"); } catch (e) { ok(/--addon needs an addon name/.test(e.message), "addon: --addon with no value throws"); }
    ok(specProblems({ ...spec(), addon: "Typo" }).some((p) => /addon must be one of LifeFramework, LF_Everon1989/.test(p)), "addon: the spec stage refuses an unknown addon");
    ok(!specProblems({ ...spec(), addon: PACK }).some((p) => /addon must be/.test(p)), "addon: the spec stage accepts the pack");
  }

  // ---- the target must exist, and a pack resource must not shadow the core's ----------------------
  {
    const root = fakeRoot();
    ok(addonProblems(root, spec({ addon: PACK })).length === 0, `addon: a pack spec at a free path passes (${addonProblems(root, spec({ addon: PACK }))[0]})`);
    ok(addonProblems(root, spec()).length === 0, "addon: a clean core spec passes");
    mkdirSync(join(root, "addons", CORE, "Prefabs", "Items", "Roleplay"), { recursive: true });
    ok(addonProblems(root, spec({ addon: PACK })).length === 0, "addon: a prefab folder the core shares with other assets is not a collision");
    writeFileSync(join(root, "addons", CORE, "Prefabs", "Items", "Roleplay", "Cuffs.et"), "core prefab");
    ok(addonProblems(root, spec({ addon: PACK })).some((p) => /Prefabs\/Items\/Roleplay\/Cuffs\.et already exists in LifeFramework.*shadow/.test(p)), "addon: a pack prefab at a core prefab's path is refused");
    mkdirSync(join(root, "addons", CORE, "Assets", "Items", "Roleplay", "Cuffs"), { recursive: true });
    ok(addonProblems(root, spec({ addon: PACK })).some((p) => /asset_dir .* already exists in LifeFramework.*shadow/.test(p)), "addon: a pack resource at a core path is refused (it would shadow it)");
    rmSync(join(root, "addons", PACK, `${PACK}.gproj`));
    ok(addonProblems(root, spec({ addon: PACK })).some((p) => /LF_Everon1989\.gproj/.test(p)), "addon: a pack with no project file is refused");
    ok(addonProblems(root, { addon: "Typo" }).some((p) => /unknown addon/.test(p)), "addon: addonProblems reports an unknown addon instead of throwing");
  }

  // ---- the red line: a core asset names nothing of the pack --------------------------------------
  {
    const root = fakeRoot();
    const withRef = (value, addon) => spec({ addon, gadget: { name_key: "#N", description_key: "#D", components: [{ class: "LF_X", props: { ref: value } }] } });
    for (const [what, value] of [["a pack GUID", `{${PACK_GUID}}Assets/P/P.xob`], ["a pack path", "Assets/P/P.xob"], ["the pack addon name", PACK]]) {
      ok(addonProblems(root, withRef(value)).some((p) => /a core asset must not reference the pack/.test(p)), `boundary: a core asset naming ${what} is refused`);
      ok(!addonProblems(root, withRef(value, PACK)).some((p) => /reference the pack/.test(p)), `boundary: a pack asset may name ${what} (the pack depends on the core)`);
    }
    ok(addonProblems(root, withRef("{BBBBBBBBBBBBBBBB}Assets/C/C.xob")).length === 0 && addonProblems(root, withRef("{1A05231C3FD1627A}Common/Materials/Game/concrete.gamemat")).length === 0, "boundary: a core asset naming core and base-game resources passes");
    // The same path in both addons is the core's, not pack-only.
    mkdirSync(join(root, "addons", CORE, "Assets", "P"), { recursive: true });
    writeFileSync(join(root, "addons", CORE, "Assets", "P", "P.xob"), "core model");
    ok(!addonProblems(root, withRef("Assets/P/P.xob")).some((p) => /pack path/.test(p)), "boundary: a path the core also holds is not a pack path");

    // Second layer: a stage wrote a core file that names the pack. The repo's own check stops it.
    const target = spec();
    assertBoundary(root, target);
    ok(true, "boundary: a clean core passes assertBoundary");
    writeFileSync(join(root, "addons", CORE, "Assets", "C", "leak.et"), `GenericEntity { Object "{${PACK_GUID}}Assets/P/P.xob" }\n`);
    await throws(() => assertBoundary(root, target), /the core now references the pack.*pack GUID/s, "boundary: a core file that names the pack stops the stage");
    assertBoundary(root, spec({ addon: PACK }));
    ok(true, "boundary: a pack target never trips it (the pack may name the core)");
  }

  // ---- the prefab stage writes into the pack, referencing the pack's model ------------------------
  {
    const root = fakeRoot();
    const s = spec({ addon: PACK });
    const pack = addonPath(root, s);
    ok(pack.endsWith(PACK), "addon: addonPath is the pack folder");
    mkdirSync(join(pack, s.asset_dir), { recursive: true });
    writeFileSync(join(pack, s.asset_dir, "Cuffs.xob"), "model");
    writeFileSync(join(pack, s.asset_dir, "Cuffs.xob.meta"), `MetaFileClass {\n Name "{0123456789ABCDEF}${s.asset_dir}/Cuffs.xob"\n}\n`);
    const dir = join(root, "tmp", "assets", "cuffs");
    mkdirSync(join(dir, "clean"), { recursive: true });
    writeFileSync(join(dir, "clean", "report.json"), JSON.stringify({
      size: [0.18, 0.032, 0.1], bounds: { min: [-0.05, -0.016, -0.09], max: [0.05, 0.016, 0.09] },
      collider: { name: "UBX_Cuffs" }, parts: [], bones: [], sockets: [], lods: [{ name: "Cuffs_LOD0", tris: 3000 }],
    }));
    const ref = await quiet(writePrefabs)({ root, spec: s, dir });
    const et = join(pack, "Prefabs", "Items", "Roleplay", "Cuffs.et");
    ok(existsSync(et) && existsSync(et + ".meta") && /^\{[0-9A-F]{16}\}Prefabs\/Items\/Roleplay\/Cuffs\.et$/.test(ref?.gadget || ""), `addon: the prefab stage writes into the pack (got ${JSON.stringify(ref)})`);
    ok(!existsSync(join(root, "addons", CORE, "Prefabs")), "addon: the prefab stage writes nothing into the core");
    ok(existsSync(et) && readFileSync(et, "utf8").includes(`{0123456789ABCDEF}${s.asset_dir}/Cuffs.xob`), "addon: the prefab names the pack's model by its GUID");
    const guid = /^\{([0-9A-F]{16})\}/.exec(ref.gadget)[1];
    ok(guid !== PACK_GUID && guid !== "BBBBBBBBBBBBBBBB" && guid !== "0123456789ABCDEF", "guid: the new prefab's GUID is a fresh one");
    // The same spec in the core finds no model there: it is looking in the core, not in the pack.
    await throws(() => quiet(writePrefabs)({ root, spec: spec(), dir }), /no imported model at Assets\/Items\/Roleplay\/Cuffs\/Cuffs\.xob/, "addon: with no addon the stage looks in the core");

    // A core scaffold whose spec names a pack GUID writes it; the boundary check then refuses it.
    const leaky = spec({ gadget: { name_key: "#N", description_key: "#D", weight: 0.3, volume: 150, components: [{ class: "LF_X", props: { ref: `{${PACK_GUID}}Assets/P/P.xob` } }] } });
    mkdirSync(join(root, "addons", CORE, leaky.asset_dir), { recursive: true });
    writeFileSync(join(root, "addons", CORE, leaky.asset_dir, "Cuffs.xob"), "model");
    writeFileSync(join(root, "addons", CORE, leaky.asset_dir, "Cuffs.xob.meta"), `MetaFileClass {\n Name "{0123456789ABCDEE}${leaky.asset_dir}/Cuffs.xob"\n}\n`);
    ok(addonProblems(root, leaky).some((p) => /pack GUID/.test(p)), "boundary: the spec check refuses it before any stage runs");
  }

  // ---- GUIDs are drawn against every addon, and metas land in the target -------------------------
  {
    const root = fakeRoot();
    const used = guidsInUse(root);
    ok(used.has(PACK_GUID) && used.has("BBBBBBBBBBBBBBBB"), "guid: guidsInUse holds the pack's and the core's GUIDs");
    const s = spec({ addon: PACK });
    const pack = addonPath(root, s);
    mkdirSync(join(pack, s.asset_dir, "Data"), { recursive: true });
    const refs = writeAssetMetas(pack, { assetDir: s.asset_dir, name: "Cuffs", collider: { name: "UBX_Cuffs", layerPreset: "ItemFireView", gamemat: "{3B2D2687F56BB4EF}x.gamemat" }, flags: KINDS.gadget.metaFlags, used: guidsInUse(root) });
    const fresh = [refs.bcr, refs.nmo, refs.emat, refs.xob].map((r) => r.slice(1, 17));
    ok(new Set(fresh).size === 4 && fresh.every((g) => !used.has(g)), "guid: a fresh GUID is never one the pack or the core holds");
    ok(existsSync(join(pack, s.asset_dir, "Cuffs.xob.meta")) && existsSync(join(pack, s.asset_dir, "Data", "Cuffs.emat.meta")), "addon: the import's metas and emat land in the pack");
    ok(!existsSync(join(root, "addons", CORE, s.asset_dir)), "addon: the import writes nothing into the core");
  }

  // ---- paths the other stages use ----------------------------------------------------------------
  {
    const root = fakeRoot();
    const s = spec({ addon: PACK });
    ok(pluginDir(root).startsWith(join(root, "addons", CORE)), "addon: the Workbench import plugin is always the core's copy, so a pack never declares the class twice");
    ok(stringTable(root, s) === join(root, "addons", PACK, "Language", "everonlife_localization.st") && stringTable(root, spec()).includes(CORE), "addon: strings go to the target addon's table");
    ok(slugTarget(root, "none", PACK).addon === PACK && slugTarget(root, "none").addon === undefined && addonOf(slugTarget(root, "none")) === CORE, "addon: sound, kit and ambience find their target through the slug, --addon wins");
    mkdirSync(join(root, "tools", "assets", "cuffs"), { recursive: true });
    writeFileSync(join(root, "tools", "assets", "cuffs", "spec.json"), JSON.stringify(s));
    ok(slugTarget(root, "cuffs").addon === PACK && slugTarget(root, "cuffs", CORE).addon === CORE, "addon: a slug's spec decides the target of its sounds");
  }

  // ---- credits, inventory, and the dry run -------------------------------------------------------
  {
    const root = fakeRoot();
    const rec = { name: "Thing", authors: "Au", url: "https://polyhaven.com/a/thing" };
    ok(creditPath(spec(), "Assets/X.xob") === "Assets/X.xob" && creditPath(spec({ addon: PACK }), "Assets/X.xob") === `${PACK}/Assets/X.xob`, "credits: a pack asset's row names its addon, a core row stays as it was");
    ok(creditRow(spec(), rec).includes("(`Assets/Items/Roleplay/Cuffs/Cuffs.xob`)") && creditRow(spec({ addon: PACK }), rec).includes(`(\`${PACK}/Assets/Items/Roleplay/Cuffs/Cuffs.xob\`)`), "credits: creditRow carries the addon");
    ok(inventoryRow(spec()).includes("`Assets/Items/Roleplay/Cuffs/`") && inventoryRow(spec()).includes("| core |"), "inventory: a core asset gets a core row to classify");
    const dry = plan(root, spec({ addon: PACK }));
    ok(dry.some((l) => l.startsWith("addon") && l.includes(PACK)) && dry.some((l) => l.includes(join(root, "addons", PACK, "Assets/Items/Roleplay/Cuffs"))) && dry.some((l) => /^workbench .*LF_Everon1989\.gproj/.test(l)) && dry.some((l) => /^inventory\s+no row/.test(l)) && !dry.some((l) => l.startsWith("FAIL")), `plan: a pack spec plans every write into the pack (${dry.join(" | ")})`);
    const bad = plan(root, spec({ gadget: { name_key: "#N", description_key: "#D", components: [{ class: "LF_X", props: { ref: `{${PACK_GUID}}Assets/P/P.xob` } }] } }));
    ok(bad.some((l) => l.startsWith("FAIL") && /pack GUID/.test(l)), "plan: a core spec that names the pack fails the plan");
  }

  // ---- run(): a changed target runs the addon stages again, never the generator -----------------
  {
    const root = fakeRoot();
    const s = spec();
    mkdirSync(join(root, "tools", "assets", "cuffs"), { recursive: true });
    mkdirSync(join(root, "tmp", "assets", "cuffs"), { recursive: true });
    writeFileSync(join(root, "tools", "assets", "cuffs", "spec.json"), JSON.stringify(s));
    const stamp = { ok: true, at: new Date().toISOString(), secs: 1, result: { marker: "kept" } };
    const state = join(root, "tmp", "assets", "cuffs", "state.json");
    writeFileSync(state, JSON.stringify({ stages: Object.fromEntries(STAGES.map((x) => [x, stamp])) }));
    ok(ADDON_STAGES.every((x) => STAGES.includes(x)) && !ADDON_STAGES.includes("generate") && !ADDON_STAGES.includes("clean"), "run: only the stages that write into the addon depend on it");
    await throws(() => quiet(run)(root, "cuffs", { addon: PACK }), /needs the Workbench launcher/, "run: a changed target reruns the import");
    const after = JSON.parse(readFileSync(state, "utf8"));
    ok(after.addon === PACK && after.stages.generate?.result?.marker === "kept" && after.stages.clean?.result?.marker === "kept" && after.stages.import?.ok === false, "run: a changed target keeps the generator and clean results and fails only the import");
    await throws(() => quiet(run)(root, "cuffs", { addon: "Typo" }), /unknown addon "Typo"/, "run: an unknown --addon is refused");
    rmSync(join(root, "addons", PACK, `${PACK}.gproj`));
    await throws(() => quiet(run)(root, "cuffs", { addon: PACK }), /addon problems:[\s\S]*LF_Everon1989\.gproj/, "run: a target with no project file is refused before any stage, even when every stage is cached");
  }

  if (failed) {
    console.log(`asset-addon-selftest: FAILED (${failed})`);
    return 1;
  }
  console.log("asset-addon-selftest: OK");
  return 0;
}

if (process.argv[1] && import.meta.url === pathToFileURL(process.argv[1]).href) process.exit(await main());
