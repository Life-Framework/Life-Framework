//------------------------------------------------------------------------------------------------
//! One row on the lot. m_sPrefab is the vanilla vehicle the player buys.
//! m_sSpawnPrefab is the locked child that carries EL_VehicleLockComponent.
//------------------------------------------------------------------------------------------------
[BaseContainerProps()]
class EL_VehicleOffer
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
class EL_VehicleLotComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class EL_VehicleLotComponent : ScriptComponent
{
	[Attribute("roadmap-lot", UIWidgets.EditBox, desc: "Shop id shared with the spawn point.")]
	protected string m_sShopId;

	[Attribute("500", UIWidgets.Auto, desc: "Prices at or below this are MoneyStack. Above this, the lot debits the ATM balance.")]
	protected int m_iCashCeiling;

	[Attribute("25", UIWidgets.Auto, desc: "Price of an extra key. Follows the same cash ceiling.")]
	protected int m_iExtraKeyPrice;

	[Attribute("", UIWidgets.Object, desc: "Vehicles for sale")]
	protected ref array<ref EL_VehicleOffer> m_aOffers;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (m_sShopId.IsEmpty())
			EL_Debug.Error("Vehicle", "lot has no shop id");

		if (!m_aOffers || m_aOffers.Count() == 0)
			EL_Debug.Error("Vehicle", "lot catalog is empty");
		else
			EL_Debug.Log("Vehicle", string.Format("lot catalog %1 offers shop=%2", m_aOffers.Count(), m_sShopId));
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
	EL_VehicleOffer GetOfferAt(int index)
	{
		if (!m_aOffers || index < 0 || index >= m_aOffers.Count())
			return null;

		return m_aOffers.Get(index);
	}

	//------------------------------------------------------------------------------------------------
	int GetPrice(ResourceName prefab)
	{
		EL_VehicleOffer offer = FindOffer(prefab);
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

		EL_VehicleOffer offer = FindOffer(prefab);
		if (!offer)
		{
			EL_Debug.Error("Vehicle", string.Format("purchase refused: %1 is not on the lot", prefab));
			return false;
		}

		vector origin;
		if (!EL_VehicleSpawnPointComponent.FindFreeOrigin(m_sShopId, origin))
		{
			reasonKey = "#EL-Vehicle_NoSpawn";
			return false;
		}

		return EL_VehicleOwnership.Buy(buyerUid, offer.GetPrefab(), offer.GetSpawnPrefab(), offer.GetPrice(), m_iCashCeiling, m_sShopId, offer.GetNameKey(), origin, buyer, vehicleId, cashCharged, reasonKey);
	}

	//------------------------------------------------------------------------------------------------
	bool IssueExtraKey(string buyerUid, IEntity buyer, out int cashCharged)
	{
		cashCharged = 0;
		string vehicleId = EL_VehicleOwnership.LatestInWorld(buyerUid);
		if (vehicleId.IsEmpty())
			vehicleId = EL_VehicleOwnership.LatestStored(buyerUid);

		if (vehicleId.IsEmpty())
		{
			EL_Debug.Log("Vehicle", "extra key rejected: this player owns nothing");
			return false;
		}

		vector origin = "0 0 0";
		if (GetOwner())
			origin = GetOwner().GetOrigin();

		return EL_VehicleOwnership.GiveExtraKey(vehicleId, buyerUid, m_iExtraKeyPrice, m_iCashCeiling, buyer, origin, cashCharged);
	}

	//------------------------------------------------------------------------------------------------
	protected EL_VehicleOffer FindOffer(ResourceName prefab)
	{
		if (!m_aOffers)
			return null;

		string wanted = prefab;
		foreach (EL_VehicleOffer offer : m_aOffers)
		{
			if (!offer)
				continue;

			string offerPrefab = offer.GetPrefab();
			if (offerPrefab == wanted)
				return offer;
		}

		return null;
	}
}
