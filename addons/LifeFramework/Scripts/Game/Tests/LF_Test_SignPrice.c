// red-proof: observed 2026-11-10 with `tools\cli test --no-build --only LF_Test_SignPrice`: LF_SignCatalog.PricedTitle ignoring its price argument failed `- a changed price changes the board (expected 'UAZ $3,100', got 'UAZ $2,500')`; restored, green.

// tier: LOGIC
//! Price boards at the car lot read the lot's price list (#540): the title is a template filled from
//! the offer, so a changed price in config is a changed board. No world, no entity.
class LF_Test_SignPrice : LF_Test
{
	override string GetName()
	{
		return "signs/price-board";
	}

	override void Run(LF_TestContext ctx)
	{
		ResourceName van = "{543799AC5C52989C}Prefabs/Vehicles/Wheeled/S1203/S1203_transport_beige.et";
		ResourceName uaz = "{C8B16ABCA57679A2}Prefabs/Vehicles/Wheeled/UAZ469/UAZ469_covered_CIV_blue.et";
		array<ref LF_VehicleOffer> offers = {};
		offers.Insert(LF_VehicleOffer.Make(van, 4000, "Van"));
		offers.Insert(LF_VehicleOffer.Make(uaz, 2500, "UAZ"));

		LF_VehicleOffer offer = LF_VehicleLotComponent.OfferIn(offers, uaz);
		ctx.True(offer != null, "the lot finds the offer for the board's car");
		if (!offer)
			return;

		ctx.Equal(2500, offer.GetPrice(), "the board reads the offer's price");
		ctx.True(LF_VehicleLotComponent.OfferIn(offers, "{0000000000000000}Prefabs/Nope.et") == null, "a car the lot does not sell has no board price");
		ctx.True(LF_VehicleLotComponent.OfferIn(null, uaz) == null, "no offers, no price");

		ctx.EqualStr("UAZ $2,500", LF_SignCatalog.PricedTitle("%1 %2", offer.GetNameKey(), offer.GetPrice()), "the board title carries the name and the grouped price");

		// The server owner changes the price in config: the same board now reads the new number.
		LF_VehicleOffer repriced = LF_VehicleOffer.Make(uaz, 3100, "UAZ");
		ctx.EqualStr("UAZ $3,100", LF_SignCatalog.PricedTitle("%1 %2", repriced.GetNameKey(), repriced.GetPrice()), "a changed price changes the board");

		LF_SignDef def = new LF_SignDef();
		def.m_sId = "lot_price_test";
		def.m_sTitle = "%1 %2";
		def.m_sWhat = "what";
		LF_SignDef priced = LF_SignCatalog.WithTitle(def, "UAZ $3,100");
		ctx.EqualStr("UAZ $3,100", priced.m_sTitle, "the priced copy has the new title");
		ctx.EqualStr("%1 %2", def.m_sTitle, "the config sign is left as written");
		ctx.EqualStr("what", priced.m_sWhat, "the priced copy keeps the other sections");
	}
}
