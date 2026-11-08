// red-proof: Observed 2026-11-08 with `tools\cli test --no-build --tier all`: EL_TraderManagerComponent.CanAccessBlackMarket
// returning true for any account failed it with `[ELTEST] FAIL trader/sell-rules`,
// `- the black market refuses a police officer`. Restored; green.
// The previous trader/item-config and trader/processing-config tests only read back struct
// fields they had just set; they were deleted. Recipe validation is covered by
// actions/guards (EL_ProcessAction.IsRecipeConfigured).

// tier: LOGIC
class EL_Test_TraderSellRules : EL_Test
{
	protected static const ResourceName APPLE_PREFAB = "{C9D661E5B0714711}Prefabs/Items/Food/Apple.et";
	protected static const ResourceName IRON_NUGGET_PREFAB = "{6853E2E2AE878611}Prefabs/Resources/Mining/IronNugget.et";

	override string GetName()
	{
		return "trader/sell-rules";
	}

	override void Run(EL_TestContext ctx)
	{
		EL_PlayerAccountManager.Reset();

		// The sell path (modded SCR_InventoryStorageManagerComponent.InsertItem) refuses a
		// value of 0, so these are the accept/refuse decisions a player sees at the trader.
		ref array<ref EL_TraderItem> listed = {};
		EL_TraderItem apple = new EL_TraderItem();
		apple.m_ItemPrefab = APPLE_PREFAB;
		apple.m_ValuePerItem = 5;
		listed.Insert(apple);
		listed.Insert(null);

		ctx.Equal(5, EL_TraderManagerComponent.GetUnitValue(listed, APPLE_PREFAB), "a listed item sells at its configured value");
		ctx.Equal(0, EL_TraderManagerComponent.GetUnitValue(listed, IRON_NUGGET_PREFAB), "an unlisted item is refused (value 0)");
		ctx.Equal(0, EL_TraderManagerComponent.GetUnitValue(listed, ResourceName.Empty), "an empty prefab is refused");
		ctx.Equal(0, EL_TraderManagerComponent.GetUnitValue(null, APPLE_PREFAB), "a trader with no list buys nothing");

		EL_PlayerAccount civilian = EL_PlayerAccountManager.GetOrCreate("test-trader-civilian");
		EL_PlayerAccount police = EL_PlayerAccountManager.GetOrCreate("test-trader-police");
		ctx.NotNull(civilian, "civilian account exists");
		ctx.NotNull(police, "police account exists");
		if (ctx.FailureCount() > 0)
			return;

		police.SetFaction(EL_Faction.POLICE);

		ctx.True(EL_TraderManagerComponent.CanAccessBlackMarket(civilian), "the black market admits a civilian");
		ctx.False(EL_TraderManagerComponent.CanAccessBlackMarket(police), "the black market refuses a police officer");
		ctx.False(EL_TraderManagerComponent.CanAccessBlackMarket(null), "the black market refuses an unverified seller");
	}
};
