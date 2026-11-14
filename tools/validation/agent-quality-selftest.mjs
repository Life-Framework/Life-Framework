// Selftest for the feature scaffold (tools/feature/new.mjs), the PR body checks `wt ship` runs
// inputs: tools/**, agent-box/**, .githooks/**, addons/**, docs/**, website/**, video/**
// (tools/lib/pr-quality.mjs) and the evidence table (tools/commands/evidence.mjs). No game, no git.
// red-proof: see the two observed lines in docs/architecture/agent-quality.md "Proof of the checks".
import { existsSync, mkdirSync, mkdtempSync, readFileSync, readdirSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import * as feature from "../feature/new.mjs";
import * as pq from "../lib/pr-quality.mjs";
import { fitWidth, pairs, table } from "../commands/evidence.mjs";

const failures = [];
const check = (ok, msg) => {
  if (!ok) failures.push(msg);
};

// ------------------------------------------------------------- the scaffold
const ST_TAIL = "\n  }\n }\n}\n";
function scratchTree() {
  const root = mkdtempSync(join(tmpdir(), "lf-feature-ws-selftest-"));
  const put = (rel, text) => {
    mkdirSync(dirname(join(root, rel)), { recursive: true });
    writeFileSync(join(root, rel), text);
  };
  put("addons/LifeFramework/Language/everonlife_localization.st", `StringTable {\n Items {\n  X {\n    CustomStringTableItem "{0000000000000001}" {\n     Id "LF-Old"\n    }${ST_TAIL}`);
  put("addons/LifeFramework/Scripts/Game/Core/LF_StaticReset.c", "class LF_StaticReset\n{\n\tstatic void ResetAll()\n\t{\n\t\tLF_Old.ResetStatics();\n\n\t\ts_iRuns = s_iRuns + 1;\n\t}\n}\n");
  put("addons/LifeFramework/Scripts/Game/Feature/Police/Feature.md", "# Police\n\n## Intent\n\nOld text.\n");
  put("docs/features.md", "# Features\n\n### `LF_Old`\n- ok\n");
  put("docs/playtest.md", "# Playtest\n\n### M2 additions\n\n- old line\n\n### Known rough edges\n\n- rough\n");
  return root;
}

const silent = async (fn) => {
  const log = console.log;
  console.log = () => {};
  try {
    return await fn();
  } finally {
    console.log = log;
  }
};
const run = (root, ...args) => silent(() => feature.main(args, { root, scratch: true }));

const root = scratchTree();
try {
  const read = (rel) => readFileSync(join(root, rel), "utf8");
  const game = "addons/LifeFramework/Scripts/Game";
  check((await run(root, "new", "police", "SpikeStrip")) === 1, "a lower-case area was accepted");
  check((await run(root, "new", "Police", "LF_SpikeStrip")) === 1, "a name with the LF_ prefix was accepted");
  check((await run(root, "new", "Police", "RadarGun", "--kind", "gadget")) === 1, "an unknown kind was accepted");
  check((await run(root, "new", "Police", "RadarGun", "--kind", "item", "--issue", "901", "--dry-run")) === 0, "the dry run failed");
  check(!existsSync(join(root, `${game}/Feature/Police/LF_RadarGun.c`)), "the dry run wrote a file");

  check((await run(root, "new", "Police", "RadarGun", "--kind", "item", "--issue", "901")) === 0, "feature new (item) failed");
  for (const f of [`${game}/Feature/Police/LF_RadarGun.c`, `${game}/Feature/Police/LF_RadarGunAction.c`, `${game}/Tests/LF_Test_RadarGun.c`, `${game}/Tests/LF_Test_RadarGunWorld.c`, "agent-box/proofs/radar-gun-proof.mjs", "tmp/feature-radar-gun.md"]) {
    check(existsSync(join(root, f)), `feature new did not write ${f}`);
  }
  const bodies = existsSync(join(root, "tmp")) ? readdirSync(join(root, "tmp")).filter((n) => n.startsWith("pr-body-")) : [];
  check(bodies.length === 1, "feature new did not write one tmp/pr-body-<slug>.md");
  if (bodies.length === 1) {
    const body = read(`tmp/${bodies[0]}`);
    check(/^## Manual testing$/m.test(body) && /^## Screenshots$/m.test(body) && /^## Review$/m.test(body), "the PR body template lacks a section");
    check(pq.bodyProblems({ body, files: [`${game}/Feature/Police/LF_RadarGun.c`] }).some((p) => p.includes("placeholder")), "wt ship would accept the unfilled PR body template");
  }
  if (existsSync(join(root, `${game}/Tests/LF_Test_RadarGun.c`))) {
    const t = read(`${game}/Tests/LF_Test_RadarGun.c`);
    check(/^\/\/ tier: LOGIC$/m.test(t), "the logic test has no // tier: LOGIC line");
    check(/^\/\/ red-proof: PENDING/m.test(t), "the red-proof slot is not PENDING, so validate-tests would accept an unobserved proof");
    check(t.includes('return "police/radar-gun";'), "the test name is not <area>/<kebab>");
  }
  for (const f of [`${game}/Feature/Police/LF_RadarGun.c`, `${game}/Feature/Police/LF_RadarGunAction.c`, `${game}/Tests/LF_Test_RadarGun.c`]) {
    if (!existsSync(join(root, f))) continue;
    const code = read(f).replace(/\/\/.*$/gm, "").replace(/"[^"\n]*"/g, '""');
    check(!/\?[^;\n]*:/.test(code), `${f} uses a ternary, which Enforce Script does not compile`);
    check(!/\bEL_/.test(code), `${f} names an EL_ identifier`);
  }
  const proof = existsSync(join(root, "agent-box/proofs/radar-gun-proof.mjs")) ? read("agent-box/proofs/radar-gun-proof.mjs") : "";
  check(proof.includes('name: "radar-gun-proof"') && /\busage:/.test(proof) && /\brun:/.test(proof), "the proof module does not have the shape validate-registries wants");
  const st = read("addons/LifeFramework/Language/everonlife_localization.st");
  check(st.endsWith(ST_TAIL), "the string table no longer ends with its closing braces");
  check(st.includes('Id "LF-RadarGun_Action"') && st.includes('Id "LF-RadarGun_Denied"') && st.includes('Id "LF-Old"'), "the string rows were not appended beside the old ones");
  check(read(`${game}/Core/LF_StaticReset.c`).includes("\t\tLF_Old.ResetStatics();\n\t\tLF_RadarGun.ResetStatics();\n\n\t\ts_iRuns"), "the ResetStatics call was not added at the end of ResetAll");
  const md = read(`${game}/Feature/Police/Feature.md`);
  check(md.startsWith("# Police\n\n## Intent\n\nOld text.") && md.includes("## Radar gun (#901)"), "Feature.md was rewritten instead of appended to");
  check(read("docs/features.md").includes("### `LF_RadarGun`") && read("docs/features.md").includes("### `LF_Old`"), "docs/features.md lost a row or did not gain one");
  const play = read("docs/playtest.md");
  check(play.indexOf("**Radar gun (#901).**") > play.indexOf("- old line") && play.indexOf("**Radar gun (#901).**") < play.indexOf("### Known rough edges"), "the playtest line is not at the end of the additions list");
  check(feature.leftovers(root).length >= 5, "feature check found too few TODO(scaffold) markers");
  check((await silent(() => feature.main(["check"], { root }))) === 1, "feature check passed while markers are left");
  check((await run(root, "new", "Police", "RadarGun")) === 1, "a second run overwrote an existing feature");

  check((await run(root, "new", "Notices", "CreditsBoard", "--kind", "ui")) === 0, "feature new (ui, new area) failed");
  check(existsSync(join(root, `${game}/Feature/Notices/LF_CreditsBoardSheet.c`)) && !existsSync(join(root, `${game}/Feature/Notices/LF_CreditsBoardAction.c`)), "the ui kind writes a sheet and no action");
  check(read(`${game}/Feature/Notices/Feature.md`).startsWith("# Notices"), "a new area did not get a Feature.md");
  check(read("addons/LifeFramework/Language/everonlife_localization.st").includes('Id "LF-CreditsBoard_Title"'), "the ui kind did not add the title key");
  check((await run(root, "new", "Notices", "Curfew")) === 0, "feature new (logic) failed");
  check(!existsSync(join(root, "agent-box/proofs/curfew-proof.mjs")) && !existsSync(join(root, `${game}/Tests/LF_Test_CurfewWorld.c`)), "the logic kind wrote a proof or a WORLD test");
} finally {
  rmSync(root, { recursive: true, force: true });
}

// ------------------------------------------------------------ the PR body
const IMG = "![bank after](https://raw.githubusercontent.com/Life-Framework/Life-Framework/main/website/public/docs-images/481/bank-after.png)";
const GOOD = `## Summary\n\nA board.\n\n${IMG}\n\n## Manual testing\n\n1. Walk to the board.\n`;
const layer = ["addons/LifeFramework/Worlds/DebugWorld/DebugWorld_Layers/Signs.layer"];
const script = ["addons/LifeFramework/Scripts/Game/Feature/Police/LF_Jail.c"];
const sheet = ["addons/LifeFramework/Scripts/Game/Feature/Notices/LF_CreditsSheet.c"];
const problems = (o) => pq.bodyProblems({ exists: () => true, ...o });

check(problems({ body: GOOD, files: layer }).length === 0, `a complete body was refused: ${problems({ body: GOOD, files: layer }).join(" | ")}`);
check(problems({ body: GOOD.replace(IMG, "Shot: `website/public/docs-images/481/bank-after.png`"), files: layer }).some((p) => p.includes("embeds no picture")), "a layer change with no picture was accepted");
check(problems({ body: GOOD.replace(IMG, ""), files: script }).length === 0, "a script-only change was asked for a picture");
check(problems({ body: GOOD.replace(IMG, "No screenshots: the prefab only gains a server-side component, nothing renders."), files: layer }).length === 0, "a written waiver was refused");
check(problems({ body: GOOD.replace(IMG, "No screenshots: n/a"), files: layer }).length === 1, "a waiver with no reason was accepted");
check(problems({ body: GOOD, files: layer, exists: () => false }).some((p) => p.includes("not in this branch")), "a picture that is not committed was accepted");
check(problems({ body: GOOD.replace("## Manual testing", "## Notes"), files: script }).some((p) => p.includes("Manual testing")), "an addons change with no Manual testing section was accepted");
check(problems({ body: `${GOOD}\n${pq.MARK}: fill in`, files: script }).some((p) => p.includes("placeholder")), "a body with a scaffold placeholder was accepted");
check(problems({ body: "", files: script, hasBodyFile: false }).some((p) => p.includes("--body-file")), "an addons change with the automatic body was accepted");
check(problems({ body: "", files: ["docs/x.md", "tools/y.mjs"], hasBodyFile: false }).length === 0, "a docs and tools change was asked for a body file");
check(problems({ body: GOOD, files: sheet }).length === 0 && problems({ body: GOOD.replace(IMG, ""), files: sheet }).length === 1, "a sheet script does not count as a visible change");

const REVIEW = "# Review\n\n- [ ] RplLoad misaligns on a format mismatch\n- [x] huge string from a client. fixed: capped at 64 in RpcAsk_Dial\n";
const OPEN = "- [ ] RplLoad misaligns on a format mismatch";
check(problems({ body: GOOD, files: script, reviewText: REVIEW }).some((p) => p.includes("1 open finding")), "an open review finding was accepted");
const done = REVIEW.replace(OPEN, "- [x] RplLoad misaligns on a format mismatch. dismissed: the reader checks the version first (LF_X.c:40)");
check(problems({ body: GOOD, files: script, reviewText: done }).length === 0, "resolved findings were refused");
check(problems({ body: GOOD, files: script, reviewText: REVIEW.replace(OPEN, "- [x] RplLoad misaligns") }).some((p) => p.includes("fixed:")), "a ticked finding with no outcome was accepted");
const sent = pq.withReview(`${GOOD}\n🤖 Generated with [Claude Code](https://claude.com/claude-code)\n`, done);
check(sent.includes("## Review findings") && sent.includes("capped at 64") && sent.trimEnd().endsWith("(https://claude.com/claude-code)"), "the review file was not carried into the PR body above the footer");
check(pq.withReview(GOOD, null) === GOOD, "a missing review file changed the body");
check(pq.repoPathOf("https://raw.githubusercontent.com/o/r/main/website/public/a%20b.png") === "website/public/a b.png" && pq.repoPathOf("https://example.com/x.png") === null, "repoPathOf reads the wrong path");

// ----------------------------------------------------------- the evidence
const files = ["bank-before.png", "bank-after.png", "sheet-after.png", "notes.txt"];
check(pairs(files).length === 2 && pairs(files)[0].before === "bank-before.png" && pairs(files)[1].before === null, "evidence pairs are wrong");
const md = table("481", files);
check(md.split("\n").length === 4 && md.includes("docs-images/481/bank-before.png") && md.includes("| (none) | ![sheet after]"), "the evidence table is wrong");
check(pq.embeddedImages(md).length === 3, "the evidence table does not embed its pictures");
const big = fitWidth(2560, 1440, Buffer.alloc(2560 * 1440 * 3, 7));
check(big.width === 1280 && big.height === 720 && big.rgb.length === 1280 * 720 * 3 && big.rgb[100] === 7, "fitWidth does not halve a 2560 px picture");
check(fitWidth(640, 360, Buffer.alloc(640 * 360 * 3)).width === 640, "fitWidth scaled a small picture up");

// ------------------------------------------------- the two ratchet checks
const props = await import("./validate-player-props.mjs");
const LAYER = `GenericEntity Till_Debug : "{0000000000000001}Prefabs/Props/Till.et" {\n coords 1 2 3\n}\n$grp GenericEntity : "{0000000000000002}Prefabs/X.et" {\n Roadmap_Kit {\n }\n}\nGenericEntity Roadmap_Kit : "{0000000000000003}Prefabs/Props/Debug/Stand.et" {\n}\nGenericEntity Bank_Till : "{0000000000000004}Prefabs/Props/Till.et" {\n components {\n  LF_DebugJailMeAction "{0000000000000005}" {\n  }\n }\n}\n`;
const hits = props.debugProps(LAYER).map((h) => h.name);
check(hits.length === 2 && hits.includes("Till_Debug") && hits.includes("Roadmap_Kit"), `debugProps found ${hits.join(", ") || "nothing"}, expected Till_Debug and Roadmap_Kit`);
const verdict = props.check([{ layer: "Street_Bank", name: "Till_Debug" }, { layer: "Street_Bank", name: "Old_Debug" }], ["Street_Bank/Old_Debug", "Street_Jail/Gone_Debug"]);
check(verdict.fresh.length === 1 && verdict.fresh[0].name === "Till_Debug" && verdict.stale.length === 1, "a new debug prop in a village layer was accepted, or a baselined one refused");

const kit = await import("./validate-ui-kit.mjs");
const FONT = (g) => `Font "${g}UI/Fonts/X/X.fnt"`;
const base = { layouts: ["addons/LifeFramework/UI/Layouts/Old.layout"], exceptions: { "addons/LifeFramework/UI/Layouts/Police/Face.layout": "A render-target face on the wanted board, not a screen." } };
const lay = (path, text = "") => ({ path, text });
check(kit.problems([lay("addons/LifeFramework/UI/Layouts/Old.layout", FONT("{1111111111111111}")), lay("addons/LifeFramework/UI/Layouts/UIKit/LF_UINew.layout")], base).length === 0, "a baselined layout or a kit part was refused");
check(kit.problems([lay("addons/LifeFramework/UI/Layouts/Shop/NewMenu.layout")], base).length === 1, "a new layout outside the kit was accepted");
check(kit.problems([lay("addons/LifeFramework/UI/Layouts/Police/Face.layout", FONT(kit.THEME_FONTS[0]))], base).length === 0, "an exception in the theme font was refused");
check(kit.problems([lay("addons/LifeFramework/UI/Layouts/Police/Face.layout", FONT("{1111111111111111}"))], base).some((p) => p.includes("uses font")), "an exception with a foreign font was accepted");

const kitScript = (name, text) => ({ path: `addons/LifeFramework/Scripts/Game/Feature/UIKit/${name}`, text });
const NL = String.fromCharCode(10);
check(kit.colorProblems([kitScript("LF_UISkin.c", "skin.m_Paper = Color.FromSRGBA(238, 230, 212, 255);")]).length === 1, "a colour literal in a kit script was accepted");
check(kit.colorProblems([kitScript("LF_UIScreen.c", ["// Color.FromSRGBA(1, 2, 3, 4) in a comment", "x = Color.FromSRGBA(r, g, b, 255);", "c = Color.FromSRGBA(0, 0, 0, 0);"].join(NL))]).length === 0, "a comment, a variable colour or a transparent colour was refused");
check(kit.colorProblems([kitScript("LF_UITheme.c", "x = Color.FromSRGBA(0, 0, 0, 255);")]).length === 0, "the theme's own script was refused a literal");

if (failures.length) {
  console.log("agent-quality-selftest: FAIL");
  for (const f of failures) console.log(`  ${f}`);
  process.exit(1);
}
console.log("agent-quality-selftest: OK (scaffold, PR body checks, evidence table)");
