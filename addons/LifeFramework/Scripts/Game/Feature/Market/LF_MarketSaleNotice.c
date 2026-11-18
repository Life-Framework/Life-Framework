//------------------------------------------------------------------------------------------------
//! The trader says it when your sale moved the price (#2923). A good's price is read in bands, each
//! BAND_PCT percent of its base below the base. A sale that moves the good into a lower band than the
//! trader last announced gets one line ("fetching less today"); when the good has recovered to a higher
//! band the next seller hears that too. A sale inside the band says nothing, and the line never carries
//! a number. Server only, in memory: after a restart the first sale re-reads the band from the book, so
//! nothing is announced until a band is actually crossed. Market off, or a good with no curve: nothing.
class LF_MarketSaleNotice
{
	static const string FEATURE = "Market";
	static const string KEY_DOWN = "#LF-Market_SaleDown";
	static const string KEY_UP = "#LF-Market_SaleUp";
	static const string KEY_TITLE = "#LF-Market_SaleTitle";
	static const int BAND_PCT = 20; // lf-fixed: a band is a fifth of the base; the trader speaks in broad strokes, not a tunable

	//! The band the trader last spoke for, per good (0 is at the base, higher is cheaper).
	protected static ref map<ResourceName, int> s_mAnnounced = new map<ResourceName, int>();

	//------------------------------------------------------------------------------------------------
	//! \return How many whole bands `price` stands below `base`; 0 at or above the base.
	static int Band(int base, int price)
	{
		if (price >= base || base <= 0)
			return 0;

		int step = LF_Utils.MaxInt(1, base * BAND_PCT / 100);
		return (base - price) / step;
	}

	//------------------------------------------------------------------------------------------------
	//! Decides the line for a sale that moved `good` from `oldPrice` to `newPrice` and remembers the band
	//! it announced. \return The localization key to say, or "" for silence.
	static string Evaluate(ResourceName good, int oldPrice, int newPrice)
	{
		LF_MarketGoodDef def = LF_MarketBook.Curved(good);
		if (!def)
			return "";

		int bandOld = Band(def.m_iBase, oldPrice);
		int bandNew = Band(def.m_iBase, newPrice);
		int announced = bandOld;
		if (s_mAnnounced.Contains(good))
			announced = s_mAnnounced.Get(good);

		if (bandNew > announced)
		{
			s_mAnnounced.Set(good, bandNew);
			return KEY_DOWN;
		}

		if (bandNew < announced)
		{
			s_mAnnounced.Set(good, bandNew);
			return KEY_UP;
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Server: called by LF_TraderSale once a curved sale completed. Tells `seller` in a toast.
	static void OnSale(IEntity seller, ResourceName good, int oldPrice, int newPrice)
	{
		string key = Evaluate(good, oldPrice, newPrice);
		if (key.IsEmpty())
			return;

		LF_Debug.Info(FEATURE, string.Format("trader notice %1 for %2", key, good));
		LF_Utils.NotifyEntity(seller, key, KEY_TITLE, 4.0, LF_ShopManagerComponent.ResolveDisplayName(good));
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_mAnnounced = new map<ResourceName, int>();
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_MarketSaleNotice's statics (LF_StaticReset finds it by reflection).
class LF_MarketSaleNotice_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_MarketSaleNotice";
	}

	override void Apply()
	{
		LF_MarketSaleNotice.ResetStatics();
	}
}
