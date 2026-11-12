// Owner config schema check (#280). Regenerates docs/config/config.schema.json
// and config.example.json from the config classes' [Attribute] declarations and
// fails when either committed file differs; validates the example, and the
// example LF_ConfigService writes at runtime, against the schema.
// Also fails when an attribute's params range and the limits the apply step passes to ReadInt disagree.
// Fix: node tools/config/schema.mjs --write
import { existsSync, readFileSync } from "node:fs";
import { build, render, rangeProblems, readerProblems, readScripts, validateJson, runtimeExample, SCHEMA_PATH, EXAMPLE_PATH, SERVICE } from "../config/schema.mjs";

const fails = [];
let gen;
try { gen = build(); } catch (e) { console.log(`FAIL cannot generate the schema: ${e.message}`); process.exit(1); }

for (const [p, obj, name] of [[SCHEMA_PATH, gen.schema, "config.schema.json"], [EXAMPLE_PATH, gen.example, "config.example.json"]]) {
  if (!existsSync(p)) fails.push(`docs/config/${name} is missing`);
  else if (readFileSync(p, "utf8") !== render(obj)) fails.push(`docs/config/${name} is stale against the config classes`);
}

for (const m of rangeProblems(readScripts())) fails.push(`range: ${m}`);
for (const m of readerProblems(readScripts(), gen.schema)) fails.push(`reader: ${m}`);

if (existsSync(EXAMPLE_PATH) && existsSync(SCHEMA_PATH)) {
  const schema = JSON.parse(readFileSync(SCHEMA_PATH, "utf8"));
  for (const e of validateJson(JSON.parse(readFileSync(EXAMPLE_PATH, "utf8")), schema)) fails.push(`config.example.json ${e}`);
  const rt = runtimeExample(readFileSync(SERVICE, "utf8"));
  if (!rt) console.log("WARN config-schema: could not rebuild the example LF_ConfigService.WriteExample writes; runtime example not checked");
  else {
    for (const e of validateJson(rt, schema)) fails.push(`runtime example (LF_ConfigService.WriteExample) ${e}`);
    if (JSON.stringify(rt) !== JSON.stringify(gen.example)) {
      console.log("WARN config-schema: the runtime example differs from docs/config/config.example.json; align WriteExample or tools/config/schema-overrides.json");
    }
  }
}

for (const f of fails) console.log(`FAIL ${f}`);
if (fails.length) {
  console.log("config-schema: run `node tools/config/schema.mjs --write` and commit docs/config/.");
  process.exit(1);
}
console.log("config-schema: OK (schema and example match the config classes)");
