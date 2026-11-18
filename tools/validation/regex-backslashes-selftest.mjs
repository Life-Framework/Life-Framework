// tools/validation/regex-backslashes-selftest.mjs
// inputs: tools/validation/validate-regex-backslashes.mjs, tools/validation/regex-backslashes-selftest.mjs, tools/lib/guard-pin.mjs
//
// Proves validate-regex-backslashes flags a regex that lost its backslashes (#2764) and leaves the
// escaped forms alone, and that the main-guard lease host name keeps matching old leases.
// red-proof: with the lone-letter rule in suspicious() (validate-regex-backslashes.mjs) made to
// return null, this printed "FAIL lone s+ literal: expected a hit, got none" (and the same for the
// RegExp string and the ps1 case) and exited 1 (2026-11-18).

import { controlBytes, cookString, scanFile, suspicious } from "./validate-regex-backslashes.mjs";
import { leaseDecision, leaseHostName, legacyLeaseHostName, sameLeaseHost } from "../lib/guard-pin.mjs";

const BS = String.fromCharCode(92);
// Source text is built with BS so this file never depends on how a shell treats a backslash.
const src = (s) => s.replaceAll("@", BS);

let failed = 0;
function fail(label, msg) {
  failed++;
  console.log(`FAIL ${label}: ${msg}`);
}
function flagged(label, file, text) {
  const hits = scanFile(file, text);
  if (!hits.length) fail(label, "expected a hit, got none");
}
function clean(label, file, text) {
  const hits = scanFile(file, text);
  if (hits.length) fail(label, `expected no hit, got ${hits.map((h) => `${h.line}: ${h.why}`).join(" | ")}`);
}

// ---- lost backslashes are flagged
flagged("lone s+ literal", "a.mjs", src('const HOST = name.replace(/s+/g, "-");'));
flagged("lone d+ literal", "a.mjs", src("const n = text.match(/^d+$/);"));
flagged("lone w+ literal", "a.mjs", "const m = /w+:/.exec(x);");
flagged("SCRIPTs*(E) literal", "a.mjs", "const COMPILE = /does not compile|SCRIPTs*(E)/i;");
flagged("lone w{2}", "a.mjs", "const ok = /^w{2}$/.test(x);");
flagged("RegExp string", "a.mjs", 'const re = new RegExp("s+" + word);');
flagged("RegExp template that ate its backslash", "a.mjs", "const re = new RegExp(`s+${fn}`);");
flagged("ps1 -replace", "a.ps1", "$name = $raw -replace 's+', '-'");
flagged("ps1 -match double quotes", "a.ps1", 'if ($line -match "SCRIPTs*(E)") { }');
flagged("control byte", "a.mjs", "const re = /LF_ConfigList" + String.fromCharCode(8) + "/;");

// ---- escaped forms, classes, words and marked lines are not
clean("escaped literal", "a.mjs", src('const HOST = name.replace(/@s+/g, "-");'));
clean("SCRIPT escaped", "a.mjs", src("const C = /SCRIPT@s*@(E@)/i;"));
clean("RegExp string escaped", "a.mjs", src('const re = new RegExp("@@s+" + word);'));
clean("RegExp String.raw", "a.mjs", src("const re = new RegExp(String.raw`void@s+${fn}@s*@(`);"));
clean("char class", "a.mjs", "const r = /[s+d*]x/.test(v);");
clean("letters in a word", "a.mjs", "const r = /^(items|bus+|ids)$/.test(v);");
clean("division and strings", "a.mjs", 'const a = s+1; const b = d*2; const c = "s+ d* w+"; // /s+/ in a comment');
clean("marked line", "a.mjs", "const r = /s+/g; // lf-regex-ok: the letter s, collapsed on purpose");
clean("ps1 escaped", "a.ps1", src("$name = $raw -replace '@s+', '-'"));

// ---- helpers
if (cookString(src("@@s+")) !== src("@s+")) fail("cookString", "a doubled backslash is one backslash");
if (cookString(src("@s+")) !== "s+") fail("cookString", "an unknown escape drops its backslash");
if (suspicious(src("@s+")) !== null) fail("suspicious", "an escaped \\s+ is fine");
if (!suspicious("s+")) fail("suspicious", "a bare s+ is a hit");
if (controlBytes("ok\nbad" + String.fromCharCode(12) + "\n").length !== 1) fail("controlBytes", "form feed on line 2");
if (controlBytes("tab\there\r\n").length !== 0) fail("controlBytes", "tab and CR are fine");

// ---- main-guard lease host name (#2764): the real name now, old leases still ours
if (leaseHostName("lf-testbox") !== "lf-testbox") fail("lease host", "the host name keeps its letter s");
if (leaseHostName("my pc  1") !== "my-pc-1") fail("lease host", "whitespace runs become one dash");
if (legacyLeaseHostName("lf-testbox") !== "lf-te-tbox") fail("lease host", "the legacy spelling is what an old guard wrote");
if (!sameLeaseHost("lf-te-tbox", "lf-testbox") || !sameLeaseHost("lf-testbox", "lf-testbox")) fail("lease host", "a lease in either spelling is ours");
if (sameLeaseHost("other-box", "lf-testbox")) fail("lease host", "another machine is not ours");
const held = (host) => ({ state: "held", host, pid: 1, at: new Date().toISOString() });
if (leaseDecision(held("lf-te-tbox"), "lf-testbox").why !== "ours") fail("lease decision", "an old-spelling lease of this machine is ours, not another guard's");
if (leaseDecision(held("other-box"), "lf-testbox").ok) fail("lease decision", "a fresh lease of another host blocks");

if (failed) {
  console.log(`regex-backslashes selftest: ${failed} case(s) failed`);
  process.exit(1);
}
console.log("regex-backslashes selftest: ok");
