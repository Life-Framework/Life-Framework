// tools/lib/money-format.mjs - find player-facing money text built by hand in script (#money-format).
//
// Every amount a player reads goes through LF_MoneyFormat.FormatMoney (the HUD's formatter: "$1,040", "-$150", the
// pack's currency symbol). A site that glues "$" on itself ("$" + amount, string.Format("$%1", amount)) shows
// "$1040" with no comma, in the wrong symbol on a pack that renames the currency, and with a minus in the wrong
// place for a debt. The crane toast read "$1040" beside the HUD's "+$1,040" in playtest round 4 (2026-11-27).
//
// Flagged (outside comments):
//   "$" + x  /  x + "$"            a bare dollar literal being concatenated
//   "...$%1..."                    a dollar directly before a %N placeholder in a string literal
//   "...{currency:sym}%1..."       the localization row's pattern, hand-written in script
// Not flagged: log lines (LF_Debug.*, Print*), Tests/ (scanned by neither the validator nor the pack), and the
// formatter itself (LF_MoneyFormat.c). A real exception is marked `// lf-money-format-ok: <why>` on its line or the
// line above.
import { readFileSync } from "node:fs";

export const MARK = /\/\/\s*lf-money-format-ok:\s*\S/;
const LOG_LINE = /\b(LF_Debug\.\w+|PrintFormat|Print)\s*\(/;

const RULES = [
  { id: "dollar-concat", re: /"\$ ?"\s*\+|\+\s*"\$ ?"/, why: 'a "$" literal glued to an amount' },
  { id: "dollar-format", re: /"(?:[^"\\\n]|\\.)*\$\s?%\d(?:[^"\\\n]|\\.)*"/, why: "a $ placed before a %N placeholder" },
  { id: "currency-format", re: /"(?:[^"\\\n]|\\.)*\{currency:sym\}\s?%\d(?:[^"\\\n]|\\.)*"/, why: "a {currency:sym}%N pattern written in script" },
];

/** The code part of one line: the text before a `//` that is not inside a string. */
export function code(line) {
  let quote = false;
  for (let i = 0; i < line.length; i++) {
    const c = line[i];
    if (c === "\\" && quote) i++;
    else if (c === '"') quote = !quote;
    else if (!quote && c === "/" && line[i + 1] === "/") return line.slice(0, i);
  }
  return line;
}

/** Hand-built money sites in one source: [{ line, rule, why, text }]. Block comments are skipped line by line. */
export function scanSource(src) {
  const lines = src.split("\n");
  const hits = [];
  let inBlock = false;
  lines.forEach((raw, i) => {
    let line = raw;
    if (inBlock) {
      const end = line.indexOf("*/");
      if (end < 0) return;
      inBlock = false;
      line = " ".repeat(end + 2) + line.slice(end + 2);
    }
    line = line.replace(/\/\*.*?\*\//g, (m) => " ".repeat(m.length));
    const open = code(line).indexOf("/*");
    if (open >= 0 && code(line).indexOf("*/", open) < 0) {
      inBlock = true;
      line = line.slice(0, open);
    }
    const c = code(line);
    if (!c.trim() || LOG_LINE.test(c)) return;
    if (MARK.test(raw) || (i > 0 && MARK.test(lines[i - 1]))) return;
    for (const r of RULES) {
      if (r.re.test(c)) {
        hits.push({ line: i + 1, rule: r.id, why: r.why, text: raw.trim() });
        break;
      }
    }
  });
  return hits;
}

export function scanFile(path) {
  return scanSource(readFileSync(path, "utf8"));
}
