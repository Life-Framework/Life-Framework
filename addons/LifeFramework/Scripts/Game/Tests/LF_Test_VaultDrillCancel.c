// red-proof: 2026-11-15 `tools/cli test --no-build --red LF_Test_HandToolEquip,LF_Test_VaultDrillAction,LF_Test_VaultDrillCancel,LF_Test_VaultDrillPayOnce` RED-OK, hook on failed `- cancelled: no haul (expected 0, got 1)`, `- put away: no haul (expected 0, got 1)`.
// tier: WORLD
//! Nothing is paid before the drill has run its time: starting pays nothing; cancelling or putting
//! the drill away stops the drilling and the vault stays shut with no haul, even after the full
//! time has passed on the server clock. Walking off and going down stop it too (the pure step).
class LF_Test_VaultDrillCancel : LF_Test
{
	override string GetName()
	{
		return "crime/vault-drill-cancel";
	}

	override void Run(LF_TestContext ctx)
	{
		IEntity drill;
		IEntity robber = LF_TestVaultDrillFixture.Robber(ctx, drill);
		if (!robber)
			return;

		string uid = "vault-drill-cancel";
		string vaultId = "vault-drill-cancel";
		LF_TestVaultDrillFixture.Vault(vaultId, robber, 10);
		LF_TestVaultDrillFixture.Hold(robber, drill, true);
		LF_VaultDrill.s_fNowOverride = 100;

		// Cancelled by the robber.
		ctx.True(LF_Bank.Rob(vaultId, uid, robber), "the drilling starts");
		ctx.True(LF_VaultDrill.IsDrillingBy(vaultId, robber), "the robber is drilling");
		ctx.Equal(0, LF_Bank.GetSpawnedBagCount(vaultId), "starting pays nothing");
		ctx.False(LF_Breach.IsOpen(vaultId), "starting opens nothing");
		LF_VaultDrill.s_fNowOverride = 105;
		LF_VaultDrill.Tick();
		if (!RedProof())
			LF_VaultDrill.Cancel(vaultId, "test cancel");

		LF_VaultDrill.s_fNowOverride = 120;
		LF_VaultDrill.Tick();
		ctx.False(LF_Breach.IsOpen(vaultId), "cancelled: the vault stays shut");
		ctx.Equal(0, LF_Bank.GetSpawnedBagCount(vaultId), "cancelled: no haul");

		// Put away mid-drill.
		ctx.True(LF_Bank.Rob(vaultId, uid, robber), "a second start");
		LF_TestVaultDrillFixture.Hold(robber, drill, false);
		LF_VaultDrill.s_fNowOverride = 140;
		LF_VaultDrill.Tick();
		ctx.False(LF_VaultDrill.IsDrilling(vaultId), "put away: the drilling stops");
		ctx.Equal(0, LF_Bank.GetSpawnedBagCount(vaultId), "put away: no haul");

		// Walked off or downed: the pure step.
		ctx.True(LF_VaultDrill.Step(true, true, LF_VaultDrill.MAX_DISTANCE + 1, true, 99, 10) == LF_EVaultDrillStep.STOP_RANGE, "out of reach stops it");
		ctx.True(LF_VaultDrill.Step(true, false, 0, true, 99, 10) == LF_EVaultDrillStep.STOP_DOWN, "a downed robber stops it");
		ctx.True(LF_VaultDrill.Step(true, true, 0, true, 9.5, 10) == LF_EVaultDrillStep.KEEP, "short of the time it keeps going");
		LF_VaultDrill.s_fNowOverride = -1;
	}
}
