// tools/lib/mcp-register.mjs - wire patched MCP tools into the clone's server.ts.
//
// A tool file under tools/mcp-patches/<server>/src/tools/ whose first lines
// carry `// lf-register: registerX(server, wbClient)` is registered by
// `tools\cli mcp install|update`: the import goes after the last import and
// the call goes right after `const wbClient = ...;` inside registerTools, so
// it can use the Workbench client. Idempotent: a call already in server.ts
// (hand-wired or injected before) is left alone.

/** The register call a patched tool file declares, or "" when it has none. */
export function registerLine(text) {
  const m = text.match(/^\/\/ lf-register: (.+?);?\s*$/m);
  return m ? m[1].trim() : "";
}

/** Add an import and a call for each { file, call } missing from serverTs. */
export function injectRegistrations(serverTs, tools) {
  let out = serverTs;
  const added = [];
  const missing = [];
  for (const { file, call } of tools) {
    const fn = call.slice(0, call.indexOf("(")).trim();
    if (!/^[A-Za-z_$][\w$]*$/.test(fn)) {
      missing.push(`${file}: bad lf-register line`);
      continue;
    }
    const body = out.replace(/^import .*$/gm, "");
    if (new RegExp(`\\b${fn}\\(`).test(body)) continue;
    const anchor = out.match(/^[ \t]*const wbClient = .*;[ \t]*$/m);
    if (!anchor) {
      missing.push(`${file}: no 'const wbClient = ...;' line in server.ts`);
      continue;
    }
    let at = anchor.index + anchor[0].length;
    out = `${out.slice(0, at)}\n  ${call};${out.slice(at)}`;
    const imp = `import { ${fn} } from "./tools/${file.replace(/\.ts$/, ".js")}";`;
    if (!out.includes(imp)) {
      const imports = [...out.matchAll(/^import .*;[ \t]*$/gm)];
      const last = imports[imports.length - 1];
      at = last ? last.index + last[0].length : 0;
      out = `${out.slice(0, at)}\n${imp}${out.slice(at)}`;
    }
    added.push(fn);
  }
  return { text: out, added, missing };
}
