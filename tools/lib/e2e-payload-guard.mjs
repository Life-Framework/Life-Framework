// The pass-2 e2e payload guard (#2812). validate-e2e-payload-guard.mjs and its selftest share this.
//
// Pass 2 of the two-boot e2e reloads a save an OLDER build can have written (--legacy-save, the pinned --pre-move
// ref the main guard runs after every merge). A check for a payload added later fails on that save and the guard
// reverts a correct PR (#2480 -> #2483/#2500, #2798 -> #2801). The one way to ask "can this save hold payload X?"
// is LF_E2E.SaveHas / RequiresPayload (Tests/LF_E2E.c), so a change to how payload versions are counted touches
// one place. Rules, over the Enforce Script tests:
//   1. A line comparing BootSaveVersion() with PayloadSinceVersion() by hand fails (use the helper).
//      PayloadSinceVersion itself stays legal for round-trip tests; in LF_Test_E2E*.c it does not.
//   2. LF_E2E.SaveHas / RequiresPayload name a payload RegisterPersistencePayloads lists.
//   3. LF_SaveSections.BootHadSection( outside LF_E2E.c fails: a save section is asked through
//      LF_E2E.SaveHasSection / RequiresSection (#2875).
//   4. Every `Owner.VerifyE2E(` call is guarded: an LF_E2E.Requires*/SaveHas call within the 3 lines above it,
//      or inside the first lines of the callee's own body.

// Blank `//` and `/* */` comments, keep strings (payload names live in them) and newlines.
export function stripComments(text) {
  let out = "";
  let i = 0;
  while (i < text.length) {
    const c = text[i];
    const n = text[i + 1];
    if (c === "/" && n === "/") {
      while (i < text.length && text[i] !== "\n") i++;
    } else if (c === "/" && n === "*") {
      i += 2;
      while (i < text.length && !(text[i] === "*" && text[i + 1] === "/")) {
        if (text[i] === "\n") out += "\n";
        i++;
      }
      i += 2;
    } else if (c === '"') {
      out += c;
      i++;
      while (i < text.length && text[i] !== '"' && text[i] !== "\n") {
        out += text[i];
        if (text[i] === "\\" && i + 1 < text.length) out += text[++i];
        i++;
      }
      out += '"';
      i++;
    } else {
      out += c;
      i++;
    }
  }
  return out;
}

// The payload names RegisterPersistencePayloads lists, in save order.
export function registeredPayloads(serializerText) {
  return [...stripComments(serializerText).matchAll(/\border\.Insert\(\s*"([^"]+)"\s*\)/g)].map((m) => m[1]);
}

const GUARD = /\bLF_E2E\s*\.\s*(?:Requires\w+|SaveHas)\s*\(/;
const NAMED = /\bLF_E2E\s*\.\s*(?:SaveHas|RequiresPayload)\s*\(\s*"([^"]*)"/g;

// files: [{ path, text }] (Tests/*.c of every addon); payloads: registeredPayloads(...). -> ["path:line message"]
export function guardFindings(files, payloads) {
  const out = [];
  const code = files.map((f) => ({ path: f.path, lines: stripComments(f.text).split("\n") }));
  const known = new Set(payloads);
  const isHelper = (p) => /(^|\/)LF_E2E\.c$/.test(p);
  const isE2eFile = (p) => /(^|\/)LF_Test_E2E[^/]*\.c$/.test(p);

  // The callee bodies: "LF_Test_X" -> first lines after `static void VerifyE2E(`.
  const callee = new Map();
  for (const f of code) {
    const cls = /(?:^|\/)(LF_\w+)\.c$/.exec(f.path)?.[1];
    const at = f.lines.findIndex((l) => /\bstatic\s+void\s+VerifyE2E\s*\(/.test(l));
    if (cls && at >= 0) callee.set(cls, f.lines.slice(at, at + 9).join("\n"));
  }

  for (const f of code) {
    if (isHelper(f.path)) continue;
    f.lines.forEach((line, i) => {
      const where = `${f.path}:${i + 1}`;
      if (/\bBootSaveVersion\s*\(/.test(line) && /\bPayloadSinceVersion\s*\(/.test(line)) out.push(`${where} hand-rolled save-version compare: use LF_E2E.SaveHas("<payload>") or LF_E2E.RequiresPayload("<payload>")`);
      else if (/\bBootHadSection\s*\(/.test(line)) out.push(`${where} hand-rolled BootHadSection check: use LF_E2E.SaveHasSection("<section>") or LF_E2E.RequiresSection("<section>")`);
      else if (isE2eFile(f.path) && /\bPayloadSinceVersion\s*\(/.test(line)) out.push(`${where} PayloadSinceVersion in an e2e pass-2 file: use LF_E2E.SaveHas("<payload>")`);
      for (const m of line.matchAll(NAMED)) {
        if (payloads.length && !known.has(m[1])) out.push(`${where} LF_E2E guard names payload "${m[1]}", which RegisterPersistencePayloads does not list`);
      }
      const call = /\b(LF_\w+)\s*\.\s*VerifyE2E\s*\(/.exec(line);
      if (call) {
        const above = f.lines.slice(Math.max(0, i - 3), i + 1).join("\n");
        if (!GUARD.test(above) && !GUARD.test(callee.get(call[1]) ?? "")) out.push(`${where} ${call[1]}.VerifyE2E is not guarded: wrap it in LF_E2E.RequiresPayload("<payload>") (or have it call LF_E2E.RequiresSeed first); an older save (--pre-move, --legacy-save) never held its data`);
      }
    });
  }
  return out;
}
