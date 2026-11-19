// red-proof: before the config class, the overlay and the header line, `tools\cli test --no-build --only LF_Test_EveronHeistBank` does not compile; with the class but no pack row it fails "the bank job is a pack row"; see the PR body for the observed run.
//! #3414 in game: under the Everon 1989 header (the pack leg of `tools\cli test`), the bank heist
//! overlay is listed and picked up by the core heist config. The bank job has three stages, door,
//! vault, getaway; each kind resolves to its world component and its LF_HeistStages gate, the door
//! lets the clerk choose hands up, and the clock (12 minutes) and the police minimum load. The core
//! file ships no job.
[LF_TestInfo(LF_TestTier.LOGIC, pack: true)]
class LF_Test_EveronHeistBank : LF_Test
{
	static const ResourceName OVERLAY = "{7A9C3E51B2D4F680}Configs/Crime/LF_HeistBank_Everon1989.conf";
	static const string BANK = "bank";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/heist-bank";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_MissionHeader header = LF_MissionHeader.GetCurrent();
		ctx.NotNull(header, "the pack leg runs under an LF mission header");
		if (header && header.m_aLF_ConfigOverlays)
			ctx.True(header.m_aLF_ConfigOverlays.Find(OVERLAY) != -1, "the header lists the bank heist overlay");
		else
			ctx.Fail("the header lists no config overlays");

		array<ref Resource> resources = {};
		array<ResourceName> names = {};
		LF_ConfigOverlay.LoadPackOverlays(LF_HeistConfig.CONFIG_PATH, LF_HeistConfig, "heist", resources, names);
		ctx.True(names.Find(OVERLAY) != -1, "the heist loader picks up its overlay");

		LF_HeistSiteRow site;
		if (!RedProof())
		{
			LF_HeistConfig.Reload();
			LF_HeistConfig config = LF_HeistConfig.Get();
			ctx.NotNull(config, "the heist config loads");
			if (config)
				site = config.Find(BANK);
		}

		ctx.NotNull(site, "the bank job is a pack row");
		if (!site)
			return;

		ctx.Equal(720, site.m_iClockSeconds, "the clock is twelve minutes");
		ctx.Equal(2, site.m_iMinPoliceOnDuty, "the job needs two police on duty");
		ctx.Equal(3, site.m_aStages.Count(), "the bank job has three stages");
		ctx.EqualStr("", site.Problem(), "the stage list is complete");

		LF_HeistStageRow door = site.m_aStages[0];
		LF_HeistStageRow vault = site.m_aStages[1];
		LF_HeistStageRow getaway = site.m_aStages[2];
		ctx.EqualStr(LF_HeistKinds.DOOR, door.m_sKind, "stage one is the staff door");
		ctx.EqualStr(LF_HeistKinds.VAULT, vault.m_sKind, "stage two is the vault");
		ctx.EqualStr(LF_HeistKinds.GETAWAY, getaway.m_sKind, "stage three is the getaway");
		ctx.True(door.m_bClerkHandsUp, "the clerk may choose hands up at the door");
		ctx.False(vault.m_bClerkHandsUp, "hands up is a door choice only");
		ctx.EqualStr("bank_vault", vault.m_sTargetId, "the vault stage names the town bank's vault id");
		ctx.True(getaway.m_fRadius > 0, "the getaway has a radius past the town edge");

		ctx.True(LF_HeistKinds.Component(door.m_sKind) == LF_BreachTargetComponent, "a door kind resolves to a breach target");
		ctx.True(LF_HeistKinds.Component(vault.m_sKind) == LF_BankVaultComponent, "a vault kind resolves to a bank vault");
		ctx.True(LF_HeistKinds.Component(getaway.m_sKind) == LF_HeistRunComponent, "a getaway kind resolves to the heist site");
		ctx.True(LF_HeistKinds.Gate(door.m_sKind) == LF_EHeistGate.TARGET_OPENED, "the door waits for a target to open");
		ctx.True(LF_HeistKinds.Gate(getaway.m_sKind) == LF_EHeistGate.CREW_IN_AREA, "the getaway waits for the crew to leave");

		LF_HeistStages stages = site.BuildStages();
		ctx.NotNull(stages, "the stage list builds");
		if (stages)
			ctx.Equal(3, stages.GetStageCount(), "the built stages match the rows");

		// A row naming no kind the core knows is refused, not run.
		LF_HeistSiteRow bad = LF_HeistSiteRow.Make("bad");
		LF_HeistStageRow odd = new LF_HeistStageRow();
		odd.m_sKind = "tunnel";
		bad.m_aStages.Insert(odd);
		ctx.True(bad.Problem() != "", "an unknown stage kind is a problem");
		typename none = LF_HeistKinds.Component("tunnel");
		ctx.True(!none, "an unknown kind resolves to no component");
	}
}
