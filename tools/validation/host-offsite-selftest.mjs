// tools/validation/host-offsite-selftest.mjs
// inputs: tools/host/offsite.mjs, tools/host/commands/offsite.mjs, tools/host/rotate.mjs
//
// Proves `tools\cli host offsite <dir>` copies every host backup not already in a
// second folder, verifies each copy by size and hash, replaces a corrupted copy,
// never deletes from the source, and applies the host rotation (plain backups beyond
// --keep, tagged ones always kept) to the target. Fixture: two temp folders.
// red-proof: before tools/host/offsite.mjs existed the import failed:
// "FAIL offsite.mjs missing (Cannot find module ...)" (2026-11-15).

import { existsSync, mkdirSync, mkdtempSync, readFileSync, readdirSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const tmp = mkdtempSync(join(tmpdir(), "lf-host-offsite-"));
let failed = 0;
function fail(msg) {
  console.log(`FAIL  ${msg}`);
  failed++;
}

function put(backups, name, files) {
  for (const [rel, body] of Object.entries(files)) {
    const f = join(backups, name, rel);
    mkdirSync(dirname(f), { recursive: true });
    writeFileSync(f, body);
  }
}

try {
  let off, cmd;
  try {
    off = await import(pathToFileURL(join(ROOT, "tools", "host", "offsite.mjs")).href);
    cmd = await import(pathToFileURL(join(ROOT, "tools", "host", "commands", "offsite.mjs")).href);
  } catch (e) {
    fail(`offsite.mjs missing (${e.message})`);
    process.exit(1);
  }

  const hostDir = join(tmp, "host");
  const src = join(hostDir, "backups");
  const dst = join(tmp, "second-drive");
  const A = "2026-11-01_06-00-00-start";
  const B = "2026-11-01_18-00-00-stop";
  const C = "2026-11-02_06-00-00-wipe"; // tagged
  put(src, A, { "meta.json": "{\"v\":1}", "save/data.bin": "aaaaaaaa" });
  put(src, B, { "meta.json": "{\"v\":2}", "save/data.bin": "bbbbbbbb" });
  put(src, C, { "meta.json": "{\"v\":3}", "save/data.bin": "cccccccc" });
  mkdirSync(join(src, "notes"), { recursive: true }); // unknown folder: not a backup, not copied

  // dry run lists, writes nothing (not even the target folder)
  const dry = off.offsiteBackups(src, dst, { dryRun: true });
  if (dry.copied.length !== 3) fail(`dry run would copy ${dry.copied.length}, want 3`);
  if (existsSync(dst)) fail("dry run created the target");

  // first run copies all three
  const r1 = off.offsiteBackups(src, dst);
  if (r1.copied.length !== 3) fail(`first run copied ${r1.copied.length}, want 3`);
  for (const n of [A, B, C]) {
    if (!existsSync(join(dst, n, "save", "data.bin"))) fail(`${n} not copied`);
  }
  if (existsSync(join(dst, "notes"))) fail("unknown folder was copied");
  if (readFileSync(join(dst, B, "save", "data.bin"), "utf8") !== "bbbbbbbb") fail("copy content differs");

  // second run copies nothing
  const r2 = off.offsiteBackups(src, dst);
  if (r2.copied.length !== 0 || r2.replaced.length !== 0) fail(`second run copied ${r2.copied.length}, replaced ${r2.replaced.length}`);
  if (r2.skipped.length !== 3) fail(`second run skipped ${r2.skipped.length}, want 3`);

  // corrupted copy (same size, different bytes) is replaced; a truncated one too
  writeFileSync(join(dst, A, "save", "data.bin"), "aaaaaaaX");
  writeFileSync(join(dst, B, "save", "data.bin"), "bb");
  const dry2 = off.offsiteBackups(src, dst, { dryRun: true });
  if (dry2.replaced.length !== 2) fail(`dry run saw ${dry2.replaced.length} corrupt copies, want 2`);
  if (readFileSync(join(dst, A, "save", "data.bin"), "utf8") !== "aaaaaaaX") fail("dry run repaired a copy");
  const r3 = off.offsiteBackups(src, dst);
  if (r3.replaced.length !== 2 || r3.copied.length !== 0) fail(`third run replaced ${r3.replaced.length}, copied ${r3.copied.length}`);
  if (readFileSync(join(dst, A, "save", "data.bin"), "utf8") !== "aaaaaaaa") fail("corrupt same-size copy not replaced");
  if (readFileSync(join(dst, B, "save", "data.bin"), "utf8") !== "bbbbbbbb") fail("truncated copy not replaced");

  // an extra file in a copy counts as a mismatch
  writeFileSync(join(dst, C, "stray.txt"), "x");
  if (off.offsiteBackups(src, dst).replaced.length !== 1) fail("copy with a stray file not replaced");
  if (existsSync(join(dst, C, "stray.txt"))) fail("stray file survived the replace");

  // no temp folders left behind, source untouched
  if (readdirSync(dst).some((n) => n.endsWith(".partial"))) fail("partial folder left in target");
  if (readdirSync(src).length !== 4) fail("source folder changed");
  if (readFileSync(join(src, A, "save", "data.bin"), "utf8") !== "aaaaaaaa") fail("source file changed");

  // rotation on the target: plain beyond keep are not copied; tagged always are
  const D = "2026-11-03_06-00-00-start";
  const E = "2026-11-03_18-00-00-stop";
  put(src, D, { "meta.json": "d" });
  put(src, E, { "meta.json": "e" });
  const r4 = off.offsiteBackups(src, dst, { keep: 2 });
  if (!existsSync(join(dst, D)) || !existsSync(join(dst, E))) fail("newest plain backups missing from target");
  if (existsSync(join(dst, A)) || existsSync(join(dst, B))) fail("target kept plain backups beyond --keep");
  if (!existsSync(join(dst, C))) fail("target rotation deleted a tagged backup");
  if (r4.pruned.length !== 2) fail(`pruned ${r4.pruned.length}, want 2`);
  // the source keeps everything, and the next run does not copy the pruned ones back
  if (![A, B, C, D, E].every((n) => existsSync(join(src, n)))) fail("source lost a backup");
  const r5 = off.offsiteBackups(src, dst, { keep: 2 });
  if (r5.copied.length !== 0 || r5.pruned.length !== 0) fail(`rotation not stable: copied ${r5.copied.length}, pruned ${r5.pruned.length}`);

  // refuses a target that is the source, inside it, or missing from the command line
  for (const bad of [src, join(src, "inner")]) {
    let threw = false;
    try { off.offsiteBackups(src, bad); } catch { threw = true; }
    if (!threw) fail(`target ${bad} accepted`);
  }
  let threw = false;
  try { off.offsiteBackups(join(tmp, "nope"), dst); } catch { threw = true; }
  if (threw) fail("missing source folder is an error, want an empty run");

  // command wrapper
  if (!cmd.help || typeof cmd.main !== "function") fail("offsite.mjs command lacks help/main");
  if ((await cmd.main([], { root: ROOT, hostDir })) === 0) fail("offsite with no folder returned 0");
  if ((await cmd.main([src], { root: ROOT, hostDir })) === 0) fail("offsite onto the source returned 0");
  if ((await cmd.main([dst, "--keep", "x"], { root: ROOT, hostDir })) === 0) fail("offsite --keep x returned 0");
  if ((await cmd.main([join(tmp, "third"), "--dry-run"], { root: ROOT, hostDir })) !== 0) fail("offsite --dry-run failed");
  if (existsSync(join(tmp, "third"))) fail("command --dry-run created the target");
  if ((await cmd.main([join(tmp, "third")], { root: ROOT, hostDir })) !== 0) fail("offsite run failed");
  if (!existsSync(join(tmp, "third", C, "meta.json"))) fail("command did not copy");
} finally {
  rmSync(tmp, { recursive: true, force: true });
}

console.log(failed ? `SUMMARY host-offsite-selftest fail ${failed}` : "SUMMARY host-offsite-selftest ok");
process.exit(failed ? 1 : 0);
