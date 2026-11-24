// red-proof: run on the bounds-based bay check (a sphere query that counts a car whose box merely reaches the sphere) the draw with a car 5 m from the bay centre is refused and "a car beside the bay does not block it" fails (#4878).
//! #4878: the first ambulance draw of a playtest was refused with "a vehicle stands in the bay" and nothing in it; the
//! next draw worked. The check counted any vehicle whose bounding box reached 4 m of the bay centre, and the taxi cab
//! parked 7.9 m away sits at the edge of that reach, so a cab that settled a little closer blocked the bay. The bay
//! is blocked by a vehicle whose origin is within `BAY_CLEAR_METRES` of the centre (it would overlap the new car);
//! a car beside the bay, bounds near but origin beyond that, does not block it, and a car on the centre does.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_MedicBayEdge : LF_Test
{
	protected static const ResourceName BOARD = "{7626F19FBE72C466}Prefabs/Props/Medic/LF_MedicCarPoolBoard.et";
	protected static const ResourceName CAR = "{E1A105BE1A000001}Prefabs/Vehicles/Wheeled/S105/EL_S105_beige.et";
	protected static const string MEDIC = "medic-bay-edge-medic";
	//! Metres along the bay's long axis: past the clear radius, where the bounds of a car whose length lies along it still reach the old sphere.
	protected static const float BESIDE_METRES = 5.0;
	protected static const float ON_BAY_METRES = 2.0;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "medic/bay-edge";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		array<ref LF_MedicRank> ranks = {};
		LF_MedicRanks.Ranks(ranks);
		ctx.True(ranks.Count() >= 1, "the medic ladder has a rung");
		if (ranks.Count() < 1)
			return;

		LF_MedicCarPool.ResetStatics();
		array<ref LF_MedicFleetRow> rows = new array<ref LF_MedicFleetRow>();
		rows.Insert(LF_MedicFleetRow.Create(ranks[0].m_sId, CAR));
		LF_MedicFleet.UseRowsForTest(rows);

		IEntity medic = LF_TestCharacterFixture.Spawn(ctx);
		if (!medic)
			return;

		IEntity board = LF_Utils.SpawnEntityPrefab(BOARD, medic.GetOrigin() + "2 0 0");
		ctx.NotNull(board, "the medic pool board spawns");
		LF_PlayerAccount account = LF_PlayerAccountManager.GetOrCreate(MEDIC);
		LF_MedicCarPoolComponent pool;
		if (board)
			pool = LF_Component<LF_MedicCarPoolComponent>.Find(board);

		array<IEntity> cars = {};
		if (pool && account)
			RunBay(ctx, medic, board, pool, account, cars);

		foreach (IEntity car : cars)
		{
			LF_TestCharacterFixture.Delete(car);
		}

		LF_TestCharacterFixture.Delete(LF_MedicCarPool.CarOf(MEDIC));
		LF_TestCharacterFixture.Delete(board);
		LF_TestCharacterFixture.Delete(medic);
		if (account)
		{
			account.SetOnDuty(false);
			account.SetFaction(LF_Faction.CIVILIAN);
		}

		LF_MedicCarPool.ResetStatics();
		LF_MedicFleet.Reload();
	}

	//------------------------------------------------------------------------------------------------
	protected void RunBay(LF_TestContext ctx, IEntity medic, IEntity board, LF_MedicCarPoolComponent pool, LF_PlayerAccount account, notnull array<IEntity> cars)
	{
		account.SetFaction(LF_Faction.MEDIC);
		account.SetMedicRank(0);
		account.SetOnDuty(true);

		vector bay[4];
		pool.BayTransform(bay);
		vector angles = Math3D.MatrixToAngles(bay);
		string refusal;
		string arg;

		// A car on the bay's centre blocks it, and the refusal names that car.
		IEntity onBay = LF_Utils.SpawnEntityPrefab(CAR, bay[3] + bay[2] * ON_BAY_METRES + "0 0.3 0", angles);
		ctx.NotNull(onBay, "a car stands near the bay centre");
		if (onBay)
			cars.Insert(onBay);

		ctx.True(LF_MedicCarPool.Draw(MEDIC, medic, board, 0, refusal, arg) == null, "a car near the bay centre blocks the draw");
		ctx.EqualStr(LF_MedicCarPool.REFUSED_BAY_BLOCKED, refusal, "the refusal says the bay is blocked");
		LF_TestCharacterFixture.Delete(onBay);
		cars.RemoveItem(onBay);

		// A car beside the bay, its length along the bay's axis, origin past the clear radius: not a blocker.
		IEntity beside = LF_Utils.SpawnEntityPrefab(CAR, bay[3] + bay[2] * BESIDE_METRES + "0 0.3 0", angles);
		ctx.NotNull(beside, "a car stands beside the bay");
		if (beside)
			cars.Insert(beside);

		LF_MedicCarPool.ClearCooldownForTest(MEDIC);
		IEntity drawn = LF_MedicCarPool.Draw(MEDIC, medic, board, 0, refusal, arg);
		ctx.NotNull(drawn, "a car beside the bay does not block it");
		ctx.EqualStr("", refusal, "a draw beside a parked car has no refusal");
		if (drawn)
			cars.Insert(drawn);
	}
}
