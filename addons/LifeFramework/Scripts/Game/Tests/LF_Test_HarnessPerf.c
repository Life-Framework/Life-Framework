// red-proof: made LF_HarnessPerf.Summary divide by `count + 1` and ran
// `tools\cli test --no-build --only LF_Test_HarnessPerf` (2026-11-12). Observed:
//   [LFTEST]   - summary mean: label=boards_1 frames=100 meanMs=50 p50Ms=50 p95Ms=95 p99Ms=99 maxMs=100 fps=20 memKB=1000->1250 (250)
//   [LFTEST] SUMMARY tier=only passed=0 failed=1 total=1
// Restored the mean and the test passed (10 assertions).

// tier: LOGIC
//! The `perf` harness verb's summary: the percentiles and fps a board measurement is judged on
//! (docs/architecture/visual-boards-budget.md). Nearest-rank percentiles of the frame times, the
//! mean, and the memory delta, from a fixed set of frames.
class LF_Test_HarnessPerf : LF_Test
{
	override string GetName()
	{
		return "harness/perf-summary";
	}

	override void Run(LF_TestContext ctx)
	{
		// 100 frames: 1..100 ms. p50 is 50, p95 is 95, p99 is 99, the worst 100, the mean 50.5.
		array<float> frames = {};
		for (int i = 100; i >= 1; i--)
			frames.Insert(i);

		array<float> sorted = {};
		sorted.Copy(frames);
		sorted.Sort();
		ctx.EqualFloat(50, LF_HarnessPerf.Percentile(sorted, 50), 0.001, "p50 of 1..100 is 50");
		ctx.EqualFloat(95, LF_HarnessPerf.Percentile(sorted, 95), 0.001, "p95 of 1..100 is 95");
		ctx.EqualFloat(100, LF_HarnessPerf.Percentile(sorted, 100), 0.001, "p100 is the worst frame");

		string summary = LF_HarnessPerf.Summary(frames, 1000, 1250, "boards_1");
		ctx.True(summary.Contains("label=boards_1 frames=100 "), "summary names the window and counts frames: " + summary);
		ctx.True(summary.Contains("meanMs=50.5 "), "summary mean: " + summary);
		ctx.True(summary.Contains("p95Ms=95 "), "summary p95: " + summary);
		ctx.True(summary.Contains("maxMs=100 "), "summary worst frame: " + summary);
		ctx.True(summary.Contains("memKB=1000->1250 (250)"), "summary memory delta: " + summary);
		ctx.True(frames[0] == 100, "the caller's frames are not sorted in place");

		string empty = LF_HarnessPerf.Summary(null, 5, 5, "none");
		ctx.True(empty.Contains("frames=0"), "no frames is a zero-frame window, not a crash: " + empty);
	}
}
