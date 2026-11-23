// red-proof: RedProof() installs an empty fleet table, so the board offers no ambulance and every draw below is refused (`tools\cli test --no-build --red LF_Test_MedicFleet`).
//! #4432: the clinic's car pool board puts the medic's rank ambulance in its bay. The lowest rank draws the first car and
//! is refused the second in words naming the rank it needs; a medic off duty, a police officer, a second draw seconds
//! after the last and a second medic while the bay is taken are refused; a later draw sends the first car back. The
//! test installs its own two-row fleet table with core cars, so the pack's ambulance prefabs are not needed here (the
//! pack's table is `everon/medic-fleet`).
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_MedicFleet : LF_Test
{
	protected static const ResourceName BOARD = "{7626F19FBE72C466}Prefabs/Props/Medic/LF_MedicCarPoolBoard.et";
	protected static const ResourceName LOW_CAR = "{E1A105BE1A000001}Prefabs/Vehicles/Wheeled/S105/EL_S105_beige.et";
	protected static const ResourceName HIGH_CAR = "{EFEB161F9BC2031D}Prefabs/Vehicles/Wheeled/UAZ469/LF_UAZ469_Police.et";
	protected static const string MEDIC = "medic-fleet-medic";
	protected static const string SECOND = "medic-fleet-second";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "medic/fleet";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		array<ref LF_MedicRank> ranks = {};
		LF_MedicRanks.Ranks(ranks);
		ctx.True(ranks.Count() >= 2, "the medic ladder has the two rungs the fleet test needs");
		if (ranks.Count() < 2)
			return;

		LF_MedicCarPool.ResetStatics();
		array<ref LF_MedicFleetRow> rows = new array<ref LF_MedicFleetRow>();
		rows.Insert(LF_MedicFleetRow.Create(ranks[0].m_sId, LOW_CAR));
		rows.Insert(LF_MedicFleetRow.Create(ranks[1].m_sId, HIGH_CAR));
		// RedProof(): the fleet table stays empty, so the board offers no car and every draw is refused.
		if (!RedProof())
			LF_MedicFleet.UseRowsForTest(rows);
		else
			LF_MedicFleet.UseRowsForTest(new array<ref LF_MedicFleetRow>());

		IEntity medic = LF_TestCharacterFixture.Spawn(ctx);
		if (!medic)
			return;

		IEntity board = LF_Utils.SpawnEntityPrefab(BOARD, medic.GetOrigin() + "2 0 0");
		ctx.NotNull(board, "the medic pool board spawns");
		LF_PlayerAccount account = LF_PlayerAccountManager.GetOrCreate(MEDIC);
		LF_PlayerAccount second = LF_PlayerAccountManager.GetOrCreate(SECOND);
		array<IEntity> cars = {};
		if (board && account && second)
			RunPool(ctx, medic, board, account, second, cars);

		foreach (IEntity car : cars)
		{
			LF_TestCharacterFixture.Delete(car);
		}

		LF_TestCharacterFixture.Delete(LF_MedicCarPool.CarOf(MEDIC));
		LF_TestCharacterFixture.Delete(board);
		LF_TestCharacterFixture.Delete(medic);
		Reset(account);
		Reset(second);
		LF_MedicCarPool.ResetStatics();
		LF_MedicFleet.Reload();
	}

	//------------------------------------------------------------------------------------------------
	protected void RunPool(LF_TestContext ctx, IEntity medic, IEntity board, LF_PlayerAccount account, LF_PlayerAccount second, notnull array<IEntity> cars)
	{
		ctx.True(LF_MedicCarPool.SlotCar(0) == LOW_CAR, "board slot 0 offers the lowest rank's car");
		ctx.True(LF_MedicCarPool.SlotCar(1) == HIGH_CAR, "board slot 1 offers the higher rank's car");
		ctx.True(LF_MedicCarPool.SlotCar(2).IsEmpty(), "board slot 2 offers nothing on a two-car table");

		string refusal;
		string arg;
		// Off duty: refused. So is a police officer on duty.
		account.SetFaction(LF_Faction.MEDIC);
		account.SetMedicRank(0);
		account.SetOnDuty(false);
		ctx.True(LF_MedicCarPool.Draw(MEDIC, medic, board, 0, refusal, arg) == null, "a medic off duty draws no car");
		ctx.EqualStr(LF_MedicCarPool.REFUSED_OFF_DUTY, refusal, "off duty, the refusal says to clock in");
		account.SetFaction(LF_Faction.POLICE);
		account.SetOnDuty(true);
		ctx.True(LF_MedicCarPool.Draw(MEDIC, medic, board, 0, refusal, arg) == null, "a police officer draws no ambulance");
		ctx.EqualStr(LF_MedicCarPool.REFUSED_OFF_DUTY, refusal, "an officer is refused like anyone not on medic duty");

		// On duty as the lowest rank: the second car is refused naming its rank, the first car stands in the bay.
		account.SetFaction(LF_Faction.MEDIC);
		ctx.True(LF_MedicCarPool.Draw(MEDIC, medic, board, 1, refusal, arg) == null, "the lowest rank is refused the higher car");
		ctx.EqualStr(LF_RankFleet.REFUSED_NEEDS_RANK, refusal, "the refusal says the rank is too low");
		ctx.EqualStr("#" + LF_MedicRanks.TitleKey(1), arg, "the refusal names the rank the car needs");

		IEntity first = LF_MedicCarPool.Draw(MEDIC, medic, board, 0, refusal, arg);
		ctx.NotNull(first, "the lowest rank draws its car");
		if (!first)
			return;

		cars.Insert(first);
		ctx.EqualStr("", refusal, "a draw has no refusal");
		ctx.True(LF_Utils.SamePrefab(first.GetPrefabData().GetPrefabName(), LOW_CAR), "the car in the bay is the rank's car");
		vector bay[4];
		LF_MedicCarPoolComponent pool = LF_Component<LF_MedicCarPoolComponent>.Find(board);
		ctx.NotNull(pool, "the board carries the pool component");
		if (pool)
			pool.BayTransform(bay);
		vector flat = first.GetOrigin() - bay[3];
		flat[1] = 0;
		ctx.True(flat.Length() < 1.5, string.Format("the car stands in the bay (%1 m off)", flat.Length()));
		LF_VehicleLockComponent vehicleLock = LF_Component<LF_VehicleLockComponent>.Find(first);
		ctx.True(vehicleLock && vehicleLock.IsServiceVehicle(), "the drawn car is a service car");
		ctx.True(vehicleLock && !vehicleLock.IsVehicleLocked(), "the drawn car stands open for its medic");
		ctx.True(LF_MedicCarPool.CarOf(MEDIC) == first, "the pool remembers the medic's car");

		// A second draw at once is refused; after the wait it sends the first car back and puts a new one out.
		ctx.True(LF_MedicCarPool.Draw(MEDIC, medic, board, 0, refusal, arg) == null, "a draw seconds after the last is refused");
		ctx.EqualStr(LF_MedicCarPool.REFUSED_TOO_SOON, refusal, "the refusal says to wait");

		// A second medic finds the bay taken.
		second.SetFaction(LF_Faction.MEDIC);
		second.SetMedicRank(1);
		second.SetOnDuty(true);
		ctx.True(LF_MedicCarPool.Draw(SECOND, medic, board, 1, refusal, arg) == null, "a second medic finds the bay taken");
		ctx.EqualStr(LF_MedicCarPool.REFUSED_BAY_BLOCKED, refusal, "the refusal says the bay is blocked");

		LF_MedicCarPool.ClearCooldownForTest(MEDIC);
		EntityID firstId = first.GetID();
		IEntity again = LF_MedicCarPool.Draw(MEDIC, medic, board, 0, refusal, arg);
		ctx.NotNull(again, "the medic draws again after the wait");
		ctx.True(again != first, "the new draw is a new car");
		ctx.True(LF_MedicCarPool.CarOf(MEDIC) == again, "the pool now remembers the new car");
		cars.RemoveItem(first);
		if (again)
			cars.Insert(again);

		ctx.True(GetGame().GetWorld().FindEntityByID(firstId) == null, "the first car went back to the pool");
	}

	//------------------------------------------------------------------------------------------------
	protected void Reset(LF_PlayerAccount account)
	{
		if (!account)
			return;

		account.SetOnDuty(false);
		account.SetMedicRank(0);
		account.SetFaction(LF_Faction.CIVILIAN);
	}
}
