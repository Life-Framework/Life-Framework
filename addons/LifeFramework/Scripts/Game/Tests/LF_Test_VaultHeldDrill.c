// red-proof: 2026-11-17 `tools/cli test --no-build --only LF_Test_VaultHeldDrill` on the unfixed LF_HandTool.c and LF_Bank.c (origin/main) failed `- the robber holds the drill`, `- a drill in the hands counts as the vault's tool`, `- the rob starts the drill`, `- the robber is drilling`; with the fix 9/9 drill and hand-tool tests pass.
// tier: WORLD
//! #2384: on a dedicated server a drill in a remote player's hands is in no storage the inventory
//! search walks, and the gadget manager does not report it either; the server only gets the drill's
//! own take-in-hand event (OnModeChanged IN_HAND). The bank proof logged exactly that at the gate:
//! stored drills 0, held gadget none. Here the drill is in no inventory and gets only that event:
//! the robber still holds it, the vault gate passes, and the rob starts the drill.
class LF_Test_VaultHeldDrill : LF_Test
{
	override string GetName()
	{
		return "crime/vault-held-drill";
	}

	override void Run(LF_TestContext ctx)
	{
		IEntity robber = LF_TestCharacterFixture.Spawn(ctx);
		if (!robber)
			return;

		string vaultId = "vault-held-drill";
		LF_TestVaultDrillFixture.Vault(vaultId, robber, 10);
		LF_VaultDrill.Cancel(vaultId, "test start");

		IEntity drill = LF_Utils.SpawnEntityPrefab(LF_Bank.DRILL_PREFAB, robber.GetOrigin());
		LF_HandToolComponent tool = LF_HandTool.Of(drill);
		ctx.NotNull(tool, "the drill is a hand tool");
		if (!tool)
			return;

		InventoryStorageManagerComponent inventory = LF_InventoryUtils.GetResponsibleStorageManager(robber);
		ctx.True(inventory && LF_InventoryUtils.FindItemsByPrefab(inventory, LF_Bank.DRILL_PREFAB).IsEmpty(), "no storage holds the drill");

		// The server's take-in-hand event, as the animation's finish sends it.
		tool.OnModeChanged(EGadgetMode.IN_HAND, robber);

		ctx.True(LF_HandTool.Holds(robber, LF_VaultDrill.TOOL_ID), "the robber holds the drill");
		ctx.True(LF_Bank.HasTool(vaultId, robber), "a drill in the hands counts as the vault's tool");
		ctx.True(LF_Bank.Rob(vaultId, "vault-held-drill", robber), "the rob starts the drill");
		ctx.True(LF_VaultDrill.IsDrillingBy(vaultId, robber), "the robber is drilling");

		LF_VaultDrill.Cancel(vaultId, "test end");
		tool.OnModeChanged(EGadgetMode.ON_GROUND, null);
		ctx.False(LF_HandTool.Holds(robber, LF_VaultDrill.TOOL_ID), "put down, it is no longer held");
		SCR_EntityHelper.DeleteEntityAndChildren(drill);
	}
}
