//------------------------------------------------------------------------------------------------
//! Shop limits: how far a payer or buyer may stand, the per-order unit cap, and how long a raided
//! back room stays shut. Prices live in ShopPrices.conf (LF_ShopPricesConfig). Loaded from
//! Configs/Shop/LF_ShopLimits.conf. A missing or broken file logs once and falls back to the same defaults.
[BaseContainerProps(configRoot: true)]
class LF_ShopLimitsConfig
{
	static const ResourceName CONFIG_PATH = "{95D2D4F9AF0F7563}Configs/Shop/LF_ShopLimits.conf";

	static const float DEFAULT_TILL_REACH = 3.5;
	static const float DEFAULT_SHOP_DISTANCE = 30;
	static const int DEFAULT_ORDER_MAX_QUANTITY = 99;
	static const float DEFAULT_BACK_ROOM_REACH = 4;
	static const int DEFAULT_BACK_ROOM_SHUT_MS = 600000;

	[Attribute("3.5", UIWidgets.Auto, "Metres a payer may stand from the till when the server takes the money", params: "1 30")]
	float m_fTillReach = 3.5;

	[Attribute("30", UIWidgets.Auto, "Metres a player may be from a shop before the server rejects a buy or sell", params: "3 200")]
	float m_fShopDistance = 30;

	[Attribute("99", UIWidgets.Auto, "Most units in one buy or sell order. Every unit is spawned or deleted, so keep it modest", params: "1 999")]
	int m_iOrderMaxQuantity = 99;

	[Attribute("4", UIWidgets.Auto, "Metres an officer may stand from a back room to raid it", params: "1 30")]
	float m_fBackRoomReach = 4;

	[Attribute("600000", UIWidgets.Auto, "Milliseconds a raided back room stays shut", params: "0 86400000")]
	int m_iBackRoomShutMs = 600000;

	protected static ref LF_ShopLimitsConfig s_Config;
	protected static bool s_bLoadAttempted;

	//------------------------------------------------------------------------------------------------
	//! The loaded config, or null (logged once) when the file does not resolve.
	static LF_ShopLimitsConfig Get()
	{
		if (s_bLoadAttempted)
			return s_Config;

		s_bLoadAttempted = true;
		Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
		if (!container || !container.GetResource())
		{
			LF_Debug.Error("Shop", string.Format("shop limits config does not resolve, using defaults: %1", CONFIG_PATH));
			return null;
		}

		s_Config = LF_ShopLimitsConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!s_Config)
			LF_Debug.Error("Shop", string.Format("shop limits config has an unexpected root class, using defaults: %1", CONFIG_PATH));

		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	static float TillReach()
	{
		LF_ShopLimitsConfig config = Get();
		if (!config || config.m_fTillReach <= 0)
			return DEFAULT_TILL_REACH;

		return config.m_fTillReach;
	}

	//------------------------------------------------------------------------------------------------
	static float ShopDistance()
	{
		LF_ShopLimitsConfig config = Get();
		if (!config || config.m_fShopDistance <= 0)
			return DEFAULT_SHOP_DISTANCE;

		return config.m_fShopDistance;
	}

	//------------------------------------------------------------------------------------------------
	static int OrderMaxQuantity()
	{
		LF_ShopLimitsConfig config = Get();
		if (!config || config.m_iOrderMaxQuantity < 1)
			return DEFAULT_ORDER_MAX_QUANTITY;

		return config.m_iOrderMaxQuantity;
	}

	//------------------------------------------------------------------------------------------------
	static float BackRoomReach()
	{
		LF_ShopLimitsConfig config = Get();
		if (!config || config.m_fBackRoomReach <= 0)
			return DEFAULT_BACK_ROOM_REACH;

		return config.m_fBackRoomReach;
	}

	//------------------------------------------------------------------------------------------------
	static int BackRoomShutMs()
	{
		LF_ShopLimitsConfig config = Get();
		if (!config || config.m_iBackRoomShutMs < 0)
			return DEFAULT_BACK_ROOM_SHUT_MS;

		return config.m_iBackRoomShutMs;
	}
}
