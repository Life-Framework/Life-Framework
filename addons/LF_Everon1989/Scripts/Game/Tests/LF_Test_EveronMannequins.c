// red-proof: pending (tools\cli test --no-build --red LF_Test_EveronMannequins)
//! everon/mannequins: every dressed mannequin in the pack sells what it wears. Each prefab carries an
//! outlet shop and an outfit list; every id on the list is a priced catalogue row that outlet sells,
//! and each mannequin offers the whole-outfit line. Reads the prefabs' sources, spawns nothing.
//! Lives in the pack because it reads the pack's prefabs and catalogue (pack leg).
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronMannequins : LF_Test
{
	//------------------------------------------------------------------------------------------------
	protected static array<ResourceName> Mannequins()
	{
		return {
			"{F55D2611E095EBAB}Prefabs/Props/Clothing/LF_Mannequin_Denim.et",
			"{88F59F323FF25661}Prefabs/Props/Clothing/LF_Mannequin_Suit.et",
			"{4DC523B95D7DFD0F}Prefabs/Props/Clothing/LF_Mannequin_Knit.et",
			"{918EEED162D663F2}Prefabs/Props/Clothing/LF_Mannequin_Raincoat.et",
			"{F0BD498E7E7D7C6D}Prefabs/Props/Clothing/LF_Mannequin_Backpack.et"
		};
	}

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/mannequins";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_ItemCatalogueConfig catalogue = LF_ItemCatalogueConfig.Get();
		ctx.True(catalogue != null, "the catalogue loads");
		if (!catalogue)
			return;

		foreach (ResourceName path : Mannequins())
		{
			Resource resource = Resource.Load(path);
			ctx.True(resource && resource.IsValid(), "mannequin prefab loads: " + path);
			if (!resource || !resource.IsValid())
				continue;

			IEntityComponentSource shopSource = SCR_BaseContainerTools.FindComponentSource(resource, LF_ShopComponent);
			IEntityComponentSource outfitSource = SCR_BaseContainerTools.FindComponentSource(resource, LF_MannequinOutfitComponent);
			ctx.True(shopSource != null, "mannequin has an outlet shop: " + path);
			ctx.True(outfitSource != null, "mannequin has an outfit list: " + path);
			if (!shopSource || !outfitSource)
				continue;

			string outlet;
			shopSource.Get("m_sOutletId", outlet);
			array<string> ids = {};
			outfitSource.Get("m_aItemIds", ids);
			if (RedProof())
				ids.Insert("no_such_piece");

			ctx.True(ids.Count() >= 2, "mannequin wears two pieces or more: " + path);
			for (int slot = 0; slot < ids.Count(); slot++)
			{
				ctx.True(LF_MannequinOutfit.Piece(catalogue, outlet, ids, slot, 0) != null, string.Format("%1 sells %2 at %3", path, ids[slot], outlet));
			}

			ctx.True(LF_MannequinOutfit.HasOutfit(catalogue, outlet, ids), "mannequin offers the whole outfit: " + path);
		}
	}
}
