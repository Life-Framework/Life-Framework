// red-proof: Observed 2026-11-19 with `--no-build --red` (batched with the persist test): `RED-OK harvest/implement-rack (33 assertions)`, the counter purchase skipped so nobody owns a reaper.
//! harvest/implement-rack (#3295): the in-session path. The counter sale puts a reaper on the buyer's owned list
//! (once, for $1,200), the rack's take-out hitches it to a hitch-list vehicle within reach, a second take-out
//! and a take-out by a character who owns none are refused, put-back needs the towing vehicle at the rack, dumps
//! the hopper and unhitches, and the reaper can go out again. The test deletes what it spawns.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_ImplementRack : LF_Test
{
	protected static const ResourceName UAZ = "{C8B16ABCA57679A2}Prefabs/Vehicles/Wheeled/UAZ469/UAZ469_covered_CIV_blue.et";
	protected static const ResourceName RACK = "{21B9456F3B79410E}Prefabs/Harvest/LF_ImplementRack.et";
	protected static const ResourceName REAPER = "{681247C1216EB815}Prefabs/Harvest/LF_Reaper_01.et";
	protected static const vector SPOT = "110 0 40";
	protected static const string KEY = "test-implement-owner";
	protected static const string STRANGER = "test-implement-stranger";
	protected static const int PRICE = 1200;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "harvest/implement-rack";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_OwnedImplements.ResetStatics();
		IEntity uaz = SpawnAt(UAZ, SPOT);
		IEntity rack = SpawnAt(RACK, SPOT + "3 0 0");
		IEntity buyer = LF_TestCharacterFixture.Spawn(ctx);
		ctx.NotNull(uaz, "a UAZ469 spawns");
		ctx.NotNull(rack, "the implement rack prefab spawns");
		if (uaz && rack && buyer)
		{
			ctx.NotNull(LF_Component<ActionsManagerComponent>.Find(rack), "the rack carries an actions manager (take out, put back)");
			Check(ctx, uaz, rack, buyer);
		}

		LF_Utils.SetUidForTests(null, "");
		LF_OwnedImplements.ResetStatics();
		if (buyer)
			LF_TestCharacterFixture.Delete(buyer);

		if (rack)
			SCR_EntityHelper.DeleteEntityAndChildren(rack);

		if (uaz)
			SCR_EntityHelper.DeleteEntityAndChildren(uaz);
	}

	//------------------------------------------------------------------------------------------------
	protected void Check(LF_TestContext ctx, IEntity uaz, IEntity rack, IEntity buyer)
	{
		vector at = rack.GetOrigin();
		ctx.True(LF_OwnedImplements.IdForPrefab(REAPER) == LF_OwnedImplements.ID_REAPER, "the reaper prefab is the 'reaper' implement");

		// Buy: the till hands an implement offer to the owned list, not the inventory.
		LF_Utils.SetUidForTests(buyer, KEY);
		ctx.Equal(PRICE, LF_MoneyUtils.AddCash(buyer, PRICE), "the buyer is seeded with the price");
		int charged;
		LF_EShopRefusal sale = LF_EShopRefusal.NO_SHOP;
		if (!RedProof())
			sale = LF_OwnedImplements.ShopBuy(REAPER, buyer, PRICE, 1, charged);

		ctx.True(sale == LF_EShopRefusal.NONE, "the counter sells the reaper");
		ctx.Equal(PRICE, charged, "the reaper cost $1,200");
		ctx.Equal(0, LF_MoneyUtils.GetCash(buyer), "the cash is taken");
		ctx.True(LF_OwnedImplements.Owns(KEY, LF_OwnedImplements.ID_REAPER), "the reaper is on the buyer's owned list");
		ctx.False(LF_TestCharacterFixture.HasItem(buyer, REAPER), "nothing went into the inventory");

		ctx.True(LF_OwnedImplements.ShopBuy(REAPER, buyer, PRICE, 1, charged) == LF_EShopRefusal.ALREADY_OWNED, "a second reaper is refused");
		int broke;
		ctx.True(LF_OwnedImplements.Purchase(REAPER, buyer, STRANGER, PRICE, 1, broke) == LF_EImplementResult.NO_CASH, "a buyer who cannot pay is refused");
		ctx.False(LF_OwnedImplements.Owns(STRANGER, LF_OwnedImplements.ID_REAPER), "the refused buyer owns nothing");

		// Take out: hitched to the vehicle beside the rack.
		IEntity reaper;
		ctx.True(LF_OwnedImplements.TakeOut(STRANGER, LF_OwnedImplements.ID_REAPER, at, reaper) == LF_EImplementResult.NOT_OWNED, "a take-out by a character who owns none is refused");
		ctx.True(LF_ImplementHitch.FindHitched(uaz) == null, "the refused take-out hitched nothing");
		ctx.True(LF_OwnedImplements.TakeOut(KEY, LF_OwnedImplements.ID_REAPER, at, reaper) == LF_EImplementResult.OK, "the owner takes the reaper out");
		ctx.True(reaper != null && reaper.GetParent() == uaz, "the reaper hangs off the UAZ469 within reach");
		IEntity again;
		ctx.True(LF_OwnedImplements.TakeOut(KEY, LF_OwnedImplements.ID_REAPER, at, again) == LF_EImplementResult.ALREADY_OUT, "a second take-out while it is out is refused");

		// Put back: the vehicle must be at the rack; the hopper is dumped.
		LF_ImplementComponent component = LF_ImplementComponent.Of(reaper);
		ctx.NotNull(component, "the reaper carries LF_ImplementComponent");
		if (component)
			component.SetHopperKg(120);

		int dumped;
		ctx.True(LF_OwnedImplements.PutBack(KEY, LF_OwnedImplements.ID_REAPER, at + "40 0 0", dumped) == LF_EImplementResult.VEHICLE_FAR, "put-back with the vehicle away from the rack is refused");
		ctx.True(LF_ImplementHitch.FindHitched(uaz) == reaper, "the refused put-back left the reaper hitched");
		ctx.True(LF_OwnedImplements.PutBack(KEY, LF_OwnedImplements.ID_REAPER, at, dumped) == LF_EImplementResult.OK, "the reaper is put back at the rack");
		ctx.Equal(120, dumped, "the hopper's grain was dumped, and the warning names how much");
		ctx.True(LF_ImplementHitch.FindHitched(uaz) == null, "the vehicle tows nothing after the put-back");
		ctx.True(LF_OwnedImplements.PutBack(KEY, LF_OwnedImplements.ID_REAPER, at, dumped) == LF_EImplementResult.NOT_OUT, "a second put-back is refused");
		ctx.True(LF_OwnedImplements.Owns(KEY, LF_OwnedImplements.ID_REAPER), "the owner still owns the reaper");

		// Out again, and the toast keys for the refusals exist.
		ctx.True(LF_OwnedImplements.TakeOut(KEY, LF_OwnedImplements.ID_REAPER, at, again) == LF_EImplementResult.OK, "the reaper can go out again");
		ctx.True(LF_ImplementRackAction.RefusalKey(LF_EImplementResult.NOT_OWNED) == "#LF-Harvest_RackNotOwned", "a refused take-out has its own toast line");
		LF_OwnedImplements.PutBack(KEY, LF_OwnedImplements.ID_REAPER, at, dumped);
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity SpawnAt(ResourceName prefab, vector at)
	{
		at[1] = GetGame().GetWorld().GetSurfaceY(at[0], at[2]);
		return LF_Utils.SpawnEntityPrefab(prefab, at);
	}
}
