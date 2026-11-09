//------------------------------------------------------------------------------------------------
//! The fence's trust decision (#271): a buyer needs enough completed crimes on the hidden record
//! (LF_CrimeRecord.Total) before the fence sells a tool. Pure server logic over the record; the
//! shop calls Check before it takes any cash, so a refusal costs nothing.
class LF_FenceRules
{
	static const string FEATURE = "Fence";

	//------------------------------------------------------------------------------------------------
	//! \return True when a record of \p total completed crimes meets the \p threshold. No threshold is open.
	static bool Allows(int total, int threshold)
	{
		if (threshold <= 0)
			return true;

		return total >= threshold;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the buyer's standing against one tool's threshold. A refusal is logged; nothing is
	//! charged here. An unknown buyer (no account) has no record and is refused any threshold.
	//! \return NONE when the fence will sell, else NOT_TRUSTED.
	static LF_EShopRefusal Check(string buyerUid, int threshold, string prefab)
	{
		int total = 0;
		if (!buyerUid.IsEmpty())
			total = LF_CrimeRecord.Total(buyerUid);

		if (Allows(total, threshold))
			return LF_EShopRefusal.NONE;

		LF_Debug.Info(FEATURE, string.Format("refused %1: record %2 of %3 needed (item=%4)", buyerUid, total, threshold, prefab));
		return LF_EShopRefusal.NOT_TRUSTED;
	}
}
