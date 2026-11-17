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
	//! Make with the locked child the buyer gets (a test seam, like Make).
	static LF_VehicleOffer MakeWithSpawn(ResourceName prefab, ResourceName spawnPrefab, int price, string nameKey)
	{
		LF_VehicleOffer offer = Make(prefab, price, nameKey);
		offer.m_sSpawnPrefab = spawnPrefab;
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
	//! What the lot charges: the vehicle prices config (#283) when it lists this vehicle, else the
	//! price written on the offer.
	int GetPrice()
	{
		return PriceIn(LF_VehiclePricesConfig.Get());
	}

	//------------------------------------------------------------------------------------------------
	//! GetPrice against a given config; null or an unlisted vehicle keeps the offer's own price.
	int PriceIn(LF_VehiclePricesConfig config)
	{
		if (!config)
			return m_iPrice;

		int configured = config.PriceFor(LF_EVehiclePriceAction.BUY, m_sPrefab);
		if (configured < 0)
			return m_iPrice;

		return configured;
	}

	//------------------------------------------------------------------------------------------------
	//! False when an override removed this vehicle from the price list: the lot neither shows nor sells it.
	bool IsOfferedIn(LF_VehiclePricesConfig config)
	{
		if (!config)
			return true;

		return !config.IsRemoved(m_sPrefab);
	}

	//------------------------------------------------------------------------------------------------
	string GetNameKey()
	{
		return m_sNameKey;
	}

	//------------------------------------------------------------------------------------------------
	//! How many colours this model sells in (epic #1826): its colour row's length, or 1 with no row.
	int ColourCount()
	{
		return ColourCountIn(LF_VehicleColoursConfig.Get());
	}

	//------------------------------------------------------------------------------------------------
	int ColourCountIn(LF_VehicleColoursConfig colours)
	{
		if (!colours)
			return 1;

		return colours.CountFor(m_sPrefab);
	}

	//------------------------------------------------------------------------------------------------
	//! True when colour is one this offer sells. A model with no colour row sells colour 0 only.
	bool ColourValid(int colour)
	{
		return ColourValidIn(LF_VehicleColoursConfig.Get(), colour);
	}

	//------------------------------------------------------------------------------------------------
	bool ColourValidIn(LF_VehicleColoursConfig colours, int colour)
	{
		return colour >= 0 && colour < ColourCountIn(colours);
	}

	//------------------------------------------------------------------------------------------------
	//! The locked child that spawns for this colour. No colour row, or a colour with no prefab,
	//! gives the offer's own spawn prefab (today's car).
	ResourceName SpawnPrefabFor(int colour)
	{
		return SpawnPrefabForIn(LF_VehicleColoursConfig.Get(), colour);
	}

	//------------------------------------------------------------------------------------------------
	ResourceName SpawnPrefabForIn(LF_VehicleColoursConfig colours, int colour)
	{
		if (colours)
		{
			LF_VehicleColour entry = colours.ColourAt(m_sPrefab, colour);
			if (entry && !entry.m_SpawnPrefab.IsEmpty())
				return entry.m_SpawnPrefab;
		}

		return m_sSpawnPrefab;
	}

	//------------------------------------------------------------------------------------------------
	//! What the lot charges for this colour: the model's price plus the colour's delta.
	int PriceFor(int colour)
	{
		return PriceForIn(LF_VehiclePricesConfig.Get(), LF_VehicleColoursConfig.Get(), colour);
	}

	//------------------------------------------------------------------------------------------------
	int PriceForIn(LF_VehiclePricesConfig prices, LF_VehicleColoursConfig colours, int colour)
	{
		int price = PriceIn(prices);
		if (colours)
		{
			LF_VehicleColour entry = colours.ColourAt(m_sPrefab, colour);
			if (entry)
				price += entry.m_iPriceDelta;
		}

		return price;
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

	[Attribute("0", UIWidgets.CheckBox, desc: "Playtest: this lot's first two offers are the test cars a new character is granted (LF_SpawnLogic.m_bGrantTestCars). One lot per world.")]
	protected bool m_bTestCarLot;

	//! Every lot in the world, on every machine, so a price board can read its lot's price list (#540).
	protected static ref array<LF_VehicleLotComponent> s_aLots;

	//------------------------------------------------------------------------------------------------
	//! Returns the lot flagged m_bTestCarLot, or null.
	static LF_VehicleLotComponent FindTestCarLot()
	{
		if (!s_aLots)
			return null;

		foreach (LF_VehicleLotComponent lot : s_aLots)
		{
			if (lot && lot.m_bTestCarLot)
				return lot;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Returns the lot with this shop id, or null.
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

		LF_VehiclePricesConfig config = LF_VehiclePricesConfig.Get();
		int count;
		foreach (LF_VehicleOffer offer : m_aOffers)
		{
			if (offer && offer.IsOfferedIn(config))
				count++;
		}

		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! The index-th offer still on sale: a vehicle an override removed is skipped, so the sheet rows
	//! and the Buy index agree (#1741).
	LF_VehicleOffer GetOfferAt(int index)
	{
		if (!m_aOffers || index < 0)
			return null;

		LF_VehiclePricesConfig config = LF_VehiclePricesConfig.Get();
		int seen;
		foreach (LF_VehicleOffer offer : m_aOffers)
		{
			if (!offer || !offer.IsOfferedIn(config))
				continue;

			if (seen == index)
				return offer;

			seen++;
		}

		return null;
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
		return BuyColour(buyerUid, prefab, 0, buyer, vehicleId, cashCharged, reasonKey);
	}

	//------------------------------------------------------------------------------------------------
	//! Buy in a colour (epic #1826). The server checks the index, whatever the client sent: a colour
	//! the model does not sell is refused and logged, and nothing is charged.
	bool BuyColour(string buyerUid, ResourceName prefab, int colour, IEntity buyer, out string vehicleId, out int cashCharged, out string reasonKey)
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

		if (!offer.ColourValid(colour))
		{
			LF_Debug.Error("Vehicle", string.Format("purchase refused: colour %1 is not sold for %2 (%3 colour(s))", colour, prefab, offer.ColourCount()));
			return false;
		}

		vector origin;
		if (!LF_VehicleSpawnPointComponent.FindFreeOrigin(m_sShopId, origin))
		{
			reasonKey = "#EL-Vehicle_NoSpawn";
			return false;
		}

		return LF_VehicleOwnership.Buy(buyerUid, offer.GetPrefab(), offer.SpawnPrefabFor(colour), offer.PriceFor(colour), m_iCashCeiling, m_sShopId, offer.GetNameKey(), origin, buyer, vehicleId, cashCharged, reasonKey);
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
	//! The car the change-locks row means (#2411): the owner's latest car in the world, else the
	//! latest stored one (the same choice as IssueExtraKey). The sheet prints its plate, so the
	//! owner sees which car is re-keyed before paying. "" when the player owns nothing.
	static string LocksTarget(string ownerUid)
	{
		string vehicleId = LF_VehicleOwnership.LatestInWorld(ownerUid);
		if (vehicleId.IsEmpty())
			vehicleId = LF_VehicleOwnership.LatestStored(ownerUid);

		return vehicleId;
	}

	//------------------------------------------------------------------------------------------------
	//! Change the locks on LocksTarget, for the price of a key: every key cut before stops working
	//! and the owner gets the one new key.
	bool ChangeLocks(string ownerUid, IEntity owner, out int cashCharged, out string reasonKey)
	{
		cashCharged = 0;
		reasonKey = "#EL-Vehicle_NotOwner";
		string vehicleId = LocksTarget(ownerUid);
		if (vehicleId.IsEmpty())
		{
			LF_Debug.Log("Vehicle", "change locks rejected: this player owns nothing");
			return false;
		}

		vector origin = "0 0 0";
		if (GetOwner())
			origin = GetOwner().GetOrigin();

		return LF_VehicleOwnership.ChangeLocks(vehicleId, ownerUid, m_iExtraKeyPrice, m_iCashCeiling, owner, origin, cashCharged, reasonKey);
	}

	//------------------------------------------------------------------------------------------------
	protected LF_VehicleOffer FindOffer(ResourceName prefab)
	{
		LF_VehicleOffer offer = OfferIn(m_aOffers, prefab);
		if (offer && !offer.IsOfferedIn(LF_VehiclePricesConfig.Get()))
			return null;

		return offer;
	}
}
