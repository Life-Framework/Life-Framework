// red-proof: RED-OK licence/truck-check-examiner, observed 2026-11-24: written before LF_TruckCheckLend existed, the test did not compile (--only failed, 'scripts do not compile'); then green (--only passed=2 failed=0); `tools/cli test --no-build --red LF_Test_TruckCheckExaminer` RED PROOF OK: with the take skipped, `the run is open`, `a lent truck exists` and `a second take while a run is open is refused` fail.
//! licence/truck-check-examiner (WK-W21, #4696): "Take the truck check" at the examiner takes the $300
//! fee, spawns the lent truck and opens the run. Short cash, a held licence and the retake wait are
//! refused in words (the wait is the toast's argument) and spawn no truck. Failing the run deletes the
//! lent truck. Leaving the truck 60 s, being 300 m away or a wreck ends the check (pure rule).
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_TruckCheckExaminer : LF_Test
{
	protected static const string UID = "lf-test-truck-check-examiner";
	protected static const int T0 = 1000000;
	protected static const ResourceName MARKER = "{C9D661E5B0714711}Prefabs/Items/Food/Apple.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "licence/truck-check-examiner";
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity TruckOf(string uid)
	{
		EntityID id = LF_TruckCheckLend.TruckOf(uid);
		if (id == EntityID.INVALID)
			return null;

		return GetGame().GetWorld().FindEntityByID(id);
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		IEntity user = LF_TestCharacterFixture.Spawn(ctx);
		if (!user)
			return;

		LF_Utils.SetUidForTests(user, UID);
		string uid = LF_Utils.GetPlayerUID(user);
		LF_Progression.Forget(uid);
		LF_DrivingCheck.ResetStatics();
		LF_TruckCheckLoan.ResetStatics();
		LF_TruckCheckLend.ResetStatics();
		LF_LicenceDesk.SetNowForTest(T0);

		// The start place is found by kind, never by coordinates.
		vector spot = user.GetOrigin() + "6 0 0";
		spot[1] = GetGame().GetWorld().GetSurfaceY(spot[0], spot[2]);
		IEntity marker = LF_Utils.SpawnEntityPrefab(MARKER, spot);
		ctx.NotNull(marker, "the start marker spawns");
		LF_Places.Register(LF_TruckCheckLend.PLACE_KIND, LF_TruckCheckLend.PLACE_ID, marker);

		// Pure rules: 60 s out of the seat, 300 m away or a wreck ends the check.
		ctx.Equal(LF_ETruckCheckEnd.NONE, LF_TruckCheckLend.Judge(59, 299, false), "59 s out of the seat and 299 m away is still a run");
		ctx.Equal(LF_ETruckCheckEnd.LEFT, LF_TruckCheckLend.Judge(60, 5, false), "60 s out of the truck fails the check");
		ctx.Equal(LF_ETruckCheckEnd.AWAY, LF_TruckCheckLend.Judge(0, 300, false), "300 m from the truck fails the check");
		ctx.Equal(LF_ETruckCheckEnd.WRECKED, LF_TruckCheckLend.Judge(0, 0, true), "a wrecked truck fails the check");

		int left;
		LF_ETruckCheckLend outcome = LF_ETruckCheckLend.FAILED;

		// Short of the fee: refused in words, nothing taken, no truck.
		LF_MoneyUtils.AddCash(user, LF_DrivingCheck.FEE - 1);
		if (!RedProof())
			outcome = LF_TruckCheckLend.Take(user, user, left);

		ctx.Equal(LF_ETruckCheckLend.NOT_ENOUGH_CASH, outcome, "short of $300 the check is refused");
		ctx.EqualStr("#LF-TruckCheck_NoCash", LF_TruckCheckLend.AnswerKey(outcome), "the refusal says the fee is short");
		ctx.True(TruckOf(uid) == null, "a refused check lends no truck");
		ctx.False(LF_DrivingCheck.IsRunning(uid), "a refused check opens no run");

		// With the fee: a lent truck stands by the marker, the run is open and the gates watch the truck.
		LF_MoneyUtils.AddCash(user, 1);
		outcome = LF_ETruckCheckLend.FAILED;
		if (!RedProof())
			outcome = LF_TruckCheckLend.Take(user, user, left);

		ctx.Equal(LF_ETruckCheckLend.OK, outcome, "with $300 the check starts");
		ctx.True(LF_DrivingCheck.IsRunning(uid), "the run is open");
		IEntity truck = TruckOf(uid);
		ctx.NotNull(truck, "a lent truck exists");
		if (truck)
		{
			ctx.True(LF_TruckCheckLoan.HasLoan(uid), "the gates watch the lent truck");
			ctx.True(vector.Distance(truck.GetOrigin(), marker.GetOrigin()) < 8, "the truck stands at the start place");
			ctx.True(vector.Distance(truck.GetOrigin(), user.GetOrigin()) < 8, "and the player is at its door");
		}

		ctx.Equal(LF_ETruckCheckLend.ALREADY_RUNNING, LF_TruckCheckLend.Take(user, user, left), "a second take while a run is open is refused");
		ctx.Equal(1, LF_TruckCheckLend.LentCount(), "one truck is lent");

		// Failing the run takes the truck back.
		LF_DrivingCheck.Fail(uid);
		LF_TruckCheckLend.Tick();
		ctx.True(TruckOf(uid) == null, "a failed run's truck is deleted");
		ctx.False(LF_TruckCheckLoan.HasLoan(uid), "and the loan is released");
		ctx.Equal(0, LF_TruckCheckLend.LentCount(), "nothing stays lent");

		// A second take inside the retake wait is refused with the wait in the toast.
		LF_MoneyUtils.AddCash(user, LF_DrivingCheck.FEE);
		outcome = LF_TruckCheckLend.Take(user, user, left);
		ctx.Equal(LF_ETruckCheckLend.RETAKE_WAIT, outcome, "inside the retake wait the check is refused");
		ctx.EqualStr("#LF-TruckCheck_RetakeWait", LF_TruckCheckLend.AnswerKey(outcome), "the refusal names the wait");
		ctx.EqualStr(LF_DrivingCheck.RETAKE_MINUTES.ToString(), LF_TruckCheckLend.AnswerArg(outcome, left), "the toast carries the minutes left");
		ctx.True(TruckOf(uid) == null, "a refused retake lends no truck");

		// A held licence is refused and lends nothing.
		LF_Progression.NoteFlag(uid, LF_DrivingCheck.LICENCE_FLAG);
		outcome = LF_TruckCheckLend.Take(user, user, left);
		ctx.Equal(LF_ETruckCheckLend.ALREADY_HELD, outcome, "a player with the licence is refused");
		ctx.EqualStr("#LF-TruckCheck_Held", LF_TruckCheckLend.AnswerKey(outcome), "the refusal says the licence is held");

		LF_Places.Unregister(LF_TruckCheckLend.PLACE_KIND, LF_TruckCheckLend.PLACE_ID);
		LF_Progression.Forget(uid);
		LF_DrivingCheck.ResetStatics();
		LF_TruckCheckLoan.ResetStatics();
		LF_TruckCheckLend.ResetStatics();
		LF_LicenceDesk.SetNowForTest(-1);
		LF_Utils.SetUidForTests(null, "");
		LF_TestCharacterFixture.Delete(marker);
		LF_TestCharacterFixture.Delete(user);
	}
}
