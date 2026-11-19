// red-proof: observed 2026-11-19 (#3172): `tools\cli test --no-build --red LF_Test_BuildDoorKit,LF_Test_EveronDoorKitStock` printed `RED-OK everon/door-kit-stock` (the hook probes with an empty prefab and fails `the door kit is in the hardware stock`); on main (no kit row) the same line fails.
//! #3172: the pack's hardware store sells the door kit the cottage's door stage asks for
//! (`LF_Property.conf`), at $250 with a visible buy price, never sold back for more than it costs.
//! The core half (the kit is a carriable item that counts at a build site) is `LF_Test_BuildDoorKit`.
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronDoorKitStock : LF_Test
{
	static const ResourceName DOOR_KIT = "{4E8B13D6A0C957F2}Prefabs/Items/Resources/LF_DoorKit.et";
	protected static const string STOCK = "hardware";
	protected static const int PRICE = 250;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/door-kit-stock";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_ShopManagerComponent manager = LF_ShopManagerComponent.GetInstance();
		ctx.NotNull(manager, "the shop manager exists");
		LF_ShopStock stock = LF_ShopStockConfig.Get().Find(STOCK);
		ctx.NotNull(stock, "the hardware store has a stock under the pack header");
		if (!manager || !stock)
			return;

		Resource resource = Resource.Load(DOOR_KIT);
		ctx.True(resource && resource.IsValid(), "the door kit prefab loads");

		ResourceName probe = DOOR_KIT;
		if (RedProof())
			probe = "";

		LF_ShopStockItem row = stock.FindItem(probe);
		ctx.NotNull(row, "the door kit is in the hardware stock");
		if (row)
			ctx.Equal(PRICE, row.m_iPrice, "the stock row prices the kit at $250");

		int buyPrice = 0;
		int sellPrice = 0;
		bool hidden = true;
		manager.GetPrices(DOOR_KIT, buyPrice, sellPrice, hidden);
		ctx.Equal(PRICE, buyPrice, "the shop charges $250 for the kit");
		ctx.False(hidden, "the kit is not hidden from the shop");
		ctx.True(buyPrice > sellPrice, string.Format("the kit costs %1 and sells back for %2: no profit loop", buyPrice, sellPrice));
	}
}
