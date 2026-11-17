//------------------------------------------------------------------------------------------------
//! A for-sale plot sign (#2649, slice of #747). The placed sign names its plot by key; the price and
//! tier come from LF_PropertyConfig and the owner of record is LF_Properties' row.
//!
//! BUYING. The client sends no plot id (LF_PropertyPlayerController.RpcAsk_LF_BuyProperty): the
//! server takes the nearest live plot sign within REACH of the caller's character, so a client can
//! never name a plot it is not standing at. The owner check comes before any cash moves and the
//! owner is set in the same server call, so two buyers can not both pay for one plot.
//------------------------------------------------------------------------------------------------
enum LF_EPlotBuy
{
	BOUGHT,
	REFUSED_NO_PLOT,	//!< no plot sign within reach of the caller
	REFUSED_NOT_FOR_SALE,	//!< the sign's plot key has no price in the config
	REFUSED_OWNED,	//!< somebody (the caller included) owns it already
	REFUSED_NO_CASH,	//!< the caller carries less than the price; nothing is taken
	REFUSED_NO_BUYER	//!< no character or no owner key
}

[ComponentEditorProps(category: "LifeFramework/Property", description: "A for-sale plot sign: names the LF_Property.conf plot it sells")]
class LF_PlotComponentClass : ScriptComponentClass
{
}

class LF_PlotComponent : ScriptComponent
{
	static const string FEATURE = "Property";
	//! Metres from the caller's character to the sign.
	static const float REACH = 5.0; // lf-fixed: interaction reach, the same 5 m as the hideout stash request
	//! The ledger reason of a plot purchase.
	static const string REASON_BUY = "property_buy";

	[Attribute("", UIWidgets.EditBox, "Plot key: the m_sPlotId of an LF_PropertyPlotDef in Configs/Property/LF_Property.conf")]
	protected string m_sPlotKey;

	//! Live plot signs; the component's own OnDelete removes it.
	protected static ref array<LF_PlotComponent> s_aLive;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (!s_aLive)
			s_aLive = new array<LF_PlotComponent>();

		s_aLive.Insert(this);
		if (m_sPlotKey.IsEmpty())
			LF_Debug.Warn(FEATURE, string.Format("plot sign %1 has no plot key; it sells nothing", LF_Utils.GetPrefabName(owner)));
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (s_aLive)
			s_aLive.RemoveItem(this);

		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	//! \return The plot key this sign sells.
	string GetPlotKey()
	{
		return m_sPlotKey;
	}

	//------------------------------------------------------------------------------------------------
	//! Sets the plot key of a spawned sign (tests, scripted placement); a placed sign sets it in its layer.
	void SetPlotKey(string plotKey)
	{
		m_sPlotKey = plotKey;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The price of this sign's plot, 0 when the config does not sell it.
	int GetPrice()
	{
		return LF_PropertyConfig.PlotPrice(m_sPlotKey);
	}

	//------------------------------------------------------------------------------------------------
	//! \return The nearest live plot sign with a key within REACH of the position, null when none.
	static LF_PlotComponent FindNear(vector position)
	{
		if (!s_aLive)
			return null;

		LF_PlotComponent nearest;
		float best = REACH;
		foreach (LF_PlotComponent plot : s_aLive)
		{
			if (!plot || plot.GetPlotKey().IsEmpty())
				continue;

			IEntity owner = plot.GetOwner();
			if (!owner)
				continue;

			float distance = vector.Distance(position, owner.GetOrigin());
			if (distance <= best)
			{
				best = distance;
				nearest = plot;
			}
		}

		return nearest;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the buyer buys the plot whose sign is nearest their feet, paying with carried cash.
	//! \param buyer The caller's character; the plot is found from its position, never from the client.
	//! \param ownerKey The buyer's character key (LF_Utils.GetPlayerUID).
	//! \param[out] plotKey The plot that was found, empty when none was in reach.
	//! \return BOUGHT, or the reason nothing changed.
	static LF_EPlotBuy Buy(IEntity buyer, string ownerKey, out string plotKey)
	{
		plotKey = "";
		if (!buyer || ownerKey.IsEmpty())
		{
			LF_Debug.Warn(FEATURE, "plot buy refused: no buyer or no owner key");
			return LF_EPlotBuy.REFUSED_NO_BUYER;
		}

		LF_PlotComponent plot = FindNear(buyer.GetOrigin());
		if (!plot)
		{
			LF_Debug.Warn(FEATURE, string.Format("plot buy refused for %1: no plot sign within %2 m", ownerKey, REACH));
			return LF_EPlotBuy.REFUSED_NO_PLOT;
		}

		plotKey = plot.GetPlotKey();
		int price = plot.GetPrice();
		if (price <= 0)
		{
			LF_Debug.Warn(FEATURE, string.Format("plot buy refused for %1: plot %2 is not for sale in the config", ownerKey, plotKey));
			return LF_EPlotBuy.REFUSED_NOT_FOR_SALE;
		}

		string current = LF_Properties.GetOwnerKey(plotKey);
		if (!current.IsEmpty())
		{
			LF_Debug.Info(FEATURE, string.Format("plot buy refused for %1: plot %2 is owned by %3", ownerKey, plotKey, current));
			return LF_EPlotBuy.REFUSED_OWNED;
		}

		int carried = LF_MoneyUtils.GetCash(buyer);
		if (carried < price)
		{
			LF_Debug.Info(FEATURE, string.Format("plot buy refused for %1: plot %2 costs %3, carried %4", ownerKey, plotKey, price, carried));
			return LF_EPlotBuy.REFUSED_NO_CASH;
		}

		int taken = LF_MoneyUtils.RemoveAmount(buyer, price, REASON_BUY);
		if (taken != price)
		{
			// The stacks moved under us: give back what was taken, sell nothing.
			if (taken > 0)
				LF_MoneyUtils.AddCash(buyer, taken, REASON_BUY, "", true);

			LF_Debug.Error(FEATURE, string.Format("plot buy refused for %1: took %2 of %3 for plot %4, refunded", ownerKey, taken, price, plotKey));
			return LF_EPlotBuy.REFUSED_NO_CASH;
		}

		LF_PropertyRecord record = LF_Properties.Create(plotKey);
		LF_Properties.SetOwner(plotKey, ownerKey);
		if (record)
			record.m_iPurchasedAt = System.GetUnixTime();

		LF_Debug.Info(FEATURE, string.Format("plot %1 bought by %2 for %3", plotKey, ownerKey, price));
		return LF_EPlotBuy.BOUGHT;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_aLive = null;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_PlotComponent's statics (LF_StaticReset finds it by reflection).
class LF_PlotComponent_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_PlotComponent";
	}

	override void Apply()
	{
		LF_PlotComponent.ResetStatics();
	}
}
