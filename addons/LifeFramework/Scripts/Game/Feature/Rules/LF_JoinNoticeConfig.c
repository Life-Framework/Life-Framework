//------------------------------------------------------------------------------------------------
//! The `joinNotice` owner section (#1448): one toast at each player's first spawn of the session.
//!
//! Owner JSON (field names are the contract JNCFG and the host code against):
//!   "joinNotice": { "enabled": false, "key": "" }
//!
//! - `enabled` (m_bEnabled): off by default in the core. A server turns it on.
//! - `key` (m_sKey): the localization key of the text, with or without the leading `#`. The pack
//!   ships it (the flagship's is `LF_Everon1989_AlphaNotice`); the core carries no wording. An
//!   empty key with the switch on logs one warning and shows nothing.
//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true)]
class LF_JoinNoticeConfig
{
	//! The owner config.json section this class reads (LF_ConfigService).
	static const string SECTION = "joinNotice";

	[Attribute("0", UIWidgets.CheckBox, "Show the join notice toast at each player's first spawn of the session. Off in the core")]
	bool m_bEnabled;

	[Attribute("", UIWidgets.Auto, "Localization key of the notice text, with or without the leading #. Empty shows nothing and logs a warning")]
	string m_sKey;

	protected static ref LF_JoinNoticeConfig s_Config;

	//------------------------------------------------------------------------------------------------
	//! The shipped values; the members carry no initializers (#1077).
	static LF_JoinNoticeConfig CreateDefault()
	{
		LF_JoinNoticeConfig config = new LF_JoinNoticeConfig();
		config.m_bEnabled = false;
		config.m_sKey = string.Empty;
		return config;
	}

	//------------------------------------------------------------------------------------------------
	//! The shipped values with the owner's "joinNotice" section over them.
	static LF_JoinNoticeConfig Get()
	{
		if (s_Config)
			return s_Config;

		s_Config = CreateDefault();
		LF_ConfigReader reader = LF_ConfigService.OpenOwnerSection(SECTION);
		if (reader)
		{
			s_Config.ApplyOwner(reader);
			LF_Debug.Info(LF_JoinNotice.FEATURE, string.Format("joinNotice: owner config read %1 fields, rejected %2", reader.GetAppliedCount(), reader.GetRejectedCount()));
		}

		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests and the config reload: the next Get() layers the owner file again.
	static void Reload()
	{
		s_Config = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests: use this config until Reload.
	static void Use(LF_JoinNoticeConfig config)
	{
		s_Config = config;
	}

	//------------------------------------------------------------------------------------------------
	//! Applies the owner's "joinNotice" section field by field; a key left out keeps the shipped value.
	void ApplyOwner(notnull LF_ConfigReader reader)
	{
		array<string> ownerKeys = {"enabled", "key"};
		reader.WarnUnknownKeys(ownerKeys);
		reader.ReadBool("enabled", m_bEnabled);
		reader.ReadString("key", m_sKey);
	}
}
