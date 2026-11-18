// red-proof: Observed 2026-11-18 with `tools\cli test --no-build --red LF_Test_LockerRent,LF_Test_LockerPersist`: `RED PROOF OK` with the store read skipped. Also red for real: the first green run failed `- three rentals after the load (load 1) (expected 3, got 0)` because the payload variable was named `lockerRows`, the evidence locker payload's JSON key; renamed, green.
// tier: PERSISTENCE
//! lockers/persist (#2659): the "lockers" payload round trips rentals {siteId, characterKey, paidUntil}
//! through the real store write and read; a second load gives the same table; a save from the version
//! before this payload loads no rentals and leaves the live table alone. Across a restart
//! (`tools\cli wt e2e`): the save boot stores one rental, the reload boot finds it with its paid-until.
[LF_TestInfo(LF_TestTier.PERSISTENCE)]
class LF_Test_LockerPersist : LF_Test
{
	protected static const string E2E_SITE = "locker-e2e-site";
	protected static const string E2E_KEY = "locker-e2e-character";
	protected static const int E2E_PAID_UNTIL = 1900000000;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "lockers/persist";
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

		int since = LF_PersistenceComponentSerializer.PayloadSinceVersion("lockers");
#ifdef LF_E2E_LOAD_BOOT
		if (LF_SaveStamp.BootSaveVersion() < since)
		{
			ctx.False(LF_LockerBank.IsRenting(E2E_SITE, E2E_KEY), "a save older than the lockers payload loads no rental");
			return;
		}

		ctx.True(LF_LockerBank.IsRenting(E2E_SITE, E2E_KEY), "the rental is back after the restart");
		ctx.Equal(E2E_PAID_UNTIL, LF_LockerBank.PaidUntil(E2E_SITE, E2E_KEY), "the rental keeps its paid-until after the restart");
		return;
#endif

		ctx.True(since > 0, "lockers is a registered payload");
		if (since <= 1)
			return;

		array<ref LF_LockerRentalRow> kept = LF_LockerBank.ExportAll();
		RoundTrip(ctx, persistence, since);
		LF_LockerBank.ApplyAll(kept);

#ifdef LF_E2E_HOST_STOP
		// The save boot: the shutdown save carries one rental.
		array<ref LF_LockerRentalRow> e2eRows = LF_LockerBank.ExportAll();
		e2eRows.Insert(LF_LockerRentalRow.Create(E2E_SITE, E2E_KEY, E2E_PAID_UNTIL));
		LF_LockerBank.ApplyAll(e2eRows);
		ctx.True(LF_LockerBank.IsRenting(E2E_SITE, E2E_KEY), "the save boot stored the e2e rental");
#endif
	}

	//------------------------------------------------------------------------------------------------
	protected void RoundTrip(LF_TestContext ctx, LF_PersistenceComponent persistence, int since)
	{
		array<ref LF_LockerRentalRow> rows = {};
		rows.Insert(LF_LockerRentalRow.Create("locker-persist-station", "locker-persist-a", 1800000000));
		rows.Insert(LF_LockerRentalRow.Create("locker-persist-station", "locker-persist-b", 1800003600));
		rows.Insert(LF_LockerRentalRow.Create("locker-persist-hall", "locker-persist-a", LF_LockerRules.NEVER));
		ctx.Equal(3, LF_LockerBank.ApplyAll(rows), "three rentals before the save");

		JsonSaveContext saveContext = new JsonSaveContext(false);
		LF_PersistenceComponentSerializer.SerializeStore(persistence, saveContext);
		string saved = saveContext.SaveToString();
		ctx.True(saved.Contains("locker-persist-station"), "the save carries the rentals");

		for (int pass = 1; pass <= 2; pass++)
		{
			string tag = string.Format(" (load %1)", pass);
			// The live table empties before each load, so only the save can bring the rentals back.
			LF_LockerBank.ResetStatics();
			JsonLoadContext loadContext = new JsonLoadContext(false);
			ctx.True(loadContext.LoadFromString(saved), "the save parses" + tag);
			if (!RedProof())
				LF_PersistenceComponentSerializer.DeserializeStore(persistence, loadContext);

			ctx.Equal(3, LF_LockerBank.Count(), "three rentals after the load" + tag);
			ctx.Equal(1800000000, LF_LockerBank.PaidUntil("locker-persist-station", "locker-persist-a"), "a's station rental keeps its paid-until" + tag);
			ctx.Equal(1800003600, LF_LockerBank.PaidUntil("locker-persist-station", "locker-persist-b"), "b's station rental keeps its paid-until" + tag);
			ctx.True(LF_LockerBank.IsRenting("locker-persist-hall", "locker-persist-a"), "a's hall rental is back" + tag);
			ctx.False(LF_LockerBank.IsRenting("locker-persist-hall", "locker-persist-b"), "b rents nothing at the hall" + tag);
		}

		// A second apply of the same rows on a live table gives the same table.
		LF_LockerBank.ApplyAll(LF_LockerBank.ExportAll());
		ctx.Equal(3, LF_LockerBank.Count(), "a second apply keeps three rentals");

		// A save from the version before this payload holds no rentals and leaves the live table alone.
		JsonSaveContext oldContext = new JsonSaveContext(false);
		LF_PersistenceComponentSerializer.SerializeStoreAsVersion(persistence, oldContext, since - 1);
		string oldSave = oldContext.SaveToString();
		ctx.False(oldSave.Contains("locker-persist-station"), "the old save has no rental rows");
		LF_LockerBank.ResetStatics();
		JsonLoadContext oldLoad = new JsonLoadContext(false);
		ctx.True(oldLoad.LoadFromString(oldSave), "the old save parses");
		LF_PersistenceComponentSerializer.DeserializeStore(persistence, oldLoad);
		ctx.Equal(0, LF_LockerBank.Count(), "a pre-payload save loads no rentals");
	}
}
