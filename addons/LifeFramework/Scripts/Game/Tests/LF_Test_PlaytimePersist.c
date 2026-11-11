// red-proof: observed 2026-11-11 with `tools\cli test --no-build --only LF_Test_PlaytimePersist`: LF_Playtime.ApplyAll adding to
// the live rows instead of replacing them failed `- the restarted session starts empty (expected 0, got 3990)` and
// `- the total is back, not doubled (load 2) (expected 3990, got 11970)`. Restored; green. The restart path is `wt e2e`.
// tier: PERSISTENCE
//! Playtime per account and per job (#692) rides the real save path (the "playtime" payload, save
//! version 26). The job comes from duty and sign-up accessors; a clock credits the seconds since
//! its last credit to the account's job now, caps a wall-clock jump and stops on disconnect. The
//! counts come back after the store is wiped, a second load gives the same counts, and a version
//! 25 save leaves the live counts alone. The test puts back what it found.
class LF_Test_PlaytimePersist : LF_Test
{
	protected static const string UID = "playtime-save-uid";
	protected static const string OTHER_UID = "playtime-other-uid";
	protected static const int PLAYER_ID = 990692;

	override string GetName()
	{
		return "account/playtime";
	}

	override void Run(LF_TestContext ctx)
	{
		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		ctx.NotNull(gameMode, "the DebugWorld game mode resolves");
		if (!gameMode)
			return;

		LF_PersistenceComponent persistence = LF_PersistenceComponent.Cast(gameMode.FindComponent(LF_PersistenceComponent));
		ctx.NotNull(persistence, "the game mode carries LF_PersistenceComponent");
		if (!persistence)
			return;

		ctx.True(LF_Playtime.IsRunning(), "the playtime clock started with the game mode");
		array<ref LF_PlaytimeRecord> before = LF_Playtime.ExportAll();
		LF_Playtime.ApplyAll(null);

		CheckJobs(ctx);
		CheckClock(ctx);

		ctx.True(LF_Playtime.Credit(UID, "POLICE", 3600), "an hour on police duty is credited");
		ctx.True(LF_Playtime.Credit(OTHER_UID, "PARCEL", 120), "another account's courier time is credited");
		ctx.False(LF_Playtime.Credit(UID, "", 10), "a credit with no job is refused");
		ctx.False(LF_Playtime.Credit(UID, "POLICE", 0), "a credit of no time is refused");
		ctx.Equal(3600, LF_Playtime.JobSeconds(UID, "POLICE"), "the account has an hour as police");
		ctx.Equal(390, LF_Playtime.JobSeconds(UID, LF_Playtime.JOB_CIVILIAN), "the clock's time went to civilian");
		ctx.Equal(3990, LF_Playtime.Total(UID), "the total is every job together");
		ctx.Equal(120, LF_Playtime.Total(OTHER_UID), "accounts do not share time");

		JsonSaveContext saveContext = new JsonSaveContext(false);
		ctx.True(LF_PersistenceComponentSerializer.SerializeStore(persistence, saveContext) == ESerializeResult.OK, "the store serializes");
		string saved = saveContext.SaveToString();
		ctx.True(saved.Contains(UID), "the save carries the playtime payload");

		// Restart: the session has no playtime.
		LF_Playtime.ApplyAll(null);
		ctx.Equal(0, LF_Playtime.Total(UID), "the restarted session starts empty");

		for (int pass = 1; pass <= 2; pass++)
		{
			JsonLoadContext loadContext = new JsonLoadContext(false);
			ctx.True(loadContext.LoadFromString(saved), "the save parses");
			int version;
			loadContext.ReadValue("version", version);
			ctx.Equal(26, version, "the save is written at version 26 (playtime is payload 26)");
			LF_PersistenceComponentSerializer.ReadPersistencePayload(persistence, loadContext, "playtime");
			string tag = string.Format(" (load %1)", pass);
			ctx.Equal(3600, LF_Playtime.JobSeconds(UID, "POLICE"), "the police hour is back" + tag);
			ctx.Equal(390, LF_Playtime.JobSeconds(UID, LF_Playtime.JOB_CIVILIAN), "the civilian time is back" + tag);
			ctx.Equal(3990, LF_Playtime.Total(UID), "the total is back, not doubled" + tag);
			ctx.Equal(120, LF_Playtime.Total(OTHER_UID), "the other account is back" + tag);
		}

		// A version 25 save has no playtime: loading it leaves the live counts alone.
		JsonSaveContext oldContext = new JsonSaveContext(false);
		LF_PersistenceComponentSerializer.SerializeStoreAsVersion(persistence, oldContext, 25);
		ctx.False(oldContext.SaveToString().Contains(UID), "a version 25 save has no playtime rows");
		LF_Playtime.Credit(UID, "MEDIC", 5);
		JsonLoadContext oldLoad = new JsonLoadContext(false);
		oldLoad.LoadFromString(oldContext.SaveToString());
		LF_PersistenceComponentSerializer.DeserializeStore(persistence, oldLoad);
		ctx.Equal(3995, LF_Playtime.Total(UID), "a version 25 save leaves the live counts alone");

		LF_Playtime.ApplyAll(before);
	}

	//------------------------------------------------------------------------------------------------
	//! Duty and sign-up state, read through their accessors, pick the job.
	protected void CheckJobs(LF_TestContext ctx)
	{
		LF_PlayerAccount account = LF_PlayerAccount.Create(UID);
		ctx.EqualStr(LF_Playtime.JOB_CIVILIAN, LF_Playtime.JobFor(account), "an account off duty with no job is civilian");

		string recordKey = account.GetActiveCharacterKey();
		ctx.True(LF_CivilianJob.SignUp(recordKey, LF_EJobType.PARCEL), "the account signs up as a courier");
		ctx.EqualStr("PARCEL", LF_Playtime.JobFor(account), "a signed-up courier counts as PARCEL");

		account.SetFaction(LF_Faction.POLICE);
		ctx.EqualStr("PARCEL", LF_Playtime.JobFor(account), "the police role off duty is not police time");
		account.SetOnDuty(true);
		ctx.EqualStr("POLICE", LF_Playtime.JobFor(account), "on duty as police counts as POLICE over the sign-up");
		account.SetFaction(LF_Faction.MEDIC);
		ctx.EqualStr("MEDIC", LF_Playtime.JobFor(account), "on duty as medic counts as MEDIC");

		LF_CivilianJob.ForgetCareer(recordKey);
	}

	//------------------------------------------------------------------------------------------------
	//! A clock credits the seconds since its last credit, caps a clock jump and stops on disconnect.
	protected void CheckClock(LF_TestContext ctx)
	{
		ctx.False(LF_Playtime.StartClock(PLAYER_ID, "", 1000), "a player with no account uid gets no clock");
		ctx.True(LF_Playtime.StartClock(PLAYER_ID, UID, 1000), "a clock starts for the player");
		ctx.Equal(0, LF_Playtime.JobSeconds(UID, LF_Playtime.JOB_CIVILIAN), "starting a clock credits nothing");
		ctx.Equal(60, LF_Playtime.Settle(PLAYER_ID, 1060), "a minute later the clock credits 60 s");
		ctx.Equal(0, LF_Playtime.Settle(PLAYER_ID, 1000), "a clock moved backwards credits nothing");
		ctx.Equal(LF_Playtime.MAX_CREDIT_SECONDS, LF_Playtime.Settle(PLAYER_ID, 5000), "a clock jump is capped");
		ctx.Equal(30, LF_Playtime.StopClock(PLAYER_ID, 5030), "disconnect credits the last stretch");
		ctx.Equal(0, LF_Playtime.StopClock(PLAYER_ID, 6000), "a stopped clock credits nothing more");
	}
}
