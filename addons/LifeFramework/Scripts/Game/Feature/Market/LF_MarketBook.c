//------------------------------------------------------------------------------------------------
//! One good's price state: what the next unit fetches, in hundredths of a cash unit (#3028, so a small drop
//! per unit adds up instead of rounding up to a whole 1), and the unix second the price last moved.
class LF_MarketPrice
{
	int m_iFine;
	int m_iLastSaleUnix;

	//------------------------------------------------------------------------------------------------
	//! \param[in] fine The price in hundredths of a cash unit (LF_PriceCurve.ToFine of a whole price).
	static LF_MarketPrice Make(int fine, int lastSaleUnix)
	{
		LF_MarketPrice p = new LF_MarketPrice();
		p.m_iFine = fine;
		p.m_iLastSaleUnix = lastSaleUnix;
		return p;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The stored price as whole cash, rounded to the nearest.
	int Cash()
	{
		return LF_PriceCurve.ToCash(m_iFine);
	}
}

//------------------------------------------------------------------------------------------------
//! The market's live prices (#2845): server-only, in memory. One entry per good that has been sold
//! since the world started; a good with no entry stands at its base. Recovery is applied lazily on
//! each quote from the stamp (LF_PriceCurve.Recover, no tick). Saved as the "market" section (LF_MarketSave.c).
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
	//! \return The price the next unit of a curved good fetches at `nowUnix`, recovery applied, in whole
	//! cash (rounded to the nearest: what the board shows). 0 when the good is not curved.
	static int CurrentPriceAt(ResourceName good, int nowUnix)
	{
		LF_MarketGoodDef def = Curved(good);
		if (!def)
			return 0;

		return LF_PriceCurve.ToCash(RecoveredPrice(def, nowUnix));
	}

	//------------------------------------------------------------------------------------------------
	//! \return The same price in hundredths of a cash unit. 0 when the good is not curved.
	static int CurrentFineAt(ResourceName good, int nowUnix)
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
	//! curve over whole lots, rounded to whole cash once; #3622: a lot is the row's `m_iLotSize` units and a
	//! part lot pays nothing). 0 when the good is not curved or the units fill no lot.
	static int QuoteAt(ResourceName good, int units, int nowUnix)
	{
		LF_MarketGoodDef def = Curved(good);
		if (!def || units <= 0)
			return 0;

		int lots = def.Lots(units);
		if (lots <= 0)
			return 0;

		return def.BulkTotal(RecoveredPrice(def, nowUnix), ClampUnits(lots));
	}

	//------------------------------------------------------------------------------------------------
	//! \return How many of `units` a sale of a curved good takes: whole lots only, the rest stays with the
	//! seller (#3622). `units` itself when the good is not curved (every unit sells); 0 below one lot.
	static int SellableUnits(ResourceName good, int units)
	{
		LF_MarketGoodDef def = Curved(good);
		if (!def)
			return units;

		return def.SellableUnits(units);
	}

	//------------------------------------------------------------------------------------------------
	//! \return The lot size of a curved good, 1 when the good is not curved.
	static int LotOf(ResourceName good)
	{
		LF_MarketGoodDef def = Curved(good);
		if (!def)
			return 1;

		return def.Lot();
	}

	//------------------------------------------------------------------------------------------------
	static int Quote(ResourceName good, int units)
	{
		return QuoteAt(good, units, System.GetUnixTime());
	}

	//------------------------------------------------------------------------------------------------
	//! Server: books a sale of `units` (whole lots count, #3622) at `nowUnix`: the recovered price moves down the curve and the
	//! stamp becomes now. Publishes nothing (the caller does, once the sale completed).
	//! \param[out] oldPrice The price before the sale (recovery applied), whole cash.
	//! \return The new price in whole cash (the book keeps it in hundredths); 0 when the good is not curved
	//! (nothing booked).
	static int ApplySaleAt(ResourceName good, int units, int nowUnix, out int oldPrice)
	{
		oldPrice = 0;
		LF_MarketGoodDef def = Curved(good);
		if (!def || units <= 0)
			return 0;

		// The price falls per lot sold; a part lot books nothing (#3622).
		int lots = def.Lots(units);
		if (lots <= 0)
			return 0;

		int oldFine = RecoveredPrice(def, nowUnix);
		int newFine = def.PriceAfterSale(oldFine, ClampUnits(lots));
		s_mPrices.Set(good, LF_MarketPrice.Make(newFine, StampAfterSale(good, nowUnix)));
		oldPrice = LF_PriceCurve.ToCash(oldFine);
		int newPrice = LF_PriceCurve.ToCash(newFine);
		LF_Debug.Info(FEATURE, string.Format("%1 sold x%2 (%3 lots): price %4 -> %5 (hundredths %6 -> %7)", good, units, lots, oldPrice, newPrice, oldFine, newFine));
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

		return LF_MarketPrice.Make(p.m_iFine, p.m_iLastSaleUnix);
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

		s_mPrices.Set(good, LF_MarketPrice.Make(snapshot.m_iFine, snapshot.m_iLastSaleUnix));
	}

	//------------------------------------------------------------------------------------------------
	//! Stored price recovered to `nowUnix` in hundredths, or the base when the good has no entry. Read-only:
	//! the stamp only moves on a sale, so a quote never changes what the next quote says.
	protected static int RecoveredPrice(LF_MarketGoodDef def, int nowUnix)
	{
		LF_MarketPrice p = s_mPrices.Get(def.m_sGood);
		if (!p)
			return def.NextPrice(def.BaseFine());

		LF_MarketConfig config = LF_MarketConfig.Get();
		int maxHours = 0;
		if (config)
			maxHours = config.m_iMaxRecoveryHours;

		int recovered = LF_PriceCurve.Recover(p.m_iFine, def.BaseFine(), def.m_iRecoverPctPerHour, nowUnix - p.m_iLastSaleUnix, maxHours, def.FloorFine(), def.CeilingFine());
		return def.NextPrice(recovered);
	}

	//------------------------------------------------------------------------------------------------
	//! The stamp a sale at `nowUnix` stores. It moves forward by the whole hours the recovery just
	//! counted, so the part hour since then still counts toward the next step: stamping `nowUnix`
	//! would throw it away, and a good sold at least once an hour would never recover. A first sale,
	//! a clock moved back, or a gap past the recovery cap stamps `nowUnix` (IsClamped).
	protected static int StampAfterSale(ResourceName good, int nowUnix)
	{
		LF_MarketPrice p = s_mPrices.Get(good);
		if (!p)
			return nowUnix;

		int gap = nowUnix - p.m_iLastSaleUnix;
		if (gap < 0)
			return nowUnix;

		int maxHours = 0;
		LF_MarketConfig config = LF_MarketConfig.Get();
		if (config)
			maxHours = config.m_iMaxRecoveryHours;
		if (LF_PriceCurve.IsClamped(gap, maxHours))
			return nowUnix;

		return p.m_iLastSaleUnix + LF_PriceCurve.RecoveryHours(gap, maxHours) * LF_PriceCurve.SECONDS_PER_HOUR;
	}

	//------------------------------------------------------------------------------------------------
	protected static int ClampUnits(int units)
	{
		return LF_Utils.ClampInt(units, 0, LF_PriceCurve.MAX_BULK_UNITS);
	}

	//------------------------------------------------------------------------------------------------
	//! The live book, good to state (#2846: LF_MarketSavePayload writes and replaces it). Never cache it.
	static map<ResourceName, ref LF_MarketPrice> Entries()
	{
		return s_mPrices;
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
