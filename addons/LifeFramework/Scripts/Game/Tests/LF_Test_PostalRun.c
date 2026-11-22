// red-proof: `tools/cli test --no-build --red LF_Test_PostalRun` RED PROOF OK (RedProof() skips the hand-in: "it pays the run pay (expected 80, got 0)", "the cash is in the worker's pocket", "a second hand-in is refused" fail). The abandon rule was red on its first run: the away clock used -1 as "not away", a fresh world's clock made the run look present, "a driver away from the van too long loses the run" failed; green after the m_bAway flag (--only passed, 47 assertions).
//! jobs/postal-run: the postal van run. Taking the run lends one van (out of the world save, locked, its
//! key in the worker's pocket) and a second take is refused while it is open. Sacks load at the home office
//! and unload at the far one only with the van there; the hand-in at home pays once (the POSTAL run pay
//! row) and takes the van and key back. A run whose driver is away too long, or whose van is wrecked, ends
//! with no pay and no van left; a fresh take waits out the cooldown. The destination is the nearest office
//! at least the run's distance away.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_PostalRun : LF_Test
{
	protected static const string UID = "postal-run-test";
	protected static const ResourceName OFFICE = "{881437787564A1A9}Prefabs/Jobs/LF_PostalPoint.et";
	protected static const vector HOME = "40 0 40"; // inside the test annex (x/z 18..114), never on the town
	protected static const vector FAR = "40 0 100";
	protected static const vector NEAR = "52 0 40";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "jobs/postal-run";
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity SpawnAt(ResourceName prefab, vector at)
	{
		at[1] = GetGame().GetWorld().GetSurfaceY(at[0], at[2]);
		return LF_Utils.SpawnEntityPrefab(prefab, at);
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity Van()
	{
		LF_PostalRunState run = LF_PostalRun.GetRun(UID);
		if (!run)
			return null;

		return GetGame().GetWorld().FindEntityByID(run.m_Van);
	}

	//------------------------------------------------------------------------------------------------
	protected void MoveVanTo(IEntity van, IEntity office)
	{
		vector at = office.GetOrigin() + Vector(6, 0, 0);
		at[1] = GetGame().GetWorld().GetSurfaceY(at[0], at[2]);
		van.SetOrigin(at);
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		array<vector> offices = {Vector(0, 0, 0), Vector(10, 0, 0), Vector(0, 0, 900), Vector(850, 0, 0)};
		ctx.Equal(3, LF_PostalRun.PickDestination(vector.Zero, offices, 800), "the destination is the nearest office past the least distance");
		ctx.Equal(-1, LF_PostalRun.PickDestination(vector.Zero, offices, 5000), "no office far enough: no destination");

		LF_PostalRunConfig config = LF_PostalRunConfig.CreateDefault();
		config.m_iSacks = 2;
		config.m_fMinDistance = 30;
		config.m_iMinRunSeconds = 0;
		config.m_iStartCooldownSeconds = 60;
		config.m_bNeedsDrivingLicence = true;
		LF_PostalRunConfig.UseForTest(config);

		IEntity home = SpawnAt(OFFICE, HOME);
		IEntity far = SpawnAt(OFFICE, FAR);
		IEntity near = SpawnAt(OFFICE, NEAR);
		IEntity user = LF_TestCharacterFixture.Spawn(ctx);
		ctx.NotNull(home, "the home office spawns");
		ctx.NotNull(far, "the far office spawns");
		ctx.NotNull(user, "the worker spawns");
		if (home && far && near && user)
		{
			ctx.True(LF_PostalRun.Start(user, UID, home) == LF_EPostalResult.NO_LICENCE, "no driving licence: no van");
			ctx.False(LF_PostalRun.HasRun(UID), "and no run");
			config.m_bNeedsDrivingLicence = false;
			CheckRun(ctx, home, far, user);
			CheckAbandon(ctx, home, user);
			CheckWrecked(ctx, home, user);
		}

		LF_PostalRun.Close(UID, "test end");
		LF_PostalRun.ResetStatics();
		LF_TestCharacterFixture.Delete(home);
		LF_TestCharacterFixture.Delete(far);
		LF_TestCharacterFixture.Delete(near);
		LF_TestCharacterFixture.Delete(user);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckRun(LF_TestContext ctx, IEntity home, IEntity far, IEntity user)
	{
		int cash = LF_MoneyUtils.GetCash(user);
		if (cash > 0)
			LF_MoneyUtils.RemoveCash(user, cash);

		int paid;
		ctx.True(LF_PostalRun.Sack(user, UID, home) == LF_EPostalResult.NO_RUN, "a sack with no run is refused");
		ctx.True(LF_PostalRun.Start(user, UID, home) == LF_EPostalResult.OK, "the run starts at the home office");
		LF_PostalRunState run = LF_PostalRun.GetRun(UID);
		IEntity van = Van();
		ctx.NotNull(van, "a van is lent");
		if (!run || !van)
			return;

		EntityID vanId = run.m_Van;
		EntityID keyId = run.m_Key;
		ctx.True(run.m_Destination == far.GetID(), "the far office (60 m) is the destination, not the near one (12 m)");
		ctx.True(vector.DistanceXZ(van.GetOrigin(), home.GetOrigin()) <= LF_PostalRunConfig.Get().m_fVanReach, "the van stands by the office");
		ctx.False(LF_VehicleWorldSave.IsSaved(van), "the van is out of the world save: never a kept car");
		ctx.True(LF_PostalRun.IsRunVan(van), "the van is known as a run van");
		ctx.NotNull(GetGame().GetWorld().FindEntityByID(keyId), "the van's key is handed out");
		LF_VehicleLockComponent vehicleLock = LF_Component<LF_VehicleLockComponent>.Find(van);
		if (vehicleLock)
			ctx.True(vehicleLock.IsVehicleLocked(), "the van is locked: its key holder drives it");

		ctx.True(LF_PostalRun.Start(user, UID, home) == LF_EPostalResult.ALREADY, "a second take while the run is open is refused");
		ctx.Equal(1, LF_PostalRun.RunCount(), "one van per player");

		ctx.True(LF_PostalRun.HandIn(user, UID, home, paid) == LF_EPostalResult.NOT_LOADED, "a hand-in before the mail is delivered is refused");
		ctx.True(LF_PostalRun.Sack(user, UID, far) == LF_EPostalResult.VAN_FAR, "unloading with the van at home is refused");
		ctx.True(LF_PostalRun.Sack(user, UID, home) == LF_EPostalResult.OK, "sack 1 loaded");
		ctx.True(LF_PostalRun.Sack(user, UID, home) == LF_EPostalResult.OK, "sack 2 loaded");
		ctx.True(LF_PostalRun.Sack(user, UID, home) == LF_EPostalResult.WRONG_OFFICE, "a third sack at home is refused: the mail goes to the far office");

		MoveVanTo(van, far);
		ctx.True(LF_PostalRun.Sack(user, UID, far) == LF_EPostalResult.OK, "sack 1 unloaded at the far office");
		ctx.True(LF_PostalRun.Sack(user, UID, far) == LF_EPostalResult.OK, "sack 2 unloaded");
		ctx.True(LF_PostalRun.Sack(user, UID, far) == LF_EPostalResult.ALL_DONE, "nothing left to unload");
		ctx.True(LF_PostalRun.HandIn(user, UID, far, paid) == LF_EPostalResult.WRONG_OFFICE, "the run is handed in at home only");
		ctx.True(LF_PostalRun.HandIn(user, UID, home, paid) == LF_EPostalResult.VAN_FAR, "a hand-in without the van back is refused");
		ctx.Equal(0, paid, "and pays nothing");

		MoveVanTo(van, home);
		LF_PostalRunConfig.Get().m_iMinRunSeconds = 100000;
		ctx.True(LF_PostalRun.HandIn(user, UID, home, paid) == LF_EPostalResult.TOO_FAST, "a run faster than its least time pays nothing");
		LF_PostalRunConfig.Get().m_iMinRunSeconds = 0;

		int expected = LF_PostalRun.Pay() + LF_PostalRun.NightPay(LF_PostalRun.Pay());
		ctx.True(expected >= LF_PostalRun.Pay() && LF_PostalRun.Pay() >= 1, "the run pays its POSTAL row");
		LF_EPostalResult handIn = LF_EPostalResult.NO_RUN;
		if (!RedProof())
			handIn = LF_PostalRun.HandIn(user, UID, home, paid);

		ctx.True(handIn == LF_EPostalResult.OK, "the hand-in at home succeeds");
		ctx.Equal(expected, paid, "it pays the run pay");
		ctx.Equal(expected, LF_MoneyUtils.GetCash(user), "the cash is in the worker's pocket");
		ctx.True(LF_PostalRun.HandIn(user, UID, home, paid) == LF_EPostalResult.NO_RUN, "a second hand-in is refused");
		ctx.Equal(expected, LF_MoneyUtils.GetCash(user), "pay once: nothing more");
		ctx.True(!GetGame().GetWorld().FindEntityByID(vanId), "the van is taken back at the hand-in");
		ctx.True(!GetGame().GetWorld().FindEntityByID(keyId), "and so is its key");
		ctx.True(LF_PostalRun.Start(user, UID, home) == LF_EPostalResult.COOLDOWN, "a new van waits out the cooldown");
		ctx.Equal(0, LF_PostalRun.RunCount(), "no run is open");
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckAbandon(LF_TestContext ctx, IEntity home, IEntity user)
	{
		LF_PostalRun.ClearCooldownForTest(UID);
		int cash = LF_MoneyUtils.GetCash(user);
		ctx.True(LF_PostalRun.Start(user, UID, home) == LF_EPostalResult.OK, "a second run starts after the cooldown");
		LF_PostalRunState run = LF_PostalRun.GetRun(UID);
		if (!run)
			return;

		EntityID vanId = run.m_Van;
		LF_PostalRun.AwayForTest(UID, LF_PostalRunConfig.Get().m_iAbandonSeconds + 1);
		LF_PostalRun.Watch();
		ctx.False(LF_PostalRun.HasRun(UID), "a driver away from the van too long loses the run");
		ctx.True(!GetGame().GetWorld().FindEntityByID(vanId), "the abandoned van goes back: no free car");
		ctx.Equal(cash, LF_MoneyUtils.GetCash(user), "an abandoned run pays nothing");
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckWrecked(LF_TestContext ctx, IEntity home, IEntity user)
	{
		LF_PostalRun.ClearCooldownForTest(UID);
		int cash = LF_MoneyUtils.GetCash(user);
		ctx.True(LF_PostalRun.Start(user, UID, home) == LF_EPostalResult.OK, "a third run starts");
		IEntity van = Van();
		if (!van)
			return;

		SCR_EntityHelper.DeleteEntityAndChildren(van);
		LF_PostalRun.Watch();
		ctx.False(LF_PostalRun.HasRun(UID), "a lost or wrecked van ends the run");
		ctx.Equal(cash, LF_MoneyUtils.GetCash(user), "and pays nothing");
	}
}
