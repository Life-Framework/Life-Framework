// red-proof: 2026-11-10 shares LF_ChainSearch.IsGood with LF_Test_ChainSearch; dropping the m_bContraband check made the flagged-goods count wrong (see that red-proof); restored; green.
// tier: WORLD
//! Officer search for chain goods (#419), against a real inventory: the contraband crate in a
//! character's inventory is collected as flagged goods, an ordinary item is not. Deletes what it spawns.
class LF_Test_ChainSearchWorld : LF_Test
{
	override string GetName()
	{
		return "chains/search-world";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_ChainCatalog.ResetStatics();
		LF_ChainCatalog.Build(LF_ResourceChainCatalog.GetConfig());

		IEntity character = LF_TestCharacterFixture.Spawn(ctx);
		if (!character)
			return;

		int units;
		int load;
		array<IEntity> goods = {};
		LF_ChainSearch.Collect(character, goods, units, load);
		ctx.Equal(0, goods.Count(), "a character carrying nothing flagged has no goods");

		ctx.Equal(1, LF_InventoryUtils.AddAmount(character, "{C9D661E5B0714711}Prefabs/Items/Food/Apple.et", 1), "an apple lands in the inventory");
		LF_ChainSearch.Collect(character, goods, units, load);
		ctx.Equal(0, goods.Count(), "an ordinary item is not flagged");

		ctx.Equal(1, LF_InventoryUtils.AddAmount(character, LF_Contraband.ITEM, 1), "the contraband crate lands in the inventory");
		LF_ChainSearch.Collect(character, goods, units, load);
		ctx.Equal(1, goods.Count(), "the crate is found as chain goods");
		ctx.True(load >= 1, "the crate has weight");

		LF_TestCharacterFixture.Delete(character);
		LF_ChainCatalog.ResetStatics();
	}
}
