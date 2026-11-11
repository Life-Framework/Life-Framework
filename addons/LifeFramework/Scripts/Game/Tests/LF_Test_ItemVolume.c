// red-proof: Observed 2026-11-11 with `toolsi test --no-build --only LF_Test_ItemVolume`: removing the SCR_GadgetComponent from FineTicket.et failed `[LFTEST] FAIL inventory/paper-items`, `- a bare fixture character has room for the ticket`. Restored; green.

// tier: WORLD
//! inventory/paper-items: the paper items (receipt, fine ticket, hospital bill) all fit a bare fixture
//! character, whichever was spawned first in the run. Without an SCR_GadgetComponent and
//! `SizeSetupStrategy Volume` the engine let only the first of them spawned in a process be stored
//! (the others read a mesh-derived volume of 1073 and the jacket refused them), so a test passed or
//! failed on which earlier test had spawned paper (#699).
class LF_Test_ItemVolume : LF_Test
{
	protected static const ResourceName RECEIPT = "{3E8A5C1D7B2F4096}Prefabs/Items/Roleplay/LF_Receipt.et";
	protected static const ResourceName TICKET = "{3D6B0E8A41C7F259}Prefabs/Items/Roleplay/FineTicket.et";
	protected static const ResourceName BILL = "{75055A5BFE4372D5}Prefabs/Items/Roleplay/HospitalBill.et";

	override string GetName()
	{
		return "inventory/paper-items";
	}

	//------------------------------------------------------------------------------------------------
	protected static void CheckPaper(LF_TestContext ctx, ResourceName prefab, float volume, string label)
	{
		IEntity body = LF_TestCharacterFixture.Spawn(ctx);
		if (!body)
			return;

		IEntity item = LF_Utils.SpawnEntityPrefab(prefab, body.GetOrigin());
		if (!item)
		{
			ctx.Fail("the " + label + " prefab does not spawn");
			LF_TestCharacterFixture.Delete(body);
			return;
		}

		InventoryItemComponent inventoryItem = LF_Component<InventoryItemComponent>.Find(item);
		ctx.NotNull(inventoryItem, "the " + label + " has an inventory item component");
		if (inventoryItem)
			ctx.Equal(volume, inventoryItem.GetTotalVolume(), "the " + label + " volume is its ItemVolume");

		InventoryStorageManagerComponent manager = LF_InventoryUtils.GetResponsibleStorageManager(body);
		ctx.True(manager && manager.CanInsertItem(item), "a bare fixture character has room for the " + label);

		SCR_EntityHelper.DeleteEntityAndChildren(item);
		LF_TestCharacterFixture.Delete(body);
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// Each in turn: a prefab spawned later in the run is the one that used to be refused.
		CheckPaper(ctx, RECEIPT, 60, "receipt");
		CheckPaper(ctx, TICKET, 113, "ticket");
		CheckPaper(ctx, BILL, 113, "hospital bill");
		CheckPaper(ctx, TICKET, 113, "ticket on a second spawn");
	}
}
