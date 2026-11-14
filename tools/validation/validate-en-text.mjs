// Non-English text must not leak into English strings (#609).
// inputs: addons/**
//
// The safe action read "Robar Dinero" because the DebugWorld layer carried the
// Spanish text as a literal UIInfo Name. This check scans two places:
//   1. every Target_en_us in the string table;
//   2. the literal Name / Description text of layers and prefabs (anything not a
//      #Key), which is what the player sees when it is not a localization key.
// A line is flagged when it holds a Spanish-only character (n-tilde, inverted
// marks, accented a/i/o/u) or a whole word from a short Spanish list.
//
// red-proof: 2026-11-10, putting `Name "Robar Dinero"` back in Crimes.layer printed
// `FAIL ...Crimes.layer:31 ... "Robar Dinero"` and exited 1; fixed, it prints OK.
import { execFileSync } from "node:child_process";
import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const CHARS = /[ñÑ¿¡áíóúÁÍÓÚ]/;
const WORDS = /\b(robar|robo|dinero|armas|armer[ií]a|veh[ií]culo|recolectar|manzanas|jugador|el|los|las|del|para|una|por|abrir|hablar|dar|quitar)\b/i;

function suspect(text) {
  if (CHARS.test(text)) return true;
  // An all-caps entry (an acronym such as DEL) is not a Spanish word.
  if (text === text.toUpperCase()) return false;
  return WORDS.test(text);
}

const bad = [];
const files = execFileSync("git", ["ls-files", "-z", "--cached", "--others", "--exclude-standard",
  "addons/*.layer", "addons/*.et", "addons/*.st"], { cwd: root, encoding: "utf8", maxBuffer: 1 << 28 })
  .split("\0").filter(Boolean);

for (const f of files) {
  let text;
  try { text = readFileSync(join(root, f), "utf8"); } catch { continue; }
  const lines = text.split("\n");
  const isSt = f.endsWith(".st");
  for (let i = 0; i < lines.length; i++) {
    let m;
    if (isSt) m = lines[i].match(/^\s*Target_en_us\s+"(.*)"\s*$/);
    else m = lines[i].match(/^\s*(?:Name|Description)\s+"([^"#][^"]*)"\s*$/);
    if (m && suspect(m[1])) bad.push(`${f}:${i + 1} ${JSON.stringify(m[1])}`);
  }
}
for (const b of bad) console.log(`FAIL ${b}: looks Spanish; write English and use a #Key from the localization table`);
if (bad.length) process.exit(1);
console.log("validate-en-text: OK");
