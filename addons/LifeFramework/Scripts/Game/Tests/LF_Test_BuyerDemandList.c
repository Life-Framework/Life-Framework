// red-proof: with `RedProof()` on no row is added or replaced, so the counts, the replaced row and the ids fail.

//! market/buyer-demand-list: the buyer-demand rows as the shared config merge sees them (LF_BuyerDemandList). A row's
//! id is the buyer plus the good's GUID, so the same good under two buyers is two rows; Set inserts past the end and
//! replaces inside it; a list over a config with no rows array makes one.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_BuyerDemandList : LF_Test
{
	protected static const ResourceName GOOD_A = "{AAAAAAAAAAAAAAAA}Prefabs/Items/test_a.et";
	protected static const ResourceName GOOD_B = "{BBBBBBBBBBBBBBBB}Prefabs/Items/test_b.et";

	override string GetName()
	{
		return "market/buyer-demand-list";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_BuyerDemandConfig config = new LF_BuyerDemandConfig();
		LF_BuyerDemandList list = new LF_BuyerDemandList(config);
		ctx.NotNull(config.m_aRows, "a list over a config with no rows array makes one");
		ctx.Equal(0, list.Count(), "a fresh list is empty");
		ctx.EqualStr("buyerDemand.rows", list.GetName(), "the list is named for the merge");

		LF_BuyerDemandRow smithA = LF_BuyerDemandRow.Make("smith", GOOD_A, 10, 2, 50);
		LF_BuyerDemandRow smithB = LF_BuyerDemandRow.Make("smith", GOOD_B, 10, 2, 60);
		LF_BuyerDemandRow baker = LF_BuyerDemandRow.Make("baker", GOOD_A, 5, 1, 40);
		LF_BuyerDemandRow smithAAgain = LF_BuyerDemandRow.Make("smith", GOOD_A, 20, 4, 55);

		if (!RedProof())
		{
			list.Set(0, smithA);
			list.Set(1, smithB);
			list.Set(2, baker);
		}

		ctx.Equal(3, list.Count(), "Set past the end inserts a row");
		if (list.Count() == 3)
		{
			ctx.True(list.Get(0) == smithA, "the first row is where it was put");
			ctx.True(list.Get(2) == baker, "the third row is where it was put");
		}

		string smithAId = LF_BuyerDemandList.RowId("smith", GOOD_A);
		string bakerAId = LF_BuyerDemandList.RowId("baker", GOOD_A);
		string smithBId = LF_BuyerDemandList.RowId("smith", GOOD_B);
		ctx.True(smithAId != bakerAId, "the same good under two buyers is two rows");
		ctx.True(smithAId != smithBId, "two goods under one buyer are two rows");
		ctx.True(smithAId.StartsWith("smith|"), "an id starts with its buyer");
		ctx.EqualStr(smithAId, list.IdOf(smithA), "a row's id is its buyer and good");
		ctx.EqualStr(bakerAId, list.IdOf(baker), "the baker's row has the baker's id");

		if (!RedProof())
			list.Set(0, smithAAgain);

		ctx.Equal(3, list.Count(), "Set inside the list replaces and does not grow it");
		if (list.Count() == 3)
			ctx.True(list.Get(0) == smithAAgain, "the replacing row sits in the old row's place");
		ctx.EqualStr(smithAId, list.IdOf(smithAAgain), "the replacing row keeps the id of the row it replaced");
	}
}
