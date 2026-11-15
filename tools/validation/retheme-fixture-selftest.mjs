// tools/validation/retheme-fixture-selftest.mjs
// inputs: tools/**, addons/**
//
// Proves the #1005 integration fixture (tools/pack/retheme-fixture.mjs) applies to a fresh `pack new`
// scaffold: the mission header lists every overlay by the GUID of its .meta, each overlay and its .meta
// exist, the pack's string rows land in its .st, and no GUID the fixture writes is used anywhere else in
// addons/. The booted proof is agent-box/proofs/retheme-proof.mjs; this is the offline half.
// red-proof: the first version of applyRetheme matched the header's overlay block with `[^}]*`, which
// stops at the first GUID brace; `pack check` then printed `overlay }Configs/ShopPrices.conf" resolves to
// no resource`. With that regex put back this prints `FAIL header ends with the overlay list and nothing
// else: ... }Configs/ShopPrices.conf" ...` and exits 1 (observed 2026-11-15; the first version of this check
// passed it, so it now matches the header's whole tail).

import { mkdirSync, mkdtempSync, readFileSync, readdirSync, rmSync, statSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { scaffoldPack } from "../pack/new.mjs";
import { applyRetheme, loadFixture } from "../pack/retheme-fixture.mjs";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..");

function put(root, path, text) {
  const full = join(root, path);
  mkdirSync(dirname(full), { recursive: true });
  writeFileSync(full, text);
}

function tree() {
  const root = mkdtempSync(join(tmpdir(), "lf-retheme-"));
  put(root, "addons/LifeFramework/LifeFramework.gproj", 'GameProject {\n ID "LifeFramework"\n GUID "4550A2B656E1B569"\n}\n');
  put(root, "addons/LifeFramework/Scripts/Game/Feature/Packs/LF_PackManifest.c", 'class LF_PackManifest\n{\n\tstatic const string CORE_VERSION = "0.1.0";\n}\n');
  put(root, "addons/LifeFramework/Worlds/DebugWorld/DebugWorld.ent.meta", 'MetaFileClass {\n Name "{F3AA388E8466788C}Worlds/DebugWorld/DebugWorld.ent"\n}\n');
  put(root, "addons/LifeFramework/Configs/Systems/LifeFrameworkSystems.conf.meta", 'MetaFileClass {\n Name "{2104C177A245B6D1}Configs/Systems/LifeFrameworkSystems.conf"\n}\n');
  return root;
}

function walk(dir, out = []) {
  for (const f of readdirSync(dir)) {
    const p = join(dir, f);
    if (statSync(p).isDirectory()) walk(p, out);
    else out.push(p);
  }
  return out;
}

let failed = 0;
const fail = (msg) => {
  failed++;
  console.log(`FAIL ${msg}`);
};

const root = tree();
try {
  const fx = loadFixture();
  const r = scaffoldPack(join(root, "addons"), fx.pack);
  if (r.error) fail(`scaffold: ${r.error}`);
  else {
    applyRetheme(join(root, "addons"));
    const header = readFileSync(join(r.dir, "Missions", `LF_${fx.pack}.conf`), "utf8");
    const listed = [...(header.match(/m_aLF_ConfigOverlays \{\n([\s\S]*?)\n \}/)?.[1] ?? "").matchAll(/"\{([0-9A-F]{16})\}([^"]+)"/g)].map((m) => [m[1], m[2]]);
    const want = fx.overlays.map((rel) => {
      const guid = readFileSync(join(r.dir, `${rel}.meta`), "utf8").match(/\{([0-9A-F]{16})\}/)?.[1];
      return `{${guid}}${rel}`;
    });
    const got = listed.map(([g, rel]) => `{${g}}${rel}`);
    if (JSON.stringify(got) !== JSON.stringify(want)) fail(`header lists every overlay by its .meta GUID: got ${JSON.stringify(got)}, want ${JSON.stringify(want)}`);
    if (!/ m_aLF_ConfigOverlays \{\n(?:  "\{[0-9A-F]{16}\}[^"\n]+"\n)+ \}\n\}\n?$/.test(header)) fail(`header ends with the overlay list and nothing else: ${JSON.stringify(header.slice(-120))}`);

    const st = readFileSync(join(r.dir, "Language", `${fx.pack.toLowerCase()}_localization.st`), "utf8");
    for (const s of fx.strings) if (!st.includes(`Id "${s.id}"`)) fail(`string row ${s.id} is in the pack's .st`);

    // A fixture GUID (a meta, a record, a string row) that another resource in addons/ already has would
    // collide at load: it must appear once in the fixture and nowhere else.
    const mine = new Set([...Object.values(fx.metaGuids), ...fx.strings.map((s) => s.guid)]);
    for (const f of walk(join(ROOT, "tools", "test", "pack-fixtures", "retheme")).filter((p) => p.endsWith(".conf"))) {
      for (const m of readFileSync(f, "utf8").matchAll(/"\{([0-9A-F]{16})\}"/g)) mine.add(m[1]);
    }
    for (const f of walk(join(ROOT, "addons")).filter((p) => /\.(meta|conf|layer|et|st|ent|layout)$/.test(p))) {
      if (f.includes("LF_Retheme")) continue; // the generated pack, when this worktree has one
      const text = readFileSync(f, "utf8");
      for (const g of mine) if (text.includes(`{${g}}`)) fail(`fixture GUID ${g} is also used in ${f.slice(ROOT.length + 1)}`);
    }
  }
} finally {
  rmSync(root, { recursive: true, force: true });
}

if (failed) {
  console.log("retheme-fixture-selftest: FAIL");
  process.exit(1);
}
console.log("retheme-fixture-selftest: OK");
