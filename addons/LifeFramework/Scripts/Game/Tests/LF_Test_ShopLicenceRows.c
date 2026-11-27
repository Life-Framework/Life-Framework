// red-proof: with `RedProof()` on the row helpers are not asked, so the names stay empty and every index stays 99
// (a real row), failing the "name", "index" and "not a licence row" checks.

//! shop/licence-rows: the licence lines of a counter slip (LF_ShopPriceList.LicenceRowName and LicenceRowIndex). A licence
//! row is `Licence<n>Row`, it maps back to its index, and it is never mistaken for an item line (`Item<n>Row`) or the other
//! way round, so a click on a licence button cannot buy the item at the same index (and the reverse). A malformed name, a
//! padded number or a missing number is no row.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_ShopLicenceRows : LF_Test
{
	override string GetName()
	{
		return "shop/licence-rows";
	}

	override void Run(LF_TestContext ctx)
	{
		string name0 = "-";
		string name12 = "-";
		int index0 = 99;
		int index12 = 99;
		int itemAsLicence = 99;
		int licenceAsItem = 99;
		int paddedIndex = 99;
		int missingNumber = 99;
		int letterIndex = 99;
		int negativeIndex = 99;
		int noSuffix = 99;
		int empty = 99;
		int payKey = 99;
		string itemName = "-";
		if (!RedProof())
		{
			name0 = LF_ShopPriceList.LicenceRowName(0);
			name12 = LF_ShopPriceList.LicenceRowName(12);
			index0 = LF_ShopPriceList.LicenceRowIndex(name0);
			index12 = LF_ShopPriceList.LicenceRowIndex(name12);
			itemName = LF_ShopPriceList.RowName(3);
			itemAsLicence = LF_ShopPriceList.LicenceRowIndex(itemName);
			licenceAsItem = LF_ShopPriceList.RowIndex(LF_ShopPriceList.LicenceRowName(3));
			paddedIndex = LF_ShopPriceList.LicenceRowIndex("Licence03Row");
			missingNumber = LF_ShopPriceList.LicenceRowIndex("LicenceRow");
			letterIndex = LF_ShopPriceList.LicenceRowIndex("LicenceXRow");
			negativeIndex = LF_ShopPriceList.LicenceRowIndex("Licence-1Row");
			noSuffix = LF_ShopPriceList.LicenceRowIndex("Licence3");
			empty = LF_ShopPriceList.LicenceRowIndex("");
			payKey = LF_ShopPriceList.LicenceRowIndex(LF_ShopPriceList.PAY);
		}

		ctx.EqualStr("Licence0Row", name0, "the first licence row's name");
		ctx.EqualStr("Licence12Row", name12, "a two digit licence row's name");
		ctx.Equal(0, index0, "the first licence row maps back to index 0");
		ctx.Equal(12, index12, "a two digit licence row maps back to its index");
		ctx.EqualStr("Item3Row", itemName, "an item line keeps its own name");
		ctx.Equal(-1, itemAsLicence, "an item line is not a licence row");
		ctx.Equal(-1, licenceAsItem, "a licence row is not an item line");
		ctx.Equal(-1, paddedIndex, "a zero-padded number is not a row (it would be a second name for row 3)");
		ctx.Equal(-1, missingNumber, "a name with no number is not a row");
		ctx.Equal(-1, letterIndex, "a name with a letter for a number is not a row");
		ctx.Equal(-1, negativeIndex, "a negative number is not a row");
		ctx.Equal(-1, noSuffix, "a name without the Row suffix is not a row");
		ctx.Equal(-1, empty, "an empty name is not a row");
		ctx.Equal(-1, payKey, "the Pay key is not a licence row");
	}
}
