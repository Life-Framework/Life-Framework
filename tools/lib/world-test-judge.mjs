// A failing test never blocks a maintainer world edit (docs/worktrees.md "Maintainer world edits").
//
// `world ship` and the T1w gate run the tests that name a touched layer or entity. On a
// town-maintainer-* branch a test that FAILS is a warning listed in the PR body under
// `## Test warnings (maintainer layout)`: the maintainer moved things on purpose, so a test that pins
// the old layout is the test's problem. Only a run that never judged the world blocks: a script
// compile error, or no `SUMMARY tier=... failed=N` line at all (timeout, crash, environment).
// The other blockers (layer parse error, unknown or duplicate GUID, validate-world-zones) are
// validate --world and world ship's own diff, not here.

const COMPILE = /Can't compile|scripts do not compile|SCRIPT\s+\(E\):\s*Compile/i;
const SUMMARY = /SUMMARY tier=[\w-]+ passed=(\d+) failed=(\d+) total=(\d+)/g;
const FAIL = /\[LFTEST\]\s+FAIL\s+(\S+)(.*)$/;
const ASSERT = /\[LFTEST\]\s+-\s+(.+?)\s*$/;

// -> { blocks: string|null, warnings: [{ test, assertion }], failed: number }
//   text      the transcript of the test run, as printed
//   status    its exit code
//   fallbackTests  the class names the run was asked for, used when a failing assertion line has no FAIL header above it
//   tests     [{ file, text }] test sources: names the test of an assertion line that has no FAIL header (the compact transcript drops it)
export function judgeWorldTests({ status = 0, text = "", fallbackTests = [], tests = [] } = {}) {
  const t = String(text);
  if (status === 0) return { blocks: null, warnings: [], failed: 0 };
  if (COMPILE.test(t)) return { blocks: "a script does not compile", warnings: [], failed: 0 };
  // A sharded or two-pass run prints one SUMMARY per pass (tier=all, tier=all-core): any pass with a failure counts;
  // the largest count stands (a remote run prints each line twice).
  const sums = [...t.matchAll(SUMMARY)];
  if (!sums.length) return { blocks: "the test run printed no SUMMARY (timeout, crash or environment failure), so it judged nothing", warnings: [], failed: 0 };
  const failed = Math.max(...sums.map((s) => Number(s[2])));
  if (failed === 0) return { blocks: `the test run exited ${status} with no failing test`, warnings: [], failed: 0 };

  const warnings = [];
  const seen = new Set();
  let test = null;
  for (const line of t.split(/\r?\n/)) {
    const f = FAIL.exec(line);
    if (f) {
      test = f[1];
      const note = f[2].trim();
      if (note) push(warnings, seen, test, note);
      continue;
    }
    const a = ASSERT.exec(line);
    if (a) push(warnings, seen, test || ownerOf(a[1], tests) || (fallbackTests.length ? fallbackTests.join(",") : "unknown test (see the test log)"), a[1]);
  }
  if (!warnings.length) warnings.push({ test: fallbackTests.join(",") || "unknown test", assertion: `${failed} test(s) failed; no assertion text in the transcript (see the test log)` });
  return { blocks: null, warnings, failed };
}

// The test class whose source holds this assertion label: `ctx.True(x, "label")`, with the
// ` (expected 250, got 0)` tail the context appends to an Equal failure cut off. Null when none or several do.
export function ownerOf(assertion, tests) {
  const label = assertion.replace(/\s*\((expected|got)\b.*$/, "").trim();
  if (!label) return null;
  for (const probe of [`"${label}"`, `"${label.split(": ")[0]}: `]) {
    const hits = tests.filter((t) => t.text.includes(probe));
    if (hits.length === 1) return hits[0].file.replace(/\.c$/, "");
  }
  return null;
}

function push(warnings, seen, test, assertion) {
  const key = `${test}\u0000${assertion}`;
  if (seen.has(key)) return; // the remote run prints the key lines twice
  seen.add(key);
  warnings.push({ test, assertion });
}

// The PR body lines for the warnings: `- <test>: <assertion>`.
export function testWarningLines(warnings) {
  return warnings.map((w) => `\`${w.test}\`: ${w.assertion}`);
}

export const TEST_WARNINGS_HEADING = "## Test warnings (maintainer layout)";
