// tools/validation/map-icons-selftest.mjs - card M3 (#2231): the LF_MapIcons checks in validate-ui-kit.
// inputs: tools/map/lib/icons.mjs, tools/validation/validate-ui-kit.mjs
// red-proof: each case below breaks one rule on a fixture tree and asserts the failure line; the
// "complete" case asserts the same tree passes, so a check that always fails or never fails is caught.
import { mkdtempSync, mkdirSync, writeFileSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import { ATLAS, FILES, catQuad, coreQuadNames, packQuadNames, layout, iconProblems, confRows } from "../map/lib/icons.mjs";

let failed = 0;
const check = (name, ok, detail = "") => {
  if (!ok) { failed++; console.log(`FAIL  ${name} ${detail}`); }
};

function imagesetText(guid, texPath, names, skip = []) {
  const pos = layout(names);
  const defs = names.filter((n) => !skip.includes(n)).map((n) => {
    const p = pos.get(n);
    return `  ImageSetDefClass ${n} {\n   Name "${n}"\n   Pos ${p.x} ${p.y}\n   Size ${p.w} ${p.h}\n   Flags 0\n  }\n`;
  }).join("");
  return `ImageSetClass {\n RefSize ${ATLAS} ${ATLAS}\n Textures {\n  ImageSetTextureClass {\n   mpix 1\n   path "{${guid}}${texPath}"\n  }\n }\n Images {\n${defs} }\n}`;
}

function put(root, rel, text) {
  mkdirSync(dirname(join(root, rel)), { recursive: true });
  writeFileSync(join(root, rel), text);
}

// A complete fixture tree; `skipCore` / `skipPack` drop quads from the imageset text.
function tree({ skipCore = [], skipPack = [] } = {}) {
  const root = mkdtempSync(join(tmpdir(), "lf-map-icons-"));
  const guids = { core: "AAAAAAAAAAAAAAA1", pack: "BBBBBBBBBBBBBBB2" };
  for (const side of ["core", "pack"]) {
    const f = FILES[side];
    const tex = f.atlas.slice(f.addon.length + 1);
    put(root, f.imageset, imagesetText(guids[side], tex, side === "core" ? coreQuadNames() : packQuadNames(), side === "core" ? skipCore : skipPack));
    put(root, f.atlas, "edds");
    put(root, `${f.atlas}.meta`, `MetaFileClass {\n Name "{${guids[side]}}${tex}"\n}\n`);
    for (const h of f.hatch) { put(root, h, "edds"); put(root, `${h}.meta`, `MetaFileClass {\n Name "{CCCCCCCCCCCCCCC3}x"\n}\n`); }
  }
  return root;
}

const conf = (rows) => ({ path: "addons/LifeFramework/Configs/Map/LF_MapPois.conf", text: `LF_MapPoiConfig {\n m_aKinds {\n${rows.map((r) => `  LF_MapPoiKind "{0000000000000001}" {\n   m_sKind "${r.kind}"\n   m_sCategory "${r.category ?? ""}"\n${r.icon ? `   m_sIcon "${r.icon}"\n` : ""}  }\n`).join("")} }\n}` });

const dirs = [];
const run = (opts, confs) => { const r = tree(opts); dirs.push(r); return iconProblems(r, confs); };

// complete tree passes; rows with and without an icon resolve
check("complete tree passes", run({}, [conf([{ kind: "Bank", category: "civic" }, { kind: "Taxi", category: "travel", icon: "poi_Taxi" }])]).length === 0);
// an absent imageset is "not built yet", not a failure
{ const r = mkdtempSync(join(tmpdir(), "lf-map-icons-")); dirs.push(r); check("absent imagesets are pending, not failing", iconProblems(r, []).length === 0); }
// a missing core category quad fails
check("missing core category quad", run({ skipCore: [catQuad("law")] }, []).some((l) => l.includes('quad "cat_law" is missing')));
// a missing pack pictogram fails
check("missing pack pictogram", run({ skipPack: ["poi_Bank"] }, []).some((l) => l.includes('quad "poi_Bank" is missing')));
// a row naming a quad that is in neither imageset fails
check("row names an absent quad", run({}, [conf([{ kind: "Bank", category: "civic", icon: "poi_Nope" }])]).some((l) => l.includes('names icon "poi_Nope"')));
// a row with a category the set does not have fails
check("row has an unknown category", run({}, [conf([{ kind: "Bank", category: "casino" }])]).some((l) => l.includes('category "casino"')));
// a texture whose meta GUID differs from the imageset's fails
{
  const r = tree(); dirs.push(r);
  put(r, `${FILES.core.atlas}.meta`, `MetaFileClass {\n Name "{DDDDDDDDDDDDDDD4}x"\n}\n`);
  check("texture GUID mismatch", iconProblems(r, []).some((l) => l.includes("does not match")));
}
// a missing hatch texture fails
{
  const r = tree(); dirs.push(r);
  rmSync(join(r, FILES.pack.hatch[0]));
  check("missing hatch texture", iconProblems(r, []).some((l) => l.includes("hatch texture")));
}
// conf parsing: rows with and without a category or icon
check("confRows reads kind, category and icon", JSON.stringify(confRows(conf([{ kind: "A", category: "law", icon: "x" }, { kind: "B" }]).text)) === JSON.stringify([{ kind: "A", category: "law", icon: "x" }, { kind: "B", category: "", icon: "" }]));

for (const d of dirs) rmSync(d, { recursive: true, force: true });
if (failed) { console.log(`map-icons-selftest: FAIL (${failed})`); process.exit(1); }
console.log("map-icons-selftest: OK");
