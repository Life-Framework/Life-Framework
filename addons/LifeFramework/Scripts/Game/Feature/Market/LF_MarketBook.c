//------------------------------------------------------------------------------------------------
//! One good's price state: what the next unit fetches and the unix second the price last moved.
class LF_MarketPrice
{
	int m_iPrice;
	int m_iLastSaleUnix;

	//------------------------------------------------------------------------------------------------
	static LF_MarketPrice Make(int price, int lastSaleUnix)
	{
		LF_MarketPrice p = new LF_MarketPrice();
		p.m_iPrice = price;
		p.m_iLastSaleUnix = lastSaleUnix;
		return p;
	}
}

//------------------------------------------------------------------------------------------------
//! The market's live prices (#2845): server-only, in memory. One entry per good that has been sold
//! since the world started; a good with no entry stands at its base. Recovery is applied lazily on
//! each quote from the stamp (LF_PriceCurve.Recover, no tick). Saving the book is #2846.
//! A good is curved only while the market is enabled and the config lists it; everything else sells
//! at the trader's fixed value, exactly as before the market existed.
class LF_MarketBook
{
	static const string FEATURE = "Market";

	protected static ref map<ResourceName, ref LF_MarketPrice> s_mPrices = new map<ResourceName, ref LF_MarketPrice>();

	//------------------------------------------------------------------------------------------------
	//! \return The good's curve when the market is on and lists it, else null (fixed price).
	static LF_MarketGoodDef Curved(ResourceName good)
	{
		LF_MarketConfig config = LF_MarketConfig.Get();
		if (!config || !config.m_bEnabled)
			return null;

		return config.FindGood(good);
	}

	//------------------------------------------------------------------------------------------------
	//! \return The price the next unit of a curved good fetches at `nowUnix`, recovery applied.
	//! 0 when the good is not curved.
	static int CurrentPriceAt(ResourceName good, int nowUnix)
	{
		LF_MarketGoodDef def = Curved(good);
		if (!def)
			return 0;

		return RecoveredPrice(def, nowUnix);
	}

	//------------------------------------------------------------------------------------------------
	static int CurrentPrice(ResourceName good)
	{
		return CurrentPriceAt(good, System.GetUnixTime());
	}

	//------------------------------------------------------------------------------------------------
	//! \return What `units` of a curved good pay together at `nowUnix` (the marginal sum of the
	//! curve). 0 when the good is not curved or no units.
	static int QuoteAt(ResourceName good, int units, int nowUnix)
	{
		LF_MarketGoodDef def = Curved(good);
		if (!def || units <= 0)
			return 0;

		return def.BulkTotal(RecoveredPrice(def, nowUnix), ClampUnits(units));
	}

	//------------------------------------------------------------------------------------------------
	static int Quote(ResourceName good, int units)
	{
		return QuoteAt(good, units, System.GetUnixTime());
	}

	//------------------------------------------------------------------------------------------------
	//! Server: books a sale of `units` at `nowUnix`: the recovered price moves down the curve and the
	//! stamp becomes now. Publishes nothing (the caller does, once the sale completed).
	//! \param[out] oldPrice The price before the sale (recovery applied).
	//! \return The new price; 0 when the good is not curved (nothing booked).
	static int ApplySaleAt(ResourceName good, int units, int nowUnix, out int oldPrice)
	{
		oldPrice = 0;
		LF_MarketGoodDef def = Curved(good);
		if (!def || units <= 0)
			return 0;

		oldPrice = RecoveredPrice(def, nowUnix);
		int newPrice = def.PriceAfterSale(oldPrice, ClampUnits(units));
		s_mPrices.Set(good, LF_MarketPrice.Make(newPrice, nowUnix));
		LF_Debug.Info(FEATURE, string.Format("%1 sold x%2: price %3 -> %4", good, units, oldPrice, newPrice));
		return newPrice;
	}

	//------------------------------------------------------------------------------------------------
	//! \return A copy of the stored state of `good`, or null when it has none (it stands at its base).
	//! A sale snapshots it before moving the price so a rollback can put it back exactly.
	static LF_MarketPrice Snapshot(ResourceName good)
	{
		LF_MarketPrice p = s_mPrices.Get(good);
		if (!p)
			return null;

		return LF_MarketPrice.Make(p.m_iPrice, p.m_iLastSaleUnix);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: puts back a state taken by Snapshot (null removes the entry: back at the base).
	static void Restore(ResourceName good, LF_MarketPrice snapshot)
	{
		if (!snapshot)
		{
			s_mPrices.Remove(good);
			return;
		}

		s_mPrices.Set(good, LF_MarketPrice.Make(snapshot.m_iPrice, snapshot.m_iLastSaleUnix));
	}

	//------------------------------------------------------------------------------------------------
	//! Stored price recovered to `nowUnix`, or the base when the good has no entry. Read-only: the stamp
	//! only moves on a sale, so a quote never changes what the next quote says.
	protected static int RecoveredPrice(LF_MarketGoodDef def, int nowUnix)
	{
		LF_MarketPrice p = s_mPrices.Get(def.m_sGood);
		if (!p)
			return def.NextPrice(def.m_iBase);

		LF_MarketConfig config = LF_MarketConfig.Get();
		int maxHours = 0;
		if (config)
			maxHours = config.m_iMaxRecoveryHours;

		int recovered = LF_PriceCurve.Recover(p.m_iPrice, def.m_iBase, def.m_iRecoverPctPerHour, nowUnix - p.m_iLastSaleUnix, maxHours, def.m_iFloor, def.m_iCeiling);
		return def.NextPrice(recovered);
	}

	//------------------------------------------------------------------------------------------------
	protected static int ClampUnits(int units)
	{
		return LF_Utils.ClampInt(units, 0, LF_PriceCurve.MAX_BULK_UNITS);
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_mPrices = new map<ResourceName, ref LF_MarketPrice>();
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_MarketBook's statics (LF_StaticReset finds it by reflection).
class LF_MarketBook_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_MarketBook";
	}

	override void Apply()
	{
		LF_MarketBook.ResetStatics();
	}
}
