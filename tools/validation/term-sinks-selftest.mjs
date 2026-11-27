// tools/validation/term-sinks-selftest.mjs
// inputs: tools/validation/validate-term-sinks.mjs, tools/lib/**
// red-proof for validate-term-sinks.mjs: a raw sink given a placeholder key fails, the routed one and a
// plain key pass; a prefab Name or Description on a placeholder key fails.
import { placeholderKeys, rawSinkHits, prefabHits } from "./validate-term-sinks.mjs";

const table = `
  CustomStringTableItem "{AAAAAAAAAAAAAAAA}" {
   Id "LF-Map_AreaLawless"
   Target_en_us "No {police}"
  }
  CustomStringTableItem "{BBBBBBBBBBBBBBBB}" {
   Id "LF-Map_AreaSafe"
   Target_en_us "Safe zone"
  }
`;

let failed = 0;
function check(name, ok) {
  if (!ok) {
    failed++;
    console.log(`FAIL ${name}`);
  }
}

const keys = placeholderKeys(table);
check("only the placeholder key is found", keys.size === 1 && keys.has("LF-Map_AreaLawless"));
check("a raw Translate on a placeholder key fails", rawSinkHits('label = WidgetManager.Translate("#LF-Map_AreaLawless");', keys).length === 1);
check("a raw reason on a placeholder key fails", rawSinkHits('SetCannotPerformReason("#LF-Map_AreaLawless");', keys).length === 1);
check("the routed call passes", rawSinkHits('label = LF_Terms.Text("#LF-Map_AreaLawless");', keys).length === 0);
check("a plain key passes", rawSinkHits('label = WidgetManager.Translate("#LF-Map_AreaSafe");', keys).length === 0);
check("a placeholder key with no raw sink on the line passes", rawSinkHits('return "#LF-Map_AreaLawless";', keys).length === 0);
check("a prefab Description on a placeholder key fails", prefabHits('     Description "#LF-Map_AreaLawless"', keys).length === 1);
check("a prefab Name on a plain key passes", prefabHits('     Name "#LF-Map_AreaSafe"', keys).length === 0);

if (failed) process.exit(1);
console.log("term-sinks-selftest: OK");
