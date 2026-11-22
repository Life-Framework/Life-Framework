// red-proof: pending (tools\cli test --no-build --red LF_Test_MannequinOutfit)
//! shop/mannequin-outfit: a dressed mannequin's action list. Each worn piece sells as its catalogue
//! row at the mannequin's outlet (id, prefab, price); an id the outlet does not sell is no piece; the
//! whole outfit costs the sum of the pieces; a sold-out piece (outlet stock) greys that piece and the
//! outfit; and each refusal or delivery has its toast line.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_MannequinOutfit : LF_Test
{
	protected static const string OUTLET = "test_mannequin_rack";
	protected static const ResourceName JACKET = "{27162F1F6EE6581A}Prefabs/Characters/Uniforms/Jacket_Denim_01/Jacket_Denim_01_brownShirt.et";
	protected static const ResourceName TROUSERS = "{6925E6C63EBCF623}Prefabs/Characters/Uniforms/Pants_Trousers_01/Pants_Trousers_01_DarkBrown.et";
	protected static const ResourceName HAT = "{A22C976003AA5BA3}Prefabs/Characters/HeadGear/Hat_Ushanka_01/Hat_Ushanka_01_folded.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "shop/mannequin-outfit";
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_OutletOffer Offer(string outlet, string item, int stock)
	{
		LF_OutletOffer offer = new LF_OutletOffer();
		offer.m_sOutlet = outlet;
		offer.m_sItem = item;
		offer.m_iStock = stock;
		return offer;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_ItemCatalogueConfig config = new LF_ItemCatalogueConfig();
		config.m_aItems = {};
		config.m_aOffers = {};
		config.m_aItems.Insert(LF_CatalogueItem.Make("test_mq_jacket", JACKET, "clothing", 45, 20, "legal"));
		config.m_aItems.Insert(LF_CatalogueItem.Make("test_mq_trousers", TROUSERS, "clothing", 35, 14, "legal"));
		config.m_aItems.Insert(LF_CatalogueItem.Make("test_mq_hat", HAT, "clothing", 20, 8, "legal"));
		config.m_aOffers.Insert(Offer(OUTLET, "test_mq_jacket", 0));
		config.m_aOffers.Insert(Offer(OUTLET, "test_mq_trousers", 0));
		// The hat is stocked: one left on the shelf.
		config.m_aOffers.Insert(Offer(OUTLET, "test_mq_hat", 1));
		// The other counter's piece is not sold at this mannequin's outlet.
		config.m_aItems.Insert(LF_CatalogueItem.Make("test_mq_boots", HAT, "clothing", 40, 16, "legal"));
		config.m_aOffers.Insert(Offer("test_mannequin_other", "test_mq_boots", 0));

		array<string> ids = {"test_mq_jacket", "test_mq_trousers", "test_mq_hat", "test_mq_ghost", "test_mq_boots"};

		// Each slot reads its own catalogue row.
		LF_MannequinPiece jacket = LF_MannequinOutfit.Piece(config, OUTLET, ids, 0, 0);
		ctx.True(jacket != null, "slot 0 sells the jacket");
		if (jacket)
		{
			ctx.EqualStr("test_mq_jacket", jacket.m_sId, "slot 0 is the jacket row");
			ctx.Equal(45, jacket.m_iPrice, "the jacket costs its catalogue price");
			ctx.True(LF_Utils.SamePrefab(JACKET, jacket.m_Prefab), "the jacket sells as its catalogue prefab");
			ctx.False(jacket.m_bSoldOut, "the jacket is in stock");
		}

		ctx.True(LF_MannequinOutfit.Piece(config, OUTLET, ids, 3, 0) == null, "an id not in the catalogue is no piece (slot hidden)");
		ctx.True(LF_MannequinOutfit.Piece(config, OUTLET, ids, 4, 0) == null, "a piece another outlet sells is no piece here");
		ctx.True(LF_MannequinOutfit.Piece(config, OUTLET, ids, 9, 0) == null, "a slot past the list is empty");
		ctx.True(LF_MannequinOutfit.Piece(config, OUTLET, ids, -1, 0) == null, "a negative slot is empty");

		// The whole outfit is the sellable pieces at the sum of their prices.
		array<ref LF_MannequinPiece> pieces = {};
		LF_MannequinOutfit.Pieces(config, OUTLET, ids, 0, pieces);
		ctx.Equal(3, pieces.Count(), "three sellable pieces");
		ctx.Equal(100, LF_MannequinOutfit.OutfitTotal(config, OUTLET, ids), "the outfit costs 45 + 35 + 20");
		ctx.True(LF_MannequinOutfit.HasOutfit(config, OUTLET, ids), "three pieces make an outfit line");
		array<string> single = {"test_mq_jacket", "test_mq_ghost"};
		ctx.False(LF_MannequinOutfit.HasOutfit(config, OUTLET, single), "one sellable piece offers no outfit line");

		// Sold out: the outlet's stock runs out, the server's mask marks the slot, the client greys it.
		int nowMs = 1000;
		ctx.Equal(0, LF_MannequinOutfit.SoldOutMask(config, OUTLET, ids, nowMs), "nothing sold out while the hat is on the shelf");
		if (!RedProof())
			LF_CatalogueStock.Take(config.FindOffer(OUTLET, "test_mq_hat"), 1, nowMs);
		int mask = LF_MannequinOutfit.SoldOutMask(config, OUTLET, ids, nowMs);
		ctx.Equal(1 << 2, mask, "the last hat sold marks slot 2 sold out");
		LF_MannequinPiece hat = LF_MannequinOutfit.Piece(config, OUTLET, ids, 2, mask);
		ctx.True(hat && hat.m_bSoldOut, "the hat reads sold out");
		ctx.False(LF_MannequinOutfit.Piece(config, OUTLET, ids, 0, mask).m_bSoldOut, "the jacket is still for sale");
		ctx.True(LF_MannequinOutfit.OutfitSoldOut(config, OUTLET, ids, mask), "a sold-out piece greys the whole outfit");
		ctx.False(LF_MannequinOutfit.OutfitSoldOut(config, OUTLET, ids, 0), "an outfit with every piece is for sale");
		ctx.Equal(100, LF_MannequinOutfit.OutfitTotal(config, OUTLET, ids), "a sold-out piece keeps its place in the price");

		// Toast lines: a refusal says why, a delivery says where the goods went.
		ctx.EqualStr("#LF-Shop_RefuseCash", LF_MannequinBuyAction.PieceMessage(LF_EShopRefusal.NOT_ENOUGH_CASH, 0), "short cash names the cash");
		ctx.EqualStr("#LF-Shop_RefuseNotSold", LF_MannequinBuyAction.PieceMessage(LF_EShopRefusal.NOT_SOLD_HERE, 0), "an unsold piece says so");
		ctx.EqualStr("#LF-Mannequin_Bought", LF_MannequinBuyAction.PieceMessage(LF_EShopRefusal.NONE, 0), "a piece in the pockets");
		ctx.EqualStr("#LF-Mannequin_BoughtDropped", LF_MannequinBuyAction.PieceMessage(LF_EShopRefusal.NONE, 1), "a piece at the feet");
		ctx.EqualStr(LF_ShopRefusal.Key(LF_EShopRefusal.OUT_OF_STOCK), LF_MannequinBuyAction.OutfitMessage(LF_EShopRefusal.OUT_OF_STOCK, 0, 0), "a sold-out outfit buys nothing and says why");
		ctx.EqualStr("#LF-Mannequin_BoughtPart", LF_MannequinBuyAction.OutfitMessage(LF_EShopRefusal.NEEDS_LICENCE, 2, 0), "an outfit stopped part way says part was bought");
		ctx.EqualStr("#LF-Mannequin_BoughtOutfit", LF_MannequinBuyAction.OutfitMessage(LF_EShopRefusal.NONE, 3, 0), "the whole outfit in the pockets");
		ctx.EqualStr("#LF-Mannequin_BoughtOutfitDropped", LF_MannequinBuyAction.OutfitMessage(LF_EShopRefusal.NONE, 3, 1), "the whole outfit, some at the feet");
	}
}
