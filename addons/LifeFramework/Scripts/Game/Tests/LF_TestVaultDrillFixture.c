//------------------------------------------------------------------------------------------------
//! Shared WORLD fixture for the timed vault drill (hand tool): a robber wearing a pack with a drill,
//! a vault whose anchor is the robber, and the hold seam. Holding sets the gadget mode the way the
//! take-in-hand animation's finish event does on the server; the quick slot equip itself is
//! proved in a session.
class LF_TestVaultDrillFixture
{
	static const ResourceName PACK_PREFAB = "{06B68C58B72EAAC6}Prefabs/Items/Equipment/Backpacks/Backpack_ALICE_Medium.et";

	//------------------------------------------------------------------------------------------------
	//! \return The robber carrying a drill, or null after a FAIL on ctx.
	static IEntity Robber(LF_TestContext ctx, out IEntity drill)
	{
		drill = null;
		IEntity robber = LF_TestCharacterFixture.Spawn(ctx);
		if (!robber)
			return null;

		InventoryStorageManagerComponent inventory = LF_InventoryUtils.GetResponsibleStorageManager(robber);
		IEntity pack = LF_Utils.SpawnEntityPrefab(PACK_PREFAB, robber.GetOrigin());
		if (!pack || !inventory || !inventory.TryInsertItem(pack, EStoragePurpose.PURPOSE_LOADOUT_PROXY))
		{
			ctx.Fail("fixture: the robber cannot wear a pack");
			return null;
		}

		if (!LF_InventoryUtils.AddItem(inventory, LF_Bank.DRILL_PREFAB))
		{
			ctx.Fail("fixture: the drill does not fit the pack");
			return null;
		}

		drill = LF_HandTool.Carried(robber, LF_VaultDrill.TOOL_ID);
		if (!drill)
		{
			ctx.Fail("fixture: the carried drill is not a hand tool with id drill");
			return null;
		}

		return robber;
	}

	//------------------------------------------------------------------------------------------------
	//! A drill vault (no jam) anchored on `anchor`, opening after `seconds` of drilling.
	static void Vault(string vaultId, IEntity anchor, float seconds)
	{
		LF_Bank.Configure(vaultId, 1, 100, 0, 0, 0);
		LF_Bank.ConfigureTools(vaultId, "drill", 0);
		LF_Bank.ConfigureDrillSeconds(vaultId, seconds);
		LF_Places.Register(LF_Bank.PLACE_KIND, vaultId, anchor);
	}

	//------------------------------------------------------------------------------------------------
	static void Hold(IEntity robber, IEntity drill, bool held)
	{
		LF_HandToolComponent tool = LF_HandTool.Of(drill);
		if (!tool)
			return;

		if (held)
			tool.OnModeChanged(EGadgetMode.IN_HAND, robber);
		else
			tool.OnModeChanged(EGadgetMode.IN_STORAGE, robber);
	}
}
