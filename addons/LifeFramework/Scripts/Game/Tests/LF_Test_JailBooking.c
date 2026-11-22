// red-proof: RED-OK jail/booking, observed 2026-11-22 with `tools\cli test --no-build --red LF_Test_JailBooking`: with the suspect left uncuffed, `a cuffed suspect at the jail can be booked (expected '', got '#LF-JailBook_CuffFirst')`. Green: `--only` passed (24 assertions).
//! jail/booking (police arrest flow review, 2026-11-22): arrest, book, held, release on the placed cell.
//! An officer may book only at the cell and only a cuffed suspect; the reduced option halves the
//! sentence; the booked suspect stands in the cell with the time on their body (the HUD line's
//! replicated seconds); the clock runs out, the prisoner stands at the release point and the HUD
//! seconds go to 0.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_JailBooking : LF_Test
{
	protected static const string OFFICER_UID = "lf-test-booking-officer";
	protected static const string PRISONER_UID = "lf-test-booking-prisoner";

	override string GetName()
	{
		return "jail/booking";
	}

	override void Run(LF_TestContext ctx)
	{
		// The pure rules first: the reduced option and the HUD write throttle.
		ctx.Equal(120, LF_JailBooking.Scaled(120, 100), "the full option keeps the whole sentence");
		ctx.Equal(60, LF_JailBooking.Scaled(120, 50), "the reduced option halves it");
		ctx.Equal(30, LF_JailBooking.Scaled(40, 50), "a reduced sentence keeps the floor");
		ctx.Equal(20, LF_JailBooking.Scaled(20, 50), "the floor never lengthens a short sentence");
		ctx.True(LF_JailClock.ShouldSync(0, 60), "a new sentence is written at once");
		ctx.False(LF_JailClock.ShouldSync(60, 55), "a plain countdown second is not written");
		ctx.True(LF_JailClock.ShouldSync(60, 45), "a whole step is written");
		ctx.True(LF_JailClock.ShouldSync(12, 0), "the release is written");
		ctx.EqualStr("2:05", LF_JailBooking.Clock(125), "the clock reads m:ss");

		ctx.True(LF_JailBooking.IsCellKnown(), "the placed cell noted its origin for the booking offer");
		if (!LF_JailBooking.IsCellKnown() || !LF_Jail.HasCell())
			return;

		IEntity officerBody = LF_TestCharacterFixture.Spawn(ctx);
		IEntity suspect = LF_TestCharacterFixture.Spawn(ctx);
		SCR_ChimeraCharacter chimera = SCR_ChimeraCharacter.Cast(suspect);
		if (!officerBody || !chimera)
		{
			ctx.Fail("fixture characters did not spawn");
			Cleanup(officerBody, suspect);
			return;
		}

		LF_Jail.Configure(60, 300, 1, 8);
		LF_PlayerAccount officer = LF_PlayerAccountManager.GetOrCreate(OFFICER_UID);
		officer.SetFaction(LF_Faction.POLICE);
		officer.SetOnDuty(true);
		LF_PlayerAccount prisoner = LF_PlayerAccountManager.GetOrCreate(PRISONER_UID);
		prisoner.SetWantedLevel(2);

		vector cell = LF_Jail.GetCellOrigin();
		vector far = cell + Vector(LF_JailBooking.BOOKING_METERS + 40, 0, 0);
		officerBody.SetOrigin(far);
		ctx.EqualStr("#LF-JailBook_NotHere", LF_JailBooking.Refusal(officerBody, suspect), "away from the jail the booking is refused");
		officerBody.SetOrigin(cell + Vector(2, 0, 0));
		ctx.EqualStr("#LF-JailBook_CuffFirst", LF_JailBooking.Refusal(officerBody, suspect), "an uncuffed suspect is not booked");

		if (!RedProof())
			chimera.LF_SetRestrained(true);

		ctx.EqualStr(string.Empty, LF_JailBooking.Refusal(officerBody, suspect), "a cuffed suspect at the jail can be booked");
		ctx.True(LF_Jail.Arrest(OFFICER_UID, PRISONER_UID, suspect, 50), "the reduced booking starts the sentence");
		ctx.True(LF_Jail.IsServing(PRISONER_UID), "the booked suspect is serving");
		ctx.EqualFloat(60, LF_Jail.GetRemaining(PRISONER_UID), 0.5, "the reduced option gave half of two wanted levels");
		ctx.True(vector.Distance(suspect.GetOrigin(), cell) <= 8, "the prisoner is held in the cell");
		ctx.Equal(60, chimera.LF_GetJailSecondsLeft(), "the prisoner's HUD seconds are on their body");

		array<string> online = {PRISONER_UID};
		LF_Jail.TickOnline(20, online);
		ctx.Equal(40, chimera.LF_GetJailSecondsLeft(), "a whole step of the clock reaches the HUD");
		LF_Jail.TickOnline(45, online);
		ctx.False(LF_Jail.IsServing(PRISONER_UID), "the clock ran out: released");
		ctx.Equal(0, chimera.LF_GetJailSecondsLeft(), "the HUD line goes away on release");
		ctx.True(vector.DistanceXZ(suspect.GetOrigin(), LF_Jail.GetReleasePoint()) < 1.5, "the released prisoner stands at the release point");
		ctx.Equal(1, prisoner.GetWantedLevel(), "the sentence took one wanted level off");

		Cleanup(officerBody, suspect);
	}

	protected void Cleanup(IEntity officerBody, IEntity suspect)
	{
		LF_JailEscort.Drop(PRISONER_UID);
		LF_Jail.ReleaseEarly(PRISONER_UID);
		LF_Jail.SyncPlacedCell();
		LF_TestCharacterFixture.Delete(officerBody);
		LF_TestCharacterFixture.Delete(suspect);
	}
}
