// Self-test for the credits generator (#481): a row in docs/credits.md reaches the generated outputs,
// and a hand-edited generated file no longer matches.
import assert from "node:assert/strict";
import { dataFile, entries, parse, patchSt } from "../commands/credits.mjs";

const md = `# Credits

## Projects

| Project | Author | Licence | What we took |
|---|---|---|---|
| [Overthrow](https://x) (\`THIRD_PARTY.md\`) | Aaron Static | APL, used with permission | Money code |

## Tools

- Piper makes the voice-over.
  Second line.

\`\`\`
- not a row
\`\`\`
`;
const list = entries(md);
assert.equal(list.length, 4);
assert.equal(list[0].label.text, "PROJECTS");
assert.equal(list[1].label.text, "Overthrow");
assert.equal(list[1].value.text, "Aaron Static, APL, used with permission");
assert.equal(list[3].label.text, "Piper makes the voice-over. Second line.");
assert.equal(parse(md).length, 2);

const st = "StringTable {\n Items {\n    CustomStringTableItem \"{AAAAAAAAAAAAAAAA}\" {\n     Id \"LF-Other_Key\"\n     Target_en_us \"x\"\n    }\n }\n}\n";
const once = patchSt(st, list);
assert.ok(once.includes('Id "LF-Credits_2"') && once.includes('Id "LF-Other_Key"'));
assert.equal(patchSt(once, list), once, "regenerating is idempotent");
const changed = entries(md.replace("Money code", "Money code").replace("Piper", "Other"));
assert.notEqual(patchSt(once, changed), once, "a source edit changes the string table");
assert.notEqual(dataFile(list), dataFile(changed) + "x");
assert.ok(dataFile(list).includes('rows.Insert("H|#LF-Credits_1|");'));
console.log("credits-selftest: OK");
