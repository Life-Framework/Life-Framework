// The example server template (server/example/, #295) stays a ready folder: server.json is in the
// engine's schema shape and boots the core then the pack, config.json is the flagship preset and
// passes config-check, the start scripts and the README say what a host needs. No game needed.
// inputs: server/example/**, tools/server-template/**, tools/release/example-zip*
// Engine half (the dedicated server reads server.json itself): tools\cli server-template check --engine
import { templateProblems } from "../server-template/template.mjs";
import { idsProblems, loadIds } from "../release/example-zip.mjs";

// The release download's id file (#296) stays in step with the addons; a placeholder is allowed until #370.
const fails = [...templateProblems(), ...idsProblems(loadIds())];
for (const f of fails) console.log(`FAIL server-template: ${f}`);
if (fails.length) process.exit(1);
console.log("OK server-template: server/example is a ready folder");
