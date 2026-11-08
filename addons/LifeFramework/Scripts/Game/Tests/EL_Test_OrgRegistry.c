// red-proof: Observed 2026-11-08 with `tools\cli test --no-build --only EL_Test_OrgRegistry,EL_Test_OrgInviteRole`:
// making EL_OrganizationManager.IsNameTaken never match failed `[ELTEST] FAIL org/registry`,
// `- the name is taken whatever the case`, `- a duplicate name is refused`. Restored; green.
// tier: LOGIC
//! M1-17 registry rules: names, duplicates, face-to-face offers (rank, accept, wrong inviter,
//! lapse, a leader who lost the rank), founder cannot be removed, leave, and the hideout lease.
class EL_Test_OrgRegistry : EL_Test
{
	override string GetName()
	{
		return "org/registry";
	}

	override void Run(EL_TestContext ctx)
	{
		EL_OrganizationManager.Reset();
		LF_OrgInvites.Reset();
		LF_Hideout.Reset();

		// Names.
		ctx.EqualStr("", LF_OrgRules.RefuseName("North Dock"), "a plain name is accepted");
		ctx.EqualStr("", LF_OrgRules.RefuseName("  O'Brien & Sons.  "), "apostrophe, ampersand, period and outer spaces are accepted");
		ctx.EqualStr("#LF-Org_NameEmpty", LF_OrgRules.RefuseName("   "), "a blank name is refused");
		ctx.EqualStr("#LF-Org_NameShort", LF_OrgRules.RefuseName("AB"), "two letters are refused");
		ctx.EqualStr("#LF-Org_NameLong", LF_OrgRules.RefuseName("ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFG"), "33 letters are refused");
		ctx.EqualStr("#LF-Org_NameChars", LF_OrgRules.RefuseName("Red|Hand"), "a separator character is refused");
		ctx.EqualStr("#LF-Org_NameChars", LF_OrgRules.RefuseName("Red  Hand"), "a double space is refused");
		ctx.True(LF_OrgRules.FOUNDING_FEE > 0, "founding costs a fee");

		string founder = "org-reg-founder";
		string member = "org-reg-member";
		string outsider = "org-reg-outsider";
		string rival = "org-reg-rival";

		EL_Organization org = EL_OrganizationManager.Create(founder, "  North Dock ");
		ctx.NotNull(org, "the founder registers North Dock");
		if (!org)
			return;

		ctx.EqualStr("North Dock", org.GetName(), "the name is stored trimmed");
		ctx.True(EL_OrganizationManager.IsNameTaken("north dock"), "the name is taken whatever the case");
		ctx.True(EL_OrganizationManager.Create(rival, "NORTH DOCK") == null, "a duplicate name is refused");
		ctx.True(EL_OrganizationManager.Create(rival, "X") == null, "a bad name is refused by the manager too");
		ctx.Equal(LF_OrgRules.RIGHT_INVITE | LF_OrgRules.RIGHT_KICK | LF_OrgRules.RIGHT_WITHDRAW | LF_OrgRules.RIGHT_BUY | LF_OrgRules.RIGHT_FOUNDER, LF_OrgRules.RightsOf(org, founder), "the founder holds every right");
		ctx.Equal(0, LF_OrgRules.RightsOf(org, outsider), "an outsider holds none");

		// Offers: only a rank that may invite, never to a member, never to yourself.
		float now = 1000;
		ctx.Equal(LF_EOrgOfferResult.REFUSED_NO_ORG, LF_OrgInvites.Offer(outsider, 3, member, now), "someone with no organization cannot offer");
		ctx.Equal(LF_EOrgOfferResult.REFUSED_SELF, LF_OrgInvites.Offer(founder, 1, founder, now), "the founder cannot offer themselves");
		ctx.Equal(LF_EOrgOfferResult.OFFERED, LF_OrgInvites.Offer(founder, 1, member, now), "the founder offers a place");
		ctx.True(LF_OrgInvites.Pending(member, now) != null, "the invitee holds the offer");
		ctx.False(org.IsMember(member), "an offer alone does not make a member");

		ctx.Equal(LF_EOrgOfferResult.REFUSED_WRONG_INVITER, LF_OrgInvites.Accept(member, outsider, now), "joining by facing someone else is refused");
		ctx.Equal(LF_EOrgOfferResult.JOINED, LF_OrgInvites.Accept(member, founder, now + 500), "facing the founder and choosing Join joins");
		ctx.True(org.IsMember(member), "the invitee is a member");
		ctx.Equal(0, LF_OrgRules.RightsOf(org, member) & LF_OrgRules.RIGHT_INVITE, "a member cannot invite");
		ctx.Equal(LF_EOrgOfferResult.REFUSED_NO_OFFER, LF_OrgInvites.Accept(member, founder, now + 600), "an offer is taken once");

		ctx.Equal(LF_EOrgOfferResult.REFUSED_RANK, LF_OrgInvites.Offer(member, 2, outsider, now), "a member rank cannot offer");
		ctx.Equal(LF_EOrgOfferResult.REFUSED_ALREADY_MEMBER, LF_OrgInvites.Offer(founder, 1, member, now), "a member cannot be offered again");

		ctx.Equal(LF_EOrgOfferResult.OFFERED, LF_OrgInvites.Offer(founder, 1, outsider, now), "the founder offers the outsider");
		ctx.Equal(LF_EOrgOfferResult.REFUSED_NO_OFFER, LF_OrgInvites.Accept(outsider, founder, now + LF_OrgRules.OFFER_MS + 1), "a lapsed offer cannot be taken");
		ctx.False(org.IsMember(outsider), "the lapsed offer did not join");

		// Removal: the founder removes a member; a member cannot remove; nobody removes the founder.
		ctx.False(EL_OrganizationManager.Kick(org.GetId(), member, founder), "a member cannot remove the founder");
		ctx.False(EL_OrganizationManager.Kick(org.GetId(), founder, founder), "the founder cannot be removed");
		ctx.True(EL_OrganizationManager.Kick(org.GetId(), founder, member), "the founder removes the member");
		ctx.False(org.IsMember(member), "the removed member is gone");

		// Leave: a member leaves, the founder stays.
		ctx.Equal(LF_EOrgOfferResult.OFFERED, LF_OrgInvites.Offer(founder, 1, member, now), "the founder asks again");
		ctx.Equal(LF_EOrgOfferResult.JOINED, LF_OrgInvites.Accept(member, founder, now), "the member rejoins");
		ctx.True(EL_OrganizationManager.Leave(member), "a member leaves at the desk");
		ctx.False(EL_OrganizationManager.Leave(founder), "the founder cannot leave");

		// Hideout lease: the founder takes the free yard; a rival cannot; a member cannot.
		EL_Organization other = EL_OrganizationManager.Create(rival, "South Dock");
		ctx.NotNull(other, "a rival registers South Dock");
		ctx.Equal(LF_EOrgLease.FREE, LF_OrgHideoutLease.StateFor(org.GetId(), LF_Hideout.HIDEOUT_ID), "the yard starts free");
		ctx.Equal(LF_EOrgOfferResult.OFFERED, LF_OrgInvites.Offer(founder, 1, member, now), "the founder asks the member back");
		ctx.Equal(LF_EOrgOfferResult.JOINED, LF_OrgInvites.Accept(member, founder, now), "the member is back");
		ctx.False(LF_OrgHideoutLease.Lease(member, LF_Hideout.HIDEOUT_ID), "a member cannot take the lease");
		ctx.True(LF_OrgHideoutLease.Lease(founder, LF_Hideout.HIDEOUT_ID), "the founder takes the lease");
		ctx.Equal(LF_EOrgLease.OURS, LF_OrgHideoutLease.StateFor(org.GetId(), LF_Hideout.HIDEOUT_ID), "the yard is ours");
		ctx.True(LF_Hideout.IsLocked(LF_Hideout.HIDEOUT_ID), "a new lease shuts the gate");
		if (other)
		{
			ctx.Equal(LF_EOrgLease.TAKEN, LF_OrgHideoutLease.StateFor(other.GetId(), LF_Hideout.HIDEOUT_ID), "the rival sees it taken");
			ctx.False(LF_OrgHideoutLease.Lease(rival, LF_Hideout.HIDEOUT_ID), "the rival cannot take it");
		}

		ctx.True(LF_Hideout.AllowsUnlock(member, LF_Hideout.HIDEOUT_ID, false), "the invited member opens the hideout gate");
		ctx.False(LF_Hideout.AllowsUnlock(rival, LF_Hideout.HIDEOUT_ID, false), "the rival does not");
		ctx.False(LF_Hideout.AllowsUnlock(outsider, LF_Hideout.HIDEOUT_ID, false), "an outsider does not");

		LF_OrgInvites.Reset();
		LF_Hideout.Reset();
		EL_OrganizationManager.Reset();
	}
}
