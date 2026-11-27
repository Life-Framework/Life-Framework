// red-proof: with `RedProof()` on the save row reader and the price helpers are not asked, so every price keeps its wrong
// default and the old-row scale, new-row passthrough, damaged-value clamp, rounding and clamp checks fail.

//! market/row-fine: how a saved market price is read back (LF_MarketSavePayload.RowFine) and the unit helpers it leans on
//! (LF_PriceCurve.ToFine, ToCash, ClampPrice). A version 1 row holds whole cash and scales up to hundredths, a version 2 row
//! is already hundredths, and a damaged value (negative, or past the largest price) is held inside 0..the largest price
//! so the scaling can never overflow an int. ToCash rounds half up and ToFine holds cash to 0..MAX_CASH.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_MarketRowFine : LF_Test
{
	override string GetName()
	{
		return "market/row-fine";
	}

	override void Run(LF_TestContext ctx)
	{
		int oldRow = -1;
		int oldNegative = -1;
		int oldHuge = -1;
		int newRow = -1;
		int newNegative = -1;
		int newHuge = -1;
		int newZero = -1;
		int fineCash = -1;
		int fineNegative = -1;
		int fineHuge = -1;
		int cashExact = -1;
		int cashHalfUp = -1;
		int cashJustBelow = -1;
		int cashNegative = -1;
		int cashHuge = -1;
		int inside = -1;
		int below = -1;
		int above = -1;
		int atFloor = -1;
		int atCeiling = -1;
		int maxFine = LF_PriceCurve.MAX_CASH * LF_PriceCurve.PRICE_SCALE;
		if (!RedProof())
		{
			LF_MarketPriceRow row = LF_MarketPriceRow.Create("{C9D661E5B0714711}Prefabs/Items/Food/Apple.et", 1234, 0);
			row.m_iVersion = 1;
			oldRow = LF_MarketSavePayload.RowFine(row);
			row.m_iCurrent = -50;
			oldNegative = LF_MarketSavePayload.RowFine(row);
			row.m_iCurrent = 2000000000;
			oldHuge = LF_MarketSavePayload.RowFine(row);

			row.m_iVersion = LF_MarketPriceRow.ROW_VERSION;
			row.m_iCurrent = 1234;
			newRow = LF_MarketSavePayload.RowFine(row);
			row.m_iCurrent = -50;
			newNegative = LF_MarketSavePayload.RowFine(row);
			row.m_iCurrent = 2000000000;
			newHuge = LF_MarketSavePayload.RowFine(row);
			row.m_iCurrent = 0;
			newZero = LF_MarketSavePayload.RowFine(row);

			fineCash = LF_PriceCurve.ToFine(20);
			fineNegative = LF_PriceCurve.ToFine(-4);
			fineHuge = LF_PriceCurve.ToFine(2000000000);
			cashExact = LF_PriceCurve.ToCash(2000);
			cashHalfUp = LF_PriceCurve.ToCash(1250);
			cashJustBelow = LF_PriceCurve.ToCash(1249);
			cashNegative = LF_PriceCurve.ToCash(-300);
			cashHuge = LF_PriceCurve.ToCash(2000000000);
			inside = LF_PriceCurve.ClampPrice(500, 200, 4000);
			below = LF_PriceCurve.ClampPrice(50, 200, 4000);
			above = LF_PriceCurve.ClampPrice(9000, 200, 4000);
			atFloor = LF_PriceCurve.ClampPrice(200, 200, 4000);
			atCeiling = LF_PriceCurve.ClampPrice(4000, 200, 4000);
		}

		ctx.Equal(123400, oldRow, "a version 1 row holds whole cash: 1234 cash is 123400 hundredths");
		ctx.Equal(0, oldNegative, "a damaged version 1 row below zero reads as 0, never a negative price");
		ctx.Equal(maxFine, oldHuge, "a damaged version 1 row past the largest price reads as the largest price (no int overflow)");
		ctx.Equal(1234, newRow, "a version 2 row is already hundredths and is read as it is");
		ctx.Equal(0, newNegative, "a damaged version 2 row below zero reads as 0");
		ctx.Equal(maxFine, newHuge, "a damaged version 2 row past the largest price is held at the largest price");
		ctx.Equal(0, newZero, "a free good stays free");

		ctx.Equal(2000, fineCash, "20 cash is 2000 hundredths");
		ctx.Equal(0, fineNegative, "a negative cash price converts to 0");
		ctx.Equal(maxFine, fineHuge, "a cash price past the largest is held at the largest");
		ctx.Equal(20, cashExact, "2000 hundredths is 20 cash");
		ctx.Equal(13, cashHalfUp, "12.50 rounds half up to 13 cash");
		ctx.Equal(12, cashJustBelow, "12.49 rounds down to 12 cash");
		ctx.Equal(0, cashNegative, "a negative price shows as 0 cash");
		ctx.Equal(LF_PriceCurve.MAX_CASH, cashHuge, "a price past the largest shows as the largest cash price");

		ctx.Equal(500, inside, "a price inside floor and ceiling is unchanged");
		ctx.Equal(200, below, "a price under the floor is raised to it");
		ctx.Equal(4000, above, "a price over the ceiling is cut to it");
		ctx.Equal(200, atFloor, "a price on the floor stays");
		ctx.Equal(4000, atCeiling, "a price on the ceiling stays");
	}
}
