// red-proof: 2026-11-17 on the unfixed fuse (OnFuse advanced by the component's 0 s and skipped LF_Bank) `tools/cli test --no-build --only LF_Test_VaultChargeFuse` failed `- the fuse opens the vault`, `- the haul is offered (expected 1, got 0)`; `--red LF_Test_VaultChargeFuse` RED-OK with the same two lines; green after the fix.
// tier: WORLD
//! A charge set on a vault whose safe type has a fuse opens the vault when the fuse fires and pays the
//! haul once (#2541). The fuse used to advance the breach by the component's own fuse (0 on a vault)
//! and skipped LF_Bank, so the robber took the wanted level and the vault never opened.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_VaultChargeFuse : LF_Test
{
	static const ResourceName SAFE_PREFAB = "{B5A9BA64B3966CC1}Prefabs/Props/Commercial/LF_SafeBox.et";
	static const float FUSE = 10;

	override string GetName()
	{
		return "crime/vault-charge-fuse";
	}

	override void Run(LF_TestContext ctx)
	{
		string uid = "vault-charge-fuse";
		string vaultId = "vault-charge-fuse";

		IEntity safe = LF_Utils.SpawnEntityPrefab(SAFE_PREFAB, "0 0 0");
		if (!safe)
		{
			ctx.Fail("the safe prefab does not spawn: " + SAFE_PREFAB);
			return;
		}

		LF_BreachTargetComponent target = LF_Component<LF_BreachTargetComponent>.Find(safe);
		if (!target)
		{
			ctx.Fail("the safe prefab has no LF_BreachTargetComponent");
			SCR_EntityHelper.DeleteEntityAndChildren(safe);
			return;
		}

		// A placed vault carries its id on the prefab; the script-spawned one is given it.
		target.SetTargetId(vaultId);

		// The vault as LF_BankVaultComponent configures it from a charge safe type with a 10 s fuse.
		LF_Bank.Configure(vaultId, 1, 100, 0, 0, FUSE);
		LF_Bank.ConfigureTools(vaultId, LF_Bank.TOOL_CHARGE, 0);
		LF_Places.Register(LF_Bank.PLACE_KIND, vaultId, safe);
		LF_Places.Register(LF_Breach.PLACE_KIND, vaultId, safe);

		ctx.True(LF_Bank.Rob(vaultId, uid), "the charge is set");
		ctx.False(LF_Breach.IsOpen(vaultId), "the vault waits for the fuse");
		ctx.Equal(0, LF_Bank.GetSpawnedBagCount(vaultId), "nothing is paid before the fuse");
		ctx.True(target.IsFusePending(), "the fuse timer runs on the vault");

		if (!RedProof())
			target.FireFuse();

		ctx.True(LF_Breach.IsOpen(vaultId), "the fuse opens the vault");
		ctx.Equal(1, LF_Bank.GetSpawnedBagCount(vaultId), "the haul is offered");

		target.FireFuse();
		LF_Bank.Advance(vaultId, FUSE);
		ctx.Equal(1, LF_Bank.GetSpawnedBagCount(vaultId), "a second fuse call pays nothing more");

		SCR_EntityHelper.DeleteEntityAndChildren(safe);
	}
}
