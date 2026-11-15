// red-proof: observed 2026-11-14 with `tools\cli test --no-build --only LF_Test_HeldGroundBadRow,...`:
// LF_Hideout.SetHeldGround skipping the lease check (`if (!state)`) failed `[LFTEST] FAIL hideouts/held-ground-bad-row`,
// `- a holder that does not lease the site, or is gone, holds nothing (apply 1) (expected '', got '<org id>')`. Restored; green.
// tier: LOGIC
//! #751: LF_Hideout.ApplyHeldGround corrects what a damaged "heldground" row holds. A row whose
//! organization is gone, whose organization does not lease the site, whose site has no lease, or
//! with no version is dropped; points over the owner cap are clamped to it, a negative hold or
//! points read as 0; the other rows still apply. A second apply gives the same sites, an empty apply
//! leaves every site unheld, and a released lease loses its held ground. The site ids are not in the
//! world: a lease outlives a missing site, so its held ground does too.
class LF_Test_HeldGroundBadRow : LF_Test
{
	protected static const string GOOD_SITE = "held-ground-bad-good";
	protected static const string CAPPED_SITE = "held-ground-bad-capped";
	protected static const string RIVAL_SITE = "held-ground-bad-rival";
	protected static const string FREE_SITE = "held-ground-bad-free";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "hideouts/held-ground-bad-row";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_TestLegacySnapshot kept = LF_TestLegacySnapshot.Take();
		RunSeeded(ctx);
		ReleaseIfLeased(GOOD_SITE);
		ReleaseIfLeased(CAPPED_SITE);
		ReleaseIfLeased(RIVAL_SITE);
		kept.Restore();
	}

	//------------------------------------------------------------------------------------------------
	protected void ReleaseIfLeased(string site)
	{
		if (!LF_Hideout.GetOwner(site).IsEmpty())
			LF_Hideout.Release(site);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunSeeded(LF_TestContext ctx)
	{
		LF_Organization holder = LF_OrganizationManager.Create("held-ground-bad-founder", "Bad Row Holders");
		LF_Organization second = LF_OrganizationManager.Create("held-ground-bad-second", "Bad Row Seconds");
		LF_Organization rival = LF_OrganizationManager.Create("held-ground-bad-rival", "Bad Row Rivals");
		ctx.NotNull(holder, "the holding organization is founded");
		ctx.NotNull(second, "the second organization is founded");
		ctx.NotNull(rival, "the rival organization is founded");
		if (!holder || !second || !rival)
			return;

		ctx.True(LF_Hideout.Assign(GOOD_SITE, holder.GetId()), "the holder leases its site");
		ctx.True(LF_Hideout.Assign(CAPPED_SITE, second.GetId()), "the second organization leases its site");
		ctx.True(LF_Hideout.Assign(RIVAL_SITE, rival.GetId()), "the rival leases its site");

		int cap = LF_OrgConfig.HeldGroundMaxPoints();
		array<ref LF_HeldGroundRow> rows = {};
		rows.Insert(LF_HeldGroundRow.Create(GOOD_SITE, holder.GetId(), 3600, 12));
		rows.Insert(LF_HeldGroundRow.Create(CAPPED_SITE, second.GetId(), -40, cap + 500));
		rows.Insert(LF_HeldGroundRow.Create(RIVAL_SITE, holder.GetId(), 600, 5));
		rows.Insert(LF_HeldGroundRow.Create(RIVAL_SITE, "held-ground-bad-gone-org", 600, 5));
		rows.Insert(LF_HeldGroundRow.Create(FREE_SITE, holder.GetId(), 600, 5));
		LF_HeldGroundRow unversioned = LF_HeldGroundRow.Create(GOOD_SITE, holder.GetId(), 1, 1);
		unversioned.m_iVersion = 0;
		rows.Insert(unversioned);

		for (int pass = 1; pass <= 2; pass++)
		{
			string tag = string.Format(" (apply %1)", pass);
			ctx.Equal(2, LF_Hideout.ApplyHeldGround(rows), "the good and the capped rows apply, the four bad rows are dropped" + tag);

			LF_HeldGroundRow good = LF_Hideout.HeldGroundOf(GOOD_SITE);
			ctx.NotNull(good, "the good site is known" + tag);
			if (good)
			{
				ctx.EqualStr(holder.GetId(), good.m_sHolderOrgId, "the good row's holder applies" + tag);
				ctx.Equal(3600, good.m_iHoldSeconds, "the good row's hold applies" + tag);
				ctx.Equal(12, good.m_iHoldPoints, "the good row's points apply, the unversioned row did not overwrite them" + tag);
			}

			LF_HeldGroundRow capped = LF_Hideout.HeldGroundOf(CAPPED_SITE);
			ctx.NotNull(capped, "the capped site is known" + tag);
			if (capped)
			{
				ctx.EqualStr(second.GetId(), capped.m_sHolderOrgId, "the capped row's holder applies" + tag);
				ctx.Equal(0, capped.m_iHoldSeconds, "a negative hold reads as 0" + tag);
				ctx.Equal(cap, capped.m_iHoldPoints, "points over the cap are clamped to it" + tag);
			}

			LF_HeldGroundRow rivalSite = LF_Hideout.HeldGroundOf(RIVAL_SITE);
			ctx.NotNull(rivalSite, "the rival's site is known" + tag);
			if (rivalSite)
				ctx.EqualStr("", rivalSite.m_sHolderOrgId, "a holder that does not lease the site, or is gone, holds nothing" + tag);

			ctx.True(LF_Hideout.HeldGroundOf(FREE_SITE) == null, "a row for a site with no lease makes no site" + tag);
			ctx.Equal(2, LF_Hideout.ExportHeldGround().Count(), "only the two held sites export rows" + tag);
		}

		ctx.True(LF_Hideout.SetHeldGround(GOOD_SITE, holder.GetId(), 100, -3), "a set by the leasing organization is accepted");
		LF_HeldGroundRow negative = LF_Hideout.HeldGroundOf(GOOD_SITE);
		if (negative)
			ctx.Equal(0, negative.m_iHoldPoints, "negative points read as 0");

		ctx.False(LF_Hideout.SetHeldGround(GOOD_SITE, rival.GetId(), 100, 3), "a set by an organization that does not lease the site is refused");

		array<ref LF_HeldGroundRow> none = {};
		ctx.Equal(0, LF_Hideout.ApplyHeldGround(none), "an empty apply applies nothing");
		LF_HeldGroundRow reset = LF_Hideout.HeldGroundOf(CAPPED_SITE);
		if (reset)
		{
			ctx.EqualStr("", reset.m_sHolderOrgId, "a site with no row is unheld");
			ctx.Equal(0, reset.m_iHoldPoints, "a site with no row has no points");
		}
		ctx.EqualStr(second.GetId(), LF_Hideout.GetOwner(CAPPED_SITE), "the lease is untouched by the held ground");

		ctx.True(LF_Hideout.SetHeldGround(CAPPED_SITE, second.GetId(), 60, 7), "the second organization holds its site again");
		ctx.True(LF_Hideout.Release(CAPPED_SITE), "the lease lapses");
		LF_HeldGroundRow lapsed = LF_Hideout.HeldGroundOf(CAPPED_SITE);
		if (lapsed)
		{
			ctx.EqualStr("", lapsed.m_sHolderOrgId, "a lapsed lease holds nothing");
			ctx.Equal(0, lapsed.m_iHoldPoints, "a lapsed lease loses its points");
		}
	}
}
