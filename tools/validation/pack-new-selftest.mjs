// tools/validation/pack-new-selftest.mjs
// inputs: tools/**, addons/**, docs/architecture/**
//
// Proves `tools\cli pack new` (#968): a freshly scaffolded pack, with no edits, passes
// validate-pack-manifest and validate-addon-boundary; every GUID it writes is unique; and the
// validators hold the new pack, not only the flagship (a planted fault in it is caught).
// red-proof: with the manifest id written as "LF_Wrong" in tools/pack/new.mjs this printed
// "FAIL scaffold passes validate-pack-manifest: LF_Test/Configs/LF_PackManifest.conf: m_sPackId
// is "LF_Wrong", expected "LF_Test"" and exited 1 (2026-11-14).

import { mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import { scaffoldPack } from "../pack/new.mjs";
import { checkBoundary } from "./validate-addon-boundary.mjs";
import { checkPackManifest } from "./validate-pack-manifest.mjs";

function put(root, path, text) {
  const full = join(root, path);
  mkdirSync(dirname(full), { recursive: true });
  writeFileSync(full, text);
}

function tree() {
  const root = mkdtempSync(join(tmpdir(), "lf-packnew-"));
  put(root, "addons/LifeFramework/LifeFramework.gproj", 'GameProject {\n ID "LifeFramework"\n GUID "79636E668EA37AC9"\n}\n');
  put(root, "addons/LifeFramework/Scripts/Game/Feature/Packs/LF_PackManifest.c", 'class LF_PackManifest\n{\n\tstatic const string CORE_VERSION = "0.1.0";\n}\n');
  put(root, "docs/architecture/public-surface.json", JSON.stringify({ version: 1, configClasses: ["LF_PackManifest", "LF_MissionHeader", "LF_ShopPricesConfig"], deprecated: [] }));
  put(root, "addons/LifeFramework/Scripts/Game/Feature/Shop/LF_ShopPricesConfig.c", "class LF_ShopPricesConfig\n{\n}\nclass LF_MissionHeader\n{\n}\n");
  return root;
}

let failed = 0;
const fail = (msg) => {
  failed++;
  console.log(`FAIL ${msg}`);
};
const roots = [];
const addons = (r) => join(r, "addons");

try {
  const root = tree();
  roots.push(root);
  const r = scaffoldPack(addons(root), "Test");
  if (r.error) fail(`scaffold: ${r.error}`);
  else {
    const m = checkPackManifest(root);
    if (m.length) fail(`scaffold passes validate-pack-manifest: ${m.join("; ")}`);
    const b = checkBoundary(addons(root));
    if (b.length) fail(`scaffold passes validate-addon-boundary: ${b.join("; ")}`);

    const gproj = readFileSync(join(r.dir, "LF_Test.gproj"), "utf8");
    if (!gproj.includes('"58D0FB3206B6F859" "79636E668EA37AC9"')) fail("gproj does not depend on the base game and the core");
    const header = readFileSync(join(r.dir, "Missions/LF_Test.conf"), "utf8");
    const manifestGuid = readFileSync(join(r.dir, "Configs/LF_PackManifest.conf.meta"), "utf8").match(/\{([0-9A-F]{16})\}/)?.[1];
    if (!manifestGuid || !header.includes(`{${manifestGuid}}Configs/LF_PackManifest.conf`)) fail("mission header does not name the manifest by its GUID");
    const manifest = readFileSync(join(r.dir, "Configs/LF_PackManifest.conf"), "utf8");
    if (!manifest.includes('m_sCoreMin "0.1.0"') || !manifest.includes('m_sCoreBelow "0.2.0"')) fail("manifest does not hold the core range");

    // Every resource GUID (each .meta Name, and the project GUID) is unique.
    const seen = new Set();
    for (const f of r.files.filter((p) => /\.(meta|gproj)$/.test(p))) {
      const text = readFileSync(join(r.dir, f), "utf8");
      const own = f.endsWith(".gproj") ? text.match(/^ GUID "([0-9A-F]{16})"/m)?.[1] : text.match(/Name "\{([0-9A-F]{16})\}/)?.[1];
      if (!own) fail(`no GUID in ${f}`);
      else if (seen.has(own)) fail(`GUID ${own} appears twice (${f})`);
      else seen.add(own);
    }

    if (!scaffoldPack(addons(root), "Test").error) fail("a second scaffold over an existing pack was allowed");
    for (const bad of ["test", "LF_Test", "Two Words", ""]) {
      if (!scaffoldPack(addons(root), bad).error) fail(`name '${bad}' was accepted`);
    }

    // The validators hold the new pack: break its range, then plant a core reference to it.
    writeFileSync(join(r.dir, "Configs/LF_PackManifest.conf"), manifest.replace('"0.2.0"', '"0.1.0"'));
    if (!checkPackManifest(root).some((e) => /LF_Test\/.*is empty/.test(e))) fail("validate-pack-manifest ignored the new pack's empty range");
    writeFileSync(join(r.dir, "Configs/LF_PackManifest.conf"), manifest);
    put(root, "addons/LifeFramework/Configs/Leak.conf", `Foo "{${manifestGuid}}Configs/LF_PackManifest.conf"\n`);
    if (!checkBoundary(addons(root)).some((e) => e.includes(`pack GUID {${manifestGuid}}`))) fail("validate-addon-boundary ignored the new pack");
  }

  // A second pack scaffolded beside the first still passes.
  const two = tree();
  roots.push(two);
  scaffoldPack(addons(two), "Alpha");
  scaffoldPack(addons(two), "Beta");
  const errs = [...checkPackManifest(two), ...checkBoundary(addons(two))];
  if (errs.length) fail(`two packs: ${errs.join("; ")}`);

  // A core name is refused; a core name that only starts with the pack id is not a clash.
  const names = tree();
  roots.push(names);
  put(names, "addons/LifeFramework/Scripts/Game/LF_Taken.c", "class LF_Taken\n{\n}\nclass LF_TestOrgSign\n{\n}\n");
  if (!scaffoldPack(addons(names), "Taken").error) fail("a name the core already uses was accepted");
  if (scaffoldPack(addons(names), "Test").error) fail("LF_Test was refused because the core has LF_TestOrgSign");
  const nerrs = [...checkPackManifest(names), ...checkBoundary(addons(names))];
  if (nerrs.length) fail(`pack Test beside core LF_TestOrgSign: ${nerrs.join("; ")}`);
} finally {
  for (const r of roots) rmSync(r, { recursive: true, force: true });
}

if (failed) {
  console.log(`pack-new-selftest: ${failed} case(s) failed`);
  process.exit(1);
}
console.log("pack-new-selftest: OK (scaffold passes both validators, unique GUIDs, new pack held to the boundary)");
