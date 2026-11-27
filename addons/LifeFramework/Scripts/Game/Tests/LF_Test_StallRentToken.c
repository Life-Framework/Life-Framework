// red-proof: with `RedProof()` on the rent book is not asked, so the tokens stay empty and the count stays -1, which fails
// the token, refusal, same-rent, release and count checks.

//! market/stall-rent-token: the rent book behind the restart refund (LF_StallRentBook). A rent is named by holder, market
//! day and the unix second it was paid, so the same pitch rented on two days is two rents and the same rent recorded twice
//! is one; a pitch with no holder or a negative fee is refused (an empty token, nothing held); releasing an unknown or empty
//! token holds nothing back; and a free pitch (fee 0) is still held. The refund path is `market/stall-rent-persist`.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_StallRentToken : LF_Test
{
	override string GetName()
	{
		return "market/stall-rent-token";
	}

	override void Run(LF_TestContext ctx)
	{
		string made = "-";
		string otherDay = "-";
		string otherSecond = "-";
		string noHolder = "-";
		string negativeFee = "-";
		string first = "-";
		string again = "-";
		string second = "-";
		string freeRent = "-";
		int afterFirst = -1;
		int afterAgain = -1;
		int afterSecond = -1;
		int afterFree = -1;
		int afterRefused = -1;
		int afterUnknownRelease = -1;
		int afterEmptyRelease = -1;
		int afterRelease = -1;
		int afterSecondRelease = -1;
		int afterReset = -1;
		if (!RedProof())
		{
			LF_StallRentBook.ResetStatics();
			made = LF_StallRentBook.MakeToken("holder-a", 12, 1800000000);
			otherDay = LF_StallRentBook.MakeToken("holder-a", 13, 1800000000);
			otherSecond = LF_StallRentBook.MakeToken("holder-a", 12, 1800000001);

			noHolder = LF_StallRentBook.Record("", 12, 50, 1800000000);
			negativeFee = LF_StallRentBook.Record("holder-a", 12, -1, 1800000000);
			afterRefused = LF_StallRentBook.ActiveCount();

			first = LF_StallRentBook.Record("holder-a", 12, 50, 1800000000);
			afterFirst = LF_StallRentBook.ActiveCount();
			again = LF_StallRentBook.Record("holder-a", 12, 50, 1800000000);
			afterAgain = LF_StallRentBook.ActiveCount();
			second = LF_StallRentBook.Record("holder-a", 13, 50, 1800000000);
			afterSecond = LF_StallRentBook.ActiveCount();
			freeRent = LF_StallRentBook.Record("holder-b", 12, 0, 1800000000);
			afterFree = LF_StallRentBook.ActiveCount();

			LF_StallRentBook.Release("no-such-token");
			afterUnknownRelease = LF_StallRentBook.ActiveCount();
			LF_StallRentBook.Release("");
			afterEmptyRelease = LF_StallRentBook.ActiveCount();
			LF_StallRentBook.Release(first);
			afterRelease = LF_StallRentBook.ActiveCount();
			LF_StallRentBook.Release(first);
			afterSecondRelease = LF_StallRentBook.ActiveCount();

			LF_StallRentBook.ResetStatics();
			afterReset = LF_StallRentBook.ActiveCount();
		}

		ctx.EqualStr("holder-a|12|1800000000", made, "a token is holder, day and unix second");
		ctx.True(made != otherDay, "the same holder on another day is another rent");
		ctx.True(made != otherSecond, "the same day paid at another second is another rent");
		ctx.EqualStr("", noHolder, "a pitch with no holder is refused");
		ctx.EqualStr("", negativeFee, "a negative fee is refused");
		ctx.Equal(0, afterRefused, "a refused rent holds nothing");
		ctx.EqualStr(made, first, "Record answers the token it holds the rent under");
		ctx.Equal(1, afterFirst, "one rent is held");
		ctx.EqualStr(first, again, "the same rent recorded again has the same token");
		ctx.Equal(1, afterAgain, "the same rent recorded twice is one rent");
		ctx.EqualStr(otherDay, second, "a rent for another day is held under its own token");
		ctx.Equal(2, afterSecond, "two different rents are two");
		ctx.True(freeRent != "", "a free pitch is a rent");
		ctx.Equal(3, afterFree, "a free pitch is held (its fee refunds as nothing, but the pitch is real)");
		ctx.Equal(3, afterUnknownRelease, "releasing an unknown token changes nothing");
		ctx.Equal(3, afterEmptyRelease, "releasing an empty token changes nothing");
		ctx.Equal(2, afterRelease, "releasing a token frees that rent");
		ctx.Equal(2, afterSecondRelease, "releasing it a second time changes nothing");
		ctx.Equal(0, afterReset, "the world-end reset empties the book");
	}
}
