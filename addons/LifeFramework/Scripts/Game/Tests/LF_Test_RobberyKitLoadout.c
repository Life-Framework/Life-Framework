// red-proof: 2026-11-23 `tools/cli test --no-build --red LF_Test_RobberyKitLoadout` (hook skips the grant, so no tool is carried and the drill is not a held-ready hand tool). Green on main as of 1e745e85: #4306's live proof found the kit sound (the chest drill was the break), this test keeps it so.
//! The robbery kit's loadout (#4197, #4156): every item the kit hands over resolves to a real
//! prefab that spawns as an inventory item, the drill is the working one (a hand tool with tool
//! id drill, the one the vault drilling and the quick bar read), every tool token a vault or store
//! asks for (the safe default, the armoury) has a kit item, the bag the vault pays into resolves,
//! and one grant puts the lot on the robber.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_RobberyKitLoadout : LF_Test
{
	override string GetName()
	{
		return "crime/robbery-kit-loadout";
	}

	//------------------------------------------------------------------------------------------------
	//! The kit prefab that serves one tool token a vault lists, or empty for none.
	protected ResourceName KitPrefabFor(string token)
	{
		if (token == LF_Bank.TOOL_DRILL)
			return LF_Bank.DRILL_PREFAB;
		if (token == LF_Bank.TOOL_KEYCARD)
			return LF_Bank.KEYCARD_PREFAB;
		if (token == LF_Bank.TOOL_CHARGE)
			return LF_Breach.FENCE_CHARGE_PREFAB;

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! A prefab that loads, spawns and carries an inventory item component; the entity is deleted.
	protected void CheckItem(LF_TestContext ctx, ResourceName prefab, string what)
	{
		Resource resource = Resource.Load(prefab);
		ctx.True(resource != null && resource.IsValid(), what + " resolves to a prefab");
		if (!resource || !resource.IsValid())
			return;

		IEntity item = LF_Utils.SpawnEntityPrefab(prefab, "0 0 0");
		ctx.NotNull(item, what + " spawns");
		if (!item)
			return;

		ctx.True(item.FindComponent(InventoryItemComponent) != null, what + " is an inventory item");
		SCR_EntityHelper.DeleteEntityAndChildren(item);
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		array<ResourceName> items = LF_BankRobKit.Items();
		ctx.True(items.Count() >= 3, "the kit holds a drill, a keycard and a charge");

		foreach (ResourceName prefab : items)
		{
			CheckItem(ctx, prefab, prefab);
		}

		// The drill is the working one: a hand tool with id drill, as LF_Bank and LF_VaultDrill read it.
		IEntity drill = LF_Utils.SpawnEntityPrefab(LF_Bank.DRILL_PREFAB, "0 0 0");
		ctx.NotNull(drill, "the kit drill spawns");
		if (drill)
		{
			LF_HandToolComponent tool = LF_HandTool.Of(drill);
			ctx.NotNull(tool, "the kit drill is a hand tool, so a quick slot can hold it");
			if (tool)
				ctx.EqualStr(LF_VaultDrill.TOOL_ID, tool.GetToolId(), "the kit drill's tool id is the one the vault drilling reads");

			ctx.True(drill.FindComponent(LF_AssetAnimComponent) != null, "the kit drill spins and sounds when used");
			SCR_EntityHelper.DeleteEntityAndChildren(drill);
		}

		// Every tool token a rung asks for has a kit item (safe default, armoury store, cash van box).
		array<string> tokens = {};
		LF_SafeTuning.DEFAULT_TOOLS.Split(",", tokens, true);
		tokens.Insert(LF_Bank.TOOL_KEYCARD);
		foreach (string token : tokens)
		{
			ResourceName kitPrefab = KitPrefabFor(token);
			ctx.False(kitPrefab.IsEmpty(), "tool '" + token + "' has a kit prefab");
			if (!kitPrefab.IsEmpty())
				ctx.True(items.Find(kitPrefab) != -1, "the kit hands out the item for tool '" + token + "'");
		}

		// The bag a vault pays into resolves.
		ResourceName bag = LF_Bank.BagPrefab();
		ctx.False(bag.IsEmpty(), "the vault's money bag has a prefab");
		if (!bag.IsEmpty())
			CheckItem(ctx, bag, "the money bag");

		IEntity robber = LF_TestCharacterFixture.Spawn(ctx);
		if (!robber)
			return;

		InventoryStorageManagerComponent inventory = LF_InventoryUtils.GetResponsibleStorageManager(robber);
		IEntity pack = LF_Utils.SpawnEntityPrefab(LF_TestVaultDrillFixture.PACK_PREFAB, robber.GetOrigin());
		ctx.True(pack != null && inventory != null && inventory.TryInsertItem(pack, EStoragePurpose.PURPOSE_LOADOUT_PROXY), "the robber wears a pack for the kit");

		if (!RedProof())
			ctx.Equal(items.Count(), LF_BankRobKit.Grant(robber), "one grant hands over every kit item");

		IEntity carried = LF_HandTool.Carried(robber, LF_VaultDrill.TOOL_ID);
		ctx.NotNull(carried, "the granted drill is a carried hand tool");
		ctx.True(LF_Breach.HasCharge(robber), "the granted charge is carried");
		ctx.True(LF_InventoryUtils.GetAmount(robber, LF_Bank.KEYCARD_PREFAB) >= 1, "the granted keycard is carried");

		LF_TestCharacterFixture.Delete(robber);
	}
}
