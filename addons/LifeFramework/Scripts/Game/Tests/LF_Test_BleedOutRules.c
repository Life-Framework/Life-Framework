// red-proof: observed 2026-11-09 with `tools\cli test --no-build --only LF_Test_BleedOutRules`: LF_BleedOut.IsExpired using `left < 0` failed `zero is expiry`; restored, green.

// tier: LOGIC
//! The pure rules of the bleed-out timer (LF_BleedOut): the clock never goes below zero, zero is
//! expiry, giving up waits for the configured delay, and the replicated copy only moves in
//! steps (no per-tick bump).
class LF_Test_BleedOutRules : LF_Test
{
	override string GetName()
	{
		return "medic/bleed-out-rules";
	}

	override void Run(LF_TestContext ctx)
	{
		ctx.EqualFloat(179, LF_BleedOut.Elapse(180, 1), 0.001, "a second off the clock");
		ctx.EqualFloat(0, LF_BleedOut.Elapse(0.5, 3), 0.001, "the clock stops at zero");
		ctx.False(LF_BleedOut.IsExpired(0.1), "time left is not expiry");
		ctx.True(LF_BleedOut.IsExpired(0), "zero is expiry");

		ctx.False(LF_BleedOut.CanGiveUp(29.9, 30), "give-up is refused before the delay");
		ctx.True(LF_BleedOut.CanGiveUp(30, 30), "give-up is allowed at the delay");
		ctx.True(LF_BleedOut.CanGiveUp(0, 0), "a zero delay allows give-up at once");

		ctx.False(LF_BleedOut.NeedsSync(180, 179), "one second of drift does not bump the replicated copy");
		ctx.False(LF_BleedOut.NeedsSync(180, 176.5), "four seconds of drift does not bump it");
		ctx.True(LF_BleedOut.NeedsSync(180, 175), "five seconds of drift bumps it");
		ctx.True(LF_BleedOut.NeedsSync(3, 0), "the end always reaches the client");
		ctx.False(LF_BleedOut.NeedsSync(0, 0), "an ended clock is not bumped again");
	}
}
