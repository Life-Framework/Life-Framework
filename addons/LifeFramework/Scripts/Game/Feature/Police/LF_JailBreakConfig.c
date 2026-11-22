//------------------------------------------------------------------------------------------------
//! Numbers of the two-layer jailbreak and of bail (LF_JailBreak, LF_JailBail), loaded from
//! Configs/Police/LF_JailBreak.conf. A pack overlay of this class (the mission header's
//! m_aLF_ConfigOverlays) sets the fields it writes directly over the core file. A missing or broken
//! file logs once and the shipped defaults below apply.
[BaseContainerProps(configRoot: true)]
class LF_JailBreakConfig
{
	static const ResourceName CONFIG_PATH = "{9A30579032B6D0BA}Configs/Police/LF_JailBreak.conf";
	//! The name pack overlay log lines use; there is no owner section.
	static const string OVERLAY_NAME = "jailbreak";

	static const float DEFAULT_OUTER_FUSE_SECONDS = 20;	// lf-fixed: the shipped default, mirrors the attribute and the conf
	static const float DEFAULT_CELL_FUSE_SECONDS = 15;	// lf-fixed: the shipped default, mirrors the attribute and the conf
	static const int DEFAULT_RESEAL_SECONDS = 600;	// lf-fixed: the shipped default, mirrors the attribute and the conf
	static const int DEFAULT_COOLDOWN_SECONDS = 3600;	// lf-fixed: the shipped default, mirrors the attribute and the conf
	static const int DEFAULT_BAIL_PER_MINUTE = 100;	// lf-fixed: the shipped default, mirrors the attribute and the conf
	static const int DEFAULT_BAIL_MINIMUM = 250;	// lf-fixed: the shipped default, mirrors the attribute and the conf
	static const int DEFAULT_BAIL_MAXIMUM = 20000;	// lf-fixed: the shipped default, mirrors the attribute and the conf

	static const float FUSE_MAX = 300;	// lf-fixed: the schema limit, five minutes
	static const int RESEAL_MIN = 30;	// lf-fixed: the schema limit, a door shorter than this cannot be used
	static const int RESEAL_MAX = 7200;	// lf-fixed: the schema limit, two hours
	static const int COOLDOWN_MAX = 86400;	// lf-fixed: the schema limit, one day
	static const int BAIL_MAX = 1000000;	// lf-fixed: the schema limit

	[Attribute("1", UIWidgets.CheckBox, "Two-layer jailbreak on: the outer door must be blown before the cells, and blowing the cells frees the prisoners inside")]
	bool m_bBreakEnabled;

	[Attribute("20", UIWidgets.Auto, "Fuse seconds of a charge on the jail's outer door", params: "0 300")]
	float m_fOuterFuseSeconds;

	[Attribute("15", UIWidgets.Auto, "Fuse seconds of a charge on the cell door", params: "0 300")]
	float m_fCellFuseSeconds;

	[Attribute("600", UIWidgets.Auto, "Seconds a blown jail door stays open before it seals itself again (police can repair it sooner)", params: "30 7200")]
	int m_iResealSeconds;

	[Attribute("3600", UIWidgets.Auto, "Seconds after the cells were blown before either jail door can be charged again", params: "0 86400")]
	int m_iCooldownSeconds;

	[Attribute("1", UIWidgets.CheckBox, "A prisoner may pay bail from the bank to leave early")]
	bool m_bBailEnabled;

	[Attribute("100", UIWidgets.Auto, "Bail per started minute of the sentence left", params: "0 1000000")]
	int m_iBailPerMinute;

	[Attribute("250", UIWidgets.Auto, "Smallest bail, whatever the time left", params: "0 1000000")]
	int m_iBailMinimum;

	[Attribute("20000", UIWidgets.Auto, "Largest bail, whatever the time left", params: "0 1000000")]
	int m_iBailMaximum;

	protected static ref LF_JailBreakConfig s_Config;
	protected static bool s_bLoadAttempted;

	//------------------------------------------------------------------------------------------------
	//! The loaded config with its pack layers, or the shipped defaults (logged once) when the file
	//! does not resolve. Never null.
	static LF_JailBreakConfig Get()
	{
		if (s_bLoadAttempted && s_Config)
			return s_Config;

		s_bLoadAttempted = true;
		Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
		if (container && container.GetResource())
			s_Config = LF_JailBreakConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));

		if (!s_Config)
		{
			LF_Debug.Error(LF_JailBreak.FEATURE, string.Format("jailbreak config does not resolve, shipped defaults apply: %1", CONFIG_PATH));
			s_Config = CreateDefault();
			return s_Config;
		}

		s_Config.ApplyLayers();
		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	//! Lays each pack overlay of this class over the core values, in load order (LF_ConfigOverlay).
	void ApplyLayers()
	{
		array<ref Resource> resources = {};
		array<ResourceName> names = {};
		LF_ConfigOverlay.LoadPackOverlays(CONFIG_PATH, LF_JailBreakConfig, OVERLAY_NAME, resources, names);
		foreach (int i, Resource resource : resources)
		{
			int changed = ApplyPackContainer(resource.GetResource().ToBaseContainer());
			LF_Debug.Info(LF_JailBreak.FEATURE, string.Format("jailbreak: pack overlay %1 changed %2 fields", names[i], changed));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Copies the fields one pack overlay writes directly. The getters clamp each to its schema limit.
	//! \return Fields changed.
	int ApplyPackContainer(notnull BaseContainer root)
	{
		int changed;
		bool flag;
		float number;
		int value;
		if (root.IsVariableSetDirectly("m_bBreakEnabled") && root.Get("m_bBreakEnabled", flag))
		{
			m_bBreakEnabled = flag;
			changed++;
		}

		if (root.IsVariableSetDirectly("m_fOuterFuseSeconds") && root.Get("m_fOuterFuseSeconds", number))
		{
			m_fOuterFuseSeconds = number;
			changed++;
		}

		if (root.IsVariableSetDirectly("m_fCellFuseSeconds") && root.Get("m_fCellFuseSeconds", number))
		{
			m_fCellFuseSeconds = number;
			changed++;
		}

		if (root.IsVariableSetDirectly("m_iResealSeconds") && root.Get("m_iResealSeconds", value))
		{
			m_iResealSeconds = value;
			changed++;
		}

		if (root.IsVariableSetDirectly("m_iCooldownSeconds") && root.Get("m_iCooldownSeconds", value))
		{
			m_iCooldownSeconds = value;
			changed++;
		}

		if (root.IsVariableSetDirectly("m_bBailEnabled") && root.Get("m_bBailEnabled", flag))
		{
			m_bBailEnabled = flag;
			changed++;
		}

		if (root.IsVariableSetDirectly("m_iBailPerMinute") && root.Get("m_iBailPerMinute", value))
		{
			m_iBailPerMinute = value;
			changed++;
		}

		if (root.IsVariableSetDirectly("m_iBailMinimum") && root.Get("m_iBailMinimum", value))
		{
			m_iBailMinimum = value;
			changed++;
		}

		if (root.IsVariableSetDirectly("m_iBailMaximum") && root.Get("m_iBailMaximum", value))
		{
			m_iBailMaximum = value;
			changed++;
		}

		return changed;
	}

	//------------------------------------------------------------------------------------------------
	//! Drops the cached instance so the next Get() loads the file again.
	static void Reload()
	{
		s_Config = null;
		s_bLoadAttempted = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests: makes `config` the one Get returns until Reload.
	static void UseForTest(LF_JailBreakConfig config)
	{
		s_Config = config;
		s_bLoadAttempted = true;
	}

	//------------------------------------------------------------------------------------------------
	//! The shipped values in code, for tests and a missing file; the members carry no initializers (#1077).
	static LF_JailBreakConfig CreateDefault()
	{
		LF_JailBreakConfig config = new LF_JailBreakConfig();
		config.m_bBreakEnabled = true;
		config.m_fOuterFuseSeconds = DEFAULT_OUTER_FUSE_SECONDS;
		config.m_fCellFuseSeconds = DEFAULT_CELL_FUSE_SECONDS;
		config.m_iResealSeconds = DEFAULT_RESEAL_SECONDS;
		config.m_iCooldownSeconds = DEFAULT_COOLDOWN_SECONDS;
		config.m_bBailEnabled = true;
		config.m_iBailPerMinute = DEFAULT_BAIL_PER_MINUTE;
		config.m_iBailMinimum = DEFAULT_BAIL_MINIMUM;
		config.m_iBailMaximum = DEFAULT_BAIL_MAXIMUM;
		return config;
	}

	//------------------------------------------------------------------------------------------------
	float OuterFuseSeconds()
	{
		return Math.Clamp(m_fOuterFuseSeconds, 0, FUSE_MAX);
	}

	//------------------------------------------------------------------------------------------------
	float CellFuseSeconds()
	{
		return Math.Clamp(m_fCellFuseSeconds, 0, FUSE_MAX);
	}

	//------------------------------------------------------------------------------------------------
	int ResealSeconds()
	{
		return LF_Utils.ClampInt(m_iResealSeconds, RESEAL_MIN, RESEAL_MAX);
	}

	//------------------------------------------------------------------------------------------------
	int CooldownSeconds()
	{
		return LF_Utils.ClampInt(m_iCooldownSeconds, 0, COOLDOWN_MAX);
	}

	//------------------------------------------------------------------------------------------------
	int BailPerMinute()
	{
		return LF_Utils.ClampInt(m_iBailPerMinute, 0, BAIL_MAX);
	}

	//------------------------------------------------------------------------------------------------
	int BailMinimum()
	{
		return LF_Utils.ClampInt(m_iBailMinimum, 0, BAIL_MAX);
	}

	//------------------------------------------------------------------------------------------------
	//! Never below the minimum.
	int BailMaximum()
	{
		return LF_Utils.MaxInt(LF_Utils.ClampInt(m_iBailMaximum, 0, BAIL_MAX), BailMinimum());
	}
}
