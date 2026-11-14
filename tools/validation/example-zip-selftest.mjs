// Proves the example server download (#296): the zip holds the five template files, a draft carries the
// placeholder id, `--final` refuses it, and a zip with real ids passes the checks it was built to meet.
// inputs: tools/release/**, tools/server-template/**, tools/config/**, server/example/**, tools/lib/addons.mjs
// red-proof: with the --final placeholder refusal switched off in buildExampleZip, the run printed
// "FAIL final's refusal does not name the id: server.json still holds a PLACEHOLDER marker | DRAFT.txt ..."
// and "FAIL a refused final still produced bytes" (verifyZip still caught it, but without naming the id).
import { cpSync, mkdirSync, mkdtempSync, readFileSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { ADDONS } from "../lib/addons.mjs";
import { makeZip, readZip } from "../release/pack-playtest.mjs";
import { DRAFT_NAME, IDS_FILE, ZIP_NAME, buildExampleZip, idsProblems, loadIds, main, verifyZip } from "../release/example-zip.mjs";
import { FILES } from "../server-template/template.mjs";

const fails = [];
const expect = (ok, msg) => { if (!ok) fails.push(msg); };
const names = (buf) => readZip(buf).map((e) => e.name).sort().join();
const get = (buf, n) => readZip(buf).find((e) => e.name === n);

// The shipped state: a draft, because the pack id is a placeholder until #370.
const draft = buildExampleZip(".", { final: false });
expect(draft.ok, `the shipped template does not build a draft: ${draft.problems?.join(" | ")}`);
if (draft.ok) {
  expect(draft.name === DRAFT_NAME, `a draft is named ${draft.name}`);
  expect(names(draft.buf) === [...FILES, "DRAFT.txt"].sort().join(), `draft entries: ${names(draft.buf)}`);
  const sj = JSON.parse(get(draft.buf, "server.json").data.toString("utf8"));
  expect(sj.game.mods[0].modId === ADDONS[0].guid, "the core is not first in the zip's server.json");
  expect(/^PLACEHOLDER/.test(sj.game.mods[1].modId), `the pack id in a draft is ${sj.game.mods[1].modId}`);
  expect(get(draft.buf, "config.json").data.equals(readFileSync("server/example/config.json")), "config.json in the zip differs from server/example/config.json");
  expect((get(draft.buf, "start.sh").mode & 0o111) !== 0, "start.sh lost its executable bit");
  expect((get(draft.buf, "start.cmd").mode & 0o111) === 0, "start.cmd is marked executable");
}

const final = buildExampleZip(".", { final: true });
expect(!final.ok, "FAIL final built a zip while mods.pack.modId is still a placeholder");
expect(/mods\.pack\.modId/.test((final.problems ?? []).join(" | ")), `final's refusal does not name the id: ${(final.problems ?? []).join(" | ")}`);
expect(!final.buf, "a refused final still produced bytes");

// A checkout copy with the maintainer's ids in the config point.
const copy = () => {
  const root = mkdtempSync(join(tmpdir(), "lf-exzip-"));
  cpSync("server/example", join(root, "server", "example"), { recursive: true });
  for (const a of ADDONS) {
    cpSync(`addons/${a.name}/${a.name}.gproj`, join(root, "addons", a.name, `${a.name}.gproj`));
    cpSync(`addons/${a.name}/Missions`, join(root, "addons", a.name, "Missions"), { recursive: true });
  }
  cpSync("tools/test/config-fixtures/presets", join(root, "tools", "test", "config-fixtures", "presets"), { recursive: true });
  mkdirSync(join(root, "tools", "release"), { recursive: true });
  cpSync(IDS_FILE, join(root, IDS_FILE));
  return root;
};
const setIds = (root, edit) => {
  const j = JSON.parse(readFileSync(join(root, IDS_FILE), "utf8"));
  edit(j.mods);
  writeFileSync(join(root, IDS_FILE), JSON.stringify(j, null, 2));
};

{
  const root = copy();
  setIds(root, (m) => { m.pack.modId = ADDONS[1].guid; });
  const r = buildExampleZip(root, { final: true });
  expect(r.ok, `final with confirmed ids: ${r.problems?.join(" | ")}`);
  if (r.ok) {
    expect(r.name === ZIP_NAME, `a final is named ${r.name}`);
    expect(names(r.buf) === [...FILES].sort().join(), `final entries: ${names(r.buf)}`);
    expect(readZip(r.buf).every((e) => !e.data.toString("utf8").includes("PLACEHOLDER")), "a final zip still holds a placeholder");
    expect(verifyZip(r.buf, { final: true }).length === 0, "verifyZip rejects the final zip it was built to pass");
    const out = mkdtempSync(join(tmpdir(), "lf-exzip-out-"));
    const lines = [];
    const log = console.log;
    console.log = (s) => lines.push(String(s));
    const code = main(["--final", "--out", out], { root });
    console.log = log;
    expect(code === 0 && /SUMMARY example-zip ok final/.test(lines.join("\n")), `main --final: ${code} ${lines.join(" | ")}`);
    expect(readFileSync(join(out, ZIP_NAME)).equals(r.buf), "the file main wrote differs from the built bytes");
  }
}

{
  const root = copy();
  setIds(root, (m) => { m.pack.modId = "0123456789ABCDEF"; });
  expect(/not Life.*GUID|not LF_Everon1989's GUID/.test(idsProblems(loadIds(root)).join(" | ")), "a hex id that is no addon's GUID passed");
  setIds(root, (m) => { m.core.modId = "TODO"; });
  expect(/neither a 16-character hex id/.test(idsProblems(loadIds(root)).join(" | ")), "an unmarked non-hex id passed");
  expect(!buildExampleZip(root, { final: false }).ok, "a draft built from a bad ids file");
}

{
  const root = copy();
  writeFileSync(join(root, "server", "example", "config.json"), "{ not json");
  expect(!buildExampleZip(root, { final: false }).ok, "a draft built with a broken config.json");
}

// verifyZip on hand-made zips: each way a zip can be wrong is named.
const files = () => FILES.map((n) => ({ name: n, data: readFileSync(join("server/example", n)) }));
expect(/no start\.sh/.test(verifyZip(makeZip(files().filter((e) => e.name !== "start.sh"))).join(" | ")), "a zip missing start.sh passed");
expect(/not valid JSON/.test(verifyZip(makeZip(files().map((e) => (e.name === "server.json" ? { ...e, data: Buffer.from("{") } : e)))).join(" | ")), "a zip with broken server.json passed");
expect(/core .* must be first/.test(verifyZip(makeZip(files().map((e) => (e.name === "server.json" ? { ...e, data: Buffer.from(e.data.toString("utf8").replace(ADDONS[0].guid, "AAAAAAAAAAAAAAAA")) } : e)))).join(" | ")), "a zip without the core first passed");
expect(/still holds a PLACEHOLDER/.test(verifyZip(makeZip(files().map((e) => (e.name === "README.md" ? { ...e, data: Buffer.from("PLACEHOLDER") } : e))), { final: true }).join(" | ")), "final verify missed a placeholder");

if (fails.length) {
  for (const f of fails) console.log(`FAIL ${f}`);
  process.exit(1);
}
console.log("example-zip-selftest: OK");
