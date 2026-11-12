//------------------------------------------------------------------------------------------------
//! One row on the lot. m_sPrefab is the vanilla vehicle the player buys.
//! m_sSpawnPrefab is the locked child that carries LF_VehicleLockComponent.
//------------------------------------------------------------------------------------------------
[BaseContainerProps()]
class LF_VehicleOffer
{
	[Attribute(ResourceName.Empty, UIWidgets.ResourcePickerThumbnail, desc: "Vanilla vehicle prefab", "et")]
	protected ResourceName m_sPrefab;

	[Attribute(ResourceName.Empty, UIWidgets.ResourcePickerThumbnail, desc: "Locked child spawned for the buyer. Empty spawns the vanilla prefab.", "et")]
	protected ResourceName m_sSpawnPrefab;

	[Attribute("0", UIWidgets.Auto, desc: "Price. At or below the lot cash ceiling this is MoneyStack. Above it, the ATM balance.")]
	protected int m_iPrice;

	[Attribute("", UIWidgets.EditBox, desc: "Localization key shown on the catalog row")]
	protected string m_sNameKey;

	//------------------------------------------------------------------------------------------------
	//! Test and setup seam: an offer built in script has no attributes to fill.
	static LF_VehicleOffer Make(ResourceName prefab, int price, string nameKey)
	{
		LF_VehicleOffer offer = new LF_VehicleOffer();
		offer.m_sPrefab = prefab;
		offer.m_iPrice = price;
		offer.m_sNameKey = nameKey;
		return offer;
	}

	//------------------------------------------------------------------------------------------------
	ResourceName GetPrefab()
	{
		return m_sPrefab;
	}

	//------------------------------------------------------------------------------------------------
	ResourceName GetSpawnPrefab()
	{
		return m_sSpawnPrefab;
	}

	//------------------------------------------------------------------------------------------------
	int GetPrice()
	{
		return m_iPrice;
	}

	//------------------------------------------------------------------------------------------------
	string GetNameKey()
	{
		return m_sNameKey;
	}
}

//------------------------------------------------------------------------------------------------
//! Vehicle lot. The catalog is the offers on this component. Purchase spawns the
//! locked child, assigns the buyer as owner, and gives one key.
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "EveronLife/Feature/Vehicles", description: "Sells the configured vanilla vehicles.")]
class LF_VehicleLotComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_VehicleLotComponent : ScriptComponent
{
	[Attribute("roadmap-lot", UIWidgets.EditBox, desc: "Shop id shared with the spawn point.")]
	protected string m_sShopId;

	[Attribute("500", UIWidgets.Auto, desc: "Prices at or below this are MoneyStack. Above this, the lot debits the ATM balance.")]
	protected int m_iCashCeiling;

	[Attribute("25", UIWidgets.Auto, desc: "Price of an extra key. Follows the same cash ceiling.")]
	protected int m_iExtraKeyPrice;

	[Attribute("", UIWidgets.Object, desc: "Vehicles for sale")]
	protected ref array<ref LF_VehicleOffer> m_aOffers;

	//! Every lot in the world, on every machine, so a price board can read its lot's price list (#540).
	protected static ref array<LF_VehicleLotComponent> s_aLots;

	//------------------------------------------------------------------------------------------------
	//! eturn The lot with this shop id, or null.
	static LF_VehicleLotComponent FindLot(string shopId)
	{
		if (!s_aLots)
			return null;

		foreach (LF_VehicleLotComponent lot : s_aLots)
		{
			if (lot && lot.m_sShopId == shopId)
				return lot;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! eturn The offer for a vehicle prefab among these offers, or null. Pure; tests drive it.
	static LF_VehicleOffer OfferIn(array<ref LF_VehicleOffer> offers, ResourceName prefab)
	{
		if (!offers)
			return null;

		string wanted = prefab;
		foreach (LF_VehicleOffer offer : offers)
		{
			if (!offer)
				continue;

			string offerPrefab = offer.GetPrefab();
			if (LF_Utils.SamePrefab(offerPrefab, wanted))
				return offer;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! eturn The offer for a vehicle prefab on this lot, or null.
	LF_VehicleOffer GetOffer(ResourceName prefab)
	{
		return OfferIn(m_aOffers, prefab);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (s_aLots)
			s_aLots.RemoveItem(this);

		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!s_aLots)
			s_aLots = {};

		if (s_aLots.Find(this) == -1)
			s_aLots.Insert(this);

		if (m_sShopId.IsEmpty())
			LF_Debug.Error("Vehicle", "lot has no shop id");

		if (!m_aOffers || m_aOffers.Count() == 0)
			LF_Debug.Error("Vehicle", "lot catalog is empty");
		else
			LF_Debug.Log("Vehicle", string.Format("lot catalog %1 offers shop=%2", m_aOffers.Count(), m_sShopId));
	}

	//------------------------------------------------------------------------------------------------
	string GetShopId()
	{
		return m_sShopId;
	}

	//------------------------------------------------------------------------------------------------
	int GetCashCeiling()
	{
		return m_iCashCeiling;
	}

	//------------------------------------------------------------------------------------------------
	int GetExtraKeyPrice()
	{
		return m_iExtraKeyPrice;
	}

	//------------------------------------------------------------------------------------------------
	int GetOfferCount()
	{
		if (!m_aOffers)
			return 0;

		return m_aOffers.Count();
	}

	//------------------------------------------------------------------------------------------------
	LF_VehicleOffer GetOfferAt(int index)
	{
		if (!m_aOffers || index < 0 || index >= m_aOffers.Count())
			return null;

		return m_aOffers.Get(index);
	}

	//------------------------------------------------------------------------------------------------
	int GetPrice(ResourceName prefab)
	{
		LF_VehicleOffer offer = FindOffer(prefab);
		if (!offer)
			return 0;

		return offer.GetPrice();
	}

	//------------------------------------------------------------------------------------------------
	bool Buy(string buyerUid, ResourceName prefab, IEntity buyer, out string vehicleId, out int cashCharged, out string reasonKey)
	{
		vehicleId = "";
		cashCharged = 0;
		reasonKey = "#EL-Vehicle_NotSold";

		LF_VehicleOffer offer = FindOffer(prefab);
		if (!offer)
		{
			LF_Debug.Error("Vehicle", string.Format("purchase refused: %1 is not on the lot", prefab));
			return false;
		}

		vector origin;
		if (!LF_VehicleSpawnPointComponent.FindFreeOrigin(m_sShopId, origin))
		{
			reasonKey = "#EL-Vehicle_NoSpawn";
			return false;
		}

		return LF_VehicleOwnership.Buy(buyerUid, offer.GetPrefab(), offer.GetSpawnPrefab(), offer.GetPrice(), m_iCashCeiling, m_sShopId, offer.GetNameKey(), origin, buyer, vehicleId, cashCharged, reasonKey);
	}

	//------------------------------------------------------------------------------------------------
	bool IssueExtraKey(string buyerUid, IEntity buyer, out int cashCharged)
	{
		cashCharged = 0;
		string vehicleId = LF_VehicleOwnership.LatestInWorld(buyerUid);
		if (vehicleId.IsEmpty())
			vehicleId = LF_VehicleOwnership.LatestStored(buyerUid);

		if (vehicleId.IsEmpty())
		{
			LF_Debug.Log("Vehicle", "extra key rejected: this player owns nothing");
			return false;
		}

		vector origin = "0 0 0";
		if (GetOwner())
			origin = GetOwner().GetOrigin();

		return LF_VehicleOwnership.GiveExtraKey(vehicleId, buyerUid, m_iExtraKeyPrice, m_iCashCeiling, buyer, origin, cashCharged);
	}

	//------------------------------------------------------------------------------------------------
	protected LF_VehicleOffer FindOffer(ResourceName prefab)
	{
		return OfferIn(m_aOffers, prefab);
	}
}
