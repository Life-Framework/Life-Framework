// red-proof: 2026-11-15 `tools/cli test --no-build --red LF_Test_HandToolEquip,LF_Test_VaultDrillAction,LF_Test_VaultDrillCancel,LF_Test_VaultDrillPayOnce` RED-OK, hook on failed `- after the full time the vault opens`, `- the haul is paid (expected 1, got 0)`.
// tier: WORLD
//! A drill that runs its full time, held at the door, opens the vault and pays exactly once: the
//! haul spawns on the tick the server clock passes the drill time; further ticks, a second rob and
//! a repeated finish pay nothing more, and the drill is left at the vault.
class LF_Test_VaultDrillPayOnce : LF_Test
{
	override string GetName()
	{
		return "crime/vault-drill-pay-once";
	}

	override void Run(LF_TestContext ctx)
	{
		IEntity drill;
		IEntity robber = LF_TestVaultDrillFixture.Robber(ctx, drill);
		if (!robber)
			return;

		string uid = "vault-drill-pay";
		string vaultId = "vault-drill-pay";
		LF_TestVaultDrillFixture.Vault(vaultId, robber, 10);
		LF_TestVaultDrillFixture.Hold(robber, drill, true);
		LF_VaultDrill.s_fNowOverride = 200;

		ctx.True(LF_Bank.Rob(vaultId, uid, robber), "the drilling starts");
		LF_VaultDrill.s_fNowOverride = 209;
		LF_VaultDrill.Tick();
		ctx.Equal(0, LF_Bank.GetSpawnedBagCount(vaultId), "at 9 of 10 s nothing is paid");

		if (!RedProof())
			LF_VaultDrill.s_fNowOverride = 210.5;

		LF_VaultDrill.Tick();
		ctx.True(LF_Breach.IsOpen(vaultId), "after the full time the vault opens");
		ctx.Equal(1, LF_Bank.GetSpawnedBagCount(vaultId), "the haul is paid");
		ctx.False(LF_VaultDrill.IsDrilling(vaultId), "the drilling is over");
		ctx.True(LF_HandTool.Carried(robber, LF_VaultDrill.TOOL_ID) == null, "the drill is left at the vault");

		LF_VaultDrill.s_fNowOverride = 230;
		LF_VaultDrill.Tick();
		ctx.False(LF_Bank.Rob(vaultId, uid, robber), "an open vault is not drilled again");
		ctx.False(LF_Bank.FinishDrill(vaultId, uid, robber, null), "a repeated finish pays nothing");
		ctx.Equal(1, LF_Bank.GetSpawnedBagCount(vaultId), "paid once");
		LF_VaultDrill.s_fNowOverride = -1;
	}
}
