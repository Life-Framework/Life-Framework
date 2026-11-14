// Proves the perf report parser and the over-budget rule on fixture lines. Does not boot the game.
// inputs: tools/**, agent-box/**, .githooks/**
import { selftestPerf } from "../perf/perf-report.mjs";

const failures = selftestPerf();
if (failures.length) {
  console.log("perf-report-selftest: FAIL");
  for (const f of failures) console.log(`  ${f}`);
  process.exit(1);
}
console.log("perf-report-selftest: OK (BENCH lines, counters and the over-budget flag parse)");
