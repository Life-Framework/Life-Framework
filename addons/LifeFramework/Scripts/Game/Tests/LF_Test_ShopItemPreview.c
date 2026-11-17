// red-proof: LF_Test_ShopItemPreview run before LF_ShopItemPreview.Resolve existed failed to compile (the resolver was
// missing), so the whole suite failed; with the `if (!RedProof())` guards below the act is skipped and every
// assertion fails. See the `--red` RED-OK line in the PR.
//------------------------------------------------------------------------------------------------
//! The order slip's preview resolver: the picked line's prefab for a catalogue line, nothing for a
//! line without one, and no throw for a row that is gone (a filter refill, a bad index, a null entry).
// tier: LOGIC
class LF_Test_ShopItemPreview : LF_Test
{
	static const string APPLE = "{C9D661E5B0714711}Prefabs/Items/Food/Apple.et";

	override string GetName()
	{
		return "shop/item-preview";
	}

	override void Run(LF_TestContext ctx)
	{
		array<ref LF_ShopCatalogItem> shown = {};
		LF_ShopCatalogItem apple = new LF_ShopCatalogItem();
		apple.m_ItemPrefab = APPLE;
		apple.m_iBuyPrice = 5;
		shown.Insert(apple);

		LF_ShopCatalogItem bare = new LF_ShopCatalogItem();
		bare.m_iBuyPrice = 1;
		shown.Insert(bare);
		shown.Insert(null);

		ResourceName picked;
		if (!RedProof())
			picked = LF_ShopItemPreview.Resolve(shown, 0);
		ctx.EqualStr(APPLE, picked, "a catalogue line resolves to its prefab");

		ctx.True(LF_ShopItemPreview.Resolve(shown, 1).IsEmpty(), "a line without a prefab resolves to nothing");
		ctx.True(LF_ShopItemPreview.Resolve(shown, 2).IsEmpty(), "a null entry resolves to nothing");
		ctx.True(LF_ShopItemPreview.Resolve(shown, -1).IsEmpty(), "a negative index resolves to nothing");
		ctx.True(LF_ShopItemPreview.Resolve(shown, 3).IsEmpty(), "an index past the list resolves to nothing");
		ctx.True(LF_ShopItemPreview.Resolve(null, 0).IsEmpty(), "no list resolves to nothing");

		shown.Clear();
		ctx.True(LF_ShopItemPreview.Resolve(shown, 0).IsEmpty(), "a removed row resolves to nothing");
	}
};
