// red-proof: 2026-11-21 `tools\cli test --no-build --red LF_Test_PoliceAlphaApproval` printed
// `RED-OK police/alpha-instant-approval (fails with the hook on, 8 assertions)`: RedProof() leaves the owner switch out.

//! rc 0.2.0: the owner switch police.alphaInstantApproval (alpha only, off by default) makes the station desk approve a
//! police application the moment it is filed, as the playtest mission header does, so two testers on the alpha can
//! clock in as police without an administrator. Off, the desk keeps the playtime gate and the admin review.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_PoliceAlphaApproval : LF_Test
{
	override string GetName()
	{
		return "police/alpha-instant-approval";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_PoliceConfig MergePolice(string sectionJson)
	{
		LF_ConfigService.Reset();
		LF_ConfigStream.Reset();
		LF_ConfigService.LoadOwnerString("{\"schemaVersion\":1,\"police\":" + sectionJson + "}", "test");
		Resource container = BaseContainerTools.LoadContainer(LF_PoliceConfig.CONFIG_PATH);
		if (!container || !container.GetResource())
			return null;

		LF_PoliceConfig config = LF_PoliceConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (config)
			config.ApplyLayers();

		return config;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// The rule: either switch approves on submit; neither keeps the admin review.
		ctx.False(LF_PoliceApplicationRules.ApprovesOnSubmit(false, false), "no header flag and no alpha switch: the admin reviews");
		ctx.True(LF_PoliceApplicationRules.ApprovesOnSubmit(true, false), "the playtest header approves on submit");
		ctx.True(LF_PoliceApplicationRules.ApprovesOnSubmit(false, true), "the alpha switch approves on submit");

		LF_PoliceConfig shipped = MergePolice("{}");
		ctx.NotNull(shipped, "the shipped police config loads");
		LF_PoliceConfig alpha;
		if (RedProof())
			alpha = MergePolice("{}");
		else
			alpha = MergePolice("{\"alphaInstantApproval\":true}");

		ctx.NotNull(alpha, "the police config loads with the owner section");
		if (shipped && alpha)
		{
			LF_PoliceConfig.UseForTest(shipped);
			ctx.False(LF_PoliceConfig.AlphaInstantApproval(), "the switch is off when the owner leaves it out");
			LF_PoliceConfig.UseForTest(alpha);
			ctx.True(LF_PoliceConfig.AlphaInstantApproval(), "the alpha owner switch turns approve-on-submit on");
		}

		LF_PoliceConfig.UseForTest(null);
		LF_ConfigService.Reset();
		LF_ConfigStream.Reset();
		LF_PoliceConfig.Reload();
	}
}
