//------------------------------------------------------------------------------------------------
//! How many characters an account may hold (#331), loaded from Configs/Account/LF_Characters.conf.
//! The framework supports any number; the flagship ships 1. With more than 1, a returning player
//! picks a character (or registers a new one while slots remain) before spawning. A missing or
//! broken file logs once and falls back to 1.
[BaseContainerProps(configRoot: true)]
class LF_CharacterConfig
{
	static const ResourceName CONFIG_PATH = "{CBD57E33C475D480}Configs/Account/LF_Characters.conf";

	static const int DEFAULT_MAX_PER_ACCOUNT = 1;
	static const int DEFAULT_MIN_AGE = 18;
	static const int DEFAULT_MAX_AGE = 80;

	[Attribute("1", UIWidgets.Auto, "Characters one account may hold. 1 is one life per player; more lets a player pick a character at spawn")]
	int m_iMaxPerAccount;

	[Attribute("18", UIWidgets.Auto, "Youngest age a new character may register with", params: "1 120")]
	int m_iMinAge = 18;

	[Attribute("80", UIWidgets.Auto, "Oldest age a new character may register with", params: "1 120")]
	int m_iMaxAge = 80;

	protected static ref LF_CharacterConfig s_Config;
	protected static bool s_bLoadAttempted;
	//! Test seam: above 0 it replaces the configured value (LF_Test_CharactersPerAccount).
	protected static int s_iMaxOverride;

	//------------------------------------------------------------------------------------------------
	//! The loaded config, or null (logged once) when the file does not resolve.
	static LF_CharacterConfig Get()
	{
		if (s_bLoadAttempted)
			return s_Config;

		s_bLoadAttempted = true;
		Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
		if (!container || !container.GetResource())
		{
			LF_Debug.Error("Account", string.Format("character config does not resolve, allowing %1 character per account: %2", DEFAULT_MAX_PER_ACCOUNT, CONFIG_PATH));
			return null;
		}

		s_Config = LF_CharacterConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!s_Config)
			LF_Debug.Error("Account", string.Format("character config has an unexpected root class, allowing %1 character per account: %2", DEFAULT_MAX_PER_ACCOUNT, CONFIG_PATH));

		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	//! characters.maxPerAccount: at least 1.
	static int MaxPerAccount()
	{
		if (s_iMaxOverride > 0)
			return s_iMaxOverride;

		LF_CharacterConfig config = Get();
		if (!config || config.m_iMaxPerAccount < 1)
			return DEFAULT_MAX_PER_ACCOUNT;

		return config.m_iMaxPerAccount;
	}

	//------------------------------------------------------------------------------------------------
	static int MinAge()
	{
		LF_CharacterConfig config = Get();
		if (!config || config.m_iMinAge < 1)
			return DEFAULT_MIN_AGE;

		return config.m_iMinAge;
	}

	//------------------------------------------------------------------------------------------------
	static int MaxAge()
	{
		LF_CharacterConfig config = Get();
		if (!config || config.m_iMaxAge < 1)
			return DEFAULT_MAX_AGE;

		return config.m_iMaxAge;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests only: 0 goes back to the config file.
	static void SetMaxPerAccountOverride(int maxPerAccount)
	{
		s_iMaxOverride = Math.Max(0, maxPerAccount);
	}
}
