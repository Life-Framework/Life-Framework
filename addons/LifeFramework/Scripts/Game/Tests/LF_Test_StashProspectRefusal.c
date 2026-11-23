// red-proof: org/stash-prospect-refusal (#4546), observed 2026-11-23 with `tools/cli test --no-build --only
// LF_Test_StashProspectRefusal` while the two helpers still returned the old wording:
// `[LFTEST] FAIL org/stash-prospect-refusal`, `- a prospect at the stash is told the prospect words`,
// `- a prospect at the ledger withdrawal is told the prospect words`. Green once the helpers ask
// LF_OrgProspect.RefusalFor.

//------------------------------------------------------------------------------------------------
//! A gang prospect refused at the stash or the ledger withdrawal is told the prospect words, not "not a
//! member" or the clerk's generic refusal (#4546). Everyone else keeps the old words.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_StashProspectRefusal : LF_Test
{
	protected static const string FOUNDER = "stash-prospect-founder";
	protected static const string RECRUIT = "stash-prospect-recruit";
	protected static const string OUTSIDER = "stash-prospect-outsider";
	protected static const string NOT_MEMBER_KEY = "#LF-HideoutStash_NotMember";
	protected static const string NO_OWNER_KEY = "#LF-HideoutStash_NoOwner";
	protected static const string CASH_REFUSED_KEY = "#LF-Org_CashRefused";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "org/stash-prospect-refusal";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_TestLegacySnapshot kept = LF_TestLegacySnapshot.Take();
		LF_OrgProspect.SetClock(0);

		LF_OrganizationManager.Reset();
		LF_OrgInvites.Reset();
		LF_OrgProspect.ResetStatics();
		LF_Hideout.Reset();

		int start = System.GetUnixTime();
		LF_OrgProspect.SetClock(start);

		LF_Organization org = LF_OrganizationManager.Create(FOUNDER, "Stash Prospect Crew");
		ctx.NotNull(org, "the crew is founded");
		if (org)
		{
			int open = LF_OrgRights.Bit(LF_EOrganizationPermission.WITHDRAW) | LF_OrgRights.Bit(LF_EOrganizationPermission.USE_STASH);
			org.ApplyRankRow(LF_OrganizationRankRow.Create(org.GetId(), LF_OrganizationConfig.RANK_MEMBER, 2, open));
			org.SortLadder();
			org.Credit(500);

			string hideout = LF_TestAnnex.HIDEOUT_ID;
			ctx.EqualStr(NO_OWNER_KEY, LF_Hideout.StashRefusalKey(RECRUIT, hideout), "an unowned hideout still says there is no owner");
			ctx.True(LF_Hideout.Assign(hideout, org.GetId()), "the crew owns the hideout");

			float now = LF_OrgInvites.Now();
			ctx.Equal(LF_EOrgOfferResult.OFFERED, LF_OrgInvites.Offer(FOUNDER, 0, RECRUIT, now), "the founder offers the recruit a place");
			ctx.True(LF_OrgInvites.Accept(RECRUIT, FOUNDER, now) == LF_EOrgOfferResult.JOINED, "the recruit joins through the invite");
			// RedProof(): the prospect mark is wiped after the join, so the prospect words never apply (`--red`).
			if (RedProof())
				LF_OrgProspect.Clear(RECRUIT, "red proof");

			ctx.False(LF_Hideout.AllowsStash(RECRUIT, hideout, false), "a prospect cannot open the stash");
			ctx.EqualStr(LF_OrgProspect.REFUSED_KEY, LF_Hideout.StashRefusalKey(RECRUIT, hideout), "a prospect at the stash is told the prospect words");
			ctx.EqualStr(LF_OrgProspect.REFUSED_KEY, LF_OrganizationPlayer.WithdrawRefusalKey(RECRUIT), "a prospect at the ledger withdrawal is told the prospect words");

			ctx.EqualStr(NOT_MEMBER_KEY, LF_Hideout.StashRefusalKey(OUTSIDER, hideout), "a stranger at the stash still hears 'not a member'");
			ctx.EqualStr(CASH_REFUSED_KEY, LF_OrganizationPlayer.WithdrawRefusalKey(OUTSIDER), "someone with no gang at the ledger still gets the clerk's refusal");
			ctx.EqualStr(CASH_REFUSED_KEY, LF_OrganizationPlayer.WithdrawRefusalKey(FOUNDER), "a full member at the ledger still gets the clerk's refusal");

			LF_OrgProspect.SetClock(start + LF_OrgProspect.PERIOD_SECONDS);
			ctx.True(LF_Hideout.AllowsStash(RECRUIT, hideout, false), "after 48 hours the stash opens");
			ctx.EqualStr(CASH_REFUSED_KEY, LF_OrganizationPlayer.WithdrawRefusalKey(RECRUIT), "after 48 hours the ledger gives no prospect words");
		}

		LF_OrganizationManager.Reset();
		LF_OrgInvites.Reset();
		LF_OrgProspect.ResetStatics();
		LF_Hideout.Reset();
		if (kept)
			kept.Restore();
	}
}
