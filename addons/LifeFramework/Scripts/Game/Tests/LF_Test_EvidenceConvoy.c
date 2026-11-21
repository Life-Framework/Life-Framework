// red-proof: 2026-11-19 `tools/cli test --no-build --red LF_Test_EvidenceConvoy`: RED PROOF OK (with `RedProof()` on the truck is never moved to its target, so `arrival delivered the rows`, the despawn checks and the second convoy fail); green without it. Re-arm fix 2026-11-21: see LF_Test_EvidenceLockerFill (batched red run, both RED PROOF OK).
//! investigation/evidence-convoy (#3382): the evidence convoy. Firing it spawns the truck, the escort
//! car and the crate on the grid cell `scene-drive` (Grid_scene_drive.layer: a spawn, three waypoints
//! and a target); the crate holds the locker's rows; the rows stay in the locker while the truck is on
//! the road and leave it when the truck arrives, after which the whole convoy despawns. A convoy that
//! never arrives delivers nothing. GridWorld has no AIWorld, so the test moves the truck along the
//! route itself (what the AI driver does on a real map) and steps the convoy by hand. A scene with no
//! placed route starts no convoy and the locker keeps its rows.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_EvidenceConvoy : LF_Test
{
	static const string SCENE = "grid_scene_drive";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "investigation/evidence-convoy";
	}

	//------------------------------------------------------------------------------------------------
	protected static void Reset()
	{
		// Only this feature's and its sinks' statics; LF_StaticReset.ResetAll() would wipe every feature's.
		LF_EventScenePlanner.ResetStatics();
		LF_EventSceneSpawn.ResetStatics();
		LF_EventSceneDrive.ResetStatics();
		LF_EvidenceConvoy.ResetStatics();
		LF_EvidenceLocker.ResetStatics();
		LF_EvidenceLockerFill.ResetStatics();
	}

	//------------------------------------------------------------------------------------------------
	protected static void FillLocker(int rows)
	{
		for (int i = 0; i < rows; i++)
		{
			LF_EvidenceLocker.LogSeizure("{C8C36ADC9DFC5FBB}Prefabs/Props/Crates/Crate_01_red.et", "Test Warehouse", "event:test-officer");
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static bool Gone(EntityID id)
	{
		return GetGame().GetWorld().FindEntityByID(id) == null;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		if (!Replication.IsServer())
			return;

		Reset();
		BaseWorld world = GetGame().GetWorld();

		// 0. A convoy order on a map with no convoy markers starts nothing and the locker keeps its rows.
		FillLocker(3);
		LF_EvidenceConvoy.Install();
		LF_EventBus.Publish(LF_OnEvidenceConvoyDue.Create(3, 12, "full"));
		ctx.True(LF_EvidenceConvoy.Active() == null, "no placed convoy route: no convoy starts");
		ctx.Equal(3, LF_EvidenceLocker.Count(), "the locker keeps its rows when no convoy can leave");
		ctx.True(LF_EvidenceConvoy.Start(SCENE + "_nowhere", false, false) == null, "a scene with no markers starts no convoy");
		LF_EvidenceLocker.ResetStatics();
		ctx.True(LF_EvidenceConvoy.Start(SCENE, false, false) == null, "an empty locker starts no convoy");

		// 0b. A full locker whose convoy is refused (no route markers placed for the real scene id) must not
		// stay "due" for good: a cool-down is armed, and when it ends a still-full locker orders again.
		LF_EvidenceLockerFill.ResetStatics();
		FillLocker(LF_EvidenceLockerFill.Threshold());
		ctx.True(LF_EvidenceLockerFill.IsDue(), "a full locker orders a convoy");
		ctx.True(LF_EvidenceConvoy.Active() == null, "the refused start left no convoy");
		if (RedProof())
			LF_EvidenceLockerFill.ResetStatics();

		ctx.True(LF_EvidenceLockerFill.IsRearmPending(), "a refused start arms the re-order cool-down");
		LF_EvidenceLockerFill.RearmNow();
		ctx.True(LF_EvidenceLockerFill.IsDue(), "after the cool-down the still-full locker orders again");
		ctx.True(LF_EvidenceLockerFill.IsRearmPending(), "and, refused again, arms the next cool-down");
		LF_EvidenceLocker.ResetStatics();
		LF_EvidenceLockerFill.ResetStatics();

		// 1. Firing it on the grid cell: truck, escort and crate spawn; the crate holds the locker's rows.
		FillLocker(3);
		LF_EvidenceConvoy convoy = LF_EvidenceConvoy.Start(SCENE, false, false);
		ctx.NotNull(convoy, "the convoy starts");
		if (!convoy)
		{
			Reset();
			return;
		}

		IEntity truck = world.FindEntityByID(convoy.m_Crew.m_VehicleId);
		IEntity escort = world.FindEntityByID(convoy.m_EscortId);
		IEntity crate = world.FindEntityByID(convoy.m_CrateId);
		ctx.NotNull(truck, "the truck spawns");
		ctx.NotNull(escort, "the escort car spawns");
		ctx.NotNull(crate, "the crate spawns");
		ctx.True(LF_Utils.SamePrefab(LF_EvidenceConvoy.ESCORT, LF_Utils.GetPrefabName(escort)), "the escort is the police car");
		if (crate && truck)
			ctx.True(crate.GetParent() == truck, "the crate rides on the truck");

		ctx.Equal(3, convoy.m_aRows.Count(), "the crate holds the locker's rows");
		array<ref LF_EvidenceLockerRow> locker = LF_EvidenceLocker.ExportAll();
		ctx.Equal(locker.Count(), convoy.m_aRows.Count(), "one row in the crate for each in the locker");
		if (locker.Count() == convoy.m_aRows.Count())
		{
			for (int r = 0; r < locker.Count(); r++)
			{
				ctx.Equal(locker[r].m_iNumber, convoy.m_aRows[r].m_iNumber, string.Format("crate row %1 is the locker's row", r));
			}
		}

		ctx.Equal(3, LF_EvidenceLocker.Count(), "the rows stay in the locker while the truck is on the road");
		if (LF_NotificationManagerComponent.GetInstance())
			ctx.True(convoy.m_bNotified, "the on-duty police were told the transfer is under way");

		ctx.True(LF_EvidenceConvoy.Start(SCENE, false, false) == null, "a second convoy is refused while one is under way");
		ctx.NotNull(convoy.m_Drive, "the truck is driven");
		ctx.NotNull(convoy.m_Escort, "the escort is driven");
		if (!truck || !convoy.m_Drive)
		{
			convoy.m_aTestEyes = {Vector(100000, 0, 0)};
			convoy.Step(1);
			Reset();
			return;
		}

		vector start = truck.GetOrigin();
		convoy.m_aTestEyes = {start + Vector(LF_EventSceneSpawn.SIGHT_RANGE_M * 3, 15, 0)};
		EntityID truckId = truck.GetID();
		EntityID escortId = escort.GetID();
		EntityID crateId = convoy.m_CrateId;

		// 2. The truck drives the route; arrival delivers the rows and empties the locker.
		convoy.Step(1);
		ctx.False(convoy.m_bDelivered, "nothing is delivered before the truck arrives");
		if (!RedProof())
		{
			for (int i = 0; i < convoy.m_Drive.m_aRoute.Count(); i++)
			{
				truck.SetOrigin(convoy.m_Drive.m_aRoute[i]);
				convoy.Step(1);
			}
		}

		ctx.True(convoy.m_bDelivered, "arrival delivered the rows");
		ctx.Equal(0, LF_EvidenceLocker.Count(), "arrival empties the locker");
		ctx.Equal(3, LF_EvidenceConvoy.Delivered(), "three rows reached the central store");
		ctx.False(LF_EvidenceLockerFill.IsDue(), "the locker is no longer due");
		ctx.True(LF_EvidenceConvoy.LastOver().Contains("delivered"), "the end line says delivered (" + LF_EvidenceConvoy.LastOver() + ")");

		// 3. Out of sight, the convoy despawns whole.
		convoy.Step(1);
		ctx.True(convoy.m_bDone, "the convoy despawns after delivery");
		ctx.True(Gone(truckId), "the truck is gone");
		ctx.True(Gone(escortId), "the escort car is gone");
		ctx.True(Gone(crateId), "the crate is gone");
		ctx.True(LF_EvidenceConvoy.Active() == null, "no convoy is under way afterwards");
		ctx.EqualStr("", LF_EventScenePlanner.RunningScene(), "the scene slot is free again");

		// 4. A convoy that never arrives delivers nothing: the rows stay in the locker.
		LF_EvidenceLocker.ResetStatics();
		LF_EvidenceConvoy.ResetStatics();
		FillLocker(2);
		convoy = LF_EvidenceConvoy.Start(SCENE, false, false);
		ctx.NotNull(convoy, "a second convoy starts once the first is over");
		if (!convoy)
		{
			Reset();
			return;
		}

		truck = world.FindEntityByID(convoy.m_Crew.m_VehicleId);
		escortId = convoy.m_EscortId;
		truckId = convoy.m_Crew.m_VehicleId;
		if (truck)
			convoy.m_aTestEyes = {truck.GetOrigin() + Vector(LF_EventSceneSpawn.SIGHT_RANGE_M * 3, 15, 0)};

		convoy.Step(LF_EvidenceConvoy.MAX_SECONDS);
		ctx.False(convoy.m_bDelivered, "a convoy that times out delivers nothing");
		ctx.Equal(2, LF_EvidenceLocker.Count(), "its rows are still in the locker");
		convoy.Step(1);
		ctx.True(convoy.m_bDone, "the called-off convoy despawns");
		ctx.True(Gone(truckId) && Gone(escortId), "its truck and escort are gone");

		Reset();
	}
}
