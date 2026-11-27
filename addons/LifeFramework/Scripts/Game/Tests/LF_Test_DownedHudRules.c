// red-proof: 2026-11-27 (#5155) `test --no-build --red LF_Test_DownedHudRules`: RED-OK medic/downed-hud-rules (fails with the hook on, 10 assertions); green with `--only`.

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
	}
}
