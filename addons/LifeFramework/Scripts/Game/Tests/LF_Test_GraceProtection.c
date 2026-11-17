// red-proof: Observed 2026-11-16 with `tools\cli test --no-build --only LF_Test_GraceProtection`: with `LF_Grace.IsProtected` stubbed to `return false;` it failed `a protected character cannot be held up (expected 4, got 0)`; restored, green.
// tier: LOGIC
//! onboarding/grace (#1780): for its first N minutes of playtime a character cannot be held up or
//! arrested and may not rob; after N minutes, or after its first crime, all three work. Off at 0.
class LF_Test_GraceProtection : LF_Test
{
	protected static const string UID_NEW = "grace-new";
	protected static const string UID_OLD = "grace-old";
	protected static const string UID_ACTOR = "grace-actor";
	protected static const string UID_CROOK = "grace-crook";

	override string GetName()
	{
		return "onboarding/grace";
	}

	override void Run(LF_TestContext ctx)
	{
		// The pure rule.
		ctx.False(LF_Grace.IsProtected(0, 0, 0), "a grace of 0 minutes is off");
		ctx.True(LF_Grace.IsProtected(30, 0, 0), "a fresh character is protected");
		ctx.True(LF_Grace.IsProtected(30, 1799, 0), "one second before the end is protected");
		ctx.False(LF_Grace.IsProtected(30, 1800, 0), "the end of the grace is not protected");
		ctx.False(LF_Grace.IsProtected(30, 60, 1), "a crime ends it for good");

		array<ref LF_GraceClockRow> clocksBefore = LF_GraceCharacterClock.ExportAll();
		array<ref LF_CrimeRecordRow> crimesBefore = LF_CrimeRecord.ExportAll();
		LF_GraceCharacterClock.ApplyAll(null);
		LF_CrimeRecord.ApplyAll(null);

		// The character grace clock (#2443): 10 minutes in for the new one, 2 hours for the old one.
		LF_GraceCharacterClock.Credit(UID_NEW, 600);
		LF_GraceCharacterClock.Credit(UID_OLD, 7200);
		LF_GraceCharacterClock.Credit(UID_ACTOR, 7200);
		LF_GraceCharacterClock.Credit(UID_CROOK, 60);

		// Core default: off.
		LF_Grace.SetMinutesOverride(-1);
		ctx.Equal(0, LF_Grace.Minutes(), "the core config has the grace off");
		ctx.False(LF_Grace.Protects(UID_NEW), "off: nobody is protected");

		LF_Grace.SetMinutesOverride(30);
		ctx.True(LF_Grace.Protects(UID_NEW), "10 minutes into a 30 minute grace is protected");
		ctx.False(LF_Grace.Protects(UID_OLD), "a character past the grace is not protected");
		ctx.False(LF_Grace.Protects(""), "no key is not protected");

		// Hold-up.
		ctx.Equal(LF_EHoldUp.GRACE, LF_HoldUp.Attempt(UID_ACTOR, UID_NEW, "0 0 0", 1, true, 1000), "a protected character cannot be held up");
		ctx.Equal(LF_EHoldUp.GRACE, LF_HoldUp.Attempt(UID_NEW, UID_OLD, "0 0 0", 1, true, 1000), "a protected character cannot hold anyone up");
		ctx.EqualStr(LF_Grace.REFUSAL_KEY, LF_HoldUp.RefusalKey(LF_EHoldUp.GRACE), "the hold-up refusal names the grace");

		// Arrest.
		ctx.True(LF_JailArrestAction.RefusedByGrace(UID_NEW), "a protected character cannot be arrested");
		ctx.False(LF_JailArrestAction.RefusedByGrace(UID_OLD), "a character past the grace can be arrested");

		// Robbery.
		ctx.EqualStr(LF_RobAction.REFUSED_GRACE, LF_RobAction.Gate(UID_NEW, 100, 5, 0, 0, 0, 0), "a protected character cannot rob");
		ctx.False(LF_RobAction.Gate(UID_OLD, 100, 5, 0, 0, 0, 0) == LF_RobAction.REFUSED_GRACE, "a character past the grace is not refused for the grace");

		// A crime ends it for good: the next gate sees it, and the grace does not come back.
		ctx.True(LF_Grace.Protects(UID_CROOK), "a character with 1 minute played is protected");
		LF_CrimeRecord.Record(LF_OnCrimeCommitted.Create(UID_CROOK, LF_WantedReports.OFFENCE_SHOPLIFTING));
		ctx.False(LF_Grace.Protects(UID_CROOK), "its first crime ends the grace");
		ctx.False(LF_JailArrestAction.RefusedByGrace(UID_CROOK), "after the crime it can be arrested");
		ctx.Equal(LF_EHoldUp.OK, LF_HoldUp.Attempt(UID_ACTOR, UID_CROOK, "0 0 0", 1, true, 2000), "after the crime it can be held up");

		// Grace over by time: the same character at 30 minutes plays on unprotected.
		LF_GraceCharacterClock.Credit(UID_NEW, 1200);
		ctx.False(LF_Grace.Protects(UID_NEW), "the clock running out ends the grace");
		ctx.Equal(LF_EHoldUp.OK, LF_HoldUp.Attempt(UID_ACTOR, UID_NEW, "0 0 0", 1, true, 3000), "after the grace it can be held up");

		LF_Grace.SetMinutesOverride(-1);
		LF_HoldUp.ResetStatics();
		LF_GraceCharacterClock.ApplyAll(clocksBefore);
		LF_CrimeRecord.ApplyAll(crimesBefore);
	}
}
