// red-proof: 2026-11-23 `tools\cli test --no-build --red LF_Test_OrgInviteActions` (RedProof hook skips the Decline call): RED PROOF OK, "a declined offer is gone", "declining leaves the recruit with no crew", "the founder invites the recruit again (expected 0, got 5)". Green with Decline.
//! GA-C5 (#4214) server rules for recruiting from the action list: invite, join, decline and
//! remove. Decline follows Join (same pair), drops only the offer of the one faced, and a
//! declined offer cannot be joined; a leader removes a member, never the founder.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_OrgInviteActions : LF_Test
{
	protected static const string FOUNDER = "lf-test-c5-founder";
	protected static const string RECRUIT = "lf-test-c5-recruit";
	protected static const string OTHER = "lf-test-c5-other";

	override string GetName()
	{
		return "org/invite-actions";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_OrganizationManager.Reset();
		LF_OrgInvites.Reset();
		float now = 1000;

		// The Decline entry is offered exactly when the invite entry reads Join.
		ctx.True(LF_OrgDeclineRole.OffersDecline(LF_EOrgRoleMode.JOIN), "Decline sits beside Join");
		ctx.False(LF_OrgDeclineRole.OffersDecline(LF_EOrgRoleMode.INVITE), "the inviter sees no Decline");
		ctx.False(LF_OrgDeclineRole.OffersDecline(LF_EOrgRoleMode.REMOVE), "a leader facing a member sees no Decline");
		ctx.False(LF_OrgDeclineRole.OffersDecline(LF_EOrgRoleMode.NONE), "no offer, no Decline");

		// The slot: filled by Register, idempotent, no entry without a pair.
		LF_PlayerInteraction.UnregisterRole(LF_OrgDeclineRole.SLOT);
		LF_OrgDeclineRole.Register();
		LF_PlayerRoleHandler handler = LF_PlayerInteraction.GetRole(LF_OrgDeclineRole.SLOT);
		ctx.True(LF_OrgDeclineRole.Cast(handler) != null, "org_decline is filled by LF_OrgDeclineRole");
		LF_OrgDeclineRole.Register();
		ctx.True(LF_PlayerInteraction.GetRole(LF_OrgDeclineRole.SLOT) == handler, "a second Register keeps the same handler");
		if (handler)
		{
			ctx.False(handler.CanOffer(null, null), "no pair, no Decline");
			ctx.EqualStr("", handler.GetActionNameFor(null, null), "no pair, no name");
		}

		LF_Organization org = LF_OrganizationManager.Create(FOUNDER, "Decline Test Crew");
		ctx.True(org != null, "the founder registers a crew");
		if (!org)
			return;

		// Invite, then decline: the offer is gone and cannot be joined.
		ctx.Equal(LF_EOrgOfferResult.OFFERED, LF_OrgInvites.Offer(FOUNDER, 1, RECRUIT, now), "the founder invites the recruit");
		ctx.False(LF_OrgInvites.Decline(RECRUIT, OTHER, now), "declining by facing someone else is refused");
		ctx.True(LF_OrgInvites.Pending(RECRUIT, now) != null, "a refused decline keeps the offer");
		if (!RedProof())
			ctx.True(LF_OrgInvites.Decline(RECRUIT, FOUNDER, now + 100), "facing the founder and choosing Decline turns it down");

		ctx.True(LF_OrgInvites.Pending(RECRUIT, now + 100) == null, "a declined offer is gone");
		ctx.Equal(LF_EOrgOfferResult.REFUSED_NO_OFFER, LF_OrgInvites.Accept(RECRUIT, FOUNDER, now + 200), "a declined offer cannot be joined");
		ctx.False(LF_OrgInvites.Decline(RECRUIT, FOUNDER, now + 200), "an offer is declined once");
		ctx.True(LF_OrganizationManager.GetByCharacter(RECRUIT) == null, "declining leaves the recruit with no crew");

		// A lapsed offer cannot be declined either.
		ctx.Equal(LF_EOrgOfferResult.OFFERED, LF_OrgInvites.Offer(FOUNDER, 1, OTHER, now), "the founder invites someone else");
		ctx.False(LF_OrgInvites.Decline(OTHER, FOUNDER, now + LF_OrgConfig.OfferMs() + 1), "a lapsed offer has nothing to decline");

		// Invite again, join, then remove.
		ctx.Equal(LF_EOrgOfferResult.OFFERED, LF_OrgInvites.Offer(FOUNDER, 1, RECRUIT, now + 300), "the founder invites the recruit again");
		ctx.Equal(LF_EOrgOfferResult.JOINED, LF_OrgInvites.Accept(RECRUIT, FOUNDER, now + 400), "the recruit joins this time");
		ctx.True(LF_OrganizationManager.GetByCharacter(RECRUIT) == org, "the recruit is a member");
		ctx.False(LF_OrganizationManager.Kick(org.GetId(), RECRUIT, FOUNDER), "a member cannot remove the founder");
		ctx.True(LF_OrganizationManager.Kick(org.GetId(), FOUNDER, RECRUIT), "the founder removes the member");
		ctx.True(LF_OrganizationManager.GetByCharacter(RECRUIT) == null, "the removed member has no crew");

		LF_OrgInvites.Reset();
		LF_OrganizationManager.Reset();
		LF_PlayerInteraction.UnregisterRole(LF_OrgDeclineRole.SLOT);
		LF_OrgDeclineRole.Register();
	}
}
