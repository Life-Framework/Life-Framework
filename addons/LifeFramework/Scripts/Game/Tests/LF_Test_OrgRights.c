// red-proof: observed 2026-11-12 with `tools\cli test --no-build --only LF_Test_OrgRights,LF_Test_OrgRightsPersist`:
// dropping the ladder check from LF_OrganizationManager.Kick failed `[LFTEST] FAIL org/rights`,
// `- an officer cannot kick an equal`, `- the demoted officer is a member again (expected 'member', got '')`.
// Restored; green.
// tier: LOGIC
//! #750a rights and ladder: the bit per right (and the replicated bits match it), the back-fill from
//! the four bools a rank saved before the mask, each server action against a rank with and without
//! its right, the ladder rules for promote, demote and kick (never up to your own rung, never on an
//! equal), and the offer side's entries agreeing with the server.
class LF_Test_OrgRights : LF_Test
{
	protected static const string FOUNDER = "org-rights-founder";
	protected static const string OFFICER = "org-rights-officer";
	protected static const string MEMBER = "org-rights-member";
	protected static const string MEMBER_TWO = "org-rights-member-two";
	protected static const string OUTSIDER = "org-rights-outsider";
	protected static const string RIVAL = "org-rights-rival";

	override string GetName()
	{
		return "org/rights";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_TestLegacySnapshot kept = LF_TestLegacySnapshot.Take();
		LF_OrganizationManager.Reset();
		LF_OrgInvites.Reset();

		CheckBits(ctx);
		CheckLegacy(ctx);
		CheckServer(ctx);
		CheckDamagedRank(ctx);

		LF_OrgInvites.Reset();
		LF_OrganizationManager.Reset();
		kept.Restore();
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckBits(LF_TestContext ctx)
	{
		ctx.Equal(1, LF_OrgRights.Bit(LF_EOrganizationPermission.WITHDRAW), "withdraw is bit 0, as the first saved bool");
		ctx.Equal(2, LF_OrgRights.Bit(LF_EOrganizationPermission.INVITE), "invite is bit 1, as the second saved bool");
		ctx.Equal(4, LF_OrgRights.Bit(LF_EOrganizationPermission.KICK), "kick is bit 2, as the third saved bool");
		ctx.Equal(8, LF_OrgRights.Bit(LF_EOrganizationPermission.BUY_UPGRADE), "buy upgrade is bit 3, as the fourth saved bool");
		ctx.Equal(16, LF_OrgRights.Bit(LF_EOrganizationPermission.MANAGE_RANKS), "manage ranks is bit 4");
		ctx.Equal(256, LF_OrgRights.Bit(LF_EOrganizationPermission.SPEND_FUND), "spend fund is bit 8, the last reserved right");
		ctx.Equal(511, LF_OrgRights.ALL, "ALL is every defined right");
		ctx.Equal(LF_OrgRules.RIGHT_WITHDRAW, LF_OrgRights.Bit(LF_EOrganizationPermission.WITHDRAW), "the replicated withdraw bit is the saved one");
		ctx.Equal(LF_OrgRules.RIGHT_INVITE, LF_OrgRights.Bit(LF_EOrganizationPermission.INVITE), "the replicated invite bit is the saved one");
		ctx.Equal(LF_OrgRules.RIGHT_KICK, LF_OrgRights.Bit(LF_EOrganizationPermission.KICK), "the replicated kick bit is the saved one");
		ctx.Equal(LF_OrgRules.RIGHT_BUY, LF_OrgRights.Bit(LF_EOrganizationPermission.BUY_UPGRADE), "the replicated buy bit is the saved one");
		ctx.Equal(LF_OrgRules.RIGHT_MANAGE_RANKS, LF_OrgRights.Bit(LF_EOrganizationPermission.MANAGE_RANKS), "the replicated manage-ranks bit is the saved one");
		ctx.Equal(0, LF_OrgRules.RIGHT_FOUNDER & LF_OrgRights.ALL, "the view-only founder bit is no saved right");
		ctx.Equal(0, LF_OrgRights.Clean(1 << 12), "a bit no right is defined for is dropped");
		ctx.False(LF_OrgRights.Has(0, LF_EOrganizationPermission.INVITE), "no rights allow nothing");
		ctx.True(LF_OrgRights.Has(LF_OrgRights.ALL, LF_EOrganizationPermission.SPEND_FUND), "every right allows the last one");
		ctx.EqualStr("INVITE,KICK", LF_OrgRights.Describe(6), "a mask reads as its rights in the log");

		LF_OrgView view = LF_OrgView.Create("org", "Org", 0, "", 0).SetLadder(2, 3);
		LF_OrgView copy = LF_OrgView.Create("org", "Org", 0, "", 0);
		copy.UnpackLadder(view.PackLadder());
		ctx.Equal(2, copy.m_iRankPosition, "the replicated ladder keeps the position");
		ctx.Equal(3, copy.m_iLadderSize, "the replicated ladder keeps the size");
		LF_OrgView none = LF_OrgView.Create("", "", 0, "", 0);
		copy.UnpackLadder(none.PackLadder());
		ctx.Equal(-1, copy.m_iRankPosition, "no rank replicates as no position");
	}

	//------------------------------------------------------------------------------------------------
	//! A rank saved as four bools: the founder could do everything, any other rank keeps its bools
	//! and gets the stash every member could open.
	protected void CheckLegacy(LF_TestContext ctx)
	{
		int stash = LF_OrgRights.Bit(LF_EOrganizationPermission.USE_STASH);
		ctx.Equal(LF_OrgRights.ALL, LF_OrgRights.MigrateLegacy(LF_OrganizationConfig.RANK_FOUNDER, true, true, true, true), "a saved founder gets every right");
		ctx.Equal(stash, LF_OrgRights.MigrateLegacy(LF_OrganizationConfig.RANK_MEMBER, false, false, false, false), "a saved member gets only the stash");
		ctx.Equal(LF_OrgRights.Bit(LF_EOrganizationPermission.WITHDRAW) | LF_OrgRights.Bit(LF_EOrganizationPermission.BUY_UPGRADE) | stash,
			LF_OrgRights.MigrateLegacy("treasurer", true, false, false, true), "any other saved rank keeps exactly its bools");
		ctx.Equal(LF_OrgRights.Bit(LF_EOrganizationPermission.INVITE) | LF_OrgRights.Bit(LF_EOrganizationPermission.KICK),
			LF_OrgRights.FromLegacy(false, true, true, false), "the bools map to bits 0-3 in their saved order");

		LF_OrganizationRankRecord oldFounder = LF_OrganizationRankRecord.Create(LF_OrganizationConfig.RANK_FOUNDER, "Founder", true, true, true, true);
		LF_OrganizationRank founderRank = LF_OrganizationRank.FromRecord(oldFounder, 0);
		ctx.True(founderRank.Allows(LF_EOrganizationPermission.MANAGE_RANKS), "a founder read from its bools may manage ranks");
		LF_OrganizationRankRecord back = founderRank.ToRecord();
		ctx.True(back.m_bCanWithdraw && back.m_bCanInvite && back.m_bCanKick && back.m_bCanBuyUpgrade, "the mask writes bits 0-3 back as the four bools");
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckServer(LF_TestContext ctx)
	{
		LF_Organization org = LF_OrganizationManager.Create(FOUNDER, "Rights Ladder");
		ctx.NotNull(org, "the founder registers Rights Ladder");
		if (!org)
			return;

		string orgId = org.GetId();
		array<string> ladder = {};
		org.GetRankIds(ladder);
		ctx.Equal(3, ladder.Count(), "a new organization has three rungs");
		if (ladder.Count() == 3)
		{
			ctx.EqualStr(LF_OrganizationConfig.RANK_FOUNDER, ladder[0], "the founder tops the ladder");
			ctx.EqualStr(LF_OrganizationConfig.RANK_OFFICER, ladder[1], "the officer is the middle rung");
			ctx.EqualStr(LF_OrganizationConfig.RANK_MEMBER, ladder[2], "the member is the bottom rung");
		}

		ctx.True(LF_OrganizationManager.Invite(orgId, FOUNDER, OFFICER), "the founder invites the officer-to-be");
		ctx.True(LF_OrganizationManager.Invite(orgId, FOUNDER, MEMBER), "the founder invites a member");
		ctx.True(LF_OrganizationManager.Invite(orgId, FOUNDER, MEMBER_TWO), "the founder invites a second member");
		ctx.EqualStr(LF_OrganizationConfig.RANK_MEMBER, org.GetRankId(MEMBER), "an invite joins on the bottom rung");

		// A member lacks every managing right; each server action refuses it.
		ctx.False(LF_OrganizationManager.Invite(orgId, MEMBER, OUTSIDER), "a member cannot invite (no invite right)");
		ctx.False(LF_OrganizationManager.Kick(orgId, MEMBER, MEMBER_TWO), "a member cannot kick (no kick right)");
		ctx.True(LF_OrganizationManager.Deposit(orgId, MEMBER, 300), "a member may deposit");
		ctx.False(LF_OrganizationManager.Withdraw(orgId, MEMBER, 10), "a member cannot withdraw (no withdraw right)");
		ctx.False(LF_OrganizationManager.BuyUpgrade(orgId, MEMBER, LF_EOrganizationUpgrade.PROCESSING_SLOT), "a member cannot buy an upgrade (no buy right)");
		ctx.False(LF_OrganizationManager.Promote(orgId, MEMBER, MEMBER_TWO), "a member cannot promote (no manage-ranks right)");
		ctx.Equal(300, org.GetFund(), "the refused withdraw and buy left the fund");

		// The founder holds every right.
		ctx.True(LF_OrganizationManager.Withdraw(orgId, FOUNDER, 10), "the founder withdraws");
		ctx.True(LF_OrganizationManager.Promote(orgId, FOUNDER, OFFICER), "the founder promotes a member to officer");
		ctx.EqualStr(LF_OrganizationConfig.RANK_OFFICER, org.GetRankId(OFFICER), "the promoted member is officer");
		ctx.Equal(LF_OrganizationConfig.DefaultRights(LF_OrganizationConfig.RANK_OFFICER), org.RightsOf(OFFICER), "the officer holds the officer rung's rights");

		// The ladder: nobody reaches their own rung, nobody moves an equal or a superior.
		ctx.False(LF_OrganizationManager.Promote(orgId, FOUNDER, OFFICER), "an officer is not promoted to the founder's rung");
		ctx.False(LF_OrganizationManager.Promote(orgId, FOUNDER, FOUNDER), "nobody changes their own rank");
		ctx.False(LF_OrganizationManager.Demote(orgId, FOUNDER, MEMBER), "the bottom rung has nowhere lower");
		ctx.False(LF_OrganizationManager.Promote(orgId, OFFICER, MEMBER), "an officer cannot promote (no manage-ranks right)");
		ctx.False(LF_OrganizationManager.Withdraw(orgId, OFFICER, 10), "an officer cannot withdraw (no withdraw right)");
		ctx.False(LF_OrganizationManager.Kick(orgId, OFFICER, FOUNDER), "an officer cannot kick the founder");
		ctx.True(LF_OrganizationManager.Kick(orgId, OFFICER, MEMBER_TWO), "an officer kicks a member below them");
		ctx.True(LF_OrganizationManager.Invite(orgId, OFFICER, MEMBER_TWO), "an officer invites, and the member is back");
		ctx.True(LF_OrganizationManager.Promote(orgId, FOUNDER, MEMBER_TWO), "the founder makes a second officer");
		ctx.False(LF_OrganizationManager.Kick(orgId, OFFICER, MEMBER_TWO), "an officer cannot kick an equal");
		ctx.True(LF_OrganizationManager.Demote(orgId, FOUNDER, MEMBER_TWO), "the founder demotes the second officer");
		ctx.EqualStr(LF_OrganizationConfig.RANK_MEMBER, org.GetRankId(MEMBER_TWO), "the demoted officer is a member again");

		// A rung below the founder with manage-ranks still cannot raise anyone to its own rung.
		array<ref LF_OrganizationRankRow> rows = {};
		rows.Insert(LF_OrganizationRankRow.Create(orgId, LF_OrganizationConfig.RANK_OFFICER, 1, org.GetRankRights(LF_OrganizationConfig.RANK_OFFICER) | LF_OrgRights.Bit(LF_EOrganizationPermission.MANAGE_RANKS)));
		LF_OrganizationManager.ApplyRanks(rows);
		ctx.True(org.Allows(OFFICER, LF_EOrganizationPermission.MANAGE_RANKS), "the officer rung now carries manage-ranks");
		ctx.False(LF_OrganizationManager.Promote(orgId, OFFICER, MEMBER), "an officer with manage-ranks cannot make an officer");

		CheckOfferSide(ctx, org);

		// Another organization's founder has no say here.
		LF_Organization rival = LF_OrganizationManager.Create(RIVAL, "Rights Rival");
		if (rival)
		{
			ctx.False(LF_OrganizationManager.Promote(orgId, RIVAL, MEMBER), "a rival founder cannot promote here");
			ctx.False(LF_OrganizationManager.Kick(orgId, RIVAL, MEMBER), "a rival founder cannot kick here");
		}
	}

	//------------------------------------------------------------------------------------------------
	//! A member saved on a rank the table lacks (a damaged save) has no rights and no rung; the founder
	//! can still remove them, nobody can promote them.
	protected void CheckDamagedRank(LF_TestContext ctx)
	{
		LF_OrganizationManager.Reset();
		LF_OrganizationRecord record = LF_OrganizationRecord.Create("org-rights-damaged", "Damaged Outfit", 0, false, false, false);
		record.m_aRanks.Insert(LF_OrganizationRankRecord.Create(LF_OrganizationConfig.RANK_FOUNDER, "Founder", true, true, true, true));
		record.m_aMembers.Insert(LF_OrganizationMemberRecord.Create(FOUNDER, LF_OrganizationConfig.RANK_FOUNDER));
		record.m_aMembers.Insert(LF_OrganizationMemberRecord.Create(MEMBER, "ghost"));
		array<ref LF_OrganizationRecord> records = {};
		records.Insert(record);
		LF_OrganizationManager.ApplyAll(records);
		LF_Organization org = LF_OrganizationManager.Get("org-rights-damaged");
		ctx.NotNull(org, "the damaged organization loads");
		if (!org)
			return;

		ctx.True(org.IsMember(MEMBER), "the member on a missing rank is kept");
		ctx.Equal(0, org.RightsOf(MEMBER), "a missing rank grants nothing");
		ctx.Equal(-1, org.RankPosition(MEMBER), "a missing rank has no rung");
		ctx.True(org.Outranks(FOUNDER, MEMBER), "every rung outranks a member with no rung");
		ctx.False(org.Outranks(MEMBER, FOUNDER), "a member with no rung outranks nobody");
		ctx.False(org.Outranks(FOUNDER, OUTSIDER), "nobody outranks a non-member");
		LF_OrgView founder = ViewOf(org, FOUNDER);
		LF_OrgView ghost = ViewOf(org, MEMBER);
		ctx.Equal(LF_EOrgRankMode.NONE, LF_OrgRules.RankModeFor(founder, ghost, false), "nobody is offered a rank change on a member with no rung");
		ctx.False(LF_OrganizationManager.Promote(org.GetId(), FOUNDER, MEMBER), "nobody promotes a member with no rung");
		ctx.True(LF_OrganizationManager.Kick(org.GetId(), FOUNDER, MEMBER), "the founder removes a member with no rung");
	}

	//------------------------------------------------------------------------------------------------
	//! The entries a client is offered agree with what the server allows.
	protected void CheckOfferSide(LF_TestContext ctx, LF_Organization org)
	{
		LF_OrgView founder = ViewOf(org, FOUNDER);
		LF_OrgView officer = ViewOf(org, OFFICER);
		LF_OrgView member = ViewOf(org, MEMBER);
		LF_OrgView stranger = LF_OrgView.Create("", "", 0, "", 0);

		ctx.Equal(LF_EOrgRankMode.PROMOTE, LF_OrgRules.RankModeFor(founder, member, true), "the founder facing a member is offered Promote");
		ctx.Equal(LF_EOrgRankMode.NONE, LF_OrgRules.RankModeFor(founder, member, false), "the founder facing a member is offered no Demote");
		ctx.Equal(LF_EOrgRankMode.NONE, LF_OrgRules.RankModeFor(founder, officer, true), "the founder facing an officer is offered no Promote");
		ctx.Equal(LF_EOrgRankMode.DEMOTE, LF_OrgRules.RankModeFor(founder, officer, false), "the founder facing an officer is offered Demote");
		ctx.Equal(LF_EOrgRankMode.NONE, LF_OrgRules.RankModeFor(officer, member, true), "an officer, even with manage-ranks, is offered no Promote to their rung");
		ctx.Equal(LF_EOrgRankMode.NONE, LF_OrgRules.RankModeFor(member, officer, false), "a member is offered nothing on an officer");
		ctx.Equal(LF_EOrgRankMode.NONE, LF_OrgRules.RankModeFor(founder, stranger, true), "nobody is offered a rank change on a stranger");
		ctx.Equal(LF_EOrgRankMode.NONE, LF_OrgRules.RankModeFor(null, member, true), "a missing view is offered nothing");

		ctx.Equal(LF_EOrgRoleMode.REMOVE, LF_OrgRules.ModeFor(officer, member, 9), "an officer facing a member is offered Remove");
		ctx.Equal(LF_EOrgRoleMode.NONE, LF_OrgRules.ModeFor(member, officer, 9), "a member is offered no Remove on an officer");

		LF_PlayerInteraction.UnregisterRole(LF_OrgRankRole.SLOT_PROMOTE);
		LF_PlayerInteraction.UnregisterRole(LF_OrgRankRole.SLOT_DEMOTE);
		LF_OrgInviteRole.Register();
		LF_OrgRankRole promote = LF_OrgRankRole.Cast(LF_PlayerInteraction.GetRole(LF_OrgRankRole.SLOT_PROMOTE));
		LF_OrgRankRole demote = LF_OrgRankRole.Cast(LF_PlayerInteraction.GetRole(LF_OrgRankRole.SLOT_DEMOTE));
		ctx.True(promote != null && promote.IsPromote(), "org_promote is filled by a promoting LF_OrgRankRole");
		ctx.True(demote != null && !demote.IsPromote(), "org_demote is filled by a demoting LF_OrgRankRole");
		LF_OrgInviteRole.Register();
		ctx.True(LF_PlayerInteraction.GetRole(LF_OrgRankRole.SLOT_PROMOTE) == promote, "a second Register keeps the same promote handler");
		if (promote)
			ctx.False(promote.CanOffer(null, null), "no pair, no entry");
	}

	//------------------------------------------------------------------------------------------------
	//! The view LF_OrgReplication.ServerView builds, without an entity.
	protected LF_OrgView ViewOf(LF_Organization org, string characterId)
	{
		return LF_OrgView.Create(org.GetId(), org.GetName(), LF_OrgRules.RightsOf(org, characterId), "", 0).SetLadder(org.RankPosition(characterId), org.GetRankCount());
	}
}
