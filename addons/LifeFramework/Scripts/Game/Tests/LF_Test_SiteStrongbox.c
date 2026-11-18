// red-proof: Observed 2026-11-18 with `tools\cli test --no-build --red LF_Test_SiteStrongbox`: `RED PROOF OK`, the take skipped failed `the bank reads 0 (expected 0, got 1200)`, `an empty bank is not saved (expected 0, got 1)` and `a second take finds nothing (expected 3, got 0)`. The same file passed with the take on (`--only`, 3 of 3 with LF_Test_SiteRent).
// tier: WORLD
//! sites/site-strongbox (#2684): a member of the organisation that holds a site, whose rank has
//! WITHDRAW, takes the site's banked rent as MoneyStack in hand and the bank reads 0. A member
//! without the right, a non-member and another organisation's founder are refused with the bank
//! and their cash unchanged; a second take finds nothing. The strongbox prefab carries the
//! component and the action.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_SiteStrongbox : LF_Test
{
	protected static const string SITE = "site-strongbox-test";
	protected static const string FOUNDER = "site-strongbox-founder";
	protected static const string MEMBER = "site-strongbox-member";
	protected static const string OUTSIDER = "site-strongbox-outsider";
	protected static const string RIVAL = "site-strongbox-rival";
	protected static const ResourceName PREFAB = "{28F82DEA95808829}Prefabs/Sites/LF_SiteStrongbox.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "sites/site-strongbox";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_TestLegacySnapshot kept = LF_TestLegacySnapshot.Take();
		LF_OrganizationManager.Reset();
		LF_SiteRent.ResetStatics();
		LF_Hideout.Release(SITE, "test");

		LF_Organization holders = LF_OrganizationManager.Create(FOUNDER, "Strongbox Crew");
		LF_Organization rivals = LF_OrganizationManager.Create(RIVAL, "Strongbox Rivals");
		ctx.NotNull(holders, "the holding organisation is founded");
		ctx.NotNull(rivals, "the rival organisation is founded");
		if (!holders || !rivals)
		{
			LF_OrganizationManager.Reset();
			kept.Restore();
			return;
		}

		ctx.True(LF_OrganizationManager.Invite(holders.GetId(), FOUNDER, MEMBER), "a plain member joins the holders");
		ctx.False(holders.Allows(MEMBER, LF_EOrganizationPermission.WITHDRAW), "the member's rank has no WITHDRAW");
		ctx.True(holders.Allows(FOUNDER, LF_EOrganizationPermission.WITHDRAW), "the founder's rank has WITHDRAW");
		ctx.True(LF_Hideout.HandOver(SITE, holders.GetId(), "captured"), "the holders capture the site");

		// Two hours of 600/h banked while held.
		int start = 1800000000;
		for (int t = 0; t < 7200; t += 5)
		{
			LF_SiteRent.Accrue(SITE, holders.GetId(), LF_ESiteHoldOutcome.GAIN, 5, start + t + 5);
		}

		ctx.Equal(1200, LF_SiteRent.BankedOf(SITE), "the site banked two hours of rent");

		IEntity body = LF_TestCharacterFixture.Spawn(ctx);
		if (body)
		{
			Run(ctx, body, holders.GetId());
			LF_TestCharacterFixture.Delete(body);
		}

		LF_Hideout.Release(SITE, "test");
		LF_SiteRent.ResetStatics();
		LF_OrganizationManager.Reset();
		kept.Restore();

		CheckPrefab(ctx);
	}

	//------------------------------------------------------------------------------------------------
	protected void Run(LF_TestContext ctx, IEntity body, string holdersId)
	{
		int cash0 = LF_MoneyUtils.GetCash(body);

		ctx.Equal(LF_ESiteStrongboxResult.NOT_HOLDER, LF_SiteStrongbox.Take(body, OUTSIDER, SITE), "a non-member is refused");
		ctx.Equal(LF_ESiteStrongboxResult.NOT_HOLDER, LF_SiteStrongbox.Take(body, RIVAL, SITE), "another organisation's founder is refused");
		ctx.Equal(LF_ESiteStrongboxResult.NO_RIGHT, LF_SiteStrongbox.Take(body, MEMBER, SITE), "a member without WITHDRAW is refused");
		ctx.Equal(LF_ESiteStrongboxResult.NOT_HOLDER, LF_SiteStrongbox.Take(body, FOUNDER, "site-strongbox-unheld"), "a site nobody holds is refused");
		ctx.Equal(1200, LF_SiteRent.BankedOf(SITE), "the bank is unchanged after every refusal");
		ctx.Equal(cash0, LF_MoneyUtils.GetCash(body), "no cash moved after every refusal");

		LF_ESiteStrongboxResult taken = LF_ESiteStrongboxResult.EMPTY;
		if (!RedProof())
			taken = LF_SiteStrongbox.Take(body, FOUNDER, SITE);

		ctx.Equal(LF_ESiteStrongboxResult.TAKEN, taken, "a holder with WITHDRAW takes the rent");
		ctx.Equal(cash0 + 1200, LF_MoneyUtils.GetCash(body), "the holder holds 1200 more cash");
		ctx.Equal(0, LF_SiteRent.BankedOf(SITE), "the bank reads 0");
		ctx.Equal(0, LF_SiteRent.ExportAll().Count(), "an empty bank is not saved");

		ctx.Equal(LF_ESiteStrongboxResult.EMPTY, LF_SiteStrongbox.Take(body, FOUNDER, SITE), "a second take finds nothing");
		ctx.Equal(cash0 + 1200, LF_MoneyUtils.GetCash(body), "a second take pays nothing");
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckPrefab(LF_TestContext ctx)
	{
		Resource res = Resource.Load(PREFAB);
		ctx.True(res != null && res.IsValid(), "LF_SiteStrongbox.et loads");
		if (!res || !res.IsValid())
			return;

		IEntity box = LF_Utils.SpawnEntityPrefab(PREFAB, "0 0 0");
		ctx.NotNull(box, "the strongbox spawns");
		if (!box)
			return;

		LF_SiteStrongboxComponent component = LF_Component<LF_SiteStrongboxComponent>.Find(box);
		ctx.NotNull(component, "the strongbox carries LF_SiteStrongboxComponent");
		if (component)
		{
			component.BindHideout(SITE);
			ctx.EqualStr(SITE, component.GetHideoutId(), "the component names the hideout it banks for");
		}

		bool action;
		ActionsManagerComponent actions = LF_Component<ActionsManagerComponent>.Find(box);
		ctx.NotNull(actions, "the strongbox carries an ActionsManagerComponent");
		if (actions)
		{
			array<BaseUserAction> list = {};
			actions.GetActionsList(list);
			foreach (BaseUserAction candidate : list)
			{
				if (LF_SiteStrongboxAction.Cast(candidate))
					action = true;
			}
		}

		ctx.True(action, "the strongbox offers the take-the-rent action");
		SCR_EntityHelper.DeleteEntityAndChildren(box);
	}
}
