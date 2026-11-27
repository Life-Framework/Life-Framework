//------------------------------------------------------------------------------------------------
//! A house's "For sale" sign (#3207, chapter #3201). The placed sign names a house identifier; the price
//! and the tier label come from LF_PropertyConfig's house list (m_aHouses, a pack prices its houses by
//! overlay), and the owner of record is LF_Properties / LF_HouseManagerComponent.
//!
//! BUYING. The client sends the house id of the sign it used (LF_HouseSaleRequest); the server takes the
//! nearest live sign with that id within REACH of the caller's character and sells the house to them. The
//! owner check comes before any cash moves and the owner is set in the same server call, so two buyers
//! can not both pay for one house. Exactly the price leaves the carried cash (ledger reason
//! property_buy_house), the buyer becomes the owner (LF_Properties.SetOwner sets it on the live house,
//! which saves it in "houses"; the properties row records the purchase time) and a house key bound to the
//! identifier goes into their inventory.
//!
//! OWNED STATE ON CLIENTS (the #2780 plot-sign pattern). A client has no property book, so each sign
//! replicates one bool, m_bOwned (RplProp + BumpMe on change; a late joiner gets it with the entity). The
//! sign's action reads only that flag: "Buy house" while it is for sale, "Sold" once it is not. The owner's
//! key is never replicated; the server decides every sale from its own book.
//------------------------------------------------------------------------------------------------
enum LF_EHouseBuy
{
	BOUGHT,
	REFUSED_NO_SIGN,	//!< no house sign within reach of the caller
	REFUSED_NOT_FOR_SALE,	//!< the sign's house id has no price in the config
	REFUSED_OWNED,	//!< somebody (the caller included) owns it already
	REFUSED_NO_CASH,	//!< the caller carries less than the price; nothing is taken
	REFUSED_NO_BUYER,	//!< no character or no owner key
	REFUSED_NOT_ABLE,	//!< the buyer is cuffed, downed or dead
	REFUSED_NO_KEY	//!< the key item could not be made; nothing is taken
}

[ComponentEditorProps(category: "LifeFramework/Property", description: "A house's for-sale sign: names the LF_Property.conf house it sells")]
class LF_HouseSaleComponentClass : ScriptComponentClass
{
}

class LF_HouseSaleComponent : ScriptComponent
{
	static const string FEATURE = "Property";
	//! Metres from the caller's character to the sign.
	static const float REACH = 5.0; // lf-fixed: interaction reach, the same 5 m as the plot sign
	//! The ledger reason of a house purchase.
	static const string REASON_BUY = "property_buy_house";
	static const ResourceName KEY_PREFAB = "{3735389521255DBE}Prefabs/Items/HouseKeyItem.et";

	[Attribute("", UIWidgets.EditBox, "House identifier: the m_sHouseId of an LF_PropertyHouseDef in Configs/Property/LF_Property.conf, the same id the house's m_sDebugIdentifier carries")]
	protected string m_sHouseId;

	//! Whether somebody owns this sign's house. Set by the server only; clients read the replicated copy.
	[RplProp()]
	protected bool m_bOwned;

	//! Live house signs; the component's own OnDelete removes it.
	protected static ref array<LF_HouseSaleComponent> s_aLive;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (!s_aLive)
			s_aLive = new array<LF_HouseSaleComponent>();

		s_aLive.Insert(this);
		if (m_sHouseId.IsEmpty())
			LF_Debug.Warn(FEATURE, string.Format("house sale sign %1 has no house id; it sells nothing", LF_Utils.GetPrefabName(owner)));

		RefreshOwned();
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (s_aLive)
			s_aLive.RemoveItem(this);

		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	//! \return The house id this sign sells.
	string GetHouseId()
	{
		return m_sHouseId;
	}

	//------------------------------------------------------------------------------------------------
	//! Sets the house id of a spawned sign (tests, scripted placement); a placed sign sets it in its layer.
	void SetHouseId(string houseId)
	{
		m_sHouseId = houseId;
		RefreshOwned();
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when somebody owns this sign's house. On a client this is the replicated copy, the
	//! only owner state a client has; on the server it is what RefreshOwned last read from the book.
	bool IsOwned()
	{
		return m_bOwned;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the config prices this house and nobody owns it: what the Buy action shows on.
	bool IsForSale()
	{
		return !m_bOwned && GetPrice() > 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: reads the owner from LF_Properties and, when it changed, replicates the owned flag.
	//! Does nothing on a client. \return True when the flag changed.
	bool RefreshOwned()
	{
		if (!Replication.IsServer())
			return false;

		bool nowOwned = !m_sHouseId.IsEmpty() && !LF_Properties.GetOwnerKey(m_sHouseId).IsEmpty();
		if (nowOwned == m_bOwned)
			return false;

		m_bOwned = nowOwned;
		Replication.BumpMe();
		LF_Debug.Info(FEATURE, string.Format("house sale sign '%1' owned=%2", m_sHouseId, nowOwned));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: refreshes the live signs of a house after its owner changed (LF_Properties.SetOwner, the
	//! house's own SetOwner), or every live sign when the id is empty (the book was replaced, ApplyAll).
	static void RefreshKey(string houseId)
	{
		if (!s_aLive)
			return;

		foreach (LF_HouseSaleComponent sign : s_aLive)
		{
			if (sign && (houseId.IsEmpty() || sign.GetHouseId() == houseId))
				sign.RefreshOwned();
		}
	}

	//------------------------------------------------------------------------------------------------
	//! \return The price of this sign's house, 0 when the config does not sell it.
	int GetPrice()
	{
		return LF_PropertyConfig.HousePrice(m_sHouseId);
	}

	//------------------------------------------------------------------------------------------------
	//! \return The tier label of this sign's house, empty when the config has none.
	string GetTierLabel()
	{
		return LF_PropertyConfig.HouseTierLabel(m_sHouseId);
	}

	//------------------------------------------------------------------------------------------------
	//! \param askedHouseId The house id the client picked; empty takes any id.
	//! \return The nearest live house sign with an id (askedHouseId when given) within REACH, null when none.
	static LF_HouseSaleComponent FindNear(vector position, string askedHouseId = "")
	{
		if (!s_aLive)
			return null;

		LF_HouseSaleComponent nearest;
		float best = REACH;
		foreach (LF_HouseSaleComponent sign : s_aLive)
		{
			if (!sign || sign.GetHouseId().IsEmpty())
				continue;

			if (!askedHouseId.IsEmpty() && sign.GetHouseId() != askedHouseId)
				continue;

			IEntity owner = sign.GetOwner();
			if (!owner)
				continue;

			float distance = vector.Distance(position, owner.GetOrigin());
			if (distance <= best)
			{
				best = distance;
				nearest = sign;
			}
		}

		return nearest;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the buyer buys the house whose sign is nearest their feet, paying with carried cash.
	//! \param buyer The caller's character; the house is found from its position, never from the client.
	//! \param ownerKey The buyer's character key (LF_Utils.GetPlayerUID).
	//! \param[out] houseId The house that was found, empty when none was in reach.
	//! \param askedHouseId The house id of the sign the client used; it must be within REACH.
	//! \return BOUGHT, or the reason nothing changed.
	static LF_EHouseBuy Buy(IEntity buyer, string ownerKey, out string houseId, string askedHouseId = "")
	{
		houseId = "";
		if (!buyer || ownerKey.IsEmpty())
		{
			LF_Debug.Warn(FEATURE, "house buy refused: no buyer or no owner key");
			return LF_EHouseBuy.REFUSED_NO_BUYER;
		}

		// A cuffed, downed or dead body still carries its cash; only a free, conscious buyer pays.
		if (LF_CuffEnforcement.Refuse(buyer, "house purchase"))
			return LF_EHouseBuy.REFUSED_NOT_ABLE;

		if (!LF_Utils.IsAliveState(buyer))
		{
			LF_Debug.Warn(FEATURE, string.Format("house buy refused for %1: not alive and conscious", ownerKey));
			return LF_EHouseBuy.REFUSED_NOT_ABLE;
		}

		LF_HouseSaleComponent sign = FindNear(buyer.GetOrigin(), askedHouseId);
		if (!sign)
		{
			LF_Debug.Warn(FEATURE, string.Format("house buy refused for %1: no house sign '%2' within %3 m", ownerKey, askedHouseId, REACH));
			return LF_EHouseBuy.REFUSED_NO_SIGN;
		}

		houseId = sign.GetHouseId();
		int price = sign.GetPrice();
		if (price <= 0)
		{
			LF_Debug.Warn(FEATURE, string.Format("house buy refused for %1: house %2 is not for sale in the config", ownerKey, houseId));
			return LF_EHouseBuy.REFUSED_NOT_FOR_SALE;
		}

		string current = LF_Properties.GetOwnerKey(houseId);
		if (!current.IsEmpty())
		{
			LF_Debug.Info(FEATURE, string.Format("house buy refused for %1: house %2 is owned by %3", ownerKey, houseId, current));
			return LF_EHouseBuy.REFUSED_OWNED;
		}

		int carried = LF_MoneyUtils.GetCash(buyer);
		if (carried < price)
		{
			LF_Debug.Info(FEATURE, string.Format("house buy refused for %1: house %2 costs %3, carried %4", ownerKey, houseId, price, carried));
			return LF_EHouseBuy.REFUSED_NO_CASH;
		}

		// The key is made before any cash moves, so a failure costs the buyer nothing.
		IEntity key = LF_Utils.SpawnEntityPrefab(KEY_PREFAB, buyer.GetOrigin() + Vector(0, 0.2, 0));
		LF_KeyComponent keyComponent;
		if (key)
			keyComponent = LF_Component<LF_KeyComponent>.Find(key);

		if (!keyComponent)
		{
			LF_Debug.Error(FEATURE, string.Format("house buy refused for %1: the house key item did not spawn, nothing taken", ownerKey));
			if (key)
				SCR_EntityHelper.DeleteEntityAndChildren(key);

			return LF_EHouseBuy.REFUSED_NO_KEY;
		}

		int taken = LF_MoneyUtils.RemoveAmount(buyer, price, REASON_BUY);
		if (taken != price)
		{
			// The stacks moved under us: give back what was taken, sell nothing.
			if (taken > 0)
				LF_MoneyUtils.AddCash(buyer, taken, REASON_BUY, "", true);

			SCR_EntityHelper.DeleteEntityAndChildren(key);
			LF_Debug.Error(FEATURE, string.Format("house buy refused for %1: took %2 of %3 for house %4, refunded", ownerKey, taken, price, houseId));
			return LF_EHouseBuy.REFUSED_NO_CASH;
		}

		LF_PropertyRecord record = LF_Properties.Create(houseId);
		LF_Properties.SetOwner(houseId, ownerKey);
		if (LF_Properties.GetOwnerKey(houseId) != ownerKey)
		{
			// The house's saved lock row kept an owner the book could not change: sell nothing.
			LF_MoneyUtils.AddCash(buyer, price, REASON_BUY, "", true);
			SCR_EntityHelper.DeleteEntityAndChildren(key);
			LF_Debug.Error(FEATURE, string.Format("house buy refused for %1: the owner of %2 could not be set, refunded %3", ownerKey, houseId, price));
			return LF_EHouseBuy.REFUSED_OWNED;
		}

		if (record)
			record.m_iPurchasedAt = System.GetUnixTime();

		keyComponent.SetHouseIdentifier(houseId);
		SCR_InventoryStorageManagerComponent inventory = LF_Component<SCR_InventoryStorageManagerComponent>.Find(buyer);
		if (!inventory || !inventory.TryInsertItem(key))
			LF_Debug.Warn(FEATURE, string.Format("the key to house %1 did not fit %2's inventory; it lies at their feet", houseId, ownerKey));

		LF_Debug.Info(FEATURE, string.Format("house %1 bought by %2 for %3", houseId, ownerKey, price));
		return LF_EHouseBuy.BOUGHT;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_aLive = null;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_HouseSaleComponent's statics (LF_StaticReset finds it by reflection).
class LF_HouseSaleComponent_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_HouseSaleComponent";
	}

	override void Apply()
	{
		LF_HouseSaleComponent.ResetStatics();
	}
}

//------------------------------------------------------------------------------------------------
//! Every change of a house's owner (a sale, a wipe, an eviction, a load of the "houses" payload)
//! refreshes the signs that sell it, so their replicated owned flag never goes stale.
modded class LF_HouseManagerComponent
{
	//------------------------------------------------------------------------------------------------
	override void SetOwner(string ownerKey)
	{
		super.SetOwner(ownerKey);
		if (Replication.IsServer())
			LF_HouseSaleComponent.RefreshKey(GetHouseIdentifier());
	}
}
