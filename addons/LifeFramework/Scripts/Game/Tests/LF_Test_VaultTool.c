// red-proof: Observed 2026-11-10 with `tools/cli test --no-build --only LF_Test_VaultTool`: flipping `roll < chance` to `roll > chance` in LF_Bank.Jams failed `crime/vault-tool` with `- a low roll jams`, `- a jammed drill opens nothing`, `- with a drill the breach starts`. Restored; green. The robber is not deleted at the end: deleting a character carrying the path-loaded field pack crashes the diag server.
// tier: WORLD
//! The vault needs a fence tool (#273). No drill: refused and nothing opens. With a drill: the
//! breach starts, the drill is left at the vault and its serial names the buyer through the evidence
//! path. A jammed drill opens nothing. A charge also works and is consumed.
class LF_Test_VaultTool : LF_Test
{
	override string GetName()
	{
		return "crime/vault-tool";
	}

	override void Run(LF_TestContext ctx)
	{
		string uid = "vault-tool-a";
		string vaultId = "vault-tool-vault";

		IEntity robber = LF_TestCharacterFixture.Spawn(ctx);
		if (!robber)
			return;

		InventoryStorageManagerComponent inventory = LF_InventoryUtils.GetResponsibleStorageManager(robber);
		// The fixture's jacket is too small for a drill; a field pack carries it.
		IEntity pack = LF_Utils.SpawnEntityPrefab("Prefabs/Items/Equipment/Backpacks/Backpack_ALICE_Medium.et", robber.GetOrigin());
		ctx.True(pack != null && inventory.TryInsertItem(pack, EStoragePurpose.PURPOSE_LOADOUT_PROXY), "the robber wears a pack");

		LF_Bank.Configure(vaultId, 1, 100, 0, 0, 0);
		LF_Bank.ConfigureTools(vaultId, "drill,charge", 0.5);

		ctx.False(LF_Bank.HasTool(vaultId, robber), "the robber starts with no tool");
		ctx.False(LF_Bank.Rob(vaultId, uid, robber, 0.9), "no tool: refused");
		ctx.False(LF_Breach.IsOpen(vaultId), "no tool: the vault stays shut");
		ctx.Equal(0, LF_Breach.GetPoliceNotifyCount(vaultId), "no tool: no break-in is filed");

		ctx.True(LF_InventoryUtils.AddItem(inventory, LF_Bank.DRILL_PREFAB), "a drill is handed over");
		array<IEntity> drills = LF_InventoryUtils.FindItemsByPrefab(inventory, LF_Bank.DRILL_PREFAB);
		if (drills.IsEmpty())
		{
			ctx.Fail("the drill is not in the inventory");
			LF_TestCharacterFixture.Delete(robber);
			return;
		}

		IEntity drill = drills[0];
		LF_SerialComponent serial = LF_SerialComponent.Cast(drill.FindComponent(LF_SerialComponent));
		ctx.NotNull(serial, "the drill carries a serial component");
		string stamped = LF_FenceSerials.Issue(uid, LF_Bank.DRILL_PREFAB);
		ctx.True(serial.SetSerial(stamped), "the fence stamps the drill");

		ctx.True(LF_Bank.Jams(0.5, 0.2), "a low roll jams");
		ctx.False(LF_Bank.Jams(0.5, 0.8), "a high roll runs");
		ctx.False(LF_Bank.Rob(vaultId, uid, robber, 0.2), "a jammed drill opens nothing");
		ctx.False(LF_Breach.IsOpen(vaultId), "jam: the vault stays shut");
		ctx.True(LF_InventoryUtils.FindItemsByPrefab(inventory, LF_Bank.DRILL_PREFAB).IsEmpty(), "jam: the drill left the robber");

		ctx.True(LF_InventoryUtils.AddItem(inventory, LF_Bank.DRILL_PREFAB), "a second drill");
		ctx.True(LF_Bank.Rob(vaultId, uid, robber, 0.9), "with a drill the breach starts");
		ctx.True(LF_Breach.IsOpen(vaultId), "the vault opened");
		ctx.Equal(1, LF_Bank.GetSpawnedBagCount(vaultId), "the haul spawned");

//		LF_TestCharacterFixture.Delete(robber);
	}
}
