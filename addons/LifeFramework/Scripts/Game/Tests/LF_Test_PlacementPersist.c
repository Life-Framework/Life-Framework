// red-proof: observed 2026-11-17 with `--red` (log test-20261117-231653): RED-OK placement/persist, the load skipped: FAIL "both placements are rows again after the load" (expected 2, got 0), "the first chair stands after the load". Hook off: green.
// tier: PERSISTENCE
//! placement/persist (#1032, slice #2653): two permanent placements go through the "placements" payload
//! and come back once each at their spot; a second apply makes no duplicate; a row whose prefab no
//! longer resolves is skipped (LF_Debug.Error) and not kept.
//! Across a restart (`tools\cli wt e2e`): every non-reload run records two e2e chairs and picks one up
//! before the save; the reload boot finds the kept one standing and the picked-up one gone.
class LF_Test_PlacementPersist : LF_Test
{
	static const ResourceName CHAIR = "{69D6D9BFDFEC3225}Prefabs/Props/Seats/LF_Seat_Chair_02_green.et";
	protected static const string OWNER = "placement-persist-owner";
	protected static const string E2E_OWNER = "placement-persist-e2e-owner";
	protected static const string E2E_KEPT = "e2e-kept-chair";
	protected static const string E2E_PICKED = "e2e-picked-chair";
	//! Inside the test annex (x/z 18..114), never on the town.
	protected static const vector SPOT = "100 0 100";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "placement/persist";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		LF_PersistenceComponent persistence;
		if (gameMode)
			persistence = LF_PersistenceComponent.Cast(gameMode.FindComponent(LF_PersistenceComponent));
		ctx.NotNull(persistence, "the game mode carries LF_PersistenceComponent");
		if (!persistence)
			return;

#ifdef LF_E2E_LOAD_BOOT
		// The reload boot: the kept chair came back from the real save, the picked-up one did not.
		ctx.Equal(1, CountKind(E2E_OWNER, E2E_KEPT), "the kept e2e chair is a row after the restart");
		ctx.Equal(0, CountKind(E2E_OWNER, E2E_PICKED), "the picked-up e2e chair stays gone after the restart");
		ctx.True(StandingKind(E2E_OWNER, E2E_KEPT), "the kept e2e chair stands after the restart");
		return;
#endif

		ctx.True(LF_PersistenceComponentSerializer.PayloadSinceVersion("placements") > 0, "placements is a registered payload");

		IEntity first = Spawn(SPOT);
		IEntity second = Spawn(SPOT + "3 0 0");
		if (!first || !second)
		{
			ctx.Fail("the chair prefab spawns");
			LF_TestCharacterFixture.Delete(first);
			LF_TestCharacterFixture.Delete(second);
			return;
		}

		string firstId = LF_PlacementSave.Record(OWNER, "chair", first);
		string secondId = LF_PlacementSave.Record(OWNER, "chair", second);
		vector firstAt = first.GetOrigin();
		ctx.Equal(2, LF_PlacementSave.CountOwned(OWNER), "two permanent placements are recorded for the owner");

		// Round trip through the payload, as a restart does.
		JsonSaveContext saveContext = new JsonSaveContext(false);
		LF_PersistenceComponentSerializer.WritePersistencePayload(persistence, saveContext, "placements");
		string saved = saveContext.SaveToString();
		ctx.True(saved.Contains(firstId) && saved.Contains(secondId), "the save carries both placement ids");
		ctx.True(saved.Contains("LF_Seat_Chair_02_green"), "the save carries the prefab");

		// A fresh boot: neither the rows nor the chairs.
		LF_PlacementSave.Forget(firstId);
		LF_PlacementSave.Forget(secondId);
		LF_TestCharacterFixture.Delete(first);
		LF_TestCharacterFixture.Delete(second);
		ctx.Equal(0, LF_PlacementSave.CountOwned(OWNER), "a fresh boot has no placements for the owner");

		if (!RedProof())
			Load(ctx, persistence, saved);

		IEntity firstBack = LF_PlacementSave.LiveEntity(firstId);
		IEntity secondBack = LF_PlacementSave.LiveEntity(secondId);
		ctx.Equal(2, LF_PlacementSave.CountOwned(OWNER), "both placements are rows again after the load");
		ctx.NotNull(firstBack, "the first chair stands after the load");
		ctx.NotNull(secondBack, "the second chair stands after the load");
		if (firstBack)
			ctx.True(vector.Distance(firstBack.GetOrigin(), firstAt) < 0.05, "the first chair is back at its spot");

		// A second apply (a load re-applied to a live session) makes no duplicate.
		Load(ctx, persistence, saved);
		ctx.Equal(2, LF_PlacementSave.CountOwned(OWNER), "a second load keeps two rows");
		ctx.True(firstBack && LF_PlacementSave.LiveEntity(firstId) == firstBack, "a second load keeps the same first chair");
		ctx.True(secondBack && LF_PlacementSave.LiveEntity(secondId) == secondBack, "a second load keeps the same second chair");
		ctx.Equal(2, CountChairsNear(SPOT, 2.5), "two chairs stand at the spot, not four");

		// A row whose prefab no longer resolves is skipped and not kept.
		array<ref LF_PlacementRow> broken = {};
		vector brokenAt[4];
		Math3D.MatrixIdentity4(brokenAt);
		brokenAt[3] = SPOT + "0 0 4";
		broken.Insert(LF_PlacementRow.Create("pl-broken", "{0000000000000000}Prefabs/Missing/Gone.et", brokenAt, OWNER, "chair", "", ""));
		ctx.Equal(0, LF_PlacementSave.ApplyAll(broken), "a row with a missing prefab does not stand");
		ctx.True(LF_PlacementSave.Find("pl-broken") == null, "a row with a missing prefab is not kept");

		LF_PlacementSave.Forget(firstId);
		LF_PlacementSave.Forget(secondId);
		LF_TestCharacterFixture.Delete(firstBack);
		LF_TestCharacterFixture.Delete(secondBack);

		// Every non-reload run (the e2e save boot among them) leaves one kept and one picked-up e2e chair.
		BookE2E(ctx);
	}

	//------------------------------------------------------------------------------------------------
	protected void BookE2E(LF_TestContext ctx)
	{
		if (CountKind(E2E_OWNER, E2E_KEPT) > 0)
			return;

		IEntity kept = Spawn(SPOT + "0 0 -8");
		IEntity picked = Spawn(SPOT + "3 0 -8");
		IEntity owner = LF_TestCharacterFixture.Spawn(ctx);
		if (!kept || !picked || !owner)
		{
			ctx.Fail("the e2e chairs and their owner spawn");
			LF_TestCharacterFixture.Delete(kept);
			LF_TestCharacterFixture.Delete(picked);
			LF_TestCharacterFixture.Delete(owner);
			return;
		}

		LF_PlacementSave.Record(E2E_OWNER, E2E_KEPT, kept);
		LF_PlacementSave.Record(E2E_OWNER, E2E_PICKED, picked);
		LF_Houses.BindCharacter(owner, E2E_OWNER);
		owner.SetOrigin(picked.GetOrigin() + "1 0 0");
		ctx.True(LF_PlacementPickup.PickUp(owner, picked), "the owner picks up the second e2e chair: " + LF_PlacementPickup.LastRefusal());
		ctx.Equal(1, LF_PlacementSave.CountOwned(E2E_OWNER), "one e2e chair is left to save");
		LF_TestCharacterFixture.Delete(owner);
	}

	//------------------------------------------------------------------------------------------------
	protected void Load(LF_TestContext ctx, LF_PersistenceComponent persistence, string saved)
	{
		JsonLoadContext loadContext = new JsonLoadContext(false);
		ctx.True(loadContext.LoadFromString(saved), "the save parses");
		LF_PersistenceComponentSerializer.ReadPersistencePayload(persistence, loadContext, "placements");
	}

	//------------------------------------------------------------------------------------------------
	static IEntity Spawn(vector at)
	{
		at[1] = GetGame().GetWorld().GetSurfaceY(at[0], at[2]);
		return LF_Utils.SpawnEntityPrefab(CHAIR, at, "30 0 0");
	}

	//------------------------------------------------------------------------------------------------
	protected static int CountKind(string ownerKey, string kind)
	{
		int count;
		foreach (LF_PlacementRow row : LF_PlacementSave.ExportAll())
		{
			if (row && row.m_sOwnerKey == ownerKey && row.m_sKind == kind)
				count++;
		}

		return count;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool StandingKind(string ownerKey, string kind)
	{
		foreach (LF_PlacementRow row : LF_PlacementSave.ExportAll())
		{
			if (row && row.m_sOwnerKey == ownerKey && row.m_sKind == kind && LF_PlacementSave.LiveEntity(row.m_sPlacementId))
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected int m_iChairsFound;

	//------------------------------------------------------------------------------------------------
	//! Every chair entity in the world near `at`, tracked or not: a duplicate spawn would count.
	protected int CountChairsNear(vector at, float radius)
	{
		m_iChairsFound = 0;
		at[1] = GetGame().GetWorld().GetSurfaceY(at[0], at[2]);
		GetGame().GetWorld().QueryEntitiesBySphere(at + "1.5 0 0", radius, CountChair, null, EQueryEntitiesFlags.ALL);
		return m_iChairsFound;
	}

	//------------------------------------------------------------------------------------------------
	protected bool CountChair(IEntity entity)
	{
		if (entity && entity.GetPrefabData() && LF_Utils.SamePrefab(entity.GetPrefabData().GetPrefabName(), CHAIR))
			m_iChairsFound++;

		return true;
	}
}
