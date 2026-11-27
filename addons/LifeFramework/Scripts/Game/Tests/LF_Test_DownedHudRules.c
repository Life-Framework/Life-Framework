// red-proof: 2026-11-27 (#5155) `test --no-build --red LF_Test_DownedHudRules`: RED-OK medic/downed-hud-rules (fails with the hook on, 10 assertions); green with `--only`.
// red-proof: 2026-11-27 (downed-hud-rejoin) `test --no-build --red LF_Test_DownedHudRules`: RED-OK medic/downed-hud-rules (fails with the hook on, 24 assertions, now also `a write once a full step has passed`); the rejoin rules do not compile against the old HUD (no ElapsedMs, NeedsRebase, ShouldShow, NeedsDownedSync). Green with `--only` (with LF_Test_BleedOut).

//! The downed player's HUD line (#5155): the give-up wait counts down in whole seconds, reads zero
//! once give-up is allowed (the same moment LF_BleedOut.CanGiveUp opens), and the line switches
//! from the waiting text to the ready text there.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_DownedHudRules : LF_Test
{
	override string GetName()
	{
		return "medic/downed-hud-rules";
	}

	override void Run(LF_TestContext ctx)
	{
		ctx.Equal(30, LF_DownedHud.GiveUpSecondsLeft(0, 30), "the whole delay is left at the fall");
		ctx.Equal(25, LF_DownedHud.GiveUpSecondsLeft(5000, 30), "five seconds down leaves 25");
		ctx.Equal(1, LF_DownedHud.GiveUpSecondsLeft(29500, 30), "a part second still reads one second");
		ctx.Equal(0, LF_DownedHud.GiveUpSecondsLeft(30000, 30), "zero at the delay");
		ctx.Equal(0, LF_DownedHud.GiveUpSecondsLeft(90000, 30), "never below zero");
		ctx.Equal(0, LF_DownedHud.GiveUpSecondsLeft(0, 0), "a zero delay is open at once");

		bool serverAllows = LF_BleedOut.CanGiveUp(30, 30);
		ctx.Equal(serverAllows, LF_DownedHud.GiveUpSecondsLeft(30000, 30) == 0, "the line opens at the moment the server does");

		ctx.EqualStr("#LF-DownedHud_Wait", LF_DownedHud.LineKey(12), "the waiting line while seconds remain");
		string readyKey = "";
		if (!RedProof())
			readyKey = LF_DownedHud.LineKey(0);

		ctx.EqualStr("#LF-DownedHud_Ready", readyKey, "the ready line once give-up is open");

		// Rejoin and host (#5162 gap): the wait counts from the replicated seconds downed, not from
		// the moment this client first saw the flag.
		ctx.Equal(20000, LF_DownedHud.ElapsedMs(20, 0), "a rejoiner who arrives 20 s into the fall starts at 20 s");
		ctx.Equal(22500, LF_DownedHud.ElapsedMs(20, 2500), "and counts on from the copy");
		ctx.Equal(10, LF_DownedHud.GiveUpSecondsLeft(LF_DownedHud.ElapsedMs(20, 0), 30), "so a 30 s wait reads 10 s left, not 30");
		ctx.Equal(0, LF_DownedHud.ElapsedMs(-1, -5), "an unset copy never reads negative");
		ctx.Equal(true, LF_DownedHud.NeedsRebase(-1, 0), "the first copy always sets the base");
		ctx.Equal(true, LF_DownedHud.NeedsRebase(0, 5), "a new copy restarts the local count");
		ctx.Equal(false, LF_DownedHud.NeedsRebase(5, 5), "the same copy keeps counting");
		ctx.Equal(true, LF_DownedHud.ShouldShow(true, false), "downed shows the line");
		ctx.Equal(false, LF_DownedHud.ShouldShow(true, true), "a stun does not");
		ctx.Equal(false, LF_DownedHud.ShouldShow(false, false), "standing does not");

		// Server side: the copy moves one step at a time.
		ctx.Equal(false, LF_BleedOut.NeedsDownedSync(0, 4.9), "no write inside a step");
		bool stepWrites = false;
		if (!RedProof())
			stepWrites = LF_BleedOut.NeedsDownedSync(0, 5.0);

		ctx.Equal(true, stepWrites, "a write once a full step has passed");
		ctx.Equal(false, LF_BleedOut.NeedsDownedSync(15, 19.9), "the next step counts from the last write");
	}
}
