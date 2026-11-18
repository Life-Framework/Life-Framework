//------------------------------------------------------------------------------------------------
//! A for-sale plot sign (#2649, slice of #747). The placed sign names its plot by key; the price and
//! tier come from LF_PropertyConfig and the owner of record is LF_Properties' row.
//!
//! BUYING. The client sends the plot key of the sign it used (LF_PropertyPlayerController.RpcAsk_LF_BuyProperty);
//! the server takes the nearest live sign with that key within REACH of the caller's character. A
//! client can never buy a plot it is not standing at, and of two signs a few metres apart the server
//! never sells the one the player did not pick, at a price the prompt did not show. The owner check comes before any cash moves and the
//! owner is set in the same server call, so two buyers can not both pay for one plot.
//!
//! OWNED STATE ON CLIENTS (#2780). A client has no property book, so the server replicates one bool
//! per sign (m_bOwned, RplProp + BumpMe on change; a late joiner gets it with the entity snapshot).
//! LF_Properties.SetOwner / ApplyAll refresh the live signs, so a sale, a clear and a save load all
//! reach every client. The owner's key is never replicated. The Buy action reads only this flag.
//!
//! FINDING A PLOT (#2984). A sign that is for sale registers in LF_Places as kind PLACE_KIND, id = its plot
//! key, and leaves it once the plot is owned (server: RefreshOwned; client: the replicated flag's callback).
//! The map draws the kind from its POI row (Configs/Map/LF_MapPois.conf) and a town board card can point at it
//! (LF_TownBoard.PLACE_NEAREST), so neither names a coordinate or a map: the placed signs are the plots.
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
	//! The LF_Places kind of a plot that is for sale now; the id is the plot key.
	static const string PLACE_KIND = "PlotForSale";

	[Attribute("", UIWidgets.EditBox, "Plot key: the m_sPlotId of an LF_PropertyPlotDef in Configs/Property/LF_Property.conf")]
	protected string m_sPlotKey;

	//! Whether somebody owns this sign's plot. Set by the server only; clients read the replicated copy.
	[RplProp(onRplName: "OnOwnedReplicated")]
	protected bool m_bOwned;

	//! Live plot signs; the component's own OnDelete removes it.
	protected static ref array<LF_PlotComponent> s_aLive;

	//! The key this sign is registered under in LF_Places, empty when it is not.
	protected string m_sPlacedKey;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (!s_aLive)
			s_aLive = new array<LF_PlotComponent>();

		s_aLive.Insert(this);
		if (m_sPlotKey.IsEmpty())
			LF_Debug.Warn(FEATURE, string.Format("plot sign %1 has no plot key; it sells nothing", LF_Utils.GetPrefabName(owner)));

		RefreshOwned();
		SyncPlace();
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (s_aLive)
			s_aLive.RemoveItem(this);

		DropPlace();
		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the server changed the owned flag; the plot enters or leaves the for-sale places.
	protected void OnOwnedReplicated()
	{
		SyncPlace();
	}

	//------------------------------------------------------------------------------------------------
	//! Makes LF_Places agree with the sign: registered while the plot is for sale, absent otherwise.
	//! A second sign of the same plot does not take the place over (Claim), so a key has one mark.
	protected void SyncPlace()
	{
		if (!GetOwner())
			return;

		if (!IsForSale())
		{
			DropPlace();
			return;
		}

		if (m_sPlacedKey == m_sPlotKey)
			return;

		DropPlace();
		if (LF_Places.Claim(PLACE_KIND, m_sPlotKey, GetOwner()))
			m_sPlacedKey = m_sPlotKey;
	}

	//------------------------------------------------------------------------------------------------
	//! Takes this sign out of LF_Places when it holds the place.
	protected void DropPlace()
	{
		if (m_sPlacedKey.IsEmpty())
			return;

		if (LF_Places.Find(PLACE_KIND, m_sPlacedKey) == GetOwner())
			LF_Places.Unregister(PLACE_KIND, m_sPlacedKey);

		m_sPlacedKey = "";
	}

	//------------------------------------------------------------------------------------------------
	//! \return true when this sign's plot is on the for-sale places now (tests).
	bool IsListed()
	{
		return !m_sPlacedKey.IsEmpty();
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
		RefreshOwned();
		SyncPlace();
	}

	//------------------------------------------------------------------------------------------------
	//! Returns true when somebody owns this sign's plot. On a client this is the replicated copy, the
	//! only owner state a client has; on the server it is what RefreshOwned last read from the book.
	bool IsOwned()
	{
		return m_bOwned;
	}

	//------------------------------------------------------------------------------------------------
	//! Returns true when the config sells this plot and nobody owns it: what the Buy action shows on.
	bool IsForSale()
	{
		return !m_bOwned && GetPrice() > 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: reads the owner from LF_Properties and, when it changed, replicates the owned flag.
	//! Does nothing on a client. Returns true when the flag changed.
	bool RefreshOwned()
	{
		if (!Replication.IsServer())
			return false;

		bool nowOwned = !m_sPlotKey.IsEmpty() && !LF_Properties.GetOwnerKey(m_sPlotKey).IsEmpty();
		if (nowOwned == m_bOwned)
			return false;

		m_bOwned = nowOwned;
		Replication.BumpMe();
		LF_Debug.Info(FEATURE, string.Format("plot sign '%1' owned=%2", m_sPlotKey, nowOwned));
		SyncPlace();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: refreshes the live signs of a plot after its owner changed (LF_Properties.SetOwner), or
	//! every live sign when the key is empty (the book was replaced, LF_Properties.ApplyAll).
	static void RefreshKey(string plotKey)
	{
		if (!s_aLive)
			return;

		foreach (LF_PlotComponent plot : s_aLive)
		{
			if (plot && (plotKey.IsEmpty() || plot.GetPlotKey() == plotKey))
				plot.RefreshOwned();
		}
	}

	//------------------------------------------------------------------------------------------------
	//! \return The price of this sign's plot, 0 when the config does not sell it.
	int GetPrice()
	{
		return LF_PropertyConfig.PlotPrice(m_sPlotKey);
	}

	//------------------------------------------------------------------------------------------------
	//! \param askedKey The plot key the client picked; empty takes any key.
	//! \return The nearest live plot sign with a key (askedKey when given) within REACH, null when none.
	static LF_PlotComponent FindNear(vector position, string askedKey = "")
	{
		if (!s_aLive)
			return null;

		LF_PlotComponent nearest;
		float best = REACH;
		foreach (LF_PlotComponent plot : s_aLive)
		{
			if (!plot || plot.GetPlotKey().IsEmpty())
				continue;

			if (!askedKey.IsEmpty() && plot.GetPlotKey() != askedKey)
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
	//! \param askedKey The plot key of the sign the client used; it must be within REACH. Empty takes the nearest sign.
	//! \return BOUGHT, or the reason nothing changed.
	static LF_EPlotBuy Buy(IEntity buyer, string ownerKey, out string plotKey, string askedKey = "")
	{
		plotKey = "";
		if (!buyer || ownerKey.IsEmpty())
		{
			LF_Debug.Warn(FEATURE, "plot buy refused: no buyer or no owner key");
			return LF_EPlotBuy.REFUSED_NO_BUYER;
		}

		LF_PlotComponent plot = FindNear(buyer.GetOrigin(), askedKey);
		if (!plot)
		{
			LF_Debug.Warn(FEATURE, string.Format("plot buy refused for %1: no plot sign '%2' within %3 m", ownerKey, askedKey, REACH));
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
