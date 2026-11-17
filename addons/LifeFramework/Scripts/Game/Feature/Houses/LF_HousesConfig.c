//------------------------------------------------------------------------------------------------
//! The houses owner-config section (#2505): Configs/Houses/LF_Houses.conf with the owner's `houses`
//! section layered over it. Read on the server only (the door timer runs there).
//!
//! Owner JSON:
//!   "houses": { "doorCloseDelaySeconds": 20 }
//! `doorCloseDelaySeconds` is how long a locked house's open door stays open before it closes itself
//! (LF_HouseDoorClose, #2472). 0 turns the auto-close off; a value over 600 is clamped to 600 with a warning.
[BaseContainerProps(configRoot: true)]
class LF_HousesConfig
{
	static const ResourceName CONFIG_PATH = "{49A4BD3891BA18EB}Configs/Houses/LF_Houses.conf";

	//! The owner config.json section this class reads (LF_ConfigService).
	static const string SECTION = "houses";

	static const float DEFAULT_DOOR_CLOSE_SECONDS = 20;
	static const float DOOR_CLOSE_MIN = 0;	// lf-fixed: the schema limit, mirrors the attribute params
	static const float DOOR_CLOSE_MAX = 600;	// lf-fixed: the schema limit, ten minutes, mirrors the attribute params

	[Attribute("20", UIWidgets.Auto, "Seconds a locked house's open door stays open before it closes itself. 0 turns the auto-close off", params: "0 600")]
	float m_fDoorCloseDelaySeconds;

	protected static ref LF_HousesConfig s_Config;
	protected static bool s_bLoadAttempted;

	//------------------------------------------------------------------------------------------------
	//! The loaded and layered config, or null (logged once) when the file does not resolve.
	static LF_HousesConfig Get()
	{
		if (s_bLoadAttempted)
			return s_Config;

		s_bLoadAttempted = true;
		Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
		if (!container || !container.GetResource())
		{
			LF_Debug.Error("Houses", string.Format("houses config does not resolve, using defaults: %1", CONFIG_PATH));
			return null;
		}

		s_Config = LF_HousesConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!s_Config)
			LF_Debug.Error("Houses", string.Format("houses config has an unexpected root class, using defaults: %1", CONFIG_PATH));
		else
			s_Config.ApplyLayers();

		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	//! Drops the cached instance so the next Get() layers the owner file again (/configreload).
	static void Reload()
	{
		s_Config = null;
		s_bLoadAttempted = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Merges the owner's "houses" section over this instance, which holds the core `.conf`.
	void ApplyLayers()
	{
		LF_ConfigReader reader = LF_ConfigService.OpenOwnerSection(SECTION);
		if (!reader)
			return;

		ApplyOwner(reader);
		LF_Debug.Info(LF_ConfigService.FEATURE, string.Format("houses: owner config read %1 fields, rejected %2", reader.GetAppliedCount(), reader.GetRejectedCount()));
	}

	//------------------------------------------------------------------------------------------------
	//! Applies the owner's "houses" section field by field; a key left out keeps the shipped value.
	//! A delay over the maximum is clamped (not refused) so a typo like 9999 still gives the longest wait;
	//! a negative or non-numeric delay is refused by the reader and keeps the shipped value.
	void ApplyOwner(notnull LF_ConfigReader reader)
	{
		array<string> ownerKeys = {"doorCloseDelaySeconds"};
		reader.WarnUnknownKeys(ownerKeys);

		reader.ReadFloat("doorCloseDelaySeconds", m_fDoorCloseDelaySeconds, DOOR_CLOSE_MIN, DOOR_CLOSE_MAX);

		// The reader refuses a delay above the maximum (it logs and keeps the shipped value). A typo
		// like 9999 should still give the longest wait, so a number over the limit is clamped here.
		float raw = -1;
		if (reader.ReadFloat("doorCloseDelaySeconds", raw, DOOR_CLOSE_MIN) && raw > DOOR_CLOSE_MAX)
		{
			LF_Debug.Warn("Houses", string.Format("houses.doorCloseDelaySeconds %1 is above the maximum %2, using %2", raw, DOOR_CLOSE_MAX));
			m_fDoorCloseDelaySeconds = DOOR_CLOSE_MAX;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Seconds a locked house's open door stays open; 0 means it never closes itself.
	static float DoorCloseDelaySeconds()
	{
		LF_HousesConfig config = Get();
		if (!config)
			return DEFAULT_DOOR_CLOSE_SECONDS;

		return Math.Clamp(config.m_fDoorCloseDelaySeconds, DOOR_CLOSE_MIN, DOOR_CLOSE_MAX);
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_HousesConfig's statics (registered at script load; Core/LF_StaticReset.c).
class LF_HousesConfig_StaticResetHook : LF_StaticResetHook
{
	protected static bool s_bRegistered = LF_StaticReset.Register(new LF_HousesConfig_StaticResetHook());

	override string Owner()
	{
		return "LF_HousesConfig";
	}

	override void Apply()
	{
		LF_HousesConfig.Reload();
	}
}
