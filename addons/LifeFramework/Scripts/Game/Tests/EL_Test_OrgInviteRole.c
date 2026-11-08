// red-proof: Observed 2026-11-08 with `tools\cli test --no-build --only EL_Test_OrgRegistry,EL_Test_OrgInviteRole`:
// dropping the inviter check (`user.m_iOfferFromPlayerId == targetPlayerId`) from LF_OrgRules.ModeFor
// failed `[ELTEST] FAIL org/invite-role`, `- the invitee facing someone else is offered nothing (expected 0, got 3)`.
// Restored; green.
// tier: LOGIC
//! M1-17 offer side: which `org_invite` entry a pair sees (invite only with the rank, join only
//! from the one who offered, remove never the founder), the slot registration, and the ledger page.
class EL_Test_OrgInviteRole : EL_Test
{
	override string GetName()
	{
		return "org/invite-role";
	}

	override void Run(EL_TestContext ctx)
	{
		int leaderRights = LF_OrgRules.RIGHT_INVITE | LF_OrgRules.RIGHT_KICK | LF_OrgRules.RIGHT_WITHDRAW | LF_OrgRules.RIGHT_BUY | LF_OrgRules.RIGHT_FOUNDER;
		LF_OrgView leader = LF_OrgView.Create("org-a", "North Dock", leaderRights, "", 0);
		LF_OrgView member = LF_OrgView.Create("org-a", "North Dock", 0, "", 0);
		LF_OrgView stranger = LF_OrgView.Create("", "", 0, "", 0);
		LF_OrgView invited = LF_OrgView.Create("", "", 0, "North Dock", 7);
		LF_OrgView rival = LF_OrgView.Create("org-b", "South Dock", leaderRights, "", 0);

		ctx.Equal(LF_EOrgRoleMode.INVITE, LF_OrgRules.ModeFor(leader, stranger, 9), "a leader facing a stranger is offered Invite");
		ctx.Equal(LF_EOrgRoleMode.NONE, LF_OrgRules.ModeFor(member, stranger, 9), "a member without the rank is offered nothing");
		ctx.Equal(LF_EOrgRoleMode.REMOVE, LF_OrgRules.ModeFor(leader, member, 9), "a leader facing their member is offered Remove");
		ctx.Equal(LF_EOrgRoleMode.NONE, LF_OrgRules.ModeFor(member, leader, 7), "nobody is offered Remove on the founder");
		ctx.Equal(LF_EOrgRoleMode.NONE, LF_OrgRules.ModeFor(leader, rival, 9), "a leader facing another organization is offered nothing");
		ctx.Equal(LF_EOrgRoleMode.JOIN, LF_OrgRules.ModeFor(invited, leader, 7), "the invitee facing the one who offered is offered Join");
		ctx.Equal(LF_EOrgRoleMode.NONE, LF_OrgRules.ModeFor(invited, leader, 8), "the invitee facing someone else is offered nothing");
		ctx.Equal(LF_EOrgRoleMode.NONE, LF_OrgRules.ModeFor(stranger, leader, 7), "a stranger with no offer is offered nothing");
		ctx.Equal(LF_EOrgRoleMode.NONE, LF_OrgRules.ModeFor(null, leader, 7), "a missing view is offered nothing, not crashed on");

		// The slot: filled by Register, idempotent, and named for the pair.
		LF_PlayerInteraction.UnregisterRole(LF_OrgInviteRole.SLOT);
		LF_OrgInviteRole.Register();
		LF_PlayerRoleHandler handler = LF_PlayerInteraction.GetRole(LF_OrgInviteRole.SLOT);
		ctx.True(LF_OrgInviteRole.Cast(handler) != null, "org_invite is filled by LF_OrgInviteRole");
		LF_OrgInviteRole.Register();
		ctx.True(LF_PlayerInteraction.GetRole(LF_OrgInviteRole.SLOT) == handler, "a second Register keeps the same handler");
		if (handler)
		{
			ctx.False(handler.CanOffer(null, null), "no pair, no entry");
			ctx.EqualStr("", handler.GetActionNameFor(null, null), "no pair, no name");
		}

		// The ledger page: the founding form without an organization, the register with one.
		array<ref LF_UIPart> found = LF_OrgLedger.BuildParts(LF_OrgLedgerPage.Create(false, "", 0, 0, false, LF_EOrgLease.FREE, ""));
		ctx.True(HasPart(found, LF_OrgLedger.PART_NAME), "the founding page has the name field");
		ctx.True(HasPart(found, LF_OrgLedger.PART_FOUND), "the founding page has Sign and pay");
		ctx.False(HasPart(found, LF_OrgLedger.PART_MEMBERS), "the founding page lists no members");

		LF_OrgLedgerPage founderPage = LF_OrgLedgerPage.Create(true, "North Dock", 300, leaderRights, false, LF_EOrgLease.FREE, "Ada Tester|#LF-Org_RankFounder;Bea Tester|#LF-Org_RankMember");
		array<ref LF_UIPart> page = LF_OrgLedger.BuildParts(founderPage);
		ctx.False(HasPart(page, LF_OrgLedger.PART_FOUND), "a member's page has no founding button");
		ctx.True(HasPart(page, LF_OrgLedger.PART_MEMBERS), "a member's page lists the members");
		ctx.True(HasPart(page, LF_OrgLedger.PART_WITHDRAW), "the founder may withdraw");
		ctx.True(HasPart(page, LF_OrgLedger.PART_INVITE_HINT), "the founder is told invites are face to face");
		ctx.EqualStr("", LF_OrgLedger.MembershipAction(founderPage), "the founder has no Leave");
		ctx.EqualStr("#LF-Org_TakeLease", LF_OrgLedger.HideoutAction(founderPage), "the founder may take the free yard");

		LF_OrgLedgerPage memberPage = LF_OrgLedgerPage.Create(true, "North Dock", 300, 0, false, LF_EOrgLease.OURS, "");
		array<ref LF_UIPart> memberParts = LF_OrgLedger.BuildParts(memberPage);
		ctx.False(HasPart(memberParts, LF_OrgLedger.PART_WITHDRAW), "a member may not withdraw");
		ctx.EqualStr("#LF-Org_Leave", LF_OrgLedger.MembershipAction(memberPage), "a member may leave");
		ctx.EqualStr("", LF_OrgLedger.HideoutAction(memberPage), "a member cannot take a lease");
		ctx.EqualStr("", LF_OrgLedger.ProcessingAction(memberPage), "a member cannot buy the upgrade");
		ctx.False(founderPage.ShapeKey() == memberPage.ShapeKey(), "a rank change rebuilds the page");

		array<string> names = {};
		array<string> ranks = {};
		founderPage.SplitMembers(names, ranks);
		ctx.Equal(2, names.Count(), "two members are listed");
		if (names.Count() == 2)
		{
			ctx.EqualStr("Bea Tester", names[1], "the second row is the member");
			ctx.EqualStr("#LF-Org_RankFounder", ranks[0], "the founder row reads Founder");
		}
	}

	protected bool HasPart(array<ref LF_UIPart> parts, string name)
	{
		foreach (LF_UIPart part : parts)
		{
			if (part && part.GetName() == name)
				return true;
		}

		return false;
	}
}
