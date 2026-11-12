// LF_VehiclePrices.c — one base price per vehicle, a multiplier per action (#688).
//
// Altis Life shape (ideas only, no code): each vehicle is priced once and buy, sell-back, chop
// payout, impound fee and garage fee derive from percent multipliers. An owner retunes one number
// per vehicle; an optional per-action override pins one price. The data lives in
// Configs/Vehicles/LF_VehiclePrices.conf. The class is a plain config class so the config service
// (#283) can adopt it as the `vehicles` section; until then it reads the .conf only.
//
// Vehicles are keyed by prefab GUID, never by path (`{GUID}path` compares by GUID, case-folded).
// The lot sells the vanilla prefab and the chop shop sees the locked child, so an entry lists both.

//------------------------------------------------------------------------------------------------
enum LF_EVehiclePriceAction
{
	BUY,
	SELL_BACK,
	CHOP,
	IMPOUND,
	GARAGE
}

//------------------------------------------------------------------------------------------------
//! One vehicle. A -1 override means "derive from the base price and the multiplier".
[BaseContainerProps()]
class LF_VehiclePriceEntry
{
	static const int NO_OVERRIDE = -1;

	[Attribute("", UIWidgets.ResourcePickerThumbnail, "Vanilla vehicle prefab the lot sells", "et")]
	ResourceName m_Prefab;

	[Attribute("", UIWidgets.ResourcePickerThumbnail, "Locked child the buyer gets (the chop shop and impound see this one). Empty: none", "et")]
	ResourceName m_SpawnPrefab;

	[Attribute("0", UIWidgets.Auto, "The one number: what the vehicle is worth", params: "0 1000000")]
	int m_iBasePrice;

	[Attribute("-1", UIWidgets.Auto, "Buy price override; -1 derives it", params: "-1 1000000")]
	int m_iBuyOverride;

	[Attribute("-1", UIWidgets.Auto, "Sell-back override; -1 derives it", params: "-1 1000000")]
	int m_iSellBackOverride;

	[Attribute("-1", UIWidgets.Auto, "Chop payout override; -1 derives it", params: "-1 1000000")]
	int m_iChopOverride;

	[Attribute("-1", UIWidgets.Auto, "Impound fee override; -1 derives it", params: "-1 1000000")]
	int m_iImpoundOverride;

	[Attribute("-1", UIWidgets.Auto, "Garage fee override; -1 derives it", params: "-1 1000000")]
	int m_iGarageOverride;

	//------------------------------------------------------------------------------------------------
	//! Test and setup seam: an entry built in script has no attributes to fill.
	static LF_VehiclePriceEntry Make(ResourceName prefab, ResourceName spawnPrefab, int basePrice)
	{
		LF_VehiclePriceEntry entry = new LF_VehiclePriceEntry();
		entry.m_Prefab = prefab;
		entry.m_SpawnPrefab = spawnPrefab;
		entry.m_iBasePrice = basePrice;
		entry.m_iBuyOverride = NO_OVERRIDE;
		entry.m_iSellBackOverride = NO_OVERRIDE;
		entry.m_iChopOverride = NO_OVERRIDE;
		entry.m_iImpoundOverride = NO_OVERRIDE;
		entry.m_iGarageOverride = NO_OVERRIDE;
		return entry;
	}

	//------------------------------------------------------------------------------------------------
	int GetOverride(LF_EVehiclePriceAction action)
	{
		switch (action)
		{
			case LF_EVehiclePriceAction.BUY: return m_iBuyOverride;
			case LF_EVehiclePriceAction.SELL_BACK: return m_iSellBackOverride;
			case LF_EVehiclePriceAction.CHOP: return m_iChopOverride;
			case LF_EVehiclePriceAction.IMPOUND: return m_iImpoundOverride;
			case LF_EVehiclePriceAction.GARAGE: return m_iGarageOverride;
		}

		return NO_OVERRIDE;
	}
}

//------------------------------------------------------------------------------------------------
//! The pure arithmetic. No config, no entities.
class LF_VehiclePriceModel
{
	static const string HEX_DIGITS = "0123456789ABCDEF";

	//------------------------------------------------------------------------------------------------
	//! An override at or above 0 wins; otherwise base times percent. Never negative.
	//! \param[in] basePrice The vehicle's base price.
	//! \param[in] percent The action's multiplier in percent (100 = the base price).
	//! \param[in] priceOverride The per-vehicle override, -1 for none.
	//! \param[in] valueCap Caps the base before the multiplier; 0 or less means no cap.
	static int Price(int basePrice, int percent, int priceOverride, int valueCap = 0)
	{
		if (priceOverride >= 0)
			return priceOverride;

		int counted = Math.Max(basePrice, 0);
		if (valueCap > 0)
			counted = Math.Min(counted, valueCap);

		return Math.Max(counted * percent / 100, 0);
	}

	//------------------------------------------------------------------------------------------------
	//! Same prefab, compared by GUID so a path's casing cannot split them.
	static bool SameResource(ResourceName a, ResourceName b)
	{
		if (a.IsEmpty() || b.IsEmpty())
			return false;

		return GuidOf(a) == GuidOf(b);
	}

	//------------------------------------------------------------------------------------------------
	//! An owner record key as a prefab GUID: 16 hex digits, braces and case ignored.
	//! \return True and the upper-case GUID when the key is one.
	static bool ParseGuidKey(string key, out string guid)
	{
		string text = key;
		text.Trim();
		if (text.StartsWith("{") && text.EndsWith("}"))
			text = text.Substring(1, text.Length() - 2);

		text.ToUpper();
		if (text.Length() != 16)
			return false;

		for (int i = 0; i < 16; i++)
		{
			if (HEX_DIGITS.IndexOf(text.Substring(i, 1)) < 0)
				return false;
		}

		guid = text;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! "{GUID}path" to "GUID" in upper case; a bare path comes back upper-cased whole.
	static string GuidOf(ResourceName resource)
	{
		string text = resource;
		if (text.StartsWith("{"))
		{
			int close = text.IndexOf("}");
			if (close > 1)
				text = text.Substring(1, close - 1);
		}

		text.ToUpper();
		return text;
	}
}

//------------------------------------------------------------------------------------------------
//! Vehicle prices. Loaded from Configs/Vehicles/LF_VehiclePrices.conf; a pack overrides the file.
[BaseContainerProps(configRoot: true)]
class LF_VehiclePricesConfig
{
	static const ResourceName CONFIG_PATH = "{B2D47E19A05C6F38}Configs/Vehicles/LF_VehiclePrices.conf";

	//! The owner config.json section #283 will read.
	static const string SECTION = "vehicles";

	//! The limits of the price attributes' `params`; the owner reader refuses a number outside them (#283).
	static const int PRICE_MIN = 0;	// lf-fixed: the schema limit, mirrors the attribute params
	static const int PRICE_MAX = 1000000;	// lf-fixed: the schema limit, mirrors the attribute params
	static const int OVERRIDE_MIN = -1;	// lf-fixed: the schema limit, -1 is "derive it"
	static const int PERCENT_MIN = 0;	// lf-fixed: the schema limit, mirrors the attribute params
	static const int PERCENT_MAX = 1000;	// lf-fixed: the schema limit, ten times the base price

	[Attribute("100", UIWidgets.Auto, "Percent of the base price the lot charges", params: "0 1000")]
	int m_iBuyPercent;

	[Attribute("50", UIWidgets.Auto, "Percent of the base price a sell-back pays (no desk consumes it yet)", params: "0 1000")]
	int m_iSellBackPercent;

	[Attribute("30", UIWidgets.Auto, "Percent of the (capped) base price the chop shop pays", params: "0 1000")]
	int m_iChopPercent;

	[Attribute("3000", UIWidgets.Auto, "The chop shop counts no vehicle for more than this; 0 for no cap", params: "0 1000000")]
	int m_iChopValueCap;

	[Attribute("10", UIWidgets.Auto, "Percent of the base price the impound desk charges", params: "0 1000")]
	int m_iImpoundPercent;

	[Attribute("0", UIWidgets.Auto, "Percent of the base price a garage charges (free today)", params: "0 1000")]
	int m_iGaragePercent;

	[Attribute("", UIWidgets.Object, "Per-vehicle base prices and overrides")]
	ref array<ref LF_VehiclePriceEntry> m_aVehicles;

	protected static ref LF_VehiclePricesConfig s_Config;
	protected static bool s_bLoadAttempted;

	//------------------------------------------------------------------------------------------------
	//! The loaded config, or null (logged once) when the file does not resolve.
	static LF_VehiclePricesConfig Get()
	{
		if (s_bLoadAttempted)
			return s_Config;

		s_bLoadAttempted = true;
		Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
		if (!container || !container.GetResource())
		{
			LF_Debug.Error("Vehicles", string.Format("vehicle prices config does not resolve: %1", CONFIG_PATH));
			return null;
		}

		s_Config = LF_VehiclePricesConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!s_Config)
			LF_Debug.Error("Vehicles", string.Format("vehicle prices config has an unexpected root class: %1", CONFIG_PATH));
		else
			s_Config.ApplyLayers();

		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	//! Merges the owner's "vehicles" section over this instance, which holds the core `.conf`.
	void ApplyLayers()
	{
		LF_ConfigReader reader = LF_ConfigService.OpenOwnerSection(SECTION);
		if (!reader)
			return;

		ApplyOwner(reader);
		LF_Debug.Info(LF_ConfigService.FEATURE, string.Format("vehicles: owner config read %1 fields, rejected %2", reader.GetAppliedCount(), reader.GetRejectedCount()));
	}

	//------------------------------------------------------------------------------------------------
	//! Applies the owner's "vehicles" section field by field: a key the owner leaves out keeps the
	//! shipped value. `vehicles` is an object keyed by the vehicle's prefab GUID (16 hex digits); a
	//! record changes the entry whose lot prefab or locked child has that GUID, and a GUID no entry
	//! has adds a vehicle, which must set basePrice (an unpriced vehicle would sell for nothing).
	//! \param[in] reader A reader on the section.
	void ApplyOwner(notnull LF_ConfigReader reader)
	{
		array<string> ownerKeys = {"buyPercent", "sellBackPercent", "chopPercent", "chopValueCap", "impoundPercent", "garagePercent", "vehicles"};
		reader.WarnUnknownKeys(ownerKeys);
		reader.ReadInt("buyPercent", m_iBuyPercent, PERCENT_MIN, PERCENT_MAX);
		reader.ReadInt("sellBackPercent", m_iSellBackPercent, PERCENT_MIN, PERCENT_MAX);
		reader.ReadInt("chopPercent", m_iChopPercent, PERCENT_MIN, PERCENT_MAX);
		reader.ReadInt("chopValueCap", m_iChopValueCap, PRICE_MIN, PRICE_MAX);
		reader.ReadInt("impoundPercent", m_iImpoundPercent, PERCENT_MIN, PERCENT_MAX);
		reader.ReadInt("garagePercent", m_iGaragePercent, PERCENT_MIN, PERCENT_MAX);

		if (!m_aVehicles)
			m_aVehicles = new array<ref LF_VehiclePriceEntry>();

		int count = reader.BeginRecords("vehicles");
		for (int i = 0; i < count; i++)
		{
			string key = reader.GetRecordKey(i);
			if (!reader.BeginRecord(key))
				continue;

			string guid;
			if (!LF_VehiclePriceModel.ParseGuidKey(key, guid))
			{
				reader.RejectRecord("the key is not a prefab GUID (16 hex digits)");
				reader.EndRecord();
				continue;
			}

			LF_VehiclePriceEntry entry = FindByGuid(guid);
			if (!entry && !reader.Has("basePrice"))
			{
				reader.RejectRecord("a vehicle the shipped prices do not list needs a basePrice");
				reader.EndRecord();
				continue;
			}

			if (!entry)
			{
				ResourceName added = "{" + guid + "}";
				entry = LF_VehiclePriceEntry.Make(added, "", 0);
				m_aVehicles.Insert(entry);
			}

			array<string> recordKeys = {"spawnPrefab", "basePrice", "buyOverride", "sellBackOverride", "chopOverride", "impoundOverride", "garageOverride"};
			reader.WarnUnknownKeys(recordKeys);
			string spawn = entry.m_SpawnPrefab;
			if (reader.ReadString("spawnPrefab", spawn))
				entry.m_SpawnPrefab = spawn;
			reader.ReadInt("basePrice", entry.m_iBasePrice, PRICE_MIN, PRICE_MAX);
			reader.ReadInt("buyOverride", entry.m_iBuyOverride, OVERRIDE_MIN, PRICE_MAX);
			reader.ReadInt("sellBackOverride", entry.m_iSellBackOverride, OVERRIDE_MIN, PRICE_MAX);
			reader.ReadInt("chopOverride", entry.m_iChopOverride, OVERRIDE_MIN, PRICE_MAX);
			reader.ReadInt("impoundOverride", entry.m_iImpoundOverride, OVERRIDE_MIN, PRICE_MAX);
			reader.ReadInt("garageOverride", entry.m_iGarageOverride, OVERRIDE_MIN, PRICE_MAX);
			reader.EndRecord();
		}

		if (count >= 0)
			reader.EndRecords();
	}

	//------------------------------------------------------------------------------------------------
	//! The entry whose lot prefab or locked child has this upper-case GUID. Null when unlisted.
	LF_VehiclePriceEntry FindByGuid(string guid)
	{
		if (!m_aVehicles)
			return null;

		foreach (LF_VehiclePriceEntry entry : m_aVehicles)
		{
			if (!entry)
				continue;

			if (LF_VehiclePriceModel.GuidOf(entry.m_Prefab) == guid || LF_VehiclePriceModel.GuidOf(entry.m_SpawnPrefab) == guid)
				return entry;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! The entry whose lot prefab or locked child is this prefab, by GUID. Null when unlisted.
	LF_VehiclePriceEntry Find(ResourceName prefab)
	{
		if (!m_aVehicles)
			return null;

		foreach (LF_VehiclePriceEntry entry : m_aVehicles)
		{
			if (!entry)
				continue;

			if (LF_VehiclePriceModel.SameResource(entry.m_Prefab, prefab) || LF_VehiclePriceModel.SameResource(entry.m_SpawnPrefab, prefab))
				return entry;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	int PercentFor(LF_EVehiclePriceAction action)
	{
		switch (action)
		{
			case LF_EVehiclePriceAction.BUY: return m_iBuyPercent;
			case LF_EVehiclePriceAction.SELL_BACK: return m_iSellBackPercent;
			case LF_EVehiclePriceAction.CHOP: return m_iChopPercent;
			case LF_EVehiclePriceAction.IMPOUND: return m_iImpoundPercent;
			case LF_EVehiclePriceAction.GARAGE: return m_iGaragePercent;
		}

		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! What an action costs (or pays) for this vehicle. -1 when the vehicle is unlisted, so the
	//! caller keeps its own fallback and a missing entry is never read as "free".
	int PriceFor(LF_EVehiclePriceAction action, ResourceName prefab)
	{
		LF_VehiclePriceEntry entry = Find(prefab);
		if (!entry)
			return -1;

		int cap = 0;
		if (action == LF_EVehiclePriceAction.CHOP)
			cap = m_iChopValueCap;

		return LF_VehiclePriceModel.Price(entry.m_iBasePrice, PercentFor(action), entry.GetOverride(action), cap);
	}

	//------------------------------------------------------------------------------------------------
	//! \return The price from the shipped config, or -1 when the config or the vehicle is missing.
	static int Lookup(LF_EVehiclePriceAction action, ResourceName prefab)
	{
		LF_VehiclePricesConfig config = Get();
		if (!config)
			return -1;

		return config.PriceFor(action, prefab);
	}
}
