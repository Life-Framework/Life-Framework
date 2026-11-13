//------------------------------------------------------------------------------------------------
//! Organization tuning: the founding fee, name length, offer lifetime, fund cash limit, upgrade
//! prices and the desk reach. Loaded from Configs/Organizations/LF_Org.conf. A missing or broken
//! file logs once and falls back to the same defaults.
[BaseContainerProps(configRoot: true)]
class LF_OrgConfig
{
	static const ResourceName CONFIG_PATH = "{EA86F8E6A6BCA687}Configs/Organizations/LF_Org.conf";

	static const int DEFAULT_FOUNDING_FEE = 250;
	static const int DEFAULT_NAME_MIN = 3;
	static const int DEFAULT_NAME_MAX = 32;
	static const int DEFAULT_OFFER_MS = 120000;
	static const int DEFAULT_MAX_CASH_MOVE = 1000000;
	static const int DEFAULT_PRICE_EXTRA_STORAGE = 500;
	static const int DEFAULT_PRICE_PROCESSING_SLOT = 250;
	static const int DEFAULT_PRICE_PARKED_VEHICLE = 750;
	static const float DEFAULT_DESK_RANGE = 5;

	[Attribute("250", UIWidgets.Auto, "Carried cash the founder hands over at the registry desk to found an organization", params: "0 10000000")]
	int m_iFoundingFee = 250;

	[Attribute("3", UIWidgets.Auto, "Shortest organization name, in characters", params: "1 64")]
	int m_iNameMin = 3;

	[Attribute("32", UIWidgets.Auto, "Longest organization name, in characters", params: "1 64")]
	int m_iNameMax = 32;

	[Attribute("120000", UIWidgets.Auto, "Milliseconds an invitation stands before the invitee has to be asked again", params: "5000 3600000")]
	int m_iOfferMs = 120000;

	[Attribute("1000000", UIWidgets.Auto, "Largest single cash move into or out of an organization fund", params: "1 100000000")]
	int m_iMaxCashMove = 1000000;

	[Attribute("500", UIWidgets.Auto, "Fund price of the extra storage upgrade", params: "0 10000000")]
	int m_iPriceExtraStorage = 500;

	[Attribute("250", UIWidgets.Auto, "Fund price of the processing slot upgrade", params: "0 10000000")]
	int m_iPriceProcessingSlot = 250;

	[Attribute("750", UIWidgets.Auto, "Fund price of the parked vehicle upgrade", params: "0 10000000")]
	int m_iPriceParkedVehicle = 750;

	[Attribute("5", UIWidgets.Auto, "Metres a player may stand from an organization desk when the server moves fund cash", params: "1 30")]
	float m_fDeskRange = 5;

	protected static ref LF_OrgConfig s_Config;
	protected static bool s_bLoadAttempted;

	//------------------------------------------------------------------------------------------------
	//! The loaded config, or null (logged once) when the file does not resolve.
	static LF_OrgConfig Get()
	{
		if (s_bLoadAttempted)
			return s_Config;

		s_bLoadAttempted = true;
		Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
		if (!container || !container.GetResource())
		{
			LF_Debug.Error("Org", string.Format("organization config does not resolve, using defaults: %1", CONFIG_PATH));
			return null;
		}

		s_Config = LF_OrgConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!s_Config)
			LF_Debug.Error("Org", string.Format("organization config has an unexpected root class, using defaults: %1", CONFIG_PATH));

		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	static int FoundingFee()
	{
		LF_OrgConfig config = Get();
		if (!config || config.m_iFoundingFee < 0)
			return DEFAULT_FOUNDING_FEE;

		return config.m_iFoundingFee;
	}

	//------------------------------------------------------------------------------------------------
	static int NameMin()
	{
		LF_OrgConfig config = Get();
		if (!config || config.m_iNameMin < 1)
			return DEFAULT_NAME_MIN;

		return config.m_iNameMin;
	}

	//------------------------------------------------------------------------------------------------
	static int NameMax()
	{
		LF_OrgConfig config = Get();
		if (!config || config.m_iNameMax < 1)
			return DEFAULT_NAME_MAX;

		return config.m_iNameMax;
	}

	//------------------------------------------------------------------------------------------------
	static int OfferMs()
	{
		LF_OrgConfig config = Get();
		if (!config || config.m_iOfferMs <= 0)
			return DEFAULT_OFFER_MS;

		return config.m_iOfferMs;
	}

	//------------------------------------------------------------------------------------------------
	static int MaxCashMove()
	{
		LF_OrgConfig config = Get();
		if (!config || config.m_iMaxCashMove <= 0)
			return DEFAULT_MAX_CASH_MOVE;

		return config.m_iMaxCashMove;
	}

	//------------------------------------------------------------------------------------------------
	static int PriceExtraStorage()
	{
		LF_OrgConfig config = Get();
		if (!config || config.m_iPriceExtraStorage < 0)
			return DEFAULT_PRICE_EXTRA_STORAGE;

		return config.m_iPriceExtraStorage;
	}

	//------------------------------------------------------------------------------------------------
	static int PriceProcessingSlot()
	{
		LF_OrgConfig config = Get();
		if (!config || config.m_iPriceProcessingSlot < 0)
			return DEFAULT_PRICE_PROCESSING_SLOT;

		return config.m_iPriceProcessingSlot;
	}

	//------------------------------------------------------------------------------------------------
	static int PriceParkedVehicle()
	{
		LF_OrgConfig config = Get();
		if (!config || config.m_iPriceParkedVehicle < 0)
			return DEFAULT_PRICE_PARKED_VEHICLE;

		return config.m_iPriceParkedVehicle;
	}

	//------------------------------------------------------------------------------------------------
	static float DeskRange()
	{
		LF_OrgConfig config = Get();
		if (!config || config.m_fDeskRange <= 0)
			return DEFAULT_DESK_RANGE;

		return config.m_fDeskRange;
	}
}
