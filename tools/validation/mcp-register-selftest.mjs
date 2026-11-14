// tools/validation/mcp-register-selftest.mjs
// inputs: tools/lib/mcp-register.mjs, tools/mcp-patches/**
//
// Proves `tools\cli mcp install|update` wires a patched tool into the clone's
// server.ts exactly once: import after the last import, call after the
// wbClient line, nothing added when the call is already there. Also checks
// that every patched tool's lf-register line names a function its file exports.
// red-proof: with the "already present" check removed from injectRegistrations
// this printed "FAIL second pass is a no-op" and exited 1 (2026-11-14).

import { readdirSync, readFileSync, existsSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { injectRegistrations, registerLine } from "../lib/mcp-register.mjs";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
let failed = 0;
function check(name, ok, detail = "") {
  if (!ok) {
    failed++;
    console.log(`FAIL ${name}${detail ? `: ${detail}` : ""}`);
  }
}

const SERVER = `import a from "x";
import { registerWbState } from "./tools/wb-state.js";

export function registerTools(server, config) {
  registerApiSearch(server);
  const wbClient = new WorkbenchClient(config.workbenchHost, config.workbenchPort, config);
  registerWbState(server, wbClient);
}
`;
const tools = [{ file: "wb-eval.ts", call: "registerWbEval(server, wbClient)" }];

const one = injectRegistrations(SERVER, tools);
check("adds the tool", one.added.length === 1 && one.added[0] === "registerWbEval", JSON.stringify(one.added));
check("import after the last import", one.text.includes(`import { registerWbState } from "./tools/wb-state.js";\nimport { registerWbEval } from "./tools/wb-eval.js";`), one.text);
check("call after wbClient", /const wbClient = .*;\n {2}registerWbEval\(server, wbClient\);/.test(one.text), one.text);

const two = injectRegistrations(one.text, tools);
check("second pass is a no-op", two.added.length === 0 && two.text === one.text, two.text);

const hand = injectRegistrations(SERVER, [{ file: "wb-state.ts", call: "registerWbState(server, wbClient)" }]);
check("hand-wired call left alone", hand.added.length === 0 && hand.text === SERVER);

const noAnchor = injectRegistrations("import a from 'x';\nexport function registerTools() {}\n", tools);
check("no wbClient line reports, never guesses", noAnchor.added.length === 0 && noAnchor.missing.length === 1);

check("registerLine reads the marker", registerLine("// lf-register: registerWbEval(server, wbClient)\nimport x;\n") === "registerWbEval(server, wbClient)");
check("registerLine empty without marker", registerLine("import x;\n") === "");

// Every real patched tool: the marker names an exported function.
const patches = join(ROOT, "tools", "mcp-patches");
if (existsSync(patches)) {
  for (const server of readdirSync(patches)) {
    const dir = join(patches, server, "src", "tools");
    if (!existsSync(dir)) continue;
    for (const f of readdirSync(dir)) {
      if (!f.endsWith(".ts")) continue;
      const text = readFileSync(join(dir, f), "utf8");
      const call = registerLine(text);
      if (!call) continue;
      const fn = call.slice(0, call.indexOf("("));
      check(`${server}/${f} exports ${fn}`, new RegExp(`export function ${fn}\\(`).test(text));
    }
  }
}

if (failed) process.exit(1);
console.log("OK   mcp-register-selftest");
