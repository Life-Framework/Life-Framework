//------------------------------------------------------------------------------------------------
//! Fuel station stock table (#3327, framework #788). Loaded from Configs/Fuel/LF_Fuel.conf. The core ships it
//! off (`m_bEnabled` 0), so no server changes behaviour until it opts in. Prices are in hundredths of a cash
//! unit per litre (`LF_FuelStockRules.PRICE_SCALE`); price 0 is the kill switch. One table for every station
//! in this card; per-station rows come with the station component.
[BaseContainerProps(configRoot: true)]
class LF_FuelConfig
{
	static const string FEATURE = "Fuel";
	static const ResourceName CONFIG_PATH = "{E66F86EF400CD08A}Configs/Fuel/LF_Fuel.conf";

	[Attribute("0", UIWidgets.CheckBox, "Whether fuel stations sell from stock. Off refuses every sale")]
	bool m_bEnabled;

	[Attribute("200", UIWidgets.Auto, "Price of one litre in hundredths of a cash unit; 0 turns fuel sales off", params: "0 10000")]
	int m_iPricePerLitre;

	[Attribute("100", UIWidgets.Auto, "What a delivery pays per litre that landed, in hundredths of a cash unit", params: "0 10000")]
	int m_iDeliveryPayPerLitre;

	[Attribute("2000", UIWidgets.Auto, "Litres a station holds at most", params: "1 100000")]
	int m_iCapacity;

	[Attribute("500", UIWidgets.Auto, "Litres a station holds when it is first made", params: "0 100000")]
	int m_iStartLitres;

	[Attribute("200", UIWidgets.Auto, "Litres one delivery run carries", params: "1 100000")]
	int m_iLitresPerDelivery;

	[Attribute("1", UIWidgets.Auto, "A station holding fewer litres than this refuses to sell", params: "1 100000")]
	int m_iMinStockToSell;

	protected static ref LF_FuelConfig s_Config;
	protected static bool s_bLoadAttempted;

	//------------------------------------------------------------------------------------------------
	static LF_FuelConfig Make(bool enabled, int pricePerLitre, int deliveryPay, int capacity, int startLitres, int litresPerDelivery, int minStockToSell)
	{
		LF_FuelConfig config = new LF_FuelConfig();
		config.m_bEnabled = enabled;
		config.m_iPricePerLitre = pricePerLitre;
		config.m_iDeliveryPayPerLitre = deliveryPay;
		config.m_iCapacity = capacity;
		config.m_iStartLitres = startLitres;
		config.m_iLitresPerDelivery = litresPerDelivery;
		config.m_iMinStockToSell = minStockToSell;
		return config;
	}

	//------------------------------------------------------------------------------------------------
	//! The loaded config, or null (logged once) when the file does not resolve. An unusable table is
	//! switched off at load (`Sanitize`), so a bad file never sells fuel.
	static LF_FuelConfig Get()
	{
		if (s_bLoadAttempted)
			return s_Config;

		s_bLoadAttempted = true;
		Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
		if (!container || !container.GetResource())
		{
			LF_Debug.Error(FEATURE, string.Format("fuel config does not resolve: %1", CONFIG_PATH));
			return null;
		}

		s_Config = LF_FuelConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!s_Config)
		{
			LF_Debug.Error(FEATURE, string.Format("fuel config has an unexpected root class: %1", CONFIG_PATH));
			return null;
		}

		s_Config.Sanitize();
		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	//! Drops the cached instance; the next Get loads the `.conf` again (world end, tests).
	static void Reload()
	{
		s_Config = null;
		s_bLoadAttempted = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests: makes `config` the one Get returns until Reload.
	static void UseForTest(LF_FuelConfig config)
	{
		s_Config = config;
		s_bLoadAttempted = true;
	}

	//------------------------------------------------------------------------------------------------
	//! A new station at the configured capacity and starting litres.
	LF_FuelStock NewStation()
	{
		return LF_FuelStock.Make(m_iCapacity, m_iStartLitres);
	}

	//------------------------------------------------------------------------------------------------
	//! \return Empty when the table is usable, else why not: a price or pay outside 0..MAX_PRICE, a capacity
	//! outside 1..MAX_LITRES, start litres or a delivery outside the capacity, or a minimum stock above it.
	string Problem()
	{
		int maxPrice = LF_FuelStockRules.MAX_PRICE;
		if (m_iPricePerLitre < 0 || m_iPricePerLitre > maxPrice)
			return string.Format("price %1 is outside 0 to %2", m_iPricePerLitre, maxPrice);
		if (m_iDeliveryPayPerLitre < 0 || m_iDeliveryPayPerLitre > maxPrice)
			return string.Format("delivery pay %1 is outside 0 to %2", m_iDeliveryPayPerLitre, maxPrice);
		if (m_iCapacity < 1 || m_iCapacity > LF_FuelStockRules.MAX_LITRES)
			return string.Format("capacity %1 is outside 1 to %2", m_iCapacity, LF_FuelStockRules.MAX_LITRES);
		if (m_iStartLitres < 0 || m_iStartLitres > m_iCapacity)
			return string.Format("start litres %1 is outside 0 to the capacity %2", m_iStartLitres, m_iCapacity);
		if (m_iLitresPerDelivery < 1 || m_iLitresPerDelivery > m_iCapacity)
			return string.Format("litres per delivery %1 is outside 1 to the capacity %2", m_iLitresPerDelivery, m_iCapacity);
		if (m_iMinStockToSell < 1 || m_iMinStockToSell > m_iCapacity)
			return string.Format("minimum stock to sell %1 is outside 1 to the capacity %2", m_iMinStockToSell, m_iCapacity);

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Switches an unusable table off with `LF_Debug.Error("Fuel", ...)` naming the problem.
	//! \return true when the table was usable.
	bool Sanitize()
	{
		string problem = Problem();
		if (problem.IsEmpty())
			return true;

		LF_Debug.Error(FEATURE, string.Format("fuel config refused at load, fuel stock stays off: %1", problem));
		m_bEnabled = false;
		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_FuelConfig's statics.
class LF_FuelConfig_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_FuelConfig";
	}

	override void Apply()
	{
		LF_FuelConfig.Reload();
	}
}
