// red-proof: the hook skips the sale and the owner clear, so every "sold" assertion fails; on the code before this change (GetSign always the for-sale sign) the same assertions fail without the hook.
//! property/sold-sign-face (sold-sign-board): a house or plot sale sign printed "FOR SALE" for good, so a
//! bought cottage still read as for sale to a passer-by (playtest round 5). The sign now follows the
//! owned flag its sale component replicates: the face text and the Read card switch to the sold sign
//! (`house_sold`, `plot_sold`) when the house or plot has an owner and back when the owner is cleared.
//! The sign's own id stays `house_for_sale` / `plot_for_sale` (the place and quest id). A sign that comes
//! into the world after the sale (what a late joiner is sent) and a save load (a restart) start sold.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_SoldSignFace : LF_Test
{
	protected static const ResourceName HOUSE_SIGN = "{2F815241C89039BC}Prefabs/Props/Property/LF_HouseSaleSign.et";
	protected static const ResourceName PLOT_SIGN = "{DA9D4FEB9BD67EA5}Prefabs/Props/Property/LF_PlotSign.et";
	protected static const string HOUSE = "sale-house-a";
	protected static const string PLOT = "plot-a";
	protected static const string OWNER_UID = "lf-test-sold-sign-owner";

	override string GetName()
	{
		return "property/sold-sign-face";
	}

	//------------------------------------------------------------------------------------------------
	//! The owner change under test; the red proof skips it.
	protected void Sell(string propertyId, string ownerKey)
	{
		if (RedProof())
			return;

		LF_Properties.Create(propertyId);
		LF_Properties.SetOwner(propertyId, ownerKey);
	}

	//------------------------------------------------------------------------------------------------
	protected static string FaceId(IEntity sign)
	{
		LF_SignComponent face = LF_Component<LF_SignComponent>.Find(sign);
		if (!face || !face.GetSign())
			return "";

		return face.GetSign().m_sId;
	}

	//------------------------------------------------------------------------------------------------
	protected static string FaceText(IEntity sign)
	{
		LF_SignComponent face = LF_Component<LF_SignComponent>.Find(sign);
		if (!face)
			return "";

		return LF_SignComponent.FaceText(face.GetSign());
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_PropertyConfig.Reload();
		LF_Properties.ResetStatics();
		ctx.True(LF_PropertyConfig.HousePrice(HOUSE) > 0, "the example house has a price");
		ctx.True(LF_PropertyConfig.PlotPrice(PLOT) > 0, "the example plot has a price");

		IEntity viewer = LF_TestCharacterFixture.Spawn(ctx);
		IEntity house;
		IEntity plot;
		IEntity late;
		if (viewer)
		{
			house = LF_Utils.SpawnEntityPrefab(HOUSE_SIGN, viewer.GetOrigin() + "1 0 0");
			plot = LF_Utils.SpawnEntityPrefab(PLOT_SIGN, viewer.GetOrigin() + "-1 0 0");
		}

		ctx.NotNull(house, "the house sale sign spawns");
		ctx.NotNull(plot, "the plot sign spawns");
		LF_HouseSaleComponent houseSale;
		LF_PlotComponent plotSale;
		if (house && plot)
		{
			houseSale = LF_Component<LF_HouseSaleComponent>.Find(house);
			plotSale = LF_Component<LF_PlotComponent>.Find(plot);
		}

		if (!houseSale || !plotSale)
		{
			ctx.Fail("the signs carry their sale components");
			Cleanup(viewer, house, plot, late);
			return;
		}

		houseSale.SetHouseId(HOUSE);
		plotSale.SetPlotKey(PLOT);

		// Nobody owns either: both read For sale.
		ctx.EqualStr("house_for_sale", FaceId(house), "an unsold house sign is the for-sale sign");
		ctx.EqualStr("FOR SALE", FaceText(house), "and prints FOR SALE");
		ctx.EqualStr("plot_for_sale", FaceId(plot), "an unsold plot sign is the for-sale sign");

		// A sale: the house sign reads Sold, the plot sign is untouched.
		Sell(HOUSE, OWNER_UID);
		ctx.True(houseSale.IsOwned(), "the sale flagged the house sign owned");
		ctx.EqualStr("house_sold", FaceId(house), "a bought house's board is the sold sign");
		ctx.EqualStr("SOLD", FaceText(house), "and prints SOLD");
		ctx.EqualStr("plot_for_sale", FaceId(plot), "the plot sign is untouched by a house sale");
		LF_SignComponent houseFace = LF_Component<LF_SignComponent>.Find(house);
		ctx.EqualStr("house_for_sale", houseFace.GetSignId(), "the sign's own id stays the for-sale id (place and quest id)");
		ctx.True(houseFace.IsSold(), "the sign reports itself sold");

		// A client has no property book: the replicated flag alone keeps the sold face.
		LF_Properties.ResetStatics();
		ctx.EqualStr("", LF_Properties.GetOwnerKey(HOUSE), "the book is empty, as on a client");
		ctx.EqualStr("house_sold", FaceId(house), "the sold face reads the flag, not the book");

		// A plot sale reads Sold as well.
		Sell(PLOT, OWNER_UID);
		ctx.EqualStr("plot_sold", FaceId(plot), "a bought plot's sign is the sold sign");
		ctx.EqualStr("SOLD", FaceText(plot), "and prints SOLD");

		// A sign placed after the sale (a late joiner is sent the entity with the flag) starts sold.
		Sell(HOUSE, OWNER_UID);
		late = LF_Utils.SpawnEntityPrefab(HOUSE_SIGN, viewer.GetOrigin() + "0 0 1");
		LF_HouseSaleComponent lateSale;
		if (late)
			lateSale = LF_Component<LF_HouseSaleComponent>.Find(late);

		ctx.NotNull(lateSale, "a sign spawned after the sale carries the sale component");
		if (lateSale)
		{
			lateSale.SetHouseId(HOUSE);
			ctx.EqualStr("house_sold", FaceId(late), "a sign that comes into the world after the sale starts sold");
		}

		// A restart: the load replaces the book. A load without the owner clears the board, with it sets it.
		array<ref LF_PropertyRecord> rows = new array<ref LF_PropertyRecord>();
		LF_Properties.ApplyAll(rows);
		ctx.EqualStr("house_for_sale", FaceId(house), "a loaded save with no owner puts the house back on sale");
		LF_PropertyRecord row = LF_PropertyRecord.Create(HOUSE);
		row.m_sOwnerKey = OWNER_UID;
		rows.Insert(row);
		if (!RedProof())
			LF_Properties.ApplyAll(rows);

		ctx.EqualStr("house_sold", FaceId(house), "a loaded save with the owner prints Sold");

		// Clearing the owner (a wipe, an eviction) puts the sign back on sale.
		if (!RedProof())
			LF_Properties.SetOwner(HOUSE, "");

		ctx.EqualStr("house_for_sale", FaceId(house), "a cleared owner prints For sale again");

		LF_Properties.ResetStatics();
		Cleanup(viewer, house, plot, late);
	}

	//------------------------------------------------------------------------------------------------
	protected void Cleanup(IEntity viewer, IEntity house, IEntity plot, IEntity late)
	{
		LF_TestCharacterFixture.Delete(viewer);
		LF_TestCharacterFixture.Delete(house);
		LF_TestCharacterFixture.Delete(plot);
		LF_TestCharacterFixture.Delete(late);
	}
}
