// red-proof: observed 2026-11-08. With LF_RobCooldown.IsActive reverted to `(nowMs - lastMs) < cooldownSec`,
// `tools\cli test --no-build --tier all` failed robustness/lifetime with "299 s into a 300 s cooldown is still active"
// and "half a second after a robbery is still active". Restored, it passes.

// tier: LOGIC
class EL_Test_Robustness : EL_Test
{
	override string GetName()
	{
		return "robustness/lifetime";
	}

	override void Run(EL_TestContext ctx)
	{
		RunCooldown(ctx);
		RunIntentStaleness(ctx);
		RunSpawnRetryCap(ctx);
		RunBreachChargeLifetime(ctx);
		RunEscortRelease(ctx);
	}

	//------------------------------------------------------------------------------------------------
	//! World time is milliseconds; the robbery cooldown attribute is seconds.
	protected void RunCooldown(EL_TestContext ctx)
	{
		float last = 10000;
		ctx.False(LF_RobCooldown.IsActive(last, LF_RobCooldown.NEVER, 300), "a never-robbed action has no cooldown");
		ctx.True(LF_RobCooldown.IsActive(last + 299000, last, 300), "299 s into a 300 s cooldown is still active");
		ctx.True(LF_RobCooldown.IsActive(last + 500, last, 300), "half a second after a robbery is still active");
		ctx.False(LF_RobCooldown.IsActive(last + 300001, last, 300), "after 300 s the cooldown is over");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunIntentStaleness(EL_TestContext ctx)
	{
		ctx.False(EL_QuantityComponent.IsIntentStale(1000, 1100), "an intent set this tick is fresh");
		ctx.True(EL_QuantityComponent.IsIntentStale(1000, 1000 + EL_QuantityComponent.INTENT_TTL_MS + 1), "an intent past its TTL is stale");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunSpawnRetryCap(EL_TestContext ctx)
	{
		ctx.True(EL_SpawnLogic.ShouldRetryRegister(1), "the first registration retry is allowed");
		ctx.True(EL_SpawnLogic.ShouldRetryRegister(EL_SpawnLogic.MAX_REGISTER_RETRIES), "the last capped retry is allowed");
		ctx.False(EL_SpawnLogic.ShouldRetryRegister(EL_SpawnLogic.MAX_REGISTER_RETRIES + 1), "retries stop at the cap");
	}

	//------------------------------------------------------------------------------------------------
	//! A charge whose fuse can no longer fire must not leave the target armed forever.
	protected void RunBreachChargeLifetime(EL_TestContext ctx)
	{
		string uid = "robustness-breach-player";
		string targetId = "robustness-breach-fuse";

		LF_Breach.Configure(targetId, 5, "");
		ctx.True(LF_Breach.Arm(targetId, uid), "a positive fuse arms");
		ctx.False(LF_Breach.CanArm(targetId), "an armed target refuses a second charge");

		LF_Breach.CancelCharge(targetId, "robustness test");
		ctx.True(LF_Breach.CanArm(targetId), "a cancelled charge frees the target");
		ctx.False(LF_Breach.IsOpen(targetId), "cancelling leaves the lock closed");

		ctx.True(LF_Breach.Arm(targetId, uid), "the freed target arms again");
		LF_Breach.Configure(targetId, 5, "");
		ctx.True(LF_Breach.CanArm(targetId), "re-configuring drops a charge whose timer died with the old entity");

		ctx.True(LF_Breach.Arm(targetId, uid), "an unplaced target keeps its script-driven fuse");
		ctx.True(LF_Breach.Advance(targetId, 5), "Advance still opens an unplaced target");
	}

	//------------------------------------------------------------------------------------------------
	//! A disconnecting officer or prisoner must not leave an escort hold behind.
	protected void RunEscortRelease(EL_TestContext ctx)
	{
		string officerUid = "robustness-escort-officer";
		string prisonerUid = "robustness-escort-prisoner";

		EL_PlayerAccount officer = EL_PlayerAccountManager.GetOrCreate(officerUid);
		officer.SetFaction(EL_Faction.POLICE);
		officer.SetOnDuty(true);
		EL_PlayerAccount prisoner = EL_PlayerAccountManager.GetOrCreate(prisonerUid);
		prisoner.SetWantedLevel(2);

		ctx.True(LF_JailEscort.Escort(officerUid, prisonerUid), "the officer escorts a wanted player");
		ctx.EqualStr(prisonerUid, LF_JailEscort.GetOnFootPrisoner(officerUid), "the hold is live");

		ctx.Equal(1, LF_JailEscort.ReleasePlayer(officerUid), "the officer's disconnect releases one hold");
		ctx.EqualStr("", LF_JailEscort.GetOnFootPrisoner(officerUid), "no hold is left for the officer");
		ctx.Equal(0, LF_JailEscort.ReleasePlayer(officerUid), "a second release is a no-op");

		ctx.True(LF_JailEscort.Escort(officerUid, prisonerUid), "the escort can start again");
		ctx.Equal(1, LF_JailEscort.ReleasePlayer(prisonerUid), "the prisoner's disconnect releases the hold too");
		ctx.EqualStr("", LF_JailEscort.GetOnFootPrisoner(officerUid), "no hold is left after the prisoner leaves");

		prisoner.SetWantedLevel(0);
		officer.SetOnDuty(false);
	}
}
