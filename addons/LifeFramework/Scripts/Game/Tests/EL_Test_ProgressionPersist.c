// red-proof: observed 2026-11-08 with `tools\cli test --no-build --tier persistence`, two mutations
// at once: the LF_CivilianJob.RestoreCareer call in LF_Progression.ApplyAll commented out, and the
// live rank carry-over in EL_PlayerAccountManager.ApplyAll turned off with `if (live && false)`.
//   SUMMARY tier=persistence passed=9 failed=2 total=11
//   [ELTEST] FAIL persist/progression-roundtrip
//   [ELTEST]   - courier level 2 after the load (expected 2, got 1)
//   [ELTEST]   - courier level 2 still opens fuel delivery
//   [ELTEST] FAIL persist/progression-old-save
//   [ELTEST]   - a version 6 save does not demote the live senior officer (expected 1, got 0)
// Restored; green.

//------------------------------------------------------------------------------------------------
//! Shared seeding and cleanup for the progression persistence tests.
class EL_TestProgressionKit
{
	static const string QUEST_ID = "intro_talk";

	//------------------------------------------------------------------------------------------------
	//! Courier at career level 2 through one paid parcel run.
	static void SeedCourier(EL_TestContext ctx, string uid)
	{
		ctx.True(LF_CivilianJob.SignUp(uid, EL_EJobType.PARCEL), "the courier signs up for parcels");
		ctx.True(LF_CivilianJob.StartRun(uid, EL_EJobType.PARCEL), "the courier starts a parcel run");
		ctx.True(LF_CivilianJob.CompleteRun(uid) > 0, "the parcel run pays");
		ctx.Equal(2, LF_CivilianJob.GetLevel(uid), "one paid run makes the courier level 2");
		ctx.True(LF_CivilianJob.CanSignUp(uid, EL_EJobType.FUEL_DELIVERY), "level 2 opens fuel delivery");
	}

	//------------------------------------------------------------------------------------------------
	//! Police officer promoted to senior by an hour on duty.
	static EL_PlayerAccount SeedSeniorOfficer(EL_TestContext ctx, string uid)
	{
		EL_PlayerAccount account = EL_PlayerAccountManager.GetOrCreate(uid);
		ctx.NotNull(account, "the officer account exists");
		if (!account)
			return null;

		account.SetFaction(EL_Faction.POLICE);
		account.SetPoliceRank(LF_PoliceRanks.PATROL);
		account.SetOnDutySeconds(0);
		account.SetUnpaidDutySeconds(0);
		ctx.True(LF_PoliceRanks.AccrueDuty(uid, LF_PoliceRanks.DUTY_SECONDS), "an hour on duty promotes the officer");
		ctx.Equal(LF_PoliceRanks.SENIOR, account.GetPoliceRank(), "the officer is senior");
		return account;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetAccount(string uid)
	{
		EL_PlayerAccount account = EL_PlayerAccountManager.GetInstance().GetFromCache(uid);
		if (!account)
			return;

		account.SetPoliceRank(LF_PoliceRanks.PATROL);
		account.SetOnDutySeconds(0);
		account.SetUnpaidDutySeconds(0);
		account.SetFaction(EL_Faction.CIVILIAN);
	}

	//------------------------------------------------------------------------------------------------
	static EL_PersistenceComponent Persistence(EL_TestContext ctx)
	{
		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		ctx.NotNull(gameMode, "the DebugWorld game mode resolves");
		if (!gameMode)
			return null;

		EL_PersistenceComponent persistence = EL_PersistenceComponent.Cast(gameMode.FindComponent(EL_PersistenceComponent));
		ctx.NotNull(persistence, "the game mode carries EL_PersistenceComponent");
		return persistence;
	}
}

//------------------------------------------------------------------------------------------------
// tier: PERSISTENCE
class EL_Test_ProgressionRoundtrip : EL_Test
{
	protected static const string COURIER = "progression-courier";
	protected static const string OFFICER = "progression-officer";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "persist/progression-roundtrip";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(EL_TestContext ctx)
	{
		EL_PersistenceComponent persistence = EL_TestProgressionKit.Persistence(ctx);
		if (!persistence)
			return;

		LF_Progression.Forget(COURIER);
		LF_Progression.Forget(OFFICER);
		array<ref LF_ProgressionRecord> before = LF_Progression.ExportAll();

		// Progression through the public mutators the game uses.
		EL_TestProgressionKit.SeedCourier(ctx, COURIER);
		string reason;
		ctx.Equal(50, LF_UnemploymentBenefit.Claim(COURIER, 0, false, LF_JobGuards.Now(), 100, 50, 3600, reason), "the courier collects the benefit");
		LF_Progression.NoteLevel(COURIER, 3, 40, 2, 2);
		array<EL_ELicenseType> licenses = {EL_ELicenseType.UNEMPLOYED, EL_ELicenseType.MINER_COAL};
		LF_Progression.NoteLicenses(COURIER, licenses);
		LF_Progression.NoteTurnedIn(COURIER, EL_TestProgressionKit.QUEST_ID);

		EL_PlayerAccount officer = EL_TestProgressionKit.SeedSeniorOfficer(ctx, OFFICER);
		if (officer)
			officer.AddUnpaidDutySeconds(120);

		// Save through the real write path.
		JsonSaveContext saveContext = new JsonSaveContext(false);
		ctx.True(EL_PersistenceComponentSerializer.SerializeStore(persistence, saveContext) == ESerializeResult.OK, "the store serializes");
		string saved = saveContext.SaveToString();
		ctx.True(saved.Contains(COURIER), "the save carries the courier's progression");
		ctx.True(saved.Contains(OFFICER), "the save carries the officer's progression");

		// Restart: the session holds none of it.
		LF_Progression.Forget(COURIER);
		LF_Progression.Forget(OFFICER);
		EL_TestProgressionKit.ResetAccount(OFFICER);
		if (officer)
			officer.SetFaction(EL_Faction.POLICE);
		ctx.Equal(1, LF_CivilianJob.GetLevel(COURIER), "the wiped courier is level 1");
		ctx.False(LF_CivilianJob.CanSignUp(COURIER, EL_EJobType.FUEL_DELIVERY), "the wiped courier cannot take fuel delivery");
		LF_Progression.Forget(COURIER);

		// Load through the real read path.
		JsonLoadContext loadContext = new JsonLoadContext(false);
		ctx.True(loadContext.LoadFromString(saved), "the save parses");
		int version;
		loadContext.ReadValue("version", version);
		ctx.Equal(8, version, "the save is written at version 8 (progression is payload 7)");
		EL_PersistenceComponentSerializer.ReadPersistencePayload(persistence, loadContext, "progression");

		ctx.Equal(2, LF_CivilianJob.GetLevel(COURIER), "courier level 2 after the load");
		ctx.True(LF_CivilianJob.CanSignUp(COURIER, EL_EJobType.FUEL_DELIVERY), "courier level 2 still opens fuel delivery");
		ctx.Equal(LF_CivilianJob.PAY_PARCEL, LF_CivilianJob.GetPaidTotal(COURIER), "the courier's paid total survives");
		ctx.EqualStr(LF_UnemploymentBenefit.REFUSED_COOLDOWN, LF_UnemploymentBenefit.Refusal(COURIER, 0, false, LF_JobGuards.Now(), 100, 50, 3600), "the benefit cooldown survives");
		if (officer)
		{
			ctx.Equal(LF_PoliceRanks.SENIOR, officer.GetPoliceRank(), "the senior officer stays senior");
			ctx.Equal(LF_PoliceRanks.DUTY_SECONDS, officer.GetOnDutySeconds(), "on-duty seconds survive");
			ctx.Equal(120, officer.GetUnpaidDutySeconds(), "unpaid duty seconds survive");
		}

		array<EL_ELicenseType> loadedLicenses = {};
		ctx.Equal(2, LF_Progression.GetLicenses(COURIER, loadedLicenses), "both licences come back");
		ctx.True(LF_Progression.HasTurnedIn(COURIER, EL_TestProgressionKit.QUEST_ID), "the turned-in quest stays done");

		// A licence name that no longer exists is skipped; the rest still resolve.
		LF_ProgressionRecord stored = LF_Progression.Get(COURIER);
		if (stored)
			stored.m_aLicenses.Insert("LF_REMOVED_LICENCE");
		ctx.Equal(2, LF_Progression.GetLicenses(COURIER, loadedLicenses), "an unknown licence name is skipped");

		// A new body gets the level and the turn-in back, twice without change.
		IEntity body = EL_TestCharacterFixture.Spawn(ctx);
		if (body)
		{
			ctx.True(LF_Progression.ApplyToCharacter(COURIER, body), "the record applies to a new body");
			ctx.True(LF_Progression.ApplyToCharacter(COURIER, body), "the record applies a second time");
			EL_PlayerLevelComponent level = EL_Component<EL_PlayerLevelComponent>.Find(body);
			ctx.NotNull(level, "the body has a level component");
			if (level)
			{
				ctx.Equal(3, level.GetLevel(), "the body is level 3");
				ctx.Equal(2, level.GetSkillPoints(), "the body has 2 skill points");
			}

			LF_QuestTrackerComponent tracker = EL_Component<LF_QuestTrackerComponent>.Find(body);
			ctx.NotNull(tracker, "the body has a quest tracker");
			if (tracker)
			{
				ctx.True(tracker.HasTurnedIn(EL_TestProgressionKit.QUEST_ID), "the body's quest stays turned in");
				ctx.Equal(1, tracker.GetTurnedInQuestIds().Count(), "the second apply adds no second turn-in");
			}

			EL_TestCharacterFixture.Delete(body);
		}

		// A second load converges on the same state.
		JsonLoadContext again = new JsonLoadContext(false);
		again.LoadFromString(saved);
		EL_PersistenceComponentSerializer.ReadPersistencePayload(persistence, again, "progression");
		ctx.Equal(2, LF_CivilianJob.GetLevel(COURIER), "a second load keeps courier level 2");
		if (officer)
			ctx.Equal(LF_PoliceRanks.SENIOR, officer.GetPoliceRank(), "a second load keeps the officer senior");

		// A rank off the ladder clamps to the top usable rank.
		array<ref LF_ProgressionRecord> offLadder = new array<ref LF_ProgressionRecord>();
		LF_ProgressionRecord promoted = LF_ProgressionRecord.Create(OFFICER);
		promoted.m_iPoliceRank = 9;
		offLadder.Insert(promoted);
		LF_Progression.ApplyAll(offLadder);
		if (officer)
			ctx.Equal(LF_PoliceRanks.SENIOR, officer.GetPoliceRank(), "rank 9 clamps to senior");

		LF_Progression.Forget(COURIER);
		LF_Progression.Forget(OFFICER);
		EL_TestProgressionKit.ResetAccount(OFFICER);
		LF_Progression.ApplyAll(before);
	}
}

//------------------------------------------------------------------------------------------------
// tier: PERSISTENCE
class EL_Test_ProgressionOldSave : EL_Test
{
	protected static const string OFFICER = "progression-old-officer";
	protected static const string NEWCOMER = "progression-old-newcomer";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "persist/progression-old-save";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(EL_TestContext ctx)
	{
		EL_PersistenceComponent persistence = EL_TestProgressionKit.Persistence(ctx);
		if (!persistence)
			return;

		LF_Progression.Forget(OFFICER);
		LF_Progression.Forget(NEWCOMER);
		EL_PlayerAccount officer = EL_TestProgressionKit.SeedSeniorOfficer(ctx, OFFICER);
		if (!officer)
			return;

		// A version 6 save written the way the previous build wrote it: header, accounts, bank,
		// then survival through carried. The newcomer exists only in that save.
		array<ref EL_PlayerAccountRecord> accountRecords = EL_PlayerAccountManager.ExportAll();
		accountRecords.Insert(EL_PlayerAccountRecord.Create(NEWCOMER));
		JsonSaveContext saveContext = new JsonSaveContext(false);
		saveContext.WriteValue("version", 6);
		saveContext.Write(accountRecords);
		array<ref EL_BankAccountRecord> bankRecords = EL_ATMManager.ExportAll();
		saveContext.Write(bankRecords);
		array<string> oldOrder = {"survival", "organizations", "hideout", "vehicles", "jail", "carried"};
		foreach (string payload : oldOrder)
		{
			EL_PersistenceComponentSerializer.WritePersistencePayload(persistence, saveContext, payload);
		}

		JsonLoadContext loadContext = new JsonLoadContext(false);
		ctx.True(loadContext.LoadFromString(saveContext.SaveToString()), "the version 6 save parses");
		ctx.True(EL_PersistenceComponentSerializer.DeserializeStore(persistence, loadContext), "the version 6 save loads");

		EL_PlayerAccount reloaded = EL_PlayerAccountManager.GetInstance().GetFromCache(OFFICER);
		ctx.NotNull(reloaded, "the officer account is there after the load");
		if (reloaded)
		{
			ctx.Equal(LF_PoliceRanks.SENIOR, reloaded.GetPoliceRank(), "a version 6 save does not demote the live senior officer");
			ctx.Equal(LF_PoliceRanks.DUTY_SECONDS, reloaded.GetOnDutySeconds(), "a version 6 save keeps live on-duty seconds");
		}

		EL_PlayerAccount newcomer = EL_PlayerAccountManager.GetInstance().GetFromCache(NEWCOMER);
		ctx.NotNull(newcomer, "the newcomer account comes from the version 6 save");
		if (newcomer)
			ctx.Equal(LF_PoliceRanks.PATROL, newcomer.GetPoliceRank(), "the newcomer loads at the default rank");
		ctx.Equal(1, LF_CivilianJob.GetLevel(NEWCOMER), "the newcomer loads at career level 1");

		EL_TestProgressionKit.ResetAccount(OFFICER);
		LF_Progression.Forget(OFFICER);
		LF_Progression.Forget(NEWCOMER);
	}
}
