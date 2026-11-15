//------------------------------------------------------------------------------------------------
//! The `staff` owner section (M4, #1343): where staff notifications are posted. A shape only; the
//! sender (S4) reads this class.
//!
//! Owner JSON (field names are the contract the dependents code against):
//!   "staff": { "webhookUrl": "" }
//!
//! - `webhookUrl` (m_sWebhookUrl): the staff channel's webhook. Empty (the default) means no
//!   notifications are sent.
//!
//! The URL is a secret: it is host-only (it lives in the host's `config.json` in the profile, never in
//! the repo, a pack or an example) and never leaves the server. Nothing may log it or stream it to
//! clients; log only whether one is set (`IsSet`).
//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true)]
class LF_StaffWebhookConfig
{
	//! The owner config.json section this class reads (LF_ConfigService).
	static const string SECTION = "staff";
	static const string FEATURE = "StaffWebhook";

	[Attribute("", UIWidgets.Auto, "Webhook URL staff notifications are posted to. A secret: set it only in the host's own config.json. Empty means no notifications")]
	string m_sWebhookUrl;

	protected static ref LF_StaffWebhookConfig s_Config;

	//------------------------------------------------------------------------------------------------
	//! The shipped values; the members carry no initializers (#1077).
	static LF_StaffWebhookConfig CreateDefault()
	{
		LF_StaffWebhookConfig config = new LF_StaffWebhookConfig();
		config.m_sWebhookUrl = string.Empty;
		return config;
	}

	//------------------------------------------------------------------------------------------------
	//! The shipped values with the owner's "staff" section over them.
	static LF_StaffWebhookConfig Get()
	{
		if (s_Config)
			return s_Config;

		s_Config = CreateDefault();
		LF_ConfigReader reader = LF_ConfigService.OpenOwnerSection(SECTION);
		if (reader)
		{
			s_Config.ApplyOwner(reader);
			// Never the URL itself: it is a secret.
			LF_Debug.Info(FEATURE, string.Format("staff webhook set: %1", s_Config.IsSet()));
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
	static void Use(LF_StaffWebhookConfig config)
	{
		s_Config = config;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when a webhook URL is configured.
	bool IsSet()
	{
		return !m_sWebhookUrl.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	//! Applies the owner's "staff" section field by field; a key left out keeps the shipped value.
	void ApplyOwner(notnull LF_ConfigReader reader)
	{
		array<string> ownerKeys = {"webhookUrl"};
		reader.WarnUnknownKeys(ownerKeys);
		reader.ReadString("webhookUrl", m_sWebhookUrl);
	}
}
