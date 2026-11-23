// red-proof: Observed 2026-11-23 with `--no-build --red`: `RED-OK org/prospect (fails with the hook on, 64 assertions)`, the prospect mark wiped after the join and the section read skipped.
//! Prospect rank (#4455, gangs A2). A member who joins through LF_OrgInvites is a prospect: the fund
//! withdrawal and the stash right are refused with the prospect words, the rank reads "Prospect", and every
//! other right stays. After 48 hours (clock injected) or a leader's promote both work. The join time is in
//! the "org_prospects" save section: a save and load keeps the prospect, a second load changes nothing, and
//! a save from before the section has no prospects. Restart: `wt e2e` keeps a prospect across the boots.
[LF_TestInfo(LF_TestTier.PERSISTENCE)]
class LF_Test_OrgProspect : LF_Test
{
	protected static const string FOUNDER = "prospect-test-founder";
	protected static const string RECRUIT = "prospect-test-recruit";
	protected static const string RECRUIT_B = "prospect-test-recruit-b";
	protected static const string E2E_FOUNDER = "e2e-prospect-founder";
	protected static const string E2E_RECRUIT = "e2e-prospect-recruit";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "org/prospect";
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
		if (LF_E2E.RequiresSection(LF_OrgProspectSavePayload.NAME))
		{
			LF_Organization e2eOrg = LF_OrganizationManager.GetByCharacter(E2E_FOUNDER);
			ctx.NotNull(e2eOrg, "the e2e crew is back after the restart");
			if (e2eOrg)
			{
				ctx.True(LF_OrgProspect.IsProspect(e2eOrg, E2E_RECRUIT), "the e2e recruit is still a prospect after the restart");
				ctx.False(e2eOrg.Allows(E2E_RECRUIT, LF_EOrganizationPermission.USE_STASH), "the e2e prospect still cannot open the stash");
			}
		}

		return;
#endif

		ctx.NotNull(LF_SaveSections.Get(LF_OrgProspectSavePayload.NAME), "org_prospects is a registered save section");

		LF_TestLegacySnapshot kept = LF_TestLegacySnapshot.Take();
		LF_OrgProspect.SetClock(0);
		array<ref LF_OrgProspectRow> keptRows = LF_OrgProspect.ExportAll();

		CheckRules(ctx);
		CheckPeriod(ctx);
		CheckPromote(ctx);
		CheckSave(ctx, persistence);

		LF_OrganizationManager.Reset();
		kept.Restore();
		LF_OrgProspect.ResetStatics();
		LF_OrgProspect.ApplyAll(keptRows);

#ifdef LF_E2E_HOST_STOP
		// The save boot: a crew with one prospect on the real clock goes into the shutdown save.
		LF_Organization e2e = LF_OrganizationManager.Create(E2E_FOUNDER, "E2E Prospect Crew");
		ctx.NotNull(e2e, "the e2e crew is founded before the save");
		if (e2e)
		{
			float now = LF_OrgInvites.Now();
			LF_OrgInvites.Offer(E2E_FOUNDER, 0, E2E_RECRUIT, now);
			ctx.True(LF_OrgInvites.Accept(E2E_RECRUIT, E2E_FOUNDER, now) == LF_EOrgOfferResult.JOINED, "the e2e recruit joins by invite before the save");
		}
#endif
	}

	//------------------------------------------------------------------------------------------------
	//! A crew with 500 in the fund whose two lower rungs may withdraw and open the stash, so only the
	//! prospect rule can refuse them.
	protected LF_Organization Found(LF_TestContext ctx)
	{
		LF_OrganizationManager.Reset();
		LF_OrgInvites.Reset();
		LF_OrgProspect.ResetStatics();
		LF_Organization org = LF_OrganizationManager.Create(FOUNDER, "Prospect Test Crew");
		ctx.NotNull(org, "the test crew is founded");
		if (!org)
			return null;

		int open = LF_OrgRights.Bit(LF_EOrganizationPermission.WITHDRAW) | LF_OrgRights.Bit(LF_EOrganizationPermission.USE_STASH) | LF_OrgRights.Bit(LF_EOrganizationPermission.INVITE);
		org.ApplyRankRow(LF_OrganizationRankRow.Create(org.GetId(), LF_OrganizationConfig.RANK_OFFICER, 1, open));
		org.ApplyRankRow(LF_OrganizationRankRow.Create(org.GetId(), LF_OrganizationConfig.RANK_MEMBER, 2, open));
		org.SortLadder();
		org.Credit(500);
		return org;
	}

	//------------------------------------------------------------------------------------------------
	protected bool Join(LF_TestContext ctx, LF_Organization org, string recruit)
	{
		float now = LF_OrgInvites.Now();
		ctx.Equal(LF_EOrgOfferResult.OFFERED, LF_OrgInvites.Offer(FOUNDER, 0, recruit, now), "the founder offers " + recruit + " a place");
		bool joined = LF_OrgInvites.Accept(recruit, FOUNDER, now) == LF_EOrgOfferResult.JOINED;
		ctx.True(joined, recruit + " joins through the invite");
		// RedProof(): the prospect mark is wiped after the join, so every refusal below fails (`--red`).
		if (RedProof())
			LF_OrgProspect.Clear(recruit, "red proof");

		return joined;
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckRules(LF_TestContext ctx)
	{
		int all = LF_OrgRights.ALL;
		ctx.False(LF_OrgRights.Has(LF_OrgRights.ForProspect(all), LF_EOrganizationPermission.WITHDRAW), "a prospect mask drops withdraw");
		ctx.False(LF_OrgRights.Has(LF_OrgRights.ForProspect(all), LF_EOrganizationPermission.USE_STASH), "a prospect mask drops the stash");
		ctx.False(LF_OrgRights.Has(LF_OrgRights.ForProspect(all), LF_EOrganizationPermission.SPEND_FUND), "a prospect mask drops fund spending");
		ctx.True(LF_OrgRights.Has(LF_OrgRights.ForProspect(all), LF_EOrganizationPermission.INVITE), "a prospect mask keeps invite");
		ctx.False(LF_OrgRights.BarsProspect(LF_EOrganizationPermission.KICK), "kick is not a prospect bar");
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckPeriod(LF_TestContext ctx)
	{
		int start = System.GetUnixTime();
		LF_OrgProspect.SetClock(start);
		LF_Organization org = Found(ctx);
		if (!org || !Join(ctx, org, RECRUIT))
			return;

		ctx.True(LF_OrgProspect.IsProspect(org, RECRUIT), "an invited member is a prospect");
		ctx.False(LF_OrgProspect.IsProspect(org, FOUNDER), "the founder is no prospect");
		ctx.EqualStr(LF_OrgProspect.RANK_KEY, org.GetRankName(RECRUIT), "the prospect's rank reads Prospect");
		ctx.False(LF_OrganizationManager.Withdraw(org.GetId(), RECRUIT, 50), "a prospect's fund withdrawal is refused");
		ctx.Equal(500, org.GetFund(), "the fund is untouched by the refused withdrawal");
		ctx.False(org.Allows(RECRUIT, LF_EOrganizationPermission.USE_STASH), "a prospect cannot open the stash");
		ctx.EqualStr(LF_OrgProspect.REFUSED_KEY, LF_OrgProspect.RefusalFor(org, RECRUIT, LF_EOrganizationPermission.WITHDRAW), "the withdrawal is refused in words");
		ctx.EqualStr(LF_OrgProspect.REFUSED_KEY, LF_OrgProspect.RefusalFor(org, RECRUIT, LF_EOrganizationPermission.USE_STASH), "the stash is refused in words");
		ctx.True(org.Allows(RECRUIT, LF_EOrganizationPermission.INVITE), "a prospect keeps the rank's other rights");
		ctx.False(LF_OrgRights.Has(org.RightsOf(RECRUIT), LF_EOrganizationPermission.USE_STASH), "the replicated rights mask has no stash for a prospect");

		LF_OrgProspect.SetClock(start + LF_OrgProspect.PERIOD_SECONDS - 60);
		ctx.True(LF_OrgProspect.IsProspect(org, RECRUIT), "a minute short of 48 hours is still a prospect");

		LF_OrgProspect.SetClock(start + LF_OrgProspect.PERIOD_SECONDS);
		ctx.False(LF_OrgProspect.IsProspect(org, RECRUIT), "after 48 hours the prospect period is over");
		ctx.EqualStr(LF_OrganizationConfig.DefaultName(LF_OrganizationConfig.RANK_MEMBER), org.GetRankName(RECRUIT), "after 48 hours the rank reads Member");
		ctx.True(LF_OrganizationManager.Withdraw(org.GetId(), RECRUIT, 50), "after 48 hours the withdrawal works");
		ctx.Equal(450, org.GetFund(), "the fund paid out 50");
		ctx.True(org.Allows(RECRUIT, LF_EOrganizationPermission.USE_STASH), "after 48 hours the stash opens");
		ctx.Equal(0, LF_OrgProspect.ExportAll().Count(), "a lapsed prospect is not saved");
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckPromote(LF_TestContext ctx)
	{
		LF_OrgProspect.SetClock(System.GetUnixTime());
		LF_Organization org = Found(ctx);
		if (!org || !Join(ctx, org, RECRUIT_B))
			return;

		ctx.False(org.Allows(RECRUIT_B, LF_EOrganizationPermission.USE_STASH), "before the promote the stash is shut");
		ctx.True(LF_OrganizationManager.Invite(org.GetId(), FOUNDER, RECRUIT), "a direct add (no offer) joins");
		ctx.False(LF_OrgProspect.IsProspect(org, RECRUIT), "a direct add is a full member, not a prospect");
		ctx.True(LF_OrganizationManager.Promote(org.GetId(), FOUNDER, RECRUIT_B), "the leader promotes the prospect");
		ctx.False(LF_OrgProspect.IsProspect(org, RECRUIT_B), "a promoted member is no prospect");
		ctx.True(LF_OrganizationManager.Withdraw(org.GetId(), RECRUIT_B, 20), "after the promote the withdrawal works");
		ctx.True(org.Allows(RECRUIT_B, LF_EOrganizationPermission.USE_STASH), "after the promote the stash opens");
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckSave(LF_TestContext ctx, LF_PersistenceComponent persistence)
	{
		int start = System.GetUnixTime();
		LF_OrgProspect.SetClock(start);
		LF_Organization org = Found(ctx);
		if (!org || !Join(ctx, org, RECRUIT))
			return;

		JsonSaveContext saveContext = new JsonSaveContext(false);
		ctx.True(LF_PersistenceComponentSerializer.SerializeStore(persistence, saveContext) == ESerializeResult.OK, "the store serializes");
		string saved = saveContext.SaveToString();
		ctx.True(saved.Contains(SectionKey()), "the save carries the org_prospects section");

		// Restart: the session holds no prospect; a day has passed.
		LF_OrgProspect.ResetStatics();
		LF_OrgProspect.SetClock(start + 86400);
		ctx.False(LF_OrgProspect.IsProspect(org, RECRUIT), "the prospect table is empty before the load");

		JsonLoadContext loadContext = new JsonLoadContext(false);
		ctx.True(loadContext.LoadFromString(saved), "the save parses");
		if (!RedProof())
			ReadSection(loadContext, ctx);

		ctx.True(LF_OrgProspect.IsProspect(org, RECRUIT), "a day after the join, the loaded member is still a prospect");
		ctx.False(LF_OrganizationManager.Withdraw(org.GetId(), RECRUIT, 50), "after the load the withdrawal is still refused");

		JsonLoadContext again = new JsonLoadContext(false);
		again.LoadFromString(saved);
		ReadSection(again, ctx);
		ctx.Equal(1, LF_OrgProspect.ExportAll().Count(), "a second load does not duplicate the prospect");

		LF_OrgProspect.SetClock(start + LF_OrgProspect.PERIOD_SECONDS);
		ctx.False(LF_OrgProspect.IsProspect(org, RECRUIT), "the saved join time still ends the period at 48 hours");

		// A boot from a save before the section: the table starts empty and stays so; nobody is a prospect.
		LF_OrgProspect.SetClock(start);
		LF_OrgProspect.ResetStatics();
		LF_OrgProspect.SetClock(start);
		LF_SavePayload section = LF_SaveSections.Get(LF_OrgProspectSavePayload.NAME);
		if (section)
		{
			section.OnAbsent();
			ctx.False(LF_OrgProspect.IsProspect(org, RECRUIT), "an older save has no prospects, so the member keeps full rights");
		}

		LF_OrgProspect.SetClock(0);
	}

	//------------------------------------------------------------------------------------------------
	protected static string SectionKey()
	{
		return "\"" + LF_SaveSections.OBJECT_PREFIX + LF_OrgProspectSavePayload.NAME + "\"";
	}

	//------------------------------------------------------------------------------------------------
	protected static void ReadSection(notnull JsonLoadContext context, LF_TestContext ctx)
	{
		LF_SavePayload section = LF_SaveSections.Get(LF_OrgProspectSavePayload.NAME);
		if (!section)
			return;

		ctx.True(context.StartObject(LF_SaveSections.OBJECT_PREFIX + LF_OrgProspectSavePayload.NAME), "the org_prospects section opens");
		int savedVersion;
		context.ReadValue(LF_SaveSections.VERSION_KEY, savedVersion);
		ctx.Equal(section.Version(), savedVersion, "the section carries its own version");
		ctx.True(section.ReadSection(context, savedVersion), "the org_prospects section reads");
		context.EndObject();
	}
}

//------------------------------------------------------------------------------------------------
//! The prospect table rides the legacy snapshot (#4455): persistence tests that reload a save
//! written before the join replace it, and the e2e prospect must still be there at the shutdown save.
modded class LF_TestLegacySnapshot
{
	protected ref array<ref LF_OrgProspectRow> m_aProspects;

	//------------------------------------------------------------------------------------------------
	override static LF_TestLegacySnapshot Take()
	{
		LF_TestLegacySnapshot snapshot = super.Take();
		if (snapshot)
			snapshot.m_aProspects = LF_OrgProspect.ExportAll();

		return snapshot;
	}

	//------------------------------------------------------------------------------------------------
	override void Restore()
	{
		super.Restore();
		if (m_aProspects)
			LF_OrgProspect.ApplyAll(m_aProspects);
	}
}
