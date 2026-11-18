// red-proof: 2026-11-18 `test --no-build --red LF_Test_SiteSweep` printed RED PROOF OK: with the hook on (no capture sweeps) the held ground stays A's (40 points), the lease stays A's and no capture is published.
//! sites/site-sweep (#2680, gang war #1022): the server sweep scores the annex hideout's site with
//! LF_SiteHold.Step under LF_SitesConfig.For(id).ToHoldRules(). Presence comes through the sweep's
//! test seam (LF_SiteSighting rows), not spawned characters, so the activity filter and the rings
//! are the sweep's own. A second organisation present freezes the site; members standing still past
//! the active window are not counted; an organisation alone long enough takes the hideout: the old
//! lease is released, the capturer leases it, and the held ground moves to it at zero.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_SiteSweep : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "sites/site-sweep";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_TestLegacySnapshot kept = LF_TestLegacySnapshot.Take();
		RunWiped(ctx);
		kept.Restore();
	}

	//------------------------------------------------------------------------------------------------
	protected void Add(notnull array<ref LF_SiteSighting> rows, string characterId, string orgId, vector position, float distance)
	{
		rows.Insert(LF_SiteSighting.Create(characterId, orgId, position, distance));
	}

	//------------------------------------------------------------------------------------------------
	protected void RunWiped(LF_TestContext ctx)
	{
		LF_OrganizationManager.Reset();
		LF_Hideout.Reset();
		LF_ContestedSites.ResetStatics();

		string site = LF_TestAnnex.HIDEOUT_ID;
		LF_Organization orgA = LF_OrganizationManager.Create("sweep-founder-a", "Sweep Crew A");
		LF_Organization orgB = LF_OrganizationManager.Create("sweep-founder-b", "Sweep Crew B");
		ctx.NotNull(orgA, "organization A exists");
		ctx.NotNull(orgB, "organization B exists");
		if (!orgA || !orgB)
			return;

		string idA = orgA.GetId();
		string idB = orgB.GetId();
		ctx.True(LF_Hideout.Assign(site, idA), "A leases the hideout");
		ctx.True(LF_Hideout.SetHeldGround(site, idA, 120, 40), "A holds ground on it");

		LF_SiteSettings settings = LF_SitesConfig.For(site);
		int window = settings.m_iActiveWindowSeconds;
		int step = LF_ContestedSites.SWEEP_SECONDS;
		int now = 1800000000;
		vector here = "100 0 100";
		vector there = "102 0 100";

		// 1. Both organisations on the site, moving: frozen.
		array<ref LF_SiteSighting> both = {};
		Add(both, "b1", idB, here, 30);
		Add(both, "b2", idB, there, 30);
		Add(both, "a1", idA, here, 30);
		Add(both, "a2", idA, there, 30);
		LF_ESiteHoldOutcome outcome = LF_ContestedSites.SweepSite(site, both, now, step);
		ctx.Equal(LF_ESiteHoldOutcome.CONTESTED, outcome, "two organisations present freeze the site");
		ctx.EqualFloat(0, LF_ContestedSites.CaptureProgressOf(site), 0.001, "a contested site builds no progress");
		ctx.Equal(1, LF_ContestedSites.ContestStarts(), "contest start is published once");

		// 2. B stands still past the active window: not counted, nothing moves.
		now = now + window + 1;
		array<ref LF_SiteSighting> still = {};
		Add(still, "b1", idB, here, 30);
		Add(still, "b2", idB, there, 30);
		outcome = LF_ContestedSites.SweepSite(site, still, now, step);
		ctx.Equal(0, LF_ContestedSites.CountedOf(site, idB), "members standing still past the window are not counted");
		ctx.EqualFloat(0, LF_ContestedSites.CaptureProgressOf(site), 0.001, "idle members build no progress");
		ctx.EqualStr(idA, LF_Hideout.GetOwner(site), "A still leases the hideout");

		// 3. B alone and moving: it builds progress and takes the hideout.
		int sweeps;
		bool flipped;
		int budget = settings.m_iCaptureSeconds / step + 10;
		while (!RedProof() && sweeps < budget && !flipped)
		{
			now = now + step;
			sweeps++;
			array<ref LF_SiteSighting> moving = {};
			float x = 100;
			if (sweeps - 2 * (sweeps / 2) == 1)
				x = 102;
			Add(moving, "b1", idB, Vector(x, 0, 100), 30);
			Add(moving, "b2", idB, Vector(x, 0, 104), 10);
			flipped = LF_ContestedSites.SweepSite(site, moving, now, step) == LF_ESiteHoldOutcome.CAPTURED;
		}

		ctx.True(flipped, "B alone for captureSeconds captures the site");
		ctx.EqualStr(idB, LF_Hideout.GetOwner(site), "the lease moved to B");
		LF_HeldGroundRow held = LF_Hideout.HeldGroundOf(site);
		ctx.True(held != null, "the site has held ground");
		if (held)
		{
			ctx.EqualStr(idB, held.m_sHolderOrgId, "the held ground moved to B");
			ctx.Equal(0, held.m_iHoldPoints, "B starts at zero points");
		}

		ctx.Equal(1, LF_ContestedSites.Captures(), "one capture is published");

		LF_ContestedSites.ResetStatics();
		LF_Hideout.Reset();
		LF_OrganizationManager.Reset();
	}
}
