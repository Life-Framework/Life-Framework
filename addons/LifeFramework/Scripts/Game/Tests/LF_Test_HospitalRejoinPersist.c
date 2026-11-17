// red-proof: Observed 2026-11-17 with `tools\cli test --no-build --red LF_Test_HospitalRejoinPersist` (RED-OK persist/hospital-rejoin): with the payload read skipped, failed "the hospital wake is back (load 2)", "one rejoin row, however many loads (load 2) (expected 1, got 0)" and "the rejoin after the restart writes the bill"; green on the real code. The restart path is `tools\cli wt e2e`.
// tier: PERSISTENCE
//! #2101: a player who logs out downed still wakes at the hospital with the bill when the server
//! restarts between the logout and the rejoin. The logout is recorded by the real rule
//! (LF_DisconnectRules.Apply), saved as the "hospitalrejoin" payload, the session is wiped, and the
//! load puts the pending wake back: the rejoin is `hospital` and the bill is written for that uid, once.
//! Loading twice gives one pending rejoin; a save from before the payload (version 39) loads nothing.
//! Across a restart (`tools\cli wt e2e`): the save boot records E2E_UID, the shutdown save carries
//! it, and the reload boot finds the wake pending and writes the bill.
class LF_Test_HospitalRejoinPersist : LF_Test
{
	protected static const string UID = "hospital-rejoin-persist-uid";
	protected static const string OTHER_UID = "hospital-rejoin-persist-other";
	//! Logged by no incident row, so the old-format save can be searched for it.
	protected static const string LEGACY_UID = "hospital-rejoin-legacy-uid";
	protected static const string E2E_UID = "hospital-rejoin-e2e-uid";
	protected static const int BILL = 150;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "persist/hospital-rejoin";
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
		// The reload boot: the downed logout of the save boot is still owed a hospital wake.
		ctx.True(LF_HospitalRespawn.HasPendingWake(E2E_UID), "the hospital wake is pending after the restart");
		ctx.True(LF_HospitalRejoin.IsPending(E2E_UID), "the rejoin row came back from the real save");
		LF_TreatmentBillRecord restarted = LF_HospitalRespawn.BillOnWake(E2E_UID, BILL, "restart");
		ctx.NotNull(restarted, "the rejoin after the restart writes the bill");
		if (restarted)
			ctx.True(restarted.m_sPatientUid == E2E_UID, "the bill is for the player who logged out downed");
		ctx.False(LF_HospitalRespawn.HasPendingWake(E2E_UID), "waking takes the wake once");
		return;
#endif

		array<ref LF_HospitalRejoinRow> before = LF_HospitalRejoin.ExportAll();

		LF_ConfigService.Reset();
		LF_DisconnectConfig.Reload();
		RoundTrip(ctx, persistence);

		LF_TestLegacySnapshot kept = LF_TestLegacySnapshot.Take();
		CheckLegacy(ctx, persistence, LF_PersistenceComponentSerializer.PayloadSinceVersion("hospitalrejoin") - 1);
		kept.Restore();

		Wipe(UID);
		Wipe(OTHER_UID);
		Wipe(LEGACY_UID);
		LF_DisconnectConfig.Reload();
		LF_HospitalRejoin.ApplyAll(before);
#ifdef LF_E2E_HOST_STOP
		// The save boot: one downed logout carried by the shutdown save.
		ctx.True(LF_HospitalRejoin.Record(E2E_UID, 1700000500), "the save boot records the downed logout");
		LF_HospitalRespawn.MarkDied(E2E_UID);
#endif
	}

	//------------------------------------------------------------------------------------------------
	//! Drops uid's pending wake without a bill (a zero bill writes nothing and still takes the wake).
	protected void Wipe(string uid)
	{
		LF_HospitalRespawn.BillOnWake(uid, 0, "");
	}

	//------------------------------------------------------------------------------------------------
	protected void RoundTrip(LF_TestContext ctx, LF_PersistenceComponent persistence)
	{
		LF_HospitalRejoin.ApplyAll(null);
		Wipe(UID);
		Wipe(OTHER_UID);

		// The real rule: a downed logout dies and owes the hospital wake.
		LF_DisconnectState state = new LF_DisconnectState();
		state.m_sPlayerUid = UID;
		state.m_bDowned = true;
		LF_DisconnectDecision decision = LF_DisconnectRules.Apply(state, null);
		ctx.True(decision.m_bDownedDies, "the downed logout dies");
		ctx.True(LF_DisconnectRules.RejoinFor(decision) == LF_DisconnectRules.REJOIN_HOSPITAL, "the rejoin decision is the hospital");
		ctx.Equal(1, LF_HospitalRejoin.Count(), "the downed logout is recorded");

		// The kill that follows the rule marks the session wake (LF_SpawnLogic's death hook).
		LF_HospitalRespawn.MarkDied(UID);
		ctx.True(LF_HospitalRejoin.IsPending(UID), "the downed logout owes the hospital wake");

		// A player standing at logout owes nothing, and an empty uid records nothing.
		LF_DisconnectState standing = new LF_DisconnectState();
		standing.m_sPlayerUid = OTHER_UID;
		standing.m_iWanted = 1;
		LF_DisconnectRules.Apply(standing, null);
		ctx.False(LF_HospitalRejoin.IsPending(OTHER_UID), "a logout that is not downed is not recorded");
		ctx.False(LF_HospitalRejoin.Record("", 1), "an empty uid is not recorded");
		ctx.Equal(1, LF_HospitalRejoin.Count(), "one downed logout is recorded");

		// A downed logout whose kill never happened owes no wake, so the save does not carry it.
		ctx.True(LF_HospitalRejoin.Record(OTHER_UID, 1700000100), "a logout is recorded before its kill");
		ctx.False(LF_HospitalRespawn.HasPendingWake(OTHER_UID), "a recorded logout alone is no wake");

		JsonSaveContext saveContext = new JsonSaveContext(false);
		LF_PersistenceComponentSerializer.WritePersistencePayload(persistence, saveContext, "hospitalrejoin");
		string saved = saveContext.SaveToString();
		ctx.True(saved.Contains(UID), "the save carries the downed logout");
		ctx.False(saved.Contains(OTHER_UID), "the save carries no logout whose kill did not happen");

		// The restart: the session forgets the logout and its wake.
		LF_HospitalRejoin.ApplyAll(null);
		Wipe(UID);
		ctx.False(LF_HospitalRespawn.HasPendingWake(UID), "the restarted session has no pending wake");
		ctx.Equal(0, LF_HospitalRejoin.Count(), "the restarted session has no rejoin rows");

		for (int pass = 1; pass <= 2; pass++)
		{
			string tag = string.Format(" (load %1)", pass);
			JsonLoadContext loadContext = new JsonLoadContext(false);
			ctx.True(loadContext.LoadFromString(saved), "the save parses" + tag);
			if (!RedProof())
				LF_PersistenceComponentSerializer.ReadPersistencePayload(persistence, loadContext, "hospitalrejoin");

			ctx.True(LF_HospitalRespawn.HasPendingWake(UID), "the hospital wake is back" + tag);
			ctx.Equal(1, LF_HospitalRejoin.Count(), "one rejoin row, however many loads" + tag);
		}

		// The rejoin: the bill is written for that uid, once.
		LF_TreatmentBillRecord bill = LF_HospitalRespawn.BillOnWake(UID, BILL, "after the restart");
		ctx.NotNull(bill, "the rejoin after the restart writes the bill");
		if (bill)
		{
			ctx.True(bill.m_sPatientUid == UID, "the bill is for the player who logged out downed");
			ctx.Equal(BILL, bill.m_iAmount, "the bill is the configured amount");
		}
		ctx.True(LF_HospitalRespawn.BillOnWake(UID, BILL, "again") == null, "no second bill for one logout");

		// A woken player is not woken again: the next save drops the row.
		JsonSaveContext after = new JsonSaveContext(false);
		LF_PersistenceComponentSerializer.WritePersistencePayload(persistence, after, "hospitalrejoin");
		ctx.False(after.SaveToString().Contains(UID), "the save after the wake carries no row");
		ctx.Equal(0, LF_HospitalRejoin.Count(), "the woken row is gone");
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckLegacy(LF_TestContext ctx, LF_PersistenceComponent persistence, int version)
	{
		// A live row exists while saving: the old format cannot carry it.
		LF_HospitalRejoin.ApplyAll(null);
		Wipe(LEGACY_UID);
		LF_HospitalRejoin.Record(LEGACY_UID, 1700000000);
		LF_HospitalRespawn.MarkDied(LEGACY_UID);

		JsonSaveContext saveContext = new JsonSaveContext(false);
		LF_PersistenceComponentSerializer.SerializeStoreAsVersion(persistence, saveContext, version);
		string saved = saveContext.SaveToString();
		ctx.False(saved.Contains(LEGACY_UID), string.Format("the version %1 save has no hospital-rejoin rows", version));

		// A fresh boot loads it, twice.
		LF_HospitalRejoin.ApplyAll(null);
		Wipe(LEGACY_UID);
		for (int pass = 1; pass <= 2; pass++)
		{
			string tag = string.Format(" (version %1 load %2)", version, pass);
			JsonLoadContext loadContext = new JsonLoadContext(false);
			ctx.True(loadContext.LoadFromString(saved), "the old save parses" + tag);
			ctx.True(LF_PersistenceComponentSerializer.DeserializeStore(persistence, loadContext), "the old save loads" + tag);
			ctx.Equal(0, LF_HospitalRejoin.Count(), "no hospital rejoin is pending" + tag);
			ctx.False(LF_HospitalRespawn.HasPendingWake(LEGACY_UID), "no hospital wake is pending" + tag);
		}
	}
}
