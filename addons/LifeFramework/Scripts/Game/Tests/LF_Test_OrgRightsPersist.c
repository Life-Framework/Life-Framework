// red-proof: observed 2026-11-12 with `tools\cli test --no-build --only LF_Test_OrgRights,LF_Test_OrgRightsPersist`:
// LF_OrganizationManager.ApplyAll migrating only `m_iVersion < 1` (never) failed `[LFTEST] FAIL org/rights-persist`,
// `- the officer rung is added once (version 26 load 1) (expected 3, got 2)`,
// `- the founder rank reads by its key (version 26 load 1) (expected '#LF-Org_RankFounder', got 'Founder')`,
// `- a migrated founder promotes onto the officer rung`. Restored; green. The restart path is `wt e2e`.
// tier: PERSISTENCE
//! #750a through the real save path. A version 26 save (the format before the rights mask: record
//! version 1, a founder and a member rank as four bools, no "orgranks" payload) loads with every
//! member on their rank, the founder holding every right, the member only the stash, and the officer
//! rung added. A version 27 save brings back masks the four bools cannot hold, and the ladder order.
//! Both loads run twice and give the same organization. Rank rows for a missing organization or rank
//! are dropped. The test puts back the organizations it found.
class LF_Test_OrgRightsPersist : LF_Test
{
	protected static const string FOUNDER = "org-persist-founder";
	protected static const string MEMBER = "org-persist-member";
	protected static const string MEMBER_TWO = "org-persist-member-two";
	protected static const string OLD_ID = "org-persist-old-outfit";

	override string GetName()
	{
		return "org/rights-persist";
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

		LF_TestLegacySnapshot kept = LF_TestLegacySnapshot.Take();
		CheckVersion26(ctx, persistence);
		CheckVersion27(ctx, persistence);
		CheckDroppedRows(ctx);
		LF_OrganizationManager.Reset();
		kept.Restore();
	}

	//------------------------------------------------------------------------------------------------
	//! The organization as the build before #750a created it: two ranks of four bools.
	protected LF_OrganizationRecord OldRecord()
	{
		LF_OrganizationRecord record = LF_OrganizationRecord.Create(OLD_ID, "Old Outfit", 120, false, true, false);
		record.m_aRanks.Insert(LF_OrganizationRankRecord.Create(LF_OrganizationConfig.RANK_FOUNDER, "Founder", true, true, true, true));
		record.m_aRanks.Insert(LF_OrganizationRankRecord.Create(LF_OrganizationConfig.RANK_MEMBER, "Member", false, false, false, false));
		record.m_aMembers.Insert(LF_OrganizationMemberRecord.Create(FOUNDER, LF_OrganizationConfig.RANK_FOUNDER));
		record.m_aMembers.Insert(LF_OrganizationMemberRecord.Create(MEMBER, LF_OrganizationConfig.RANK_MEMBER));
		record.m_aMembers.Insert(LF_OrganizationMemberRecord.Create(MEMBER_TWO, LF_OrganizationConfig.RANK_MEMBER));
		return record;
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckVersion26(LF_TestContext ctx, LF_PersistenceComponent persistence)
	{
		// Put the old organization live as it was (record version 2 skips the load-time migration),
		// then write the store as version 26: the serializer stamps it record version 1 and writes
		// no rank rows, which is what the build before #750a wrote.
		array<ref LF_OrganizationRecord> old = {};
		old.Insert(OldRecord());
		LF_OrganizationManager.Reset();
		LF_OrganizationManager.ApplyAll(old);
		LF_Organization before = LF_OrganizationManager.Get(OLD_ID);
		ctx.True(before != null && before.GetRankCount() == 2, "the old organization is live with its two ranks");

		JsonSaveContext saveContext = new JsonSaveContext(false);
		LF_PersistenceComponentSerializer.SerializeStoreAsVersion(persistence, saveContext, 26);
		string saved = saveContext.SaveToString();
		ctx.True(saved.Contains(OLD_ID), "the version 26 save carries the organization");
		ctx.False(saved.Contains("m_iRights"), "the version 26 save has no rank rows");

		LF_OrganizationManager.Reset();
		for (int pass = 1; pass <= 2; pass++)
		{
			JsonLoadContext loadContext = new JsonLoadContext(false);
			ctx.True(loadContext.LoadFromString(saved), "the version 26 save parses");
			LF_PersistenceComponentSerializer.DeserializeStore(persistence, loadContext);
			string tag = string.Format(" (version 26 load %1)", pass);

			LF_Organization org = LF_OrganizationManager.Get(OLD_ID);
			ctx.NotNull(org, "the old organization is back" + tag);
			if (!org)
				return;

			ctx.EqualStr("Old Outfit", org.GetName(), "the name is kept" + tag);
			ctx.Equal(120, org.GetFund(), "the fund is kept" + tag);
			ctx.True(org.HasProcessingSlot(), "the upgrade is kept" + tag);
			ctx.Equal(3, org.GetMemberCount(), "no member is lost" + tag);
			ctx.EqualStr(LF_OrganizationConfig.RANK_FOUNDER, org.GetRankId(FOUNDER), "the founder keeps the founder rank" + tag);
			ctx.EqualStr(LF_OrganizationConfig.RANK_MEMBER, org.GetRankId(MEMBER), "a member keeps the member rank" + tag);
			ctx.EqualStr(LF_OrganizationConfig.RANK_MEMBER, org.GetRankId(MEMBER_TWO), "the second member keeps the member rank" + tag);
			ctx.True(LF_OrganizationManager.GetByCharacter(MEMBER) == org, "the member index points at the organization" + tag);
			ctx.Equal(3, org.GetRankCount(), "the officer rung is added once" + tag);
			ctx.EqualStr(LF_OrganizationConfig.RANK_OFFICER, org.RankIdAt(1), "the officer rung sits between founder and member" + tag);
			ctx.Equal(LF_OrgRights.ALL, org.RightsOf(FOUNDER), "the founder's four true bools become every right" + tag);
			ctx.Equal(LF_OrgRights.Bit(LF_EOrganizationPermission.USE_STASH), org.RightsOf(MEMBER), "the member's four false bools become the stash only" + tag);
			ctx.EqualStr("#LF-Org_RankFounder", org.GetRankName(FOUNDER), "the founder rank reads by its key" + tag);
		}

		// The migrated organization works: the founder promotes onto the new rung, a member still may not.
		ctx.False(LF_OrganizationManager.Withdraw(OLD_ID, MEMBER, 10), "a migrated member still cannot withdraw");
		ctx.True(LF_OrganizationManager.Withdraw(OLD_ID, FOUNDER, 10), "a migrated founder still withdraws");
		ctx.True(LF_OrganizationManager.Promote(OLD_ID, FOUNDER, MEMBER), "a migrated founder promotes onto the officer rung");
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckVersion27(LF_TestContext ctx, LF_PersistenceComponent persistence)
	{
		LF_OrganizationManager.Reset();
		LF_Organization org = LF_OrganizationManager.Create(FOUNDER, "New Outfit");
		ctx.NotNull(org, "a new organization is founded");
		if (!org)
			return;

		string orgId = org.GetId();
		ctx.True(LF_OrganizationManager.Invite(orgId, FOUNDER, MEMBER), "a member joins");
		ctx.True(LF_OrganizationManager.Invite(orgId, FOUNDER, MEMBER_TWO), "a second member joins");
		ctx.True(LF_OrganizationManager.Promote(orgId, FOUNDER, MEMBER), "the founder promotes one to officer");

		// Masks the four bools cannot hold: the officer may manage territory, the member may not use the stash.
		int officerRights = LF_OrganizationConfig.DefaultRights(LF_OrganizationConfig.RANK_OFFICER) | LF_OrgRights.Bit(LF_EOrganizationPermission.MANAGE_TERRITORY);
		array<ref LF_OrganizationRankRow> rows = {};
		rows.Insert(LF_OrganizationRankRow.Create(orgId, LF_OrganizationConfig.RANK_OFFICER, 1, officerRights));
		rows.Insert(LF_OrganizationRankRow.Create(orgId, LF_OrganizationConfig.RANK_MEMBER, 2, 0));
		LF_OrganizationManager.ApplyRanks(rows);

		JsonSaveContext saveContext = new JsonSaveContext(false);
		ctx.True(LF_PersistenceComponentSerializer.SerializeStore(persistence, saveContext) == ESerializeResult.OK, "the store serializes");
		string saved = saveContext.SaveToString();
		ctx.True(saved.Contains("m_iRights"), "the version 27 save carries the rank rows");

		LF_OrganizationManager.Reset();
		for (int pass = 1; pass <= 2; pass++)
		{
			JsonLoadContext loadContext = new JsonLoadContext(false);
			ctx.True(loadContext.LoadFromString(saved), "the version 27 save parses");
			int version;
			loadContext.ReadValue("version", version);
			ctx.Equal(32, version, "the save is written at version 32 (orgranks is payload 27)");

			JsonLoadContext load = new JsonLoadContext(false);
			load.LoadFromString(saved);
			LF_PersistenceComponentSerializer.DeserializeStore(persistence, load);
			string tag = string.Format(" (version 27 load %1)", pass);

			LF_Organization back = LF_OrganizationManager.Get(orgId);
			ctx.NotNull(back, "the organization is back" + tag);
			if (!back)
				return;

			ctx.Equal(3, back.GetMemberCount(), "three members" + tag);
			ctx.Equal(3, back.GetRankCount(), "three rungs, none added" + tag);
			ctx.EqualStr(LF_OrganizationConfig.RANK_OFFICER, back.GetRankId(MEMBER), "the officer is still officer" + tag);
			ctx.Equal(1, back.RankPosition(MEMBER), "the officer is the second rung" + tag);
			ctx.Equal(officerRights, back.RightsOf(MEMBER), "the officer mask comes back whole" + tag);
			ctx.True(back.Allows(MEMBER, LF_EOrganizationPermission.MANAGE_TERRITORY), "the officer keeps the right no bool holds" + tag);
			ctx.Equal(0, back.RightsOf(MEMBER_TWO), "the member keeps the stash taken away" + tag);
			ctx.Equal(LF_OrgRights.ALL, back.RightsOf(FOUNDER), "the founder keeps every right" + tag);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckDroppedRows(LF_TestContext ctx)
	{
		LF_Organization org = LF_OrganizationManager.GetByCharacter(FOUNDER);
		if (!org)
			return;

		int before = org.GetRankRights(LF_OrganizationConfig.RANK_OFFICER);
		array<ref LF_OrganizationRankRow> rows = {};
		rows.Insert(LF_OrganizationRankRow.Create("org-persist-gone", LF_OrganizationConfig.RANK_OFFICER, 0, LF_OrgRights.ALL));
		rows.Insert(LF_OrganizationRankRow.Create(org.GetId(), "no-such-rank", 0, LF_OrgRights.ALL));
		rows.Insert(LF_OrganizationRankRow.Create(org.GetId(), LF_OrganizationConfig.RANK_OFFICER, 1, before | (1 << 20)));
		LF_OrganizationManager.ApplyRanks(rows);
		ctx.Equal(before, org.GetRankRights(LF_OrganizationConfig.RANK_OFFICER), "a row for a missing organization or rank changes nothing, an unknown bit is dropped");
		ctx.Equal(3, org.GetRankCount(), "a row for a missing rank adds no rank");
	}
}

//------------------------------------------------------------------------------------------------
//! What a test found in the stores an older-format or partial save of the live store drops, put back
//! when it is done: the organization registry with its rank masks (#750a) and the job XP only the
//! "joblevels" payload holds (#749; a "progression" load seeds just CIVILIAN and DELIVERY).
//! Persistence-tier tests reload such saves; the e2e save boot seeds both earlier in that tier.
class LF_TestLegacySnapshot : Managed
{
	protected ref array<ref LF_OrganizationRecord> m_aRecords;
	protected ref array<ref LF_OrganizationRankRow> m_aRanks;
	protected ref array<ref LF_JobXpRow> m_aJobXp;
	//! The crime reports with their looks (#758): an older save has no "wantedlooks", so loading
	//! one wipes the looks the shutdown save of `wt e2e` pass 1 must still hold.
	protected ref array<ref LF_WantedReportRecord> m_aWanted;
	//! The identity card registry (#902): a test that resets it must not take the e2e card's row.
	protected ref array<ref LF_PhotoIdRow> m_aIdCards;

	//------------------------------------------------------------------------------------------------
	static LF_TestLegacySnapshot Take()
	{
		LF_TestLegacySnapshot snapshot = new LF_TestLegacySnapshot();
		snapshot.m_aRecords = LF_OrganizationManager.ExportAll();
		snapshot.m_aRanks = LF_OrganizationManager.ExportRanks();
		snapshot.m_aJobXp = LF_JobLevels.ExportAll();
		snapshot.m_aWanted = LF_WantedReports.ExportAll();
		snapshot.m_aIdCards = LF_PhotoIdCard.ExportAll();
		return snapshot;
	}

	//------------------------------------------------------------------------------------------------
	void Restore()
	{
		LF_OrganizationManager.ApplyAll(m_aRecords);
		LF_OrganizationManager.ApplyRanks(m_aRanks);
		LF_JobLevels.ApplyAll(m_aJobXp);
		LF_WantedReports.ApplyAll(m_aWanted);
		LF_PhotoIdCard.ApplyAll(m_aIdCards);
	}
}
