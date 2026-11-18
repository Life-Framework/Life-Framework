// An LF keyboard action never shares a key with a vanilla action that is live at the same time (#3089, #2496).
// inputs: addons/LifeFramework/Configs/System/chimeraInputCommon.conf, tools/validation/fixtures/vanilla-ingame-keys.json
//
// Two actions on one key in contexts that are active together fire both: `U` accepted a group invite and locked the
// car, `F9` voted and stamped a clip. The vanilla list is a checked-in fixture (the CI runner has no Reforger install),
// extracted once from the game's own `Configs/System/chimeraInputCommon.conf`:
//   node tools/validation/validate-input-vanilla-clash.mjs --extract <chimeraInputCommon.conf> [--build <n>]
// prints the fixture JSON for a pasted conf (`tools\cli call game_read '{"path":"Configs/System/chimeraInputCommon.conf"}'`
// strips to the same text). Refresh it when the game build changes.
//
// What counts as a clash: an LF action bound to a bare keyboard key (no modifier combo) in the LF context
// that holds it, against a vanilla action on the same key in a vanilla context that can be live with that LF
// context. Vanilla menu contexts are live together with the gameplay contexts only when listed in
// `liveWith` in the fixture. LF Menu/TextEdit contexts are checked against the menu contexts.
//
// red-proof: tools/validation/validate-input-vanilla-clash-selftest.mjs feeds the 2026-11-18 U and F9 bindings and
// sees both fail; with the new keys it passes.
import { existsSync, readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
export const CONF = "addons/LifeFramework/Configs/System/chimeraInputCommon.conf";
export const FIXTURE = "tools/validation/fixtures/vanilla-ingame-keys.json";

// Parse an ActionManager conf into { actions: {name: [{keys:[...], combo:bool}]}, contexts: {name: [actionName]} }.
export function parseInputConf(text) {
  const actions = {};
  const contexts = {};
  const stack = [];
  let action = null;
  let ctx = null;
  let inRefs = false;
  let comboDepth = 0;
  const lines = text.split("\n");
  for (const raw of lines) {
    const line = raw.trim();
    if (!line) continue;
    if (line === "}") {
      const top = stack.pop();
      if (top === "action") action = null;
      else if (top === "context") ctx = null;
      else if (top === "refs") inRefs = false;
      else if (top === "combo") comboDepth--;
      continue;
    }
    if (line.endsWith("{")) {
      let m;
      if ((m = /^Action (\w+) \{$/.exec(line))) {
        action = m[1];
        actions[action] = actions[action] || [];
        if (ctx) contexts[ctx].push(action);
        stack.push("action");
      } else if ((m = /^ActionContext (\w+) \{$/.exec(line))) {
        ctx = m[1];
        contexts[ctx] = contexts[ctx] || [];
        stack.push("context");
      } else if (/^ActionRefs\b/.test(line)) {
        inRefs = true;
        stack.push("refs");
      } else if (/^InputSourceCombo\b/.test(line)) {
        comboDepth++;
        stack.push("combo");
      } else stack.push("other");
      continue;
    }
    if (inRefs && ctx) {
      for (const m of line.matchAll(/"(\w+)"/g)) contexts[ctx].push(m[1]);
      continue;
    }
    if (action) {
      const m = /^Input "keyboard:(\w+)"/.exec(line);
      if (m) actions[action].push({ key: m[1], combo: comboDepth > 0 });
    }
  }
  return { actions, contexts };
}

// The fixture: per context, per action, the bare keyboard keys that action takes. Combo bindings are listed apart.
export function extractFixture(text, build) {
  const { actions, contexts } = parseInputConf(text);
  const out = {};
  for (const ctx of Object.keys(contexts).sort()) {
    const row = {};
    for (const name of [...new Set(contexts[ctx])].sort()) {
      const keys = [...new Set((actions[name] || []).filter((b) => !b.combo).map((b) => b.key))].sort();
      if (keys.length) row[name] = keys;
    }
    if (Object.keys(row).length) out[ctx] = row;
  }
  return out;
}

// Returns the clash lines for an LF conf text against a fixture object.
export function findClashes(lfText, fixture) {
  const lf = parseInputConf(lfText);
  const liveWith = fixture.liveWith || {};
  const bad = [];
  const lfCtxOf = {};
  for (const [ctx, names] of Object.entries(lf.contexts)) for (const n of names) (lfCtxOf[n] = lfCtxOf[n] || []).push(ctx);
  for (const [name, binds] of Object.entries(lf.actions)) {
    if (!name.startsWith("LF_")) continue;
    for (const b of binds) {
      if (b.combo) continue;
      for (const ctx of lfCtxOf[name] || []) {
        // The vanilla contexts live with this LF context: itself plus its liveWith list.
        // Accepted older clashes (listed in the fixture with a reason, tracked on #2496) pass; a new one fails.
        if ((fixture.knownClashes || []).some((k) => k.lf === name && k.key === b.key)) continue;
        const vanillaCtxs = [...new Set([ctx, ...(liveWith[ctx] || [])])];
        for (const vctx of vanillaCtxs) {
          const row = (fixture.contexts || {})[vctx] || {};
          for (const [vname, vkeys] of Object.entries(row)) {
            if (vname === name || (fixture.workbenchOnly || []).includes(vname)) continue;
            if (vkeys.includes(b.key)) bad.push({ lf: name, key: b.key, lfCtx: ctx, vanilla: vname, vanillaCtx: vctx });
          }
        }
      }
    }
  }
  return bad;
}

export function formatClash(c) {
  return `FAIL ${CONF} ${c.lf} (${c.lfCtx}) uses keyboard:${c.key}, the vanilla action ${c.vanilla} (${c.vanillaCtx}) uses it too`;
}

function main() {
  const args = process.argv.slice(2);
  if (args[0] === "--extract") {
    const text = readFileSync(args[1], "utf8");
    const bi = args.indexOf("--build");
    const fixture = { build: bi >= 0 ? args[bi + 1] : "unknown", contexts: extractFixture(text) };
    console.log(JSON.stringify(fixture, null, 1));
    return;
  }
  if (!existsSync(join(root, FIXTURE))) {
    console.log(`FAIL ${FIXTURE} is missing`);
    process.exit(1);
  }
  const fixture = JSON.parse(readFileSync(join(root, FIXTURE), "utf8"));
  const bad = findClashes(readFileSync(join(root, CONF), "utf8"), fixture);
  if (bad.length) {
    console.log(bad.map(formatClash).join("\n"));
    console.log(`validate-input-vanilla-clash: ${bad.length} clash(es) against vanilla build ${fixture.build}.`);
    process.exit(1);
  }
  console.log(`validate-input-vanilla-clash: OK (vanilla build ${fixture.build})`);
}

if (process.argv[1] && pathToFileURL(process.argv[1]).href === import.meta.url) main();
