// tools/lib/rdb-stale-verdict.mjs - what a test run on this PC does when its resourceDatabase.rdb is still
// stale after the refresh (Workbench queue timed out, the refresh failed).
//
// Before this the run went ahead silently: on 2026-11-17 lf-testbox logged 14 failed refreshes in 4 h
// (12 "no free Workbench slot within N s"), 36 runs started on a stale rdb, and 8 of them printed a green
// SUMMARY (slice-2607's suite ran without the pack's rdb after 670 s in the queue). A server on a stale
// rdb cannot resolve the unregistered prefabs, so a WORLD test that needs them may pass vacuously.
//
// The rule: a whole-suite run (no --only, tier other than fast) on a stale rdb FAILS with the reason; a
// fast or --only run keeps its exit code but prints a SUMMARY warning; neither is stored in the pass
// cache. LF_ALLOW_STALE_RDB=1 turns the failure into the warning.

// Pure: { fail, warn, line } for a finished local run. `stale` is the addon names still stale.
export function staleRdbVerdict({ stale = [], code = 0, tier = "all", only = [], env = process.env } = {}) {
  if (!stale.length) return { fail: false, warn: false, line: "" };
  const what = stale.join(",");
  const whole = only.length === 0 && tier !== "fast";
  if (code === 0 && whole && env.LF_ALLOW_STALE_RDB !== "1") {
    return {
      fail: true,
      warn: false,
      line: `SUMMARY test fail rdb-stale=${what}: the suite passed on a stale resource database (the refresh failed or timed out), so tests that use unregistered resources proved nothing. Rerun when 'tools\\cli pool status' shows a free Workbench slot (LF_ALLOW_STALE_RDB=1 accepts it)`,
    };
  }
  return {
    fail: false,
    warn: true,
    line: `SUMMARY test warn rdb-stale=${what}: this run used a stale resource database (refresh failed or timed out); a WORLD test touching unregistered resources may pass vacuously. Not cached.`,
  };
}
