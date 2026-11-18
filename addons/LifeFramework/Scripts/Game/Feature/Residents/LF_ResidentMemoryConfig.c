// LF_ResidentMemoryConfig.c - the numbers behind resident memory (#2859, epic #991 step 6).
// Awards, thresholds, cooldown and who knows whom are data; LF_ResidentMemory is the pure rules.

//------------------------------------------------------------------------------------------------
//! One reason a resident's feeling changes: `helped`, `crime_seen`. `m_iDelta` is signed.
[BaseContainerProps()]
class LF_ResidentMemoryReason
{
	[Attribute("", UIWidgets.EditBox, "Reason id the callers use (helped, crime_seen)")]
	string m_sId;

	[Attribute("0", UIWidgets.EditBox, "Score change for the resident who saw it, signed")]
	int m_iDelta;

	[Attribute("0", UIWidgets.CheckBox, "Residents this one knows hear of it, one hop, at the gossip share")]
	bool m_bGossip;

	//------------------------------------------------------------------------------------------------
	static LF_ResidentMemoryReason Make(string id, int delta, bool gossip)
	{
		LF_ResidentMemoryReason reason = new LF_ResidentMemoryReason();
		reason.m_sId = id;
		reason.m_iDelta = delta;
		reason.m_bGossip = gossip;
		return reason;
	}
}

//------------------------------------------------------------------------------------------------
//! The residents one resident knows (neighbours, family). Gossip travels this list, one step.
[BaseContainerProps()]
class LF_ResidentKnowsDef
{
	[Attribute("", UIWidgets.EditBox, "Resident id (LF_ResidentDef.m_sId)")]
	string m_sResident;

	[Attribute("", UIWidgets.Object, "Resident ids this resident knows")]
	ref array<string> m_aKnown;

	//------------------------------------------------------------------------------------------------
	static LF_ResidentKnowsDef Make(string resident, array<string> known)
	{
		LF_ResidentKnowsDef def = new LF_ResidentKnowsDef();
		def.m_sResident = resident;
		def.m_aKnown = {};
		foreach (string id : known)
			def.m_aKnown.Insert(id);

		return def;
	}
}

//------------------------------------------------------------------------------------------------
//! Loaded from Configs/Residents/LF_ResidentMemory.conf.
[BaseContainerProps(configRoot: true)]
class LF_ResidentMemoryConfig
{
	static const ResourceName CONFIG_PATH = "{7EC202753E99A160}Configs/Residents/LF_ResidentMemory.conf";

	[Attribute("600", UIWidgets.EditBox, "Seconds before the same reason counts again for the same resident and character")]
	int m_iCooldownSeconds;

	[Attribute("50", UIWidgets.EditBox, "Percent of the score change a known resident hears of (rounded toward zero)")]
	int m_iGossipPercent;

	[Attribute("-60", UIWidgets.EditBox, "Below this score: refuses")]
	int m_iRefusesBelow;

	[Attribute("-20", UIWidgets.EditBox, "Below this score (and not refusing): wary")]
	int m_iWaryBelow;

	[Attribute("20", UIWidgets.EditBox, "From this score: friendly")]
	int m_iFriendlyFrom;

	[Attribute("60", UIWidgets.EditBox, "From this score: regular")]
	int m_iRegularFrom;

	[Attribute("", UIWidgets.Object, "Reasons and their score change")]
	ref array<ref LF_ResidentMemoryReason> m_aReasons;

	[Attribute("", UIWidgets.Object, "Who knows whom, per resident")]
	ref array<ref LF_ResidentKnowsDef> m_aKnows;

	protected static ref LF_ResidentMemoryConfig s_Config;
	protected static bool s_bLoadAttempted;

	//------------------------------------------------------------------------------------------------
	//! The loaded config, or null (logged once) when the file does not resolve.
	static LF_ResidentMemoryConfig Get()
	{
		if (s_bLoadAttempted)
			return s_Config;

		s_bLoadAttempted = true;
		Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
		if (!container || !container.GetResource())
		{
			LF_Debug.Error("Residents", string.Format("resident memory config does not resolve: %1", CONFIG_PATH));
			return null;
		}

		s_Config = LF_ResidentMemoryConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!s_Config)
			LF_Debug.Error("Residents", string.Format("resident memory config has an unexpected root class: %1", CONFIG_PATH));

		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests: makes `config` the one Get returns until ResetStatics.
	static void UseForTest(LF_ResidentMemoryConfig config)
	{
		s_Config = config;
		s_bLoadAttempted = true;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_Config = null;
		s_bLoadAttempted = false;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The reason with this id, or null.
	LF_ResidentMemoryReason FindReason(string id)
	{
		if (!m_aReasons)
			return null;

		foreach (LF_ResidentMemoryReason reason : m_aReasons)
		{
			if (reason && reason.m_sId == id)
				return reason;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Fills `outKnown` with the ids `resident` knows (empty when none).
	void GetKnown(string resident, notnull array<string> outKnown)
	{
		outKnown.Clear();
		if (!m_aKnows)
			return;

		foreach (LF_ResidentKnowsDef def : m_aKnows)
		{
			if (!def || def.m_sResident != resident || !def.m_aKnown)
				continue;

			foreach (string id : def.m_aKnown)
				outKnown.Insert(id);
		}
	}
}
