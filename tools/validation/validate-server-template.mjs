// The example server template (server/example/, #295) stays a ready folder: server.json is in the
// engine's schema shape and boots the core then the pack, config.json is the flagship preset and
// passes config-check, the start scripts and the README say what a host needs. No game needed.
// inputs: server/example/**, tools/server-template/**
// Engine half (the dedicated server reads server.json itself): tools\cli server-template check --engine
import { templateProblems } from "../server-template/template.mjs";

const fails = templateProblems();
for (const f of fails) console.log(`FAIL server-template: ${f}`);
if (fails.length) process.exit(1);
console.log("OK server-template: server/example is a ready folder");
