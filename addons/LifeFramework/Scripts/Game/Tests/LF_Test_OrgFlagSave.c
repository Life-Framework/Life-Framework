// red-proof: Observed 2026-11-22 with `--no-build --red`: `RED-OK org/flag-save (fails with the hook on, 35 assertions)`, the section read skipped.
//! The gang flag (#4213, gangs C2): set on an organization, written in the "orgflags" save section, read
//! back after the flag table is gone, and the same after a second load. An organization with no row takes
//! the first free flag; a save from before the section (OnAbsent) gives every organization the first free
//! flag in id order and keeps a live flag. Bad rows are skipped. Restart: the e2e organization keeps its
//! flag through `wt e2e`. The test puts back the organizations and flags it found.
[LF_TestInfo(LF_TestTier.PERSISTENCE)]
class LF_Test_OrgFlagSave : LF_Test
{
	protected static const string ORG_A = "orgflag-test-a";
	protected static const string ORG_B = "orgflag-test-b";
	protected static const string ORG_C = "orgflag-test-c";
	//! The crew the e2e save boot founds; the reload boot finds it by its founder.
	protected static const string E2E_ORG_NAME = "E2E Flag Crew";
	protected static const string E2E_FOUNDER = "e2e-orgflag-founder";
	protected static const int E2E_FLAG = 13;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "org/flag-save";
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
		if (LF_E2E.RequiresSection(LF_OrgFlagSavePayload.NAME))
		{
			LF_Organization e2eOrg = LF_OrganizationManager.GetByCharacter(E2E_FOUNDER);
			ctx.NotNull(e2eOrg, "the e2e crew is back after the restart");
			if (e2eOrg)
				ctx.Equal(E2E_FLAG, LF_OrgFlags.FlagOf(e2eOrg.GetId()), "the e2e crew keeps its flag through the restart");
		}

		return;
#endif

		ctx.NotNull(LF_SaveSections.Get(LF_OrgFlagSavePayload.NAME), "orgflags is a registered save section");

		LF_TestLegacySnapshot kept = LF_TestLegacySnapshot.Take();
		array<ref LF_OrgFlagRow> keptFlags = LF_OrgFlags.ExportAll();

		CheckRoundTrip(ctx, persistence);
		CheckOldSave(ctx, persistence);
		CheckBadRows(ctx);

		LF_OrganizationManager.Reset();
		kept.Restore();
		LF_OrgFlags.ResetStatics();
		LF_OrgFlags.ApplyAll(keptFlags);

#ifdef LF_E2E_HOST_STOP
		// The save boot: this test's own crew flies E2E_FLAG into the shutdown save (LF_Test_E2E_SaveStateWrite
		// founds its organization after this test, so it is not used here).
		LF_Organization org = LF_OrganizationManager.Create(E2E_FOUNDER, E2E_ORG_NAME);
		ctx.NotNull(org, "the e2e crew is founded before the save");
		if (org)
			ctx.True(LF_OrgFlags.Set(org.GetId(), E2E_FLAG), "the e2e crew flies its flag before the save");
#endif
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_OrganizationRecord Record(string id, string name, string founder)
	{
		LF_OrganizationRecord record = LF_OrganizationRecord.Create(id, name, 0, false, false, false);
		record.m_aRanks.Insert(LF_OrganizationRankRecord.Create(LF_OrganizationConfig.RANK_FOUNDER, "Founder", true, true, true, true));
		record.m_aRanks.Insert(LF_OrganizationRankRecord.Create(LF_OrganizationConfig.RANK_MEMBER, "Member", false, false, false, false));
		record.m_aMembers.Insert(LF_OrganizationMemberRecord.Create(founder, LF_OrganizationConfig.RANK_FOUNDER));
		return record;
	}

	//------------------------------------------------------------------------------------------------
	//! Puts organizations A and B (and C when asked) on the register, with an empty flag table.
	protected static void Register(bool withC)
	{
		array<ref LF_OrganizationRecord> records = {};
		records.Insert(Record(ORG_B, "Flag Test B", "orgflag-founder-b"));
		records.Insert(Record(ORG_A, "Flag Test A", "orgflag-founder-a"));
		if (withC)
			records.Insert(Record(ORG_C, "Flag Test C", "orgflag-founder-c"));

		LF_OrganizationManager.Reset();
		LF_OrganizationManager.ApplyAll(records);
		LF_OrgFlags.ResetStatics();
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckRoundTrip(LF_TestContext ctx, LF_PersistenceComponent persistence)
	{
		Register(false);
		ctx.True(LF_OrgFlags.Set(ORG_A, 7), "organization A picks flag 7");
		ctx.Equal(7, LF_OrgFlags.FlagOf(ORG_A), "organization A flies flag 7");
		ctx.False(LF_OrgFlags.Set(ORG_B, LF_OrgFlags.NONE), "no flag is not a flag to pick");
		ctx.False(LF_OrgFlags.Set(ORG_B, LF_OrgFlags.COUNT + 1), "a flag past the presets is refused");
		ctx.False(LF_OrgFlags.Set("orgflag-no-such-org", 3), "an organization not on the register gets no flag");
		ctx.Equal(LF_OrgFlags.NONE, LF_OrgFlags.FlagOf(ORG_B), "organization B has no flag before the save");

		JsonSaveContext saveContext = new JsonSaveContext(false);
		ctx.True(LF_PersistenceComponentSerializer.SerializeStore(persistence, saveContext) == ESerializeResult.OK, "the store serializes");
		string saved = saveContext.SaveToString();
		ctx.True(saved.Contains(SectionKey()), "the save carries the orgflags section");
		ctx.Equal(0, LF_PersistenceComponentSerializer.PayloadSinceVersion(LF_OrgFlagSavePayload.NAME), "orgflags is not a legacy positional payload");

		// Restart: the session holds no flag.
		LF_OrgFlags.ResetStatics();
		ctx.Equal(LF_OrgFlags.NONE, LF_OrgFlags.FlagOf(ORG_A), "the flag table is empty before the load");

		JsonLoadContext loadContext = new JsonLoadContext(false);
		ctx.True(loadContext.LoadFromString(saved), "the save parses");
		// RedProof(): the load is skipped, so every read-back assertion fails (`--red`).
		if (!RedProof())
			ReadSection(loadContext, ctx);

		ctx.Equal(7, LF_OrgFlags.FlagOf(ORG_A), "organization A flies flag 7 after the load");
		ctx.Equal(1, LF_OrgFlags.FlagOf(ORG_B), "organization B, saved with no flag, takes the first free flag");

		JsonLoadContext again = new JsonLoadContext(false);
		again.LoadFromString(saved);
		ReadSection(again, ctx);
		ctx.Equal(7, LF_OrgFlags.FlagOf(ORG_A), "a second load keeps flag 7");
		ctx.Equal(1, LF_OrgFlags.FlagOf(ORG_B), "a second load hands B the same first free flag");
		ctx.Equal(2, LF_OrgFlags.ExportAll().Count(), "a second load does not duplicate rows");
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckOldSave(LF_TestContext ctx, LF_PersistenceComponent persistence)
	{
		JsonSaveContext oldContext = new JsonSaveContext(false);
		array<string> omit = {LF_OrgFlagSavePayload.NAME};
		LF_PersistenceComponentSerializer.SerializeStoreOmitting(persistence, oldContext, omit);
		ctx.False(oldContext.SaveToString().Contains(SectionKey()), "the older save has no orgflags section");

		LF_SavePayload section = LF_SaveSections.Get(LF_OrgFlagSavePayload.NAME);
		if (!section)
			return;

		// A boot from the older save: three organizations, no flag table. They take 1, 2, 3 in id order.
		Register(true);
		section.OnAbsent();
		ctx.Equal(1, LF_OrgFlags.FlagOf(ORG_A), "old save: A takes the first free flag");
		ctx.Equal(2, LF_OrgFlags.FlagOf(ORG_B), "old save: B takes the next free flag");
		ctx.Equal(3, LF_OrgFlags.FlagOf(ORG_C), "old save: C takes the next free flag");

		// Re-applied to a live session: a live flag stays, and the first free flag skips held ones.
		Register(true);
		LF_OrgFlags.Set(ORG_A, 2);
		section.OnAbsent();
		ctx.Equal(2, LF_OrgFlags.FlagOf(ORG_A), "old save re-applied: A keeps its live flag");
		ctx.Equal(1, LF_OrgFlags.FlagOf(ORG_B), "old save re-applied: B takes flag 1");
		ctx.Equal(3, LF_OrgFlags.FlagOf(ORG_C), "old save re-applied: C skips the held flag 2");
		section.OnAbsent();
		ctx.Equal(3, LF_OrgFlags.FlagOf(ORG_C), "a second old-save pass changes nothing");
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckBadRows(LF_TestContext ctx)
	{
		Register(false);
		array<ref LF_OrgFlagRow> rows = {};
		rows.Insert(LF_OrgFlagRow.Create("", 4));
		rows.Insert(LF_OrgFlagRow.Create(ORG_A, 0));
		rows.Insert(LF_OrgFlagRow.Create(ORG_B, LF_OrgFlags.COUNT + 5));
		LF_OrgFlagRow noVersion = LF_OrgFlagRow.Create(ORG_A, 9);
		noVersion.m_iVersion = 0;
		rows.Insert(noVersion);
		rows.Insert(LF_OrgFlagRow.Create("orgflag-gone", 6));
		ctx.Equal(1, LF_OrgFlags.ApplyAll(rows), "only the row of the organization off the register is kept");
		ctx.Equal(LF_OrgFlags.NONE, LF_OrgFlags.FlagOf("orgflag-gone"), "a kept row counts for nothing off the register");
		ctx.Equal(1, LF_OrgFlags.FlagOf(ORG_A), "bad rows skipped: A takes the first free flag");
		ctx.Equal(2, LF_OrgFlags.FlagOf(ORG_B), "bad rows skipped: B takes the next free flag");
	}

	//------------------------------------------------------------------------------------------------
	//! The quoted JSON key, so a neighbouring section with a longer name never matches.
	protected static string SectionKey()
	{
		return "\"" + LF_SaveSections.OBJECT_PREFIX + LF_OrgFlagSavePayload.NAME + "\"";
	}

	//------------------------------------------------------------------------------------------------
	protected static void ReadSection(notnull JsonLoadContext context, LF_TestContext ctx)
	{
		LF_SavePayload section = LF_SaveSections.Get(LF_OrgFlagSavePayload.NAME);
		if (!section)
			return;

		ctx.True(context.StartObject(LF_SaveSections.OBJECT_PREFIX + LF_OrgFlagSavePayload.NAME), "the orgflags section opens");
		int savedVersion;
		context.ReadValue(LF_SaveSections.VERSION_KEY, savedVersion);
		ctx.Equal(section.Version(), savedVersion, "the section carries its own version");
		ctx.True(section.ReadSection(context, savedVersion), "the orgflags section reads");
		context.EndObject();
	}
}
