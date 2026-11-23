// red-proof: observed 2026-11-23 `tools\cli test --no-build --red LF_Test_PoliceCarPool`: RED PROOF OK; RedProof() installs an empty fleet table, so `the lowest rank draws the small car` and the needs-rank refusal fail. Green with `--only`.
//! #4352: the police car pool board puts the officer's rank car in its bay. A Cadet draws the small car and is
//! refused the Patrol car in words naming Patrol; a second draw sends the first car back; an officer off duty,
//! a second officer while the bay is taken and a draw seconds after the last are refused. Core cars only (the
//! test installs its own two-row fleet table), so the pack's livery prefabs are not needed here.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_PoliceCarPool : LF_Test
{
	protected static const ResourceName BOARD = "{AEFD4F9B8A69C66E}Prefabs/Props/Police/LF_PoliceCarPoolBoard.et";
	protected static const ResourceName SMALL_CAR = "{E1A105BE1A000001}Prefabs/Vehicles/Wheeled/S105/EL_S105_beige.et";
	protected static const ResourceName PATROL_CAR = "{EFEB161F9BC2031D}Prefabs/Vehicles/Wheeled/UAZ469/LF_UAZ469_Police.et";
	protected static const string OFFICER = "police-car-pool-officer";
	protected static const string SECOND = "police-car-pool-second";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "police/car-pool";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		array<ref LF_PoliceRank> ranks = {};
		LF_PoliceConfig.Ranks(ranks);
		ctx.True(ranks.Count() >= 2, "the ladder has the two rungs the pool test needs");
		if (ranks.Count() < 2)
			return;

		LF_PoliceCarPool.ResetStatics();
		array<ref LF_RankFleetRow> rows = new array<ref LF_RankFleetRow>();
		rows.Insert(LF_RankFleetRow.Create(ranks[0].m_sId, SMALL_CAR));
		rows.Insert(LF_RankFleetRow.Create(ranks[1].m_sId, PATROL_CAR));
		// RedProof(): the fleet table stays empty, so the board offers no car and every draw is refused.
		if (!RedProof())
			LF_RankFleet.UseRowsForTest(rows);
		else
			LF_RankFleet.UseRowsForTest(new array<ref LF_RankFleetRow>());

		IEntity officer = LF_TestCharacterFixture.Spawn(ctx);
		if (!officer)
			return;

		IEntity board = LF_Utils.SpawnEntityPrefab(BOARD, officer.GetOrigin() + "2 0 0");
		ctx.NotNull(board, "the pool board spawns");
		LF_PlayerAccount account = LF_PlayerAccountManager.GetOrCreate(OFFICER);
		LF_PlayerAccount second = LF_PlayerAccountManager.GetOrCreate(SECOND);
		array<IEntity> cars = {};
		if (board && account && second)
			RunPool(ctx, officer, board, account, second, cars);

		foreach (IEntity car : cars)
		{
			LF_TestCharacterFixture.Delete(car);
		}

		LF_TestCharacterFixture.Delete(LF_PoliceCarPool.CarOf(OFFICER));
		LF_TestCharacterFixture.Delete(board);
		LF_TestCharacterFixture.Delete(officer);
		OffDuty(account);
		OffDuty(second);
		LF_PoliceCarPool.ResetStatics();
		LF_RankFleet.Reload();
	}

	//------------------------------------------------------------------------------------------------
	protected void RunPool(LF_TestContext ctx, IEntity officer, IEntity board, LF_PlayerAccount account, LF_PlayerAccount second, notnull array<IEntity> cars)
	{
		ctx.True(LF_PoliceCarPool.SlotCar(0) == SMALL_CAR, "board slot 0 offers the lowest rank's car");
		ctx.True(LF_PoliceCarPool.SlotCar(1) == PATROL_CAR, "board slot 1 offers the Patrol car");
		ctx.True(LF_PoliceCarPool.SlotCar(2).IsEmpty(), "board slot 2 offers nothing on a two-car table");

		string refusal;
		string arg;
		// Off duty: refused.
		account.SetFaction(LF_Faction.POLICE);
		account.SetPoliceRank(0);
		account.SetOnDuty(false);
		ctx.True(LF_PoliceCarPool.Draw(OFFICER, officer, board, 0, refusal, arg) == null, "an officer off duty draws no car");
		ctx.EqualStr(LF_PoliceCarPool.REFUSED_OFF_DUTY, refusal, "off duty, the refusal says to clock in");

		// On duty as the lowest rank: the Patrol car is refused naming its rank, the small car stands in the bay.
		account.SetOnDuty(true);
		ctx.True(LF_PoliceCarPool.Draw(OFFICER, officer, board, 1, refusal, arg) == null, "the lowest rank is refused the Patrol car");
		ctx.EqualStr(LF_RankFleet.REFUSED_NEEDS_RANK, refusal, "the refusal says the rank is too low");
		ctx.EqualStr("#" + LF_PoliceRanks.TitleKey(1), arg, "the refusal names the rank the car needs");

		IEntity first = LF_PoliceCarPool.Draw(OFFICER, officer, board, 0, refusal, arg);
		ctx.NotNull(first, "the lowest rank draws the small car");
		if (!first)
			return;

		cars.Insert(first);
		ctx.EqualStr("", refusal, "a draw has no refusal");
		ctx.True(LF_Utils.SamePrefab(first.GetPrefabData().GetPrefabName(), SMALL_CAR), "the car in the bay is the small car");
		vector bay[4];
		LF_PoliceCarPoolComponent pool = LF_Component<LF_PoliceCarPoolComponent>.Find(board);
		ctx.NotNull(pool, "the board carries the pool component");
		if (pool)
			pool.BayTransform(bay);
		vector flat = first.GetOrigin() - bay[3];
		flat[1] = 0;
		ctx.True(flat.Length() < 1.5, string.Format("the car stands in the bay (%1 m off)", flat.Length()));
		LF_VehicleLockComponent vehicleLock = LF_Component<LF_VehicleLockComponent>.Find(first);
		ctx.True(vehicleLock && vehicleLock.IsServiceVehicle(), "the drawn car is a service car");
		ctx.True(vehicleLock && !vehicleLock.IsVehicleLocked(), "the drawn car stands open for its officer");
		ctx.True(LF_PoliceCarPool.CarOf(OFFICER) == first, "the pool remembers the officer's car");

		// A second draw at once is refused; after the wait it sends the first car back and puts a new one out.
		ctx.True(LF_PoliceCarPool.Draw(OFFICER, officer, board, 0, refusal, arg) == null, "a draw seconds after the last is refused");
		ctx.EqualStr(LF_PoliceCarPool.REFUSED_TOO_SOON, refusal, "the refusal says to wait");

		// A second officer finds the bay taken.
		second.SetFaction(LF_Faction.POLICE);
		second.SetPoliceRank(1);
		second.SetOnDuty(true);
		ctx.True(LF_PoliceCarPool.Draw(SECOND, officer, board, 1, refusal, arg) == null, "a second officer finds the bay taken");
		ctx.EqualStr(LF_PoliceCarPool.REFUSED_BAY_BLOCKED, refusal, "the refusal says the bay is blocked");

		LF_PoliceCarPool.ClearCooldownForTest(OFFICER);
		EntityID firstId = first.GetID();
		IEntity again = LF_PoliceCarPool.Draw(OFFICER, officer, board, 0, refusal, arg);
		ctx.NotNull(again, "the officer draws again after the wait");
		ctx.True(again != first, "the new draw is a new car");
		ctx.True(LF_PoliceCarPool.CarOf(OFFICER) == again, "the pool now remembers the new car");
		cars.RemoveItem(first);
		if (again)
			cars.Insert(again);

		ctx.True(GetGame().GetWorld().FindEntityByID(firstId) == null, "the first car went back to the pool");
	}

	//------------------------------------------------------------------------------------------------
	protected void OffDuty(LF_PlayerAccount account)
	{
		if (!account)
			return;

		account.SetOnDuty(false);
		account.SetPoliceRank(0);
		account.SetFaction(LF_Faction.CIVILIAN);
	}
}
