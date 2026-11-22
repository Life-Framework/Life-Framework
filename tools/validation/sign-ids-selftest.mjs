// tools/validation/sign-ids-selftest.mjs - validate-sign-ids.
// inputs: tools/validation/validate-sign-ids.mjs, tools/lib/sign-ids.mjs, tools/validation/sign-ids-selftest.mjs
//
// Runs the logic over a planted file set:
//   - a layer entity of a sign prefab with no id anywhere fails with its line;
//   - the same entity with an id on the instance passes; so does an id set on the prefab itself;
//   - a `$grp` member with no id fails, one with an id passes;
//   - a sign with `m_bShowFace 0` (nothing printed) and a prefab with no LF_SignComponent pass.
//
// red-proof: 2026-11-22 with the id test removed from signsWithoutId, "an entity with no id fails" printed FAIL.
import { signsWithoutId } from "../lib/sign-ids.mjs";

let failed = 0;
const check = (ok, msg) => {
  console.log(`${ok ? "PASS" : "FAIL"}  ${msg}`);
  if (!ok) failed++;
};

const meta = (guid) => `MetaFileClass {\n Name "{${guid}}Prefabs/x.et"\n}\n`;
const SIGN = "AAAAAAAAAAAAAAA1";
const SIGN_WITH_ID = "AAAAAAAAAAAAAAA2";
const PLAIN = "AAAAAAAAAAAAAAA3";
const OFF = "AAAAAAAAAAAAAAA4";
const files = new Map([
  ["addons/X/Prefabs/Sign.et", `GenericEntity : "{${PLAIN}}Prefabs/Plain.et" {\n components {\n  LF_SignComponent "{1}" {\n   m_vRimColor 0 0 0\n  }\n }\n}\n`],
  ["addons/X/Prefabs/Sign.et.meta", meta(SIGN)],
  ["addons/X/Prefabs/SignWithId.et", `GenericEntity : "{${SIGN}}Prefabs/Sign.et" {\n components {\n  LF_SignComponent "{1}" {\n   m_sSignId "bakery"\n  }\n }\n}\n`],
  ["addons/X/Prefabs/SignWithId.et.meta", meta(SIGN_WITH_ID)],
  ["addons/X/Prefabs/Plain.et", "GenericEntity {\n}\n"],
  ["addons/X/Prefabs/Plain.et.meta", meta(PLAIN)],
  ["addons/X/Prefabs/Off.et", `GenericEntity : "{${SIGN}}Prefabs/Sign.et" {\n components {\n  LF_SignComponent "{1}" {\n   m_bShowFace 0\n  }\n }\n}\n`],
  ["addons/X/Prefabs/Off.et.meta", meta(OFF)],
]);

const layer = (body) => `${body}\n`;
const run = (text) => {
  const all = new Map(files);
  all.set("addons/X/Worlds/W/L.layer", layer(text));
  return signsWithoutId(all);
};
const entity = (name, guid, comp) => `GenericEntity ${name} : "{${guid}}Prefabs/x.et" {\n${comp}coords 1 2 3\n}`;
const withId = ` components {\n  LF_SignComponent "{1}" {\n   m_sSignId "barber"\n  }\n }\n`;

const noId = run(entity("Fascia", SIGN, ""));
check(noId.length === 1 && noId[0].line === 1 && noId[0].name === "Fascia", "an entity of a sign prefab with no id fails with its line");
check(run(entity("Fascia", SIGN, withId)).length === 0, "an id on the instance passes");
check(run(entity("Fascia", SIGN_WITH_ID, "")).length === 0, "an id set on the prefab passes");
check(run(entity("Fascia", SIGN, ` components {\n  LF_SignComponent "{1}" {\n   m_sSignId ""\n  }\n }\n`)).length === 1, "an empty id on the instance fails");
check(run(entity("Thing", PLAIN, "")).length === 0, "a prefab with no LF_SignComponent passes");
check(run(entity("Quiet", OFF, "")).length === 0, "a sign with m_bShowFace 0 passes");

const grp = (member) => `$grp GenericEntity : "{${SIGN}}Prefabs/Sign.et" {\n ${member}\n}`;
const groupBad = run(grp("Member {\n  coords 1 2 3\n }"));
check(groupBad.length === 1 && groupBad[0].name === "Member", "a $grp member with no id fails");
check(run(grp(`Member {\n  components {\n   LF_SignComponent "{1}" {\n    m_sSignId "rules"\n   }\n  }\n  coords 1 2 3\n }`)).length === 0, "a $grp member with an id passes");
check(run(`${entity("A", SIGN, "")}\n${entity("B", SIGN, withId)}`).length === 1, "two entities: only the one with no id fails");

if (failed) {
  console.log(`SUMMARY sign-ids-selftest fail ${failed}`);
  process.exit(1);
}
console.log("PASS  sign-ids-selftest");
