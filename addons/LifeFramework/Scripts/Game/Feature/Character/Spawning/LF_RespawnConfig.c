//------------------------------------------------------------------------------------------------
//! Where a dead player wakes and what it costs. Loaded from Configs/Spawning/LF_Respawn.conf;
//! the spawn logic holds no prices. Decision 2026-11-08 "You wake up in hospital".
//!
//! Owner JSON (#1779): the "respawn" section sets any field below over the .conf, a key left out keeps it:
//!   "respawn": { "rejoinAtLastPosition": true, "rejoinMaxAgeHours": 12 }
[BaseContainerProps(configRoot: true)]
class LF_RespawnConfig
{
	//! The owner config.json section this class reads (LF_ConfigService).
	static const string SECTION = "respawn";
	static const int BILL_MAX = 1000000;	// lf-fixed: the owner limit, mirrors the shop price cap
	static const int LOITER_MAX = 64;	// lf-fixed: the owner limit, far above the base game's loiter types
	static const float LIE_MAX = 600;	// lf-fixed: the owner limit, ten minutes
	static const int REJOIN_AGE_MIN = 0;	// lf-fixed: the schema limit, mirrors the attribute params
	static const int REJOIN_AGE_MAX = 720;	// lf-fixed: the schema limit, thirty days

	static const ResourceName CONFIG_PATH = "{3092F3800BA35C61}Configs/Spawning/LF_Respawn.conf";

	[Attribute("1", UIWidgets.CheckBox, "After a death, respawn in a hospital bed (LF_HospitalBedComponent). Off, or no bed in the world: the spawn plaza")]
	bool m_bWakeInHospital;

	[Attribute("150", UIWidgets.Auto, "Treatment bill written on paper at every respawn after a death, paid in cash at the hospital desk (#260). 0 disables the bill", params: "0 1000000")]
	int m_iBillAmount;

	[Attribute("7", UIWidgets.Auto, "Loiter animation the waking body plays: the base game tutorial's bed wake (7 full, 8 fast). 0 skips the animation", params: "0 64")]
	int m_iWakeLoiterType;

	[Attribute("1", UIWidgets.CheckBox, "The wake animation moves the body (root motion), so it gets off the bed instead of standing on it")]
	bool m_bWakeRootMotion;

	[Attribute("0", UIWidgets.Auto, "Seconds before the bed loiter is stopped into the get-up. 0 lets the animation end by itself", params: "0 600")]
	float m_fWakeLieSeconds;

	[Attribute("0", UIWidgets.CheckBox, "A returning character spawns where it logged out (rejoinAtLastPosition, #1778). Off: the normal spawn point. A jail return and a hospital wake come first; a stale or unsafe spot falls back to the spawn point (LF_RejoinSpot)")]
	bool m_bRejoinAtLastPosition;

	[Attribute("12", UIWidgets.Auto, "A saved spot older than this many hours is stale and the character takes the spawn point. 0 means no age limit", params: "0 720")]
	int m_iRejoinMaxAgeHours;

	protected static ref LF_RespawnConfig s_Config;
	protected static bool s_bLoadAttempted;

	//------------------------------------------------------------------------------------------------
	//! The loaded config, or null (logged once) when the file does not resolve.
	static LF_RespawnConfig Get()
	{
		if (s_bLoadAttempted)
			return s_Config;

		s_bLoadAttempted = true;
		Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
		if (!container || !container.GetResource())
		{
			LF_Debug.Error("Respawn", string.Format("respawn config does not resolve: %1", CONFIG_PATH));
			return null;
		}

		s_Config = LF_RespawnConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!s_Config)
			LF_Debug.Error("Respawn", string.Format("respawn config has an unexpected root class: %1", CONFIG_PATH));

		// The owner's "respawn" section over the .conf (server; a client has no owner file).
		if (s_Config)
		{
			LF_ConfigReader reader = LF_ConfigService.OpenOwnerSection(SECTION);
			if (reader)
			{
				s_Config.ApplyOwner(reader);
				LF_Debug.Info(LF_ConfigService.FEATURE, string.Format("respawn: owner config read %1 fields, rejected %2", reader.GetAppliedCount(), reader.GetRejectedCount()));
			}
		}

		// The setting is logged once, when the config first loads.
		if (s_Config)
			LF_Debug.Info("Respawn", string.Format("rejoinAtLastPosition=%1 maxAgeHours=%2", s_Config.m_bRejoinAtLastPosition, s_Config.m_iRejoinMaxAgeHours));

		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests and the config reload: the next Get() loads the .conf and layers the owner file again.
	static void Reload()
	{
		s_Config = null;
		s_bLoadAttempted = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Applies the owner's "respawn" section field by field; a key left out keeps the .conf value.
	void ApplyOwner(notnull LF_ConfigReader reader)
	{
		array<string> ownerKeys = {"wakeInHospital", "billAmount", "wakeLoiterType", "wakeRootMotion", "wakeLieSeconds", "rejoinAtLastPosition", "rejoinMaxAgeHours"};
		reader.WarnUnknownKeys(ownerKeys);
		reader.ReadBool("wakeInHospital", m_bWakeInHospital);
		reader.ReadInt("billAmount", m_iBillAmount, 0, BILL_MAX);
		reader.ReadInt("wakeLoiterType", m_iWakeLoiterType, 0, LOITER_MAX);
		reader.ReadBool("wakeRootMotion", m_bWakeRootMotion);
		reader.ReadFloat("wakeLieSeconds", m_fWakeLieSeconds, 0, LIE_MAX);
		reader.ReadBool("rejoinAtLastPosition", m_bRejoinAtLastPosition);
		reader.ReadInt("rejoinMaxAgeHours", m_iRejoinMaxAgeHours, REJOIN_AGE_MIN, REJOIN_AGE_MAX);
	}
}
