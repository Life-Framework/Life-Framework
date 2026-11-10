// No UTF-8 byte-order mark in committed .json, .md or .c files.
//
// A BOM breaks JSON.parse (the website build failed on gallery.json) and shows as junk
// in diffs. Writers: Node `writeFileSync` adds none; in PowerShell use
// `Set-Content -Encoding utf8NoBOM` (pwsh 7) or Node, never `-Encoding utf8` on Windows
// PowerShell 5.1 or `Out-File`/`>`.
//
// red-proof: 2026-11-10, prepending EF BB BF to website/content/gallery.json printed
// `FAIL website/content/gallery.json: UTF-8 BOM` and exited 1; restored, it prints OK.
import { execFileSync } from "node:child_process";
import { closeSync, openSync, readSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const files = execFileSync("git", ["ls-files", "-z", "--cached", "--others", "--exclude-standard", "*.json", "*.md", "*.c"], {
  cwd: root,
  encoding: "utf8",
  maxBuffer: 1 << 28,
}).split("\0").filter(Boolean);

const bad = [];
for (const f of files) {
  if (f.includes("node_modules/")) continue;
  let fd;
  try {
    fd = openSync(join(root, f), "r");
  } catch {
    continue; // deleted in the working tree
  }
  const buf = Buffer.alloc(3);
  const n = readSync(fd, buf, 0, 3, 0);
  closeSync(fd);
  if (n === 3 && buf[0] === 0xef && buf[1] === 0xbb && buf[2] === 0xbf) bad.push(f);
}
for (const f of bad) console.log(`FAIL ${f}: UTF-8 BOM (strip it; write with Node or PowerShell -Encoding utf8NoBOM)`);
if (bad.length) process.exit(1);
console.log("validate-no-bom: OK");
