// red-proof: observed 2026-11-18 `tools/cli test --no-build --red LF_Test_MedicRanks,LF_Test_MedicRanksPersist`: RED PROOF OK; medic/ranks first `rank 0 promotion seconds read the config row (expected 600, got 3600)`, medic/ranks-persist first `the medic rank is back after the load (expected 1, got 0)`. Green with `--only`. Restart: `tools/cli wt e2e slice-2708`.
// tier: LOGIC
//! Medic ranks (#2708): the ladder reads LF_MedicConfig.m_aRanks (built-in three rungs when empty), a
//! rank-0 medic is refused the rank-2 kit item with a refusal key, PayPerSecond reads the configured
//! row, and promotion is due after the configured duty seconds.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_MedicRanks : LF_Test
{
	override string GetName()
	{
		return "medic/ranks";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_MedicConfig config = LF_MedicConfig.Get();
		ctx.NotNull(config, "the medic config resolves");
		if (!config)
			return;

		// Built-in ladder: trainee, paramedic, doctor.
		array<ref LF_MedicRank> saved = config.m_aRanks;
		config.m_aRanks = null;
		ctx.Equal(3, LF_MedicRanks.RankCount(), "an empty config gives the built-in three rungs");
		string refusal;
		ctx.False(LF_MedicRanks.CanDrawItem(LF_MedicRanks.TRAINEE, LF_MedicRanks.MORPHINE, refusal), "a trainee may not draw the doctor's morphine");
		ctx.EqualStr(LF_MedicRanks.KIT_REFUSED_KEY, refusal, "the refusal is put in words");
		ctx.True(LF_MedicRanks.CanDrawItem(LF_MedicRanks.TRAINEE, LF_MedicRanks.BANDAGE, refusal), "a trainee draws a bandage");
		ctx.EqualStr("", refusal, "an allowed draw has no refusal");
		ctx.True(LF_MedicRanks.CanDrawItem(LF_MedicRanks.DOCTOR, LF_MedicRanks.BANDAGE, refusal), "a doctor draws every lower rank's kit");
		ctx.True(LF_MedicRanks.CanDrawItem(LF_MedicRanks.DOCTOR, LF_MedicRanks.MORPHINE, refusal), "a doctor draws morphine");
		ctx.False(LF_MedicRanks.CanDrawItem(LF_MedicRanks.DOCTOR, LF_PoliceRanks.SIDEARM, refusal), "an item on no medic kit is refused");
		array<ResourceName> kit = {};
		LF_MedicRanks.CollectKit(LF_MedicRanks.PARAMEDIC, kit);
		ctx.Equal(3, kit.Count(), "a paramedic's kit is the trainee's two items and the saline bag");
		ctx.Equal(LF_MedicRanks.DUTY_SECONDS_PARAMEDIC, LF_MedicRanks.DutySecondsForNext(LF_MedicRanks.TRAINEE), "a trainee is promoted after the built-in hour");
		ctx.Equal(0, LF_MedicRanks.DutySecondsForNext(LF_MedicRanks.DOCTOR), "the top rank never promotes");
		ctx.Equal(LF_MedicRanks.DOCTOR, LF_MedicRanks.UsableRank(9), "a rank past the top clamps to the top");
		ctx.Equal(0, LF_MedicRanks.UsableRank(-3), "a negative rank clamps to 0");

		// A configured ladder: pay, promotion seconds and kit come from the rows.
		array<ref LF_MedicRank> ladder = new array<ref LF_MedicRank>();
		LF_MedicRank first = LF_MedicRank.Create("first", "LF-MedicRank_Trainee", 0.1, 600);
		first.m_aKit.Insert(LF_MedicRanks.BANDAGE);
		ladder.Insert(first);
		LF_MedicRank second = LF_MedicRank.Create("second", "LF-MedicRank_Paramedic", 0.2, 1200);
		ladder.Insert(second);
		LF_MedicRank third = LF_MedicRank.Create("third", "LF-MedicRank_Doctor", 0.3, 0);
		third.m_aKit.Insert(LF_MedicRanks.SALINE);
		ladder.Insert(third);
		// RedProof(): the configured ladder is never set, so the config assertions below fail (`--red`).
		if (!RedProof())
			config.m_aRanks = ladder;

		ctx.True(Math.AbsFloat(LF_MedicRanks.PayPerSecond(0) - 0.1) < 0.0001, "rank 0 pay reads the config row");
		ctx.True(Math.AbsFloat(LF_MedicRanks.PayPerSecond(2) - 0.3) < 0.0001, "rank 2 pay reads the config row");
		ctx.Equal(600, LF_MedicRanks.DutySecondsForNext(0), "rank 0 promotion seconds read the config row");
		ctx.False(LF_MedicRanks.PromotionDue(599, LF_MedicRanks.DutySecondsForNext(0)), "promotion is not due a second early");
		ctx.True(LF_MedicRanks.PromotionDue(600, LF_MedicRanks.DutySecondsForNext(0)), "promotion is due at the configured seconds");
		ctx.False(LF_MedicRanks.PromotionDue(100000, LF_MedicRanks.DutySecondsForNext(2)), "the configured top rank never promotes");
		ctx.False(LF_MedicRanks.CanDrawItem(0, LF_MedicRanks.SALINE, refusal), "a rank-0 medic is refused the configured rank-2 item");
		ctx.EqualStr(LF_MedicRanks.KIT_REFUSED_KEY, refusal, "the configured refusal is put in words");
		ctx.True(LF_MedicRanks.CanDrawItem(2, LF_MedicRanks.SALINE, refusal), "rank 2 draws its own item");

		config.m_aRanks = saved;
	}
}

//------------------------------------------------------------------------------------------------
//! Medic rank and duty seconds ride the real save path (#2708, the "medicranks" save section): time on
//! duty promotes, the rank survives a save and load, loading twice gives the same rank, a rank off
//! the ladder clamps, and a save without the section loads the medic at rank 0. Across a restart
//! (`tools\cli wt e2e`): every non-reload run leaves the e2e medic at rank 2; the reload boot finds it.
[LF_TestInfo(LF_TestTier.PERSISTENCE)]
class LF_Test_MedicRanksPersist : LF_Test
{
	static const string MEDIC = "test-medic-ranks";
	//! The e2e character: every non-reload run seeds it, the reload boot reads it after the restart.
	static const string E2E_KEY = "e2e-medic-ranks";

	override string GetName()
	{
		return "medic/ranks-persist";
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
		// The reload boot: the e2e medic's rank came back from the real save, when it carried the section
		// (a --pre-move or --legacy-save ref build wrote none; LF_E2E logs the skip).
		if (LF_E2E.RequiresSeed("medicranks save section", LF_SaveSections.BootHadSection(LF_MedicRanksSavePayload.NAME)))
			VerifyE2E(ctx);

		return;
#endif

		ctx.NotNull(LF_SaveSections.Get(LF_MedicRanksSavePayload.NAME), "medicranks is a registered save section");

		LF_PlayerAccount medic = LF_PlayerAccountManager.GetOrCreate(MEDIC);
		ctx.NotNull(medic, "the medic account exists");
		if (!medic)
			return;

		// Time on duty promotes through the public mutators the duty clock uses.
		medic.SetFaction(LF_Faction.MEDIC);
		medic.SetMedicRank(0);
		medic.SetMedicDutySeconds(0);
		int toParamedic = LF_MedicRanks.DutySecondsForNext(0);
		ctx.False(LF_MedicRanks.AccrueDuty(medic, toParamedic - 1), "a second short of the threshold does not promote");
		ctx.True(LF_MedicRanks.AccrueDuty(medic, 1), "the threshold promotes the medic");
		ctx.Equal(1, medic.GetMedicRank(), "the medic is rank 1");
		ctx.Equal(0, medic.GetPoliceRank(), "medic duty leaves the police rank alone");
		float rate = LF_DutyPay.RateForAccount(medic);
		ctx.True(Math.AbsFloat(rate - LF_MedicRanks.PayPerSecond(1)) < 0.0001, "duty pay uses the rank's rate");
		string refusal;
		ctx.False(LF_MedicRanks.CanDraw(medic, LF_MedicRanks.SALINE, refusal), "an off-duty medic draws nothing");

		JsonSaveContext saveContext = new JsonSaveContext(false);
		ctx.True(LF_PersistenceComponentSerializer.SerializeStore(persistence, saveContext) == ESerializeResult.OK, "the store serializes");
		string saved = saveContext.SaveToString();
		ctx.True(saved.Contains(MEDIC), "the save carries the medic's key");
		ctx.True(saved.Contains(LF_SaveSections.OBJECT_PREFIX + LF_MedicRanksSavePayload.NAME), "the save carries the medicranks section");

		// Restart: the session holds no rank.
		medic.SetMedicRank(0);
		medic.SetMedicDutySeconds(0);

		JsonLoadContext loadContext = new JsonLoadContext(false);
		ctx.True(loadContext.LoadFromString(saved), "the save parses");
		int version;
		loadContext.ReadValue("version", version);
		ctx.Equal(LF_TestSaveVersion.CURRENT, version, "the save is written in the sectioned format");
		ctx.Equal(0, LF_PersistenceComponentSerializer.PayloadSinceVersion(LF_MedicRanksSavePayload.NAME), "medicranks is not a legacy positional payload");
		// RedProof(): the load is skipped, so every read-back assertion fails (`--red`).
		if (!RedProof())
			ReadSection(loadContext, ctx);

		ctx.Equal(1, medic.GetMedicRank(), "the medic rank is back after the load");
		ctx.Equal(toParamedic, medic.GetMedicDutySeconds(), "the medic duty seconds are back after the load");

		JsonLoadContext again = new JsonLoadContext(false);
		again.LoadFromString(saved);
		ReadSection(again, ctx);
		ctx.Equal(1, medic.GetMedicRank(), "loading twice keeps the same rank");

		// A rank off the ladder clamps to the top; a row with no key is skipped.
		array<ref LF_MedicRankRow> rows = new array<ref LF_MedicRankRow>();
		rows.Insert(LF_MedicRankRow.Create("", 2, 10));
		rows.Insert(LF_MedicRankRow.Create(MEDIC, 9, 10));
		ctx.Equal(1, LF_MedicRanks.ApplyAll(rows), "a row with no key is skipped");
		ctx.Equal(LF_MedicRanks.TopRank(), medic.GetMedicRank(), "a rank past the top clamps to the top");

		// A save without this section (an older save): the medic loads at rank 0.
		medic.SetMedicRank(0);
		medic.SetMedicDutySeconds(0);
		JsonSaveContext oldContext = new JsonSaveContext(false);
		array<string> omit = {LF_MedicRanksSavePayload.NAME};
		LF_PersistenceComponentSerializer.SerializeStoreOmitting(persistence, oldContext, omit);
		string oldSave = oldContext.SaveToString();
		ctx.False(oldSave.Contains(LF_SaveSections.OBJECT_PREFIX + LF_MedicRanksSavePayload.NAME), "the older save has no medicranks section");
		JsonLoadContext oldLoad = new JsonLoadContext(false);
		ctx.True(oldLoad.LoadFromString(oldSave), "the old save parses");
		LF_PersistenceComponentSerializer.DeserializeStore(persistence, oldLoad);
		medic = LF_PlayerAccountManager.GetInstance().GetFromCache(MEDIC);
		if (medic)
			ctx.Equal(0, medic.GetMedicRank(), "a pre-payload save loads the medic at rank 0");

		if (medic)
		{
			medic.SetMedicRank(0);
			medic.SetMedicDutySeconds(0);
			medic.SetFaction(LF_Faction.CIVILIAN);
		}

		// Every non-reload run (the e2e save boot among them) leaves the e2e medic at rank 2.
		SeedE2E(ctx);
	}

	//------------------------------------------------------------------------------------------------
	//! Reads the medicranks section of a whole-store JSON save, as LF_SaveSections.ReadAll does.
	protected static void ReadSection(notnull JsonLoadContext context, LF_TestContext ctx)
	{
		LF_SavePayload section = LF_SaveSections.Get(LF_MedicRanksSavePayload.NAME);
		if (!section)
			return;

		ctx.True(context.StartObject(LF_SaveSections.OBJECT_PREFIX + LF_MedicRanksSavePayload.NAME), "the medicranks section opens");
		int savedVersion;
		context.ReadValue(LF_SaveSections.VERSION_KEY, savedVersion);
		ctx.Equal(section.Version(), savedVersion, "the section carries its own version");
		ctx.True(section.ReadSection(context, savedVersion), "the medicranks section reads");
		context.EndObject();
	}

	//------------------------------------------------------------------------------------------------
	//! wt e2e save boot: one medic character holds rank 2.
	static void SeedE2E(LF_TestContext ctx)
	{
		LF_PlayerAccount medic = LF_PlayerAccountManager.GetOrCreate(E2E_KEY);
		ctx.NotNull(medic, "e2e medic account exists");
		if (!medic)
			return;

		medic.SetMedicRank(LF_MedicRanks.DOCTOR);
		medic.SetMedicDutySeconds(LF_MedicRanks.DUTY_SECONDS_DOCTOR);
		ctx.Equal(LF_MedicRanks.DOCTOR, medic.GetMedicRank(), "e2e medic is rank 2 before the save");
	}

	//------------------------------------------------------------------------------------------------
	//! wt e2e reload boot: the same rank after the restart.
	static void VerifyE2E(LF_TestContext ctx)
	{
		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		LF_CharacterStanding standing;
		if (accounts)
			standing = accounts.FindStanding(E2E_KEY);

		ctx.NotNull(standing, "e2e medic character is back after the restart");
		if (!standing)
			return;

		ctx.Equal(LF_MedicRanks.DOCTOR, standing.m_iMedicRank, "e2e medic rank survives the restart");
		ctx.Equal(LF_MedicRanks.DUTY_SECONDS_DOCTOR, standing.m_iMedicDutySeconds, "e2e medic duty seconds survive the restart");
		LF_Debug.Info(LF_MedicRanks.FEATURE, string.Format("e2e restart: %1 is medic rank %2 with %3 s", E2E_KEY, standing.m_iMedicRank, standing.m_iMedicDutySeconds));
	}
}
