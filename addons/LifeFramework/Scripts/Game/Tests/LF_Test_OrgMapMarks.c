// red-proof: 2026-11-17, cli test --no-build --red LF_Test_OrgMapMarks: RED PROOF OK with the orgmarks payload read skipped ("re-applying the rows applies the same 20 (expected 20, got 0)"); the first red run crashed on an unguarded loaded[0], guarded since.
// tier: PERSISTENCE
//! map/org-marks (#2240, card M12): an officer whose rank holds the territory right adds org marks that
//! reach the organization's members only (LF_MapShares ORG audience); a member without the right, an
//! outsider and a too-fast add are refused; the cap is 20 per organization; the "orgmarks" payload
//! round trips and re-puts the shares; a re-applied load is idempotent; leaving the org takes the
//! marks off that player's map. Across a restart (`tools\cli wt e2e`): the save boot records
//! an organization and its mark 1 (every non-reload run does), the reload boot finds it in the store and in the share table.
class LF_Test_OrgMapMarks : LF_Test
{
	protected static const string FOUNDER = "orgmarks-founder";
	protected static const string OFFICER = "orgmarks-officer";
	protected static const string MEMBER = "orgmarks-member";
	protected static const string OUTSIDER = "orgmarks-outsider";
	protected static const string E2E_FOUNDER = "orgmarks-e2e-founder";
	protected static const string E2E_ORG_NAME = "Marks Restart Crew";
	protected static const int P_OFFICER = 1;
	protected static const int P_MEMBER = 2;
	protected static const int P_OUTSIDER = 3;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "map/org-marks";
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
		// The reload boot: the organization and its mark the save boot recorded came back from the real save.
		LF_Organization reloadedOrg = LF_OrganizationManager.FindByName(E2E_ORG_NAME);
		ctx.NotNull(reloadedOrg, "the e2e organization is back after the restart");
		if (!reloadedOrg)
			return;

		string reloadedKey = LF_OrgMapMarks.KeyOf(reloadedOrg.GetId(), 1);
		ctx.True(LF_OrgMapMarks.Has(reloadedKey), "the org mark is in the store after the restart");
		ctx.True(LF_MapShares.Has(reloadedKey), "the org mark is shared again after the restart");
		return;
#endif

		ctx.True(LF_PersistenceComponentSerializer.PayloadSinceVersion("orgmarks") > 0, "orgmarks is a registered payload");

		array<ref LF_OrgMapMarkRow> before = LF_OrgMapMarks.ExportAll();
		array<ref LF_OrganizationRecord> orgsBefore = LF_OrganizationManager.ExportAll();
		array<ref LF_OrganizationRankRow> ranksBefore = LF_OrganizationManager.ExportRanks();
		LF_OrgMapMarks.ApplyAll(null);
		LF_OrgMapMarks.ResetStatics();
		LF_MapShares.ResetStatics();
		LF_OrganizationManager.Reset();

		LF_Test_MapShareSink sink = new LF_Test_MapShareSink();
		LF_Test_MapShareFacts facts = new LF_Test_MapShareFacts();
		LF_MapShares.SetSink(sink);
		LF_MapShares.SetFacts(facts);

		LF_Organization org = LF_OrganizationManager.Create(FOUNDER, "Marks Crew");
		ctx.NotNull(org, "the organization is founded");
		if (!org)
		{
			Restore(before, orgsBefore, ranksBefore);
			return;
		}

		string orgId = org.GetId();
		ctx.True(LF_OrganizationManager.JoinDirect(orgId, OFFICER, LF_OrganizationConfig.RANK_OFFICER), "the officer joins");
		ctx.True(LF_OrganizationManager.JoinDirect(orgId, MEMBER, LF_OrganizationConfig.RANK_MEMBER), "the member joins");
		int officerRights = org.GetRankRights(LF_OrganizationConfig.RANK_OFFICER) | LF_OrgRights.Bit(LF_OrgMapMarks.RIGHT);
		ctx.True(org.ApplyRankRow(LF_OrganizationRankRow.Create(orgId, LF_OrganizationConfig.RANK_OFFICER, 1, officerRights)), "the officer rank gets the territory right");
		ctx.False(org.Allows(MEMBER, LF_OrgMapMarks.RIGHT), "a plain member has no territory right");

		facts.Set(P_OFFICER, OFFICER, "", orgId);
		facts.Set(P_MEMBER, MEMBER, "", orgId);
		facts.Set(P_OUTSIDER, OUTSIDER, "", "");

		// An officer adds two marks; the members get them, the outsider nothing.
		string first = LF_OrgMapMarks.Add(OFFICER, 1200, 3400, 1, 2, "  stash by the mill  ", 100);
		string second = LF_OrgMapMarks.Add(OFFICER, 1300, 3500, 0, 0, "meet", 110);
		ctx.EqualStr(LF_OrgMapMarks.KeyOf(orgId, 1), first, "the first mark is serial 1");
		ctx.EqualStr(LF_OrgMapMarks.KeyOf(orgId, 2), second, "the second mark is serial 2");
		ctx.Equal(2, LF_OrgMapMarks.CountOf(orgId), "the organization holds two marks");
		ctx.True(sink.Got(string.Format("add:%1:%2", P_OFFICER, first)), "the officer's map gets the mark");
		ctx.True(sink.Got(string.Format("add:%1:%2", P_MEMBER, first)), "the member's map gets the mark");
		ctx.Equal(0, sink.CountFor(P_OUTSIDER), "the outsider gets nothing");

		// Refusals: no right, no organization, too fast, outside the wire range.
		ctx.EqualStr("", LF_OrgMapMarks.Add(MEMBER, 10, 10, 0, 0, "", 200), "a member without the right is refused");
		ctx.EqualStr("", LF_OrgMapMarks.Add(OUTSIDER, 10, 10, 0, 0, "", 200), "an outsider is refused");
		ctx.EqualStr("", LF_OrgMapMarks.Add(OFFICER, 10, 10, 0, 0, "", 111), "a second add within the rate limit is refused");
		ctx.EqualStr("", LF_OrgMapMarks.Add(OFFICER, 10, 10, 300, 0, "", 200), "an icon past the wire range is refused");
		ctx.Equal(2, LF_OrgMapMarks.CountOf(orgId), "refusals add nothing");

		// The cap: 20 per organization.
		float now = 200;
		for (int i = 0; i < 18; i++)
		{
			now += 10;
			LF_OrgMapMarks.Add(OFFICER, i, i, 0, 0, "", now);
		}
		ctx.Equal(LF_OrgMapMarks.CAP, LF_OrgMapMarks.CountOf(orgId), "the organization reaches the cap of 20");
		ctx.EqualStr("", LF_OrgMapMarks.Add(OFFICER, 99, 99, 0, 0, "", now + 10), "a 21st mark is refused");

		// Round trip through the payload.
		JsonSaveContext saveContext = new JsonSaveContext(false);
		LF_PersistenceComponentSerializer.WritePersistencePayload(persistence, saveContext, "orgmarks");
		string saved = saveContext.SaveToString();
		ctx.True(saved.Contains("stash by the mill"), "the save carries the mark's text");

		LF_OrgMapMarks.ApplyAll(null);
		ctx.Equal(0, LF_OrgMapMarks.CountOf(orgId), "the restarted session starts with no org marks");
		ctx.False(LF_MapShares.Has(first), "clearing the marks takes them out of the share table");

		JsonLoadContext loadContext = new JsonLoadContext(false);
		ctx.True(loadContext.LoadFromString(saved), "the save parses");
		if (!RedProof())
			LF_PersistenceComponentSerializer.ReadPersistencePayload(persistence, loadContext, "orgmarks");

		ctx.Equal(LF_OrgMapMarks.CAP, LF_OrgMapMarks.CountOf(orgId), "all 20 marks are back after the load");
		ctx.True(LF_OrgMapMarks.Has(first) && LF_OrgMapMarks.Has(second), "both first marks are back");
		ctx.True(LF_MapShares.Has(first), "a loaded mark is shared again");
		ctx.True(LF_MapShares.IsDelivered(P_MEMBER, first), "a loaded mark reaches the member");
		ctx.False(LF_MapShares.IsDelivered(P_OUTSIDER, first), "a loaded mark does not reach the outsider");
		array<ref LF_OrgMapMarkRow> loaded = LF_OrgMapMarks.ExportAll();
		ctx.True(loaded.Count() > 0, "the loaded rows export");
		if (loaded.Count() > 0)
		{
			ctx.EqualStr("stash by the mill", loaded[0].m_sText, "the text is trimmed and kept");
			ctx.Equal(1200, loaded[0].m_iX, "the position is kept");
		}

		// Idempotent re-apply; a bad and a repeated row are skipped; the cap holds on load.
		ctx.Equal(LF_OrgMapMarks.CAP, LF_OrgMapMarks.ApplyAll(LF_OrgMapMarks.ExportAll()), "re-applying the rows applies the same 20");
		array<ref LF_OrgMapMarkRow> rows = LF_OrgMapMarks.ExportAll();
		rows.Insert(LF_OrgMapMarkRow.Make("", 5, 0, 0, 0, 0, "", ""));
		rows.Insert(LF_OrgMapMarkRow.Make(orgId, 1, 0, 0, 0, 0, "", ""));
		rows.Insert(LF_OrgMapMarkRow.Make(orgId, 99, 0, 0, 0, 0, "", ""));
		ctx.Equal(LF_OrgMapMarks.CAP, LF_OrgMapMarks.ApplyAll(rows), "no-org, repeated and over-cap rows are skipped");

		// Removal: the right is needed; a removed serial is not reused.
		ctx.False(LF_OrgMapMarks.Remove(MEMBER, first), "a member without the right cannot remove a mark");
		ctx.True(LF_OrgMapMarks.Remove(OFFICER, first), "the officer removes a mark");
		ctx.False(LF_MapShares.Has(first), "the removed mark leaves the share table");
		string latest = LF_OrgMapMarks.Add(OFFICER, 5, 5, 0, 0, " #LF-Org_RankFounder ", now + 100);
		ctx.EqualStr(LF_OrgMapMarks.KeyOf(orgId, 21), latest, "a new mark takes the next serial, not the removed one");
		array<ref LF_OrgMapMarkRow> afterAdd = LF_OrgMapMarks.ExportAll();
		ctx.EqualStr("LF-Org_RankFounder", afterAdd[afterAdd.Count() - 1].m_sText, "player text cannot be a string table key");
		ctx.True(LF_OrgMapMarks.Remove(OFFICER, latest), "the officer removes the newest mark");
		ctx.EqualStr(LF_OrgMapMarks.KeyOf(orgId, 22), LF_OrgMapMarks.Add(OFFICER, 6, 6, 0, 0, "", now + 200), "the removed newest serial is not handed out again");

		// Leaving: the member's map loses the org's marks.
		ctx.True(LF_OrganizationManager.LeaveDirect(MEMBER), "the member leaves");
		facts.Set(P_MEMBER, MEMBER, "", "");
		LF_MapShares.Refresh(P_MEMBER);
		ctx.Equal(0, LF_MapShares.DeliveredCount(P_MEMBER), "a player who left holds no org marks");

		Restore(before, orgsBefore, ranksBefore);
		// Every non-reload run (the e2e save boot among them) leaves one organization with one org mark
		// for the reload boot, as LF_Test_E2E_SaveStateWrite does.
		LF_Organization e2eOrg = LF_OrganizationManager.FindByName(E2E_ORG_NAME);
		if (!e2eOrg)
			e2eOrg = LF_OrganizationManager.Create(E2E_FOUNDER, E2E_ORG_NAME);
		ctx.NotNull(e2eOrg, "the save boot founded the e2e organization");
		if (e2eOrg)
		{
			string e2eKey = LF_OrgMapMarks.KeyOf(e2eOrg.GetId(), 1);
			if (!LF_OrgMapMarks.Has(e2eKey))
				LF_OrgMapMarks.Add(E2E_FOUNDER, 640, 480, 0, 0, "e2e", LF_MapShares.Now() + 1000);
			ctx.True(LF_OrgMapMarks.Has(e2eKey), "the save boot recorded the e2e org mark");
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void Restore(array<ref LF_OrgMapMarkRow> before, array<ref LF_OrganizationRecord> orgsBefore, array<ref LF_OrganizationRankRow> ranksBefore)
	{
		LF_OrganizationManager.Reset();
		LF_OrganizationManager.ApplyAll(orgsBefore);
		LF_OrganizationManager.ApplyRanks(ranksBefore);
		LF_MapShares.ResetStatics();
		LF_OrgMapMarks.ResetStatics();
		LF_MapShares.EnsureStarted();
		LF_OrgMapMarks.ApplyAll(before);
	}
}
