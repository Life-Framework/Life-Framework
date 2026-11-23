//------------------------------------------------------------------------------------------------
//! Amount class of a load of goods (#4201, CR-F1). The charge sheet classes what a search finds.
enum LF_EGoodsClass
{
	NONE,
	A,
	B,
	C
}

//------------------------------------------------------------------------------------------------
//! Pure rules: class A is up to 2 units, B up to 10, C above. Crime only tags and counts goods; the
//! law book (Police) decides the charge. No statics, so there is nothing to reset.
class LF_GoodsClass
{
	static const int MAX_A = 2; // lf-fixed: the class boundary is the charge-sheet definition in the 0.3.0 plan (A up to 2), not a server tunable
	static const int MAX_B = 10; // lf-fixed: the class boundary is the charge-sheet definition in the 0.3.0 plan (B up to 10), not a server tunable

	//------------------------------------------------------------------------------------------------
	//! \return The class of a load of this many units. NONE for zero or a negative count.
	//! Plain integer compares: unix-second-sized counts never pass through float math.
	static LF_EGoodsClass FromUnits(int units)
	{
		if (units < 1)
			return LF_EGoodsClass.NONE;
		if (units <= MAX_A)
			return LF_EGoodsClass.A;
		if (units <= MAX_B)
			return LF_EGoodsClass.B;
		return LF_EGoodsClass.C;
	}

	//------------------------------------------------------------------------------------------------
	//! \return "A", "B" or "C" for a charge sheet line; an empty string for NONE.
	static string Letter(LF_EGoodsClass goodsClass)
	{
		if (goodsClass == LF_EGoodsClass.A)
			return "A";
		if (goodsClass == LF_EGoodsClass.B)
			return "B";
		if (goodsClass == LF_EGoodsClass.C)
			return "C";
		return "";
	}
}
