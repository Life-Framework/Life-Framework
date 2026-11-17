// red-proof: 2026-11-17 `test --no-build --red LF_Test_SiteHold` printed `RED-OK sites/site-hold (fails with the hook on, 33 assertions)`; with the hook on only one organisation is present, so the contested freeze assertions fail (expected 20 points, got 40).
// tier: LOGIC
//! Site hold (#2678): the scoring maths with no world. Who gains, who is frozen, how progress decays.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_SiteHold : LF_Test
{
	override string GetName()
	{
		return "sites/site-hold";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_SiteRules TestRules()
	{
		LF_SiteRules rules = new LF_SiteRules();
		rules.m_iMinMembersPresent = 2;
		rules.m_iMaxCountedPerOrg = 6;
		rules.m_fCaptureSeconds = 100;
		rules.m_fDecayPerHour = 3600;	// one progress second per second
		rules.m_fFlipCooldownSeconds = 300;
		rules.m_fPointsPerHour = 3600;	// one point per second
		rules.m_iMaxPoints = 50;
		return rules;
	}

	//------------------------------------------------------------------------------------------------
	protected array<ref LF_SitePresence> Present(string orgA, int membersA, int innerA, string orgB = "", int membersB = 0, int innerB = 0)
	{
		array<ref LF_SitePresence> list = {};
		list.Insert(LF_SitePresence.Create(orgA, membersA, innerA));
		if (!orgB.IsEmpty())
			list.Insert(LF_SitePresence.Create(orgB, membersB, innerB));

		return list;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_SiteRules rules = TestRules();
		array<ref LF_SitePresence> nobody = {};

		// One organisation alone gains: the holder banks hold points and seconds.
		LF_SiteHoldState held = new LF_SiteHoldState();
		held.m_sHolderOrgId = "orgA";
		LF_ESiteHoldOutcome outcome = LF_SiteHold.Step(held, Present("orgA", 3, 0), 10, rules);
		ctx.Equal(LF_ESiteHoldOutcome.GAIN, outcome, "the holder alone gains");
		ctx.Equal(10, held.HoldPoints(), "ten seconds bank ten points");
		ctx.Equal(10, held.HoldSeconds(), "ten seconds of hold are counted");
		outcome = LF_SiteHold.Step(held, Present("orgA", 3, 0), 1000, rules);
		ctx.Equal(rules.m_iMaxPoints, held.HoldPoints(), "points stop at the cap");

		// Two organisations present freeze everything, whoever holds.
		LF_SiteHoldState contested = new LF_SiteHoldState();
		contested.m_sHolderOrgId = "orgA";
		contested.m_fHoldPoints = 20;
		contested.m_fHoldSeconds = 40;
		contested.m_sCapturerOrgId = "orgB";
		contested.m_fCaptureProgress = 30;
		// Red-proof hook: with --red only one organisation is let in, so it gains and the freeze assertions fail.
		array<ref LF_SitePresence> both = Present("orgA", 3, 0, "orgB", 3, 0);
		if (RedProof())
			both = Present("orgA", 3, 0);

		outcome = LF_SiteHold.Step(contested, both, 20, rules);
		ctx.Equal(LF_ESiteHoldOutcome.CONTESTED, outcome, "two organisations present is contested");
		ctx.Equal(20, contested.HoldPoints(), "contested: hold points do not move");
		ctx.Equal(40, contested.HoldSeconds(), "contested: hold seconds do not move");
		ctx.EqualFloat(30, contested.m_fCaptureProgress, 0.001, "contested: capture progress does not move");
		ctx.EqualStr("orgA", contested.m_sHolderOrgId, "contested: the holder keeps the site");

		// Below the member minimum a lone organisation gains nothing.
		LF_SiteHoldState weak = new LF_SiteHoldState();
		weak.m_sHolderOrgId = "orgA";
		outcome = LF_SiteHold.Step(weak, Present("orgB", 1, 0), 60, rules);
		ctx.Equal(LF_ESiteHoldOutcome.NONE, outcome, "one member under a minimum of two gains nothing");
		ctx.EqualFloat(0, weak.m_fCaptureProgress, 0.001, "below the minimum: no capture progress");
		outcome = LF_SiteHold.Step(weak, Present("orgA", 1, 0), 60, rules);
		ctx.Equal(0, weak.HoldPoints(), "below the minimum: the holder banks nothing");

		// An empty site decays the capture progress and keeps the banked points.
		LF_SiteHoldState empty = new LF_SiteHoldState();
		empty.m_sHolderOrgId = "orgA";
		empty.m_fHoldPoints = 25;
		empty.m_sCapturerOrgId = "orgB";
		empty.m_fCaptureProgress = 40;
		outcome = LF_SiteHold.Step(empty, nobody, 15, rules);
		ctx.Equal(LF_ESiteHoldOutcome.DECAY, outcome, "an empty site decays");
		ctx.EqualFloat(25, empty.m_fCaptureProgress, 0.001, "fifteen seconds remove fifteen progress");
		ctx.Equal(25, empty.HoldPoints(), "decay leaves the banked points");
		LF_SiteHold.Step(empty, nobody, 1000, rules);
		ctx.EqualFloat(0, empty.m_fCaptureProgress, 0.001, "progress never goes below zero");

		// The inner ring counts double: two members, one of them inner, progress like three.
		LF_SiteHoldState ring = new LF_SiteHoldState();
		ring.m_sHolderOrgId = "orgA";
		LF_SiteHold.Step(ring, Present("orgB", 2, 1), 10, rules);
		ctx.EqualFloat(30, ring.m_fCaptureProgress, 0.001, "two members, one inner, gain as three");
		LF_SiteHold.Step(ring, Present("orgB", 10, 10), 1, rules);
		ctx.EqualFloat(30 + 6, ring.m_fCaptureProgress, 0.001, "the counted size is capped per organisation");

		// A capture flips the site at the threshold, and the cooldown blocks a flip back.
		LF_SiteHoldState flip = new LF_SiteHoldState();
		flip.m_sHolderOrgId = "orgA";
		flip.m_fHoldPoints = 30;
		outcome = LF_SiteHold.Step(flip, Present("orgB", 2, 0), 40, rules);
		ctx.Equal(LF_ESiteHoldOutcome.GAIN, outcome, "a challenger alone gains progress");
		ctx.EqualStr("orgA", flip.m_sHolderOrgId, "below the threshold the holder stays");
		outcome = LF_SiteHold.Step(flip, Present("orgB", 2, 0), 10, rules);
		ctx.Equal(LF_ESiteHoldOutcome.CAPTURED, outcome, "the threshold flips the site");
		ctx.EqualStr("orgB", flip.m_sHolderOrgId, "the challenger now holds");
		ctx.Equal(0, flip.HoldPoints(), "the new holder starts at zero points");
		ctx.EqualFloat(0, flip.m_fCaptureProgress, 0.001, "a flip clears the capture progress");

		outcome = LF_SiteHold.Step(flip, Present("orgA", 5, 0), 100, rules);
		ctx.EqualStr("orgB", flip.m_sHolderOrgId, "the cooldown blocks the flip back");
		ctx.True(outcome != LF_ESiteHoldOutcome.CAPTURED, "a blocked capture is not reported as one");
		outcome = LF_SiteHold.Step(flip, Present("orgA", 5, 0), 500, rules);
		ctx.EqualStr("orgA", flip.m_sHolderOrgId, "after the cooldown the held-back capture completes");

		// The holder returning defends: a challenger's progress is wiped.
		LF_SiteHoldState defend = new LF_SiteHoldState();
		defend.m_sHolderOrgId = "orgA";
		defend.m_sCapturerOrgId = "orgB";
		defend.m_fCaptureProgress = 60;
		outcome = LF_SiteHold.Step(defend, Present("orgA", 2, 0), 5, rules);
		ctx.Equal(LF_ESiteHoldOutcome.DEFENDED, outcome, "the holder alone against a challenger's progress defends");
		ctx.EqualFloat(0, defend.m_fCaptureProgress, 0.001, "defending wipes the challenger's progress");
	}
}
