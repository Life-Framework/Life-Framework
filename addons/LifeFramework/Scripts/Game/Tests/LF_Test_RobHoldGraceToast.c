// red-proof: Observed 2026-11-17 with `tools\cli test --no-build --red LF_Test_RobHoldGraceToast`: RED-OK, fails with the hook on (the grace refusal sends no toast); green without it.
// tier: WORLD
//! A held robbery refused by the grace period tells the robber why (#2157, #2075): the grace toast
//! every other refused crime shows, and the grace text as the attendant's conversation caption.
//! Other refusals send no toast.
class LF_Test_RobHoldGraceToast : LF_Test
{
	override string GetName()
	{
		return "crime/rob-hold-grace-toast";
	}

	override void Run(LF_TestContext ctx)
	{
		string sent = "";
		if (!RedProof())
			sent = LF_RobHold.NoteRefusal(null, LF_RobAction.REFUSED_GRACE);

		ctx.EqualStr(LF_Grace.REFUSAL_KEY, sent, "a grace refusal returns the grace toast key");
		ctx.EqualStr(LF_Grace.REFUSAL_KEY, LF_RobHold.LastRefusalNotice(), "the grace toast was sent to the robber");

		ctx.EqualStr("", LF_RobHold.NoteRefusal(null, LF_RobAction.REFUSED_COOLDOWN), "a cooldown refusal sends no grace toast");
		ctx.EqualStr("", LF_RobHold.LastRefusalNotice(), "the last notice is cleared by a refusal with no toast");
		ctx.EqualStr("", LF_RobHold.NoteRefusal(null, LF_RobHold.REFUSED_WEAPON), "a no-weapon refusal sends no grace toast");

		ctx.EqualStr(LF_Grace.REFUSAL_KEY, LF_RobHoldRules.RefusalKey(LF_RobAction.REFUSED_GRACE), "the attendant's caption names the grace");
	}
}
