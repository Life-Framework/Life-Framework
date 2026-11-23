//------------------------------------------------------------------------------------------------
//! Rebel outposts (0.3.0 gangs O1, decision N28). An outpost is a safe zone away from town:
//! nobody fights inside (the SAFE rule of LF_Zones), police may walk in, and the outpost's traders
//! do not serve an on-duty officer. A trader asks LF_Outpost.RefusesService(customer, traderPos)
//! before it serves; a refusal is a spoken line (a toast), never silence.
//!
//! A placed LF_OutpostComponent registers the place under kind "outpost" (LF_Places) and the
//! SAFE circle under the zone name "outpost:<id>". Systems find the site by component, never by
//! coordinate or entity name (core-vs-pack.md "Map independence").
//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true)]
class LF_OutpostConfig
{
	static const ResourceName CONFIG_PATH = "{5B2E8A41C7D903F6}Configs/Outposts/LF_Outposts.conf";

	protected static ref LF_OutpostConfig s_Config;
	protected static bool s_bLoadAttempted;

	[Attribute("60", UIWidgets.EditBox, "Safe circle radius around an outpost, metres, flat", params: "10 300")]
	float m_fSafeRadius;

	[Attribute("1", UIWidgets.CheckBox, "Traders inside an outpost refuse to serve on-duty police")]
	bool m_bTradersRefuseOnDutyPolice;

	//------------------------------------------------------------------------------------------------
	//! Attribute defaults built in script (tests).
	static LF_OutpostConfig CreateDefault()
	{
		LF_OutpostConfig config = new LF_OutpostConfig();
		config.m_fSafeRadius = 60;	// lf-fixed: mirrors the attribute default
		config.m_bTradersRefuseOnDutyPolice = true;
		return config;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_Config = null;
		s_bLoadAttempted = false;
	}

	//------------------------------------------------------------------------------------------------
	//! The loaded file, or the defaults when it does not resolve (logged once).
	static LF_OutpostConfig Get()
	{
		if (!s_bLoadAttempted)
		{
			s_bLoadAttempted = true;
			Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
			if (container && container.GetResource())
				s_Config = LF_OutpostConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));

			if (!s_Config)
				LF_Debug.Error(LF_Outpost.FEATURE, string.Format("outpost config does not resolve, using defaults: %1", CONFIG_PATH));
		}

		if (s_Config)
			return s_Config;

		return CreateDefault();
	}
}

//------------------------------------------------------------------------------------------------
class LF_Outpost
{
	static const string FEATURE = "Outposts";
	static const string SYSTEM = "Outposts";
	static const string PLACE_KIND = "outpost";
	static const string ZONE_PREFIX = "outpost:";
	static const string REFUSE_KEY = "#LF-Outpost_TraderRefusesOfficer";
	static const string REFUSE_TITLE_KEY = "#LF-Outpost_TraderRefusesOfficer_Title";

	//------------------------------------------------------------------------------------------------
	//! The rule, pure: a trader inside an outpost refuses an on-duty officer. Off-duty police, and
	//! everyone else, are served.
	static bool ShouldRefuseService(bool onDutyPolice, bool traderInsideOutpost, bool ruleOn)
	{
		return onDutyPolice && traderInsideOutpost && ruleOn;
	}

	//------------------------------------------------------------------------------------------------
	//! \return true when position is inside a registered outpost zone.
	static bool IsInsideOutpost(vector position)
	{
		array<ref LF_ZoneArea> zones = LF_Zones.GetAll();
		foreach (LF_ZoneArea zone : zones)
		{
			if (zone.m_sName.StartsWith(ZONE_PREFIX) && zone.Contains(position))
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: whether the trader at traderPos refuses to serve customer. The caller then calls
	//! NotifyRefusal and stops the sale.
	static bool RefusesService(IEntity customer, vector traderPos)
	{
		if (!customer)
			return false;

		bool onDuty = LF_PoliceUtils.IsOnDutyCharacter(customer);
		bool refuse = ShouldRefuseService(onDuty, IsInsideOutpost(traderPos), LF_OutpostConfig.Get().m_bTradersRefuseOnDutyPolice);
		if (refuse)
			LF_Debug.Warn(FEATURE, string.Format("trader at %1 refused an on-duty officer", traderPos));

		return refuse;
	}

	//------------------------------------------------------------------------------------------------
	//! The refusal in words, to the officer.
	static void NotifyRefusal(IEntity customer)
	{
		LF_Utils.NotifyEntity(customer, REFUSE_KEY, REFUSE_TITLE_KEY, 4.0);
	}

	//------------------------------------------------------------------------------------------------
	//! Registers the outpost place and its SAFE circle.
	static bool Register(string id, IEntity site, float radius)
	{
		if (id.IsEmpty() || !site)
		{
			LF_Debug.Error(FEATURE, string.Format("outpost refused: id '%1' site %2", id, site != null));
			return false;
		}

		if (LF_Systems.Refuses(SYSTEM,"outpost " + id))
			return false;

		LF_Places.Register(PLACE_KIND, id, site);
		return LF_Zones.RegisterCircle(ZONE_PREFIX + id, LF_ZoneRule.SAFE, site.GetOrigin(), radius);
	}

	//------------------------------------------------------------------------------------------------
	static void Unregister(string id)
	{
		LF_Zones.Unregister(ZONE_PREFIX + id);
	}
}

//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "LifeFramework/Outposts", description: "A rebel outpost: a safe zone whose traders do not serve on-duty police")]
class LF_OutpostComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_OutpostComponent : ScriptComponent
{
	[Attribute("", UIWidgets.EditBox, "Stable outpost id, for example north")]
	protected string m_sOutpostId;

	[Attribute("0", UIWidgets.EditBox, "Safe radius in metres; 0 uses the config value", params: "0 300")]
	protected float m_fRadius;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		float radius = m_fRadius;
		if (radius <= 0)
			radius = LF_OutpostConfig.Get().m_fSafeRadius;

		LF_Outpost.Register(m_sOutpostId, owner, radius);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		LF_Outpost.Unregister(m_sOutpostId);
		super.OnDelete(owner);
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_OutpostConfig's statics (LF_StaticReset finds it by reflection).
class LF_Outpost_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_Outpost";
	}

	override void Apply()
	{
		LF_OutpostConfig.ResetStatics();
	}
}
