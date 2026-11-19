// red-proof: 2026-11-19 `tools/cli test --no-build --red LF_Test_HeistRun` printed `RED PROOF OK`; RedProof() skips the door arm and the tick past the clock (`the run is complete (expected 2, got 0)`, `the clock ran out (expected 3, got 1)`). Green without the hook.
//! Heist run (#3411): a site's three stages bound to the world. A vault opened before the door does
//! nothing; the real breach arm on the door starts the run (the alarm), the real vault drill is
//! stage two, and the crew leaving the radius completes it. A second run whose clock runs out fails.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_HeistRun : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "crime/heist-run";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		IEntity drill;
		IEntity robber = LF_TestVaultDrillFixture.Robber(ctx, drill);
		if (!robber)
			return;

		string uid = "heist-run-robber";
		string doorId = "heist-run-door";
		string vaultId = "heist-run-vault";
		LF_Breach.Configure(doorId, 0, "");
		LF_TestVaultDrillFixture.Vault(vaultId, robber, 10);
		LF_TestVaultDrillFixture.Hold(robber, drill, true);
		vector site = robber.GetOrigin();
		LF_HeistRun.s_iNowOverride = 1000;

		LF_HeistRun run = LF_HeistRun.Create("heist-run-site", doorId, vaultId, site, 20, 600000);
		ctx.True(run != null, "the run is built");
		if (!run)
		{
			Cleanup(robber, vaultId, doorId, null);
			return;
		}

		ctx.True(run.Listen(), "the run listens on the bus");

		// Out of order: the vault reported open before the door does nothing.
		LF_EventBus.Publish(LF_OnVaultBreach.Create(uid, vaultId, "drill", "", "opened"));
		ctx.Equal(LF_EHeistState.IDLE, run.GetState(), "a vault before the door does nothing to the run");

		// Stage one: the real breach arm opens the door (fuse 0) and starts the run.
		if (!RedProof())
			ctx.True(LF_Breach.Arm(doorId, uid), "the door charge is armed");

		ctx.True(LF_Breach.IsOpen(doorId), "the door is open");
		ctx.Equal(LF_EHeistState.RUNNING, run.GetState(), "the door opening starts the run");
		ctx.Equal(LF_HeistRun.STAGE_VAULT, run.GetStage(), "the vault is the next stage");
		ctx.True(run.IsAlarmRaised(), "the alarm rings at the first stage");

		// Stage two: the real drill runs its full time and the vault opens.
		LF_VaultDrill.s_fNowOverride = 200;
		ctx.True(LF_Bank.Rob(vaultId, uid, robber), "the drilling starts");
		LF_VaultDrill.s_fNowOverride = 210.5;
		LF_VaultDrill.Tick();
		LF_VaultDrill.s_fNowOverride = -1;
		ctx.True(LF_Breach.IsOpen(vaultId), "the drill opened the vault");
		ctx.Equal(LF_HeistRun.STAGE_GETAWAY, run.GetStage(), "the getaway is the next stage");

		// Stage three: inside the radius the run goes on; out of it, it is complete.
		array<vector> inside = {site};
		ctx.False(run.Tick(5000, inside), "the crew still in the area: the run goes on");
		ctx.Equal(LF_EHeistState.RUNNING, run.GetState(), "still running inside the area");
		array<vector> away = {site + Vector(30, 0, 0)};
		ctx.True(run.Tick(6000, away), "the crew out of the area finishes the run");
		ctx.Equal(LF_EHeistState.SUCCEEDED, run.GetState(), "door, vault, getaway: the run is complete");
		run.Stop();

		// The clock: a run whose door opened but that never got away fails when its time runs out.
		LF_HeistRun clocked = LF_HeistRun.Create("heist-run-clock", doorId, vaultId, site, 20, 5000);
		clocked.Listen();
		LF_EventBus.Publish(LF_OnBreachOpened.Create(doorId, uid));
		ctx.Equal(LF_EHeistState.RUNNING, clocked.GetState(), "the second run started at the door");
		ctx.False(clocked.Tick(5000, inside), "inside the clock nothing ends");
		if (!RedProof())
			clocked.Tick(7000, inside);

		ctx.Equal(LF_EHeistState.FAILED, clocked.GetState(), "the clock ran out: the run failed");
		ctx.True(clocked.GetFailReason() == LF_HeistStages.FAIL_CLOCK, "failed on the clock");

		Cleanup(robber, vaultId, doorId, clocked);
	}

	//------------------------------------------------------------------------------------------------
	protected void Cleanup(IEntity robber, string vaultId, string doorId, LF_HeistRun run)
	{
		if (run)
			run.Stop();

		LF_HeistRun.s_iNowOverride = -1;
		LF_VaultDrill.s_fNowOverride = -1;
		LF_Breach.Reset(doorId);
		LF_TestVaultDrillFixture.Release(robber, vaultId);
	}
}
