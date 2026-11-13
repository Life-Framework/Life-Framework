// red-proof: observed 2026-11-10 with `tools\cli test --no-build --only LF_Test_ChainStationsPersist`: a wrong expected value failed `with the same 3 units of work (expected 2, got 3)` and `SUMMARY tier=only passed=0 failed=3` (run with its two siblings); restored, green.

// tier: PERSISTENCE
//! A half-finished batch rides the real save path (the "chainstations" payload, save version 20):
//! after the store is wiped the barrel is empty, after the payload is read it is back at the same
//! work and still finishes with the remaining units, and reading twice changes nothing. A saved
//! batch whose barrel is not in the world waits and is kept, and a batch for a changed step is
//! dropped. The test puts every barrel back the way it found it.
class LF_Test_ChainStationsPersist : LF_Test
{
	override string GetName()
	{
		return "persist/chain-stations-roundtrip";
	}

	override void Run(LF_TestContext ctx)
	{
		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		ctx.NotNull(gameMode, "the DebugWorld game mode resolves");
		if (!gameMode)
			return;

		LF_PersistenceComponent persistence = LF_PersistenceComponent.Cast(gameMode.FindComponent(LF_PersistenceComponent));
		ctx.NotNull(persistence, "the game mode carries LF_PersistenceComponent");
		LF_ChainStationComponent barrel = LF_ChainStations.Find(LF_TestAnnex.CHAINS_BARREL_2);
		LF_ChainStationComponent other = LF_ChainStations.Find(LF_TestAnnex.CHAINS_BARREL_3);
		ctx.NotNull(barrel, "barrel 2 of the site is registered");
		ctx.NotNull(other, "barrel 3 of the site is registered");
		if (!persistence || !barrel || !other)
			return;

		array<ref LF_ChainStationRow> before = LF_ChainStations.ExportAll();

		// Barrel 2 is loaded and 3 of 8 units are in; barrel 3 stays empty.
		LF_ChainStations.ApplyAll(null);
		barrel.GetProgress().Restore(true, 3, 8);
		ctx.Equal(37, barrel.GetPercent(), "the barrel is part way");

		JsonSaveContext saveContext = new JsonSaveContext(false);
		ctx.True(LF_PersistenceComponentSerializer.SerializeStore(persistence, saveContext) == ESerializeResult.OK, "the store serializes");
		string saved = saveContext.SaveToString();
		ctx.True(saved.Contains(LF_TestAnnex.CHAINS_BARREL_2), "the save carries the batch");
		ctx.False(saved.Contains(LF_TestAnnex.CHAINS_BARREL_3), "an empty barrel is not saved");

		// A restart: the world comes back with empty barrels.
		LF_ChainStations.ApplyAll(null);
		ctx.Equal(LF_StationProgress.EMPTY, barrel.GetPercent(), "the restarted barrel is empty");

		JsonLoadContext loadContext = new JsonLoadContext(false);
		ctx.True(loadContext.LoadFromString(saved), "the save parses");
		int version;
		loadContext.ReadValue("version", version);
		ctx.Equal(29, version, "the save is written at version 29 (chainstations is payload 20)");
		LF_PersistenceComponentSerializer.ReadPersistencePayload(persistence, loadContext, "chainstations");
		ctx.Equal(37, barrel.GetPercent(), "the half-finished batch is back at 37 percent");
		ctx.Equal(3, barrel.GetProgress().GetWork(), "with the same 3 units of work");
		ctx.Equal(8, barrel.GetProgress().GetNeeded(), "and the same need");
		ctx.Equal(LF_StationProgress.EMPTY, other.GetPercent(), "the other barrel is still empty");

		// Loading again converges (idempotent), and the batch still finishes.
		JsonLoadContext again = new JsonLoadContext(false);
		again.LoadFromString(saved);
		again.ReadValue("version", version);
		LF_PersistenceComponentSerializer.ReadPersistencePayload(persistence, again, "chainstations");
		ctx.Equal(3, barrel.GetProgress().GetWork(), "a second load leaves the same work");
		ctx.Equal(1, LF_ChainStations.ExportAll().Count(), "and exactly one saved batch");
		ctx.True(barrel.GetProgress().AddWork(5), "the remaining 5 units finish the restored batch");
		ctx.Equal(LF_StationProgress.READY, barrel.GetPercent(), "the restored batch reads ready");

		// A batch for a barrel that is not in the world waits, and is kept across a save.
		array<ref LF_ChainStationRow> ghosts = new array<ref LF_ChainStationRow>();
		LF_ChainStationRow ghost = new LF_ChainStationRow();
		ghost.m_sStationKey = "Chains_Ghost_Barrel";
		ghost.m_sChainId = "cider";
		ghost.m_sStepId = "press";
		ghost.m_iWork = 2;
		ghost.m_iNeeded = 8;
		ghosts.Insert(ghost);
		LF_ChainStations.ApplyAll(ghosts);
		ctx.Equal(1, LF_ChainStations.ExportAll().Count(), "a batch with no barrel yet is kept");
		ctx.Equal(LF_StationProgress.EMPTY, barrel.GetPercent(), "and does not touch the barrels that exist");

		// A batch saved for a step the barrel no longer runs is dropped.
		LF_ChainStationRow stale = new LF_ChainStationRow();
		stale.m_sStationKey = LF_TestAnnex.CHAINS_BARREL_2;
		stale.m_sChainId = "cider";
		stale.m_sStepId = "a-step-that-was-removed";
		stale.m_iWork = 4;
		stale.m_iNeeded = 8;
		barrel.Import(stale);
		ctx.Equal(LF_StationProgress.EMPTY, barrel.GetPercent(), "a batch for a changed step is dropped");

		// Put the world back.
		LF_ChainStations.ApplyAll(before);
	}
}
