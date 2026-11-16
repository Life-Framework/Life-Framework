// Finds LF_StaticReset.ResetAll() calls in Enforce Script test files (#2093).
// validate-no-test-resetall.mjs and its selftest share this.

// Blank out comments and string literals (keeping newlines) so only real code is scanned.
export function stripNonCode(text) {
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
      out += '""';
      i++;
      while (i < text.length && text[i] !== '"' && text[i] !== "\n") i += text[i] === "\\" ? 2 : 1;
      i++;
    } else {
      out += c;
      i++;
    }
  }
  return out;
}

// 1-based line numbers of every `LF_StaticReset.ResetAll(` in code (not comments, not strings).
export function resetAllLines(text) {
  const lines = [];
  stripNonCode(text).split("\n").forEach((line, i) => {
    if (/\bLF_StaticReset\s*\.\s*ResetAll\s*\(/.test(line)) lines.push(i + 1);
  });
  return lines;
}
