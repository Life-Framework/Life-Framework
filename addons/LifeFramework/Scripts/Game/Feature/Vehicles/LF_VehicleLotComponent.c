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
	//! #4417: false when an override removed this vehicle, or its price row names another lot.
	bool IsOfferedAt(LF_VehiclePricesConfig config, string lotId)
	{
		if (!config)
			return true;

		return config.IsOfferedAt(m_sPrefab, lotId);
	}

	//------------------------------------------------------------------------------------------------
	//! The capacity class the price row names ("truck"), or "" when the row names none.
	string CapacityClassIn(LF_VehiclePricesConfig config)
	{
		if (!config)
			return string.Empty;

		return config.CapacityClassOf(m_sPrefab);
	}

	//------------------------------------------------------------------------------------------------
	//! An offer a lot lists straight from its price row (a lot with no placed offers of its own).
	static LF_VehicleOffer FromRow(notnull LF_VehiclePriceEntry entry)
	{
		LF_VehicleOffer offer = MakeWithSpawn(entry.m_Prefab, entry.m_SpawnPrefab, entry.m_iBasePrice, entry.m_sNameKey);
		return offer;
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

	[Attribute("", UIWidgets.EditBox, desc: "Lot id (#4417). The lot sells the price rows that name it, and every row that names none. Empty: only the rows that name no lot (the car lot).")]
	protected string m_sLotId;

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
	//! \return The offer for a vehicle prefab among these offers, or null. Pure; tests drive it.
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
	//! The first lot in the world that offers this vehicle prefab (a starter car, LF_StarterVehicle).
	//! \param[out] shopId That lot's shop id, empty when none offers it.
	//! \return The offer, or null when no lot offers the prefab.
	static LF_VehicleOffer FindOfferOnAnyLot(ResourceName prefab, out string shopId)
	{
		shopId = string.Empty;
		if (!s_aLots || prefab.IsEmpty())
			return null;

		foreach (LF_VehicleLotComponent lot : s_aLots)
		{
			if (!lot)
				continue;

			LF_VehicleOffer offer = lot.GetOffer(prefab);
			if (offer)
			{
				shopId = lot.m_sShopId;
				return offer;
			}
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The offer for a vehicle prefab on this lot, or null.
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

		// A lot with an id may list only its price rows (the truck yard); a lot with neither has nothing to sell.
		if ((!m_aOffers || m_aOffers.Count() == 0) && m_sLotId.IsEmpty())
			LF_Debug.Error("Vehicle", "lot catalog is empty");
		else
			LF_Debug.Log("Vehicle", string.Format("lot catalog %1 offers shop=%2 lot=%3", GetOfferCount(), m_sShopId, m_sLotId));
	}

	//------------------------------------------------------------------------------------------------
	string GetLotId()
	{
		return m_sLotId;
	}

	//------------------------------------------------------------------------------------------------
	//! #4417: the offers a lot with this id sells: its placed offers whose price row is not another
	//! lot's, then the price rows that name the lot and are not placed yet. Pure; tests drive it.
	//! \param[in] lotId The lot's id; empty for the car lot, which sells only rows that name no lot.
	//! \param[in] placed The offers set on the lot entity, or null.
	//! \param[in] config The vehicle prices, or null (then every placed offer is sold).
	//! \param[out] result The offers on sale, in list order.
	static void CollectOffers(string lotId, array<ref LF_VehicleOffer> placed, LF_VehiclePricesConfig config, notnull array<ref LF_VehicleOffer> result)
	{
		if (placed)
		{
			foreach (LF_VehicleOffer offer : placed)
			{
				if (offer && offer.IsOfferedAt(config, lotId))
					result.Insert(offer);
			}
		}

		if (lotId.IsEmpty() || !config || !config.m_aVehicles)
			return;

		foreach (LF_VehiclePriceEntry entry : config.m_aVehicles)
		{
			if (!entry || entry.m_sLotId != lotId || entry.m_Prefab.IsEmpty())
				continue;

			if (config.IsRemoved(entry.m_Prefab) || OfferIn(result, entry.m_Prefab))
				continue;

			result.Insert(LF_VehicleOffer.FromRow(entry));
		}
	}

	//! #4417: the truck yard's refusal when the buyer has no truck licence. Its %1 is the check fee.
	static const string KEY_NEEDS_TRUCK = "#LF-Lot_NeedsTruckLicence";
	static const string CLASS_TRUCK = "truck";

	//------------------------------------------------------------------------------------------------
	//! The refusal key when this offer is a truck and the buyer has not passed the truck check, else "".
	static string TruckRefusal(LF_VehicleOffer offer, string buyerUid, LF_VehiclePricesConfig config)
	{
		if (!offer || offer.CapacityClassIn(config) != CLASS_TRUCK)
			return string.Empty;

		if (LF_DrivingCheck.HoldsTruckLicence(buyerUid))
			return string.Empty;

		return KEY_NEEDS_TRUCK;
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
		array<ref LF_VehicleOffer> offers = {};
		CollectOffers(m_sLotId, m_aOffers, LF_VehiclePricesConfig.Get(), offers);
		return offers.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! The index-th offer still on sale: a vehicle an override removed is skipped, so the sheet rows
	//! and the Buy index agree (#1741).
	LF_VehicleOffer GetOfferAt(int index)
	{
		if (index < 0)
			return null;

		array<ref LF_VehicleOffer> offers = {};
		CollectOffers(m_sLotId, m_aOffers, LF_VehiclePricesConfig.Get(), offers);
		if (index >= offers.Count())
			return null;

		return offers[index];
	}

	//------------------------------------------------------------------------------------------------
	int GetPrice(ResourceName prefab)
	{
		LF_VehicleOffer offer = FindOffer(prefab);
		if (!offer)
			return 0;

		return offer.GetPrice();
	}

	//! #3151: the lot's refusal when the server asks for a driving licence and the buyer has none.
	//! Its %1 is the licence fee (ReasonArg); the pack's row with the same Id names the place.
	static const string KEY_NEEDS_DRIVING = "#LF-Lot_NeedsDrivingLicence";

	//------------------------------------------------------------------------------------------------
	//! Whether the buyer may buy here as far as the driving licence goes: always when the server's
	//! vehicles config does not ask for it or the License system is off (nobody could get one).
	static bool LicenceAllows(string buyerUid, IEntity buyer)
	{
		if (!LF_VehiclePricesConfig.LotNeedsDrivingLicence())
			return true;

		if (!LF_Systems.IsEnabled(LF_LicenseGate.SYSTEM))
			return true;

		return LF_LicenceDesk.HoldsLicence(buyer, buyerUid, LF_LicenceDesk.DRIVING);
	}

	//------------------------------------------------------------------------------------------------
	//! The argument a refusal's toast fills in: the licence fee for KEY_NEEDS_DRIVING, else empty.
	static string ReasonArg(string reasonKey)
	{
		if (reasonKey == KEY_NEEDS_DRIVING)
			return LF_MoneyFormat.FormatMoney(LF_LicencesConfig.DriverLicenceFee());

		if (reasonKey == KEY_NEEDS_TRUCK)
			return LF_MoneyFormat.FormatMoney(LF_DrivingCheck.FEE);

		return string.Empty;
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

		// #3151: the licence gate, before any cash moves. The server reads the buyer's licence.
		if (!LicenceAllows(buyerUid, buyer))
		{
			reasonKey = KEY_NEEDS_DRIVING;
			LF_Debug.Info("Vehicle", string.Format("purchase refused for %1: no driving licence", buyerUid));
			return false;
		}

		// #4417: a truck needs the truck licence, before any cash moves.
		string truckReason = TruckRefusal(offer, buyerUid, LF_VehiclePricesConfig.Get());
		if (!truckReason.IsEmpty())
		{
			reasonKey = truckReason;
			LF_Debug.Info("Vehicle", string.Format("purchase refused for %1: no truck licence", buyerUid));
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
		array<ref LF_VehicleOffer> offers = {};
		CollectOffers(m_sLotId, m_aOffers, LF_VehiclePricesConfig.Get(), offers);
		return OfferIn(offers, prefab);
	}
}
