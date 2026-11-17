// red-proof: 2026-11-16 `tools\cli test --no-build --red LF_Test_PricedSignProvider`: RED-OK signs/priced-provider (fails with the hook on, 6 assertions: no answer from the provider). Green without it (--only, with LF_Test_SignPrice and LF_Test_BoardProvider).
// tier: WORLD
//! The priced sign reads through the board provider (#1946, chapter #1014): the answer for a lot's
//! car carries the offer's name and price, the sign built from the answer shows the same title the
//! price list gives (name and grouped price), a changed price changes the answer on the next read,
//! and the kind is registered but not readable over the network (it names no read action).
class LF_Test_PricedSignProvider : LF_Test
{
	override string GetName()
	{
		return "signs/priced-provider";
	}

	override void Run(LF_TestContext ctx)
	{
		// The grid's car lot (flagged m_bTestCarLot) stands for the priced signs' lot in any world.
		LF_VehicleLotComponent lot = LF_VehicleLotComponent.FindTestCarLot();
		ctx.NotNull(lot, "the world places a car lot flagged m_bTestCarLot (GridWorld: Grid_orgs_vehicles_Lot)");
		if (!lot)
			return;

		string lotId = lot.GetShopId();
		LF_VehicleOffer offer = lot.GetOfferAt(0);
		ctx.NotNull(offer, "the lot sells a car the sign can show");
		if (!offer)
			return;

		ResourceName car = offer.GetPrefab();

		// The kind is registered, and a client cannot ask the server for it.
		LF_BoardProvider provider = LF_BoardProviders.Find(LF_PricedSignProvider.KIND);
		ctx.True(provider != null, "the priced sign registers as a board kind");
		if (provider)
			ctx.True(provider.GetReadAction().IsEmpty(), "the priced kind names no read action, so no client can ask for it");

		// The read: the offer's name and price, as the old direct read gave them.
		LF_BoardAnswer answer;
		if (!RedProof())
			answer = LF_PricedSignProvider.AnswerFor(lotId, car);
		ctx.NotNull(answer, "the provider answers for a car the lot sells");
		if (!answer)
			return;

		ctx.Equal(1, answer.m_aNumbers.Count(), "one price in the answer");
		ctx.Equal(offer.GetPrice(), answer.m_aNumbers[0], "the answer carries the offer's price");
		ctx.EqualStr(offer.GetNameKey(), answer.m_aTexts[0], "the answer carries the offer's name");
		ctx.EqualStr(LF_SignCatalog.PricedTitle("%1 %2", offer.GetNameKey(), offer.GetPrice()), LF_PricedSignProvider.Title("%1 %2", answer), "the answer fills the title the way the sign always did");

		array<ref LF_UIPart> parts = LF_BoardProviders.Paint(LF_PricedSignProvider.KIND, answer);
		ctx.Equal(1, parts.Count(), "the answer paints one line");
		if (parts.Count() == 1)
			ctx.EqualStr(LF_PricedSignProvider.Title("%1 %2", answer), parts[0].GetText(), "the painted line is the title");

		// Nothing is cached: a second read is a new answer.
		ctx.True(LF_PricedSignProvider.AnswerFor(lotId, car) != answer, "a second read builds a new answer");

		// The sign shows the price list's number, through the provider: a price board's catalog entry
		// (a title format), filled from the answer, reads exactly what the direct read gave.
		LF_SignDef def = LF_SignCatalog.Find("lot_price_uaz469");
		ctx.NotNull(def, "the price board has its catalog entry");
		if (!def)
			return;

		LF_SignDef shown = LF_SignComponent.PricedFrom(def, answer);
		ctx.EqualStr(LF_SignCatalog.PricedTitle(def.m_sTitle, offer.GetNameKey(), offer.GetPrice()), shown.m_sTitle, "the board shows the price list's name and price");
		ctx.True(shown.m_sTitle != def.m_sTitle, "the shown title is filled, not the bare format");
		ctx.EqualStr(def.m_sId, shown.m_sId, "the shown sign is still the board's own entry");
	}
}
