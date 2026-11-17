// red-proof: pending
// tier: WORLD
//! #2384: a drill in the hands opens a drill vault even when no storage search finds it. The engine
//! keeps a held gadget in the character's left-hand slot; the tool check reads the hands before
//! the inventory. Here the drill goes straight from the ground into the hand slot, so only the
//! hands hold it: the robber holds a drill, the gate passes, and the rob starts the drill.
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

		ChimeraCharacter character = ChimeraCharacter.Cast(robber);
		CharacterControllerComponent controller;
		if (character)
			controller = character.GetCharacterController();

		IEntity drill = LF_Utils.SpawnEntityPrefab(LF_Bank.DRILL_PREFAB, robber.GetOrigin());
		SCR_GadgetComponent gadget;
		if (drill)
			gadget = SCR_GadgetComponent.Cast(drill.FindComponent(SCR_GadgetComponent));

		ctx.NotNull(controller, "the robber has a character controller");
		ctx.NotNull(gadget, "the drill is a gadget");
		if (!controller || !gadget)
			return;

		// The quick slot's engine call, without the animation.
		controller.TakeGadgetInLeftHand(drill, gadget.GetAnimVariable(), false, true);
		ctx.True(controller.GetAttachedGadgetAtLeftHandSlot() == drill, "the drill sits in the left-hand slot");

		InventoryStorageManagerComponent inventory = LF_InventoryUtils.GetResponsibleStorageManager(robber);
		int inStorage = -1;
		if (inventory)
			inStorage = LF_InventoryUtils.FindItemsByPrefab(inventory, LF_Bank.DRILL_PREFAB).Count();
		LF_Debug.Info("Test", string.Format("vault-held-drill: storage search finds %1 drill(s) with the drill in the hand slot", inStorage));

		ctx.True(LF_HandTool.Holds(robber, LF_VaultDrill.TOOL_ID), "the robber holds the drill");
		ctx.True(LF_Bank.HasTool(vaultId, robber), "a drill in the hands counts as the vault's tool");
		ctx.True(LF_Bank.Rob(vaultId, "vault-held-drill", robber), "the rob starts the drill");
		ctx.True(LF_VaultDrill.IsDrillingBy(vaultId, robber), "the robber is drilling");
		LF_VaultDrill.Cancel(vaultId, "test end");
	}
}
