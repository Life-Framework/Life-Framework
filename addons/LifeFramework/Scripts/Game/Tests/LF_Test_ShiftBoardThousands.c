// red-proof: with `RedProof()` on the grouping is not asked, so every expected string ("1,240", "-1,500", ...) fails.

//! level/shift-board-thousands: the XP column of the shift board (LF_ShiftBoard.GroupThousands) puts a comma every
//! three digits counted from the right, keeps small numbers bare and puts the minus sign outside the digits.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_ShiftBoardThousands : LF_Test
{
	override string GetName()
	{
		return "level/shift-board-thousands";
	}

	override void Run(LF_TestContext ctx)
	{
		string zero = "?";
		string small = "?";
		string edge = "?";
		string thousand = "?";
		string board = "?";
		string million = "?";
		string negative = "?";
		string negativeSmall = "?";
		string wide = "?";
		if (!RedProof())
		{
			zero = LF_ShiftBoard.GroupThousands(0);
			small = LF_ShiftBoard.GroupThousands(7);
			edge = LF_ShiftBoard.GroupThousands(999);
			thousand = LF_ShiftBoard.GroupThousands(1000);
			board = LF_ShiftBoard.GroupThousands(1240);
			million = LF_ShiftBoard.GroupThousands(1234567);
			negative = LF_ShiftBoard.GroupThousands(-1500);
			negativeSmall = LF_ShiftBoard.GroupThousands(-12);
			wide = LF_ShiftBoard.GroupThousands(100000);
		}

		ctx.EqualStr("0", zero, "zero is a bare 0");
		ctx.EqualStr("7", small, "a single digit is bare");
		ctx.EqualStr("999", edge, "three digits get no comma");
		ctx.EqualStr("1,000", thousand, "four digits get one comma");
		ctx.EqualStr("1,240", board, "1240 reads 1,240");
		ctx.EqualStr("1,234,567", million, "seven digits get two commas");
		ctx.EqualStr("-1,500", negative, "the minus sits in front of the grouped digits");
		ctx.EqualStr("-12", negativeSmall, "a small negative number has no comma");
		ctx.EqualStr("100,000", wide, "a comma falls exactly on the three-digit boundary");
	}
}
