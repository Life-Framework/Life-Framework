// Selftest for validate-meta-name-path.mjs. Red proof: with isBareName returning false, the first case fails.
import { isBareName } from "./validate-meta-name-path.mjs";

const cases = [
  ['MetaFileClass {\n Name "{B4C1A2A68D627B9F}.edds"\n}', true],
  ['MetaFileClass {\n Name "{B4C1A2A68D627B9F}UI/Imagesets/LF_MapIcons_Everon1989_HatchGated.edds"\n}', false],
  ['MetaFileClass {\n Name "{BF263B38BFBB5166}Sandbag_01_BCR_Sandy.edds"\n}', false],
];
let fail = 0;
for (const [text, want] of cases) {
  if (isBareName(text) !== want) {
    fail++;
    console.log(`FAIL  meta-name-path-selftest: expected ${want} for ${JSON.stringify(text)}`);
  }
}
console.log(fail ? "FAIL  meta-name-path-selftest" : "PASS  meta-name-path-selftest");
process.exit(fail ? 1 : 0);
