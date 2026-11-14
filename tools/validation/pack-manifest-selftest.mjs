// tools/validation/pack-manifest-selftest.mjs
//
// Proves validate-pack-manifest fails on each violation it exists for, in a fixture tree.
// red-proof: with the baseline check disabled this printed "FAIL removed public name: expected
// /was removed/" and exited 1 (2026-11-14).

import { mkdirSync, mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import { checkPackManifest } from "./validate-pack-manifest.mjs";

function put(root, path, text) {
  const full = join(root, path);
  mkdirSync(dirname(full), { recursive: true });
  writeFileSync(full, text);
}

const manifest = (min, below) =>
  `LF_PackManifest {\n m_sPackId "LF_Everon1989"\n m_sCoreMin "${min}"\n m_sCoreBelow "${below}"\n}\n`;
const surface = (names, deprecated = []) => JSON.stringify({ version: 1, configClasses: names, deprecated });

function cleanTree() {
  const root = mkdtempSync(join(tmpdir(), "lf-packmanifest-"));
  put(root, "addons/LifeFramework/Scripts/Game/Feature/Packs/LF_PackManifest.c",
    'class LF_PackManifest\n{\n\tstatic const string CORE_VERSION = "0.1.0";\n}\nclass LF_ShopPricesConfig\n{\n}\nclass LF_InternalThing\n{\n}\n');
  put(root, "docs/architecture/public-surface.json", surface(["LF_PackManifest", "LF_ShopPricesConfig"]));
  put(root, "docs/architecture/public-surface-baseline.json", surface(["LF_PackManifest", "LF_ShopPricesConfig"]));
  put(root, "addons/LF_Everon1989/Configs/LF_PackManifest.conf", manifest("0.1.0", "0.2.0"));
  put(root, "addons/LF_Everon1989/Configs/Shops.conf", "LF_ShopPricesConfig {\n}\n");
  return root;
}

const cases = [
  { name: "range excludes the core", plant: (r) => put(r, "addons/LF_Everon1989/Configs/LF_PackManifest.conf", manifest("0.2.0", "0.3.0")), expect: /excludes the running core 0\.1\.0/ },
  { name: "range is not x.y.z", plant: (r) => put(r, "addons/LF_Everon1989/Configs/LF_PackManifest.conf", manifest("0.1", "0.2.0")), expect: /m_sCoreMin "0\.1" is not x\.y\.z/ },
  { name: "range is empty", plant: (r) => put(r, "addons/LF_Everon1989/Configs/LF_PackManifest.conf", manifest("0.2.0", "0.2.0")), expect: /is empty/ },
  { name: "no manifest", plant: (r) => rmSync(join(r, "addons/LF_Everon1989/Configs/LF_PackManifest.conf")), expect: /exactly one LF_PackManifest/ },
  { name: "pack uses an internal core class", plant: (r) => put(r, "addons/LF_Everon1989/Configs/Internal.conf", "LF_InternalThing {\n}\n"), expect: /uses LF_InternalThing, which is not in the core's public surface/ },
  { name: "listed class missing from core", plant: (r) => put(r, "docs/architecture/public-surface.json", surface(["LF_PackManifest", "LF_ShopPricesConfig", "LF_Ghost"])), expect: /lists LF_Ghost, which core script does not declare/ },
  { name: "removed public name", plant: (r) => put(r, "docs/architecture/public-surface.json", surface(["LF_PackManifest"])), expect: /LF_ShopPricesConfig was removed/ },
];

let failed = 0;
const roots = [];
try {
  const clean = cleanTree();
  roots.push(clean);
  const cleanErrors = checkPackManifest(clean);
  if (cleanErrors.length) {
    failed++;
    console.log(`FAIL clean tree reported violations:\n  ${cleanErrors.join("\n  ")}`);
  }
  const deprecatedOk = cleanTree();
  roots.push(deprecatedOk);
  put(deprecatedOk, "docs/architecture/public-surface.json",
    surface(["LF_PackManifest"], [{ name: "LF_ShopPricesConfig", replacement: "LF_ShopPricesConfig2", since: "0.1.0", removeIn: "0.2.0" }]));
  put(deprecatedOk, "addons/LF_Everon1989/Configs/Shops.conf", "SCR_Foo {\n}\n");
  const depErrors = checkPackManifest(deprecatedOk);
  if (depErrors.length) {
    failed++;
    console.log(`FAIL a deprecated name was reported as removed:\n  ${depErrors.join("\n  ")}`);
  }
  for (const c of cases) {
    const root = cleanTree();
    roots.push(root);
    c.plant(root);
    const errors = checkPackManifest(root);
    if (!errors.some((e) => c.expect.test(e))) {
      failed++;
      console.log(`FAIL ${c.name}: expected ${c.expect}, got:\n  ${errors.join("\n  ") || "(no violations)"}`);
    }
  }
} finally {
  for (const r of roots) rmSync(r, { recursive: true, force: true });
}

if (failed) {
  console.log(`pack-manifest-selftest: ${failed} case(s) failed`);
  process.exit(1);
}
console.log(`pack-manifest-selftest: OK (${cases.length + 2} cases)`);
