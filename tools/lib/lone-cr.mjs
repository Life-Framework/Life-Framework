// tools/lib/lone-cr.mjs - find carriage returns that are not part of a CRLF pair (validate-no-lone-cr).
//
// A lone CR inside a `//` comment splits the line for any tool that treats CR as a line break
// (an agent rewriting the file, a line-based editor): the rest of the comment becomes bare code and
// the compile breaks. Two files hit this on 2026-11-17 (`\return` written through a shell that
// turned `\r` into a real CR). The repo is LF-only (`* text=auto eol=lf`).
//
// Pure helpers; the validator owns the file walk.

/** 1-based line numbers (counted by LF) holding a CR that is not followed by LF. Buffer or string. */
export function loneCrLines(data) {
  const buf = Buffer.isBuffer(data) ? data : Buffer.from(String(data), "latin1");
  const lines = [];
  let line = 1;
  for (let i = 0; i < buf.length; i++) {
    if (buf[i] === 10) line++;
    else if (buf[i] === 13 && buf[i + 1] !== 10 && lines[lines.length - 1] !== line) lines.push(line);
  }
  return lines;
}

/** True for content a text check should skip: a NUL byte marks a binary file. */
export function looksBinary(buf) {
  return buf.includes(0);
}

/**
 * Path patterns that .gitattributes marks `-text` (binary assets, the CRLF language confs).
 * Handles the forms the repo uses: `*.ext`, `dir/*.ext`, `dir/file`. Returns a predicate on a
 * repo-relative forward-slash path.
 */
export function minusTextMatcher(gitattributes) {
  const rules = [];
  for (const raw of String(gitattributes || "").split("\n")) {
    const line = raw.trim();
    if (!line || line.startsWith("#")) continue;
    const parts = line.split(/\s+/);
    const pattern = parts[0];
    const attrs = parts.slice(1);
    if (!attrs.includes("-text") && !attrs.includes("binary")) continue;
    const anchored = pattern.includes("/");
    const body = pattern
      .replace(/^\//, "")
      .replace(/[.+^${}()|[\]\\]/g, "\\$&")
      .replace(/\*\*/g, "\u0000")
      .replace(/\*/g, "[^/]*")
      .replace(/\?/g, "[^/]")
      .replace(/\u0000/g, ".*");
    rules.push(new RegExp(anchored ? `^${body}$` : `(^|/)${body}$`));
  }
  return (path) => rules.some((r) => r.test(path));
}

/** CRLF is the repo's rule for these (.gitattributes `eol=crlf`). */
export function isCrlfFile(path) {
  return /\.(cmd|bat)$/i.test(path);
}
