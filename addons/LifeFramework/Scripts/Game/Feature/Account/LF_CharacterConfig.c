//------------------------------------------------------------------------------------------------
//! How many characters an account may hold (#331), loaded from Configs/Account/LF_Characters.conf.
//! The framework supports any number; the core ships 1. With more than 1, a returning player
//! picks a character (or registers a new one while slots remain) before spawning, and can change
//! character later at the town clerk (LF_CharacterSwitch). A pack overlay of this class (the mission
//! header's m_aLF_ConfigOverlays) sets the fields it writes directly over the core file: the Everon
//! pack allows 3 and opens each new account with its own starting bank. A missing or broken file logs once and falls back to 1.
[BaseContainerProps(configRoot: true)]
class LF_CharacterConfig
{
	static const ResourceName CONFIG_PATH = "{CBD57E33C475D480}Configs/Account/LF_Characters.conf";

	static const int DEFAULT_MAX_PER_ACCOUNT = 1;
	static const int DEFAULT_MIN_AGE = 18;
	static const int DEFAULT_MAX_AGE = 80;
	static const int DEFAULT_SWITCH_COMBAT_SECONDS = 120;
	//! The name pack overlay log lines use; there is no owner section.
	static const string OVERLAY_NAME = "characters";

	[Attribute("1", UIWidgets.Auto, "Characters one account may hold. 1 is one life per player; more lets a player pick a character at spawn")]
	int m_iMaxPerAccount;

	[Attribute("18", UIWidgets.Auto, "Youngest age a new character may register with", params: "1 120")]
	int m_iMinAge;

	[Attribute("80", UIWidgets.Auto, "Oldest age a new character may register with", params: "1 120")]
	int m_iMaxAge;

	[Attribute("120", UIWidgets.Auto, "Seconds after a player hits or is hit by another player during which they cannot change character", params: "0 3600")]
	int m_iSwitchCombatSeconds;

	//! -1 keeps the game mode's own opening bank (LF_SpawnLogic.m_iStartingBank); 0 or more replaces it.
	[Attribute("-1", UIWidgets.Auto, "Opening bank balance of a newly registered character. -1 keeps the game mode's value; a pack sets the figure its economy is built around", params: "-1 1000000")]
	int m_iStartingBank;

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
		else
			s_Config.ApplyLayers();

		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	//! Lays each pack overlay of this class over the core values, in load order (LF_ConfigOverlay).
	void ApplyLayers()
	{
		array<ref Resource> resources = {};
		array<ResourceName> names = {};
		LF_ConfigOverlay.LoadPackOverlays(CONFIG_PATH, LF_CharacterConfig, OVERLAY_NAME, resources, names);
		foreach (int i, Resource resource : resources)
		{
			int changed = ApplyPackContainer(resource.GetResource().ToBaseContainer(), names[i]);
			LF_Debug.Info("Account", string.Format("characters: pack overlay %1 changed %2 fields (max per account %3)", names[i], changed, m_iMaxPerAccount));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Copies the fields one pack overlay writes directly. A value out of range is refused and logged;
	//! the earlier value stays.
	//! \return Fields changed.
	int ApplyPackContainer(notnull BaseContainer root, string source)
	{
		int changed;
		int value;
		if (root.IsVariableSetDirectly("m_iMaxPerAccount") && root.Get("m_iMaxPerAccount", value))
		{
			if (value >= 1)
			{
				m_iMaxPerAccount = value;
				changed++;
			}
			else
			{
				LF_Debug.Error("Account", string.Format("characters: %1 sets m_iMaxPerAccount %2, below 1; keeping %3", source, value, m_iMaxPerAccount));
			}
		}

		if (root.IsVariableSetDirectly("m_iMinAge") && root.Get("m_iMinAge", value) && value >= 1)
		{
			m_iMinAge = value;
			changed++;
		}

		if (root.IsVariableSetDirectly("m_iMaxAge") && root.Get("m_iMaxAge", value) && value >= 1)
		{
			m_iMaxAge = value;
			changed++;
		}

		if (root.IsVariableSetDirectly("m_iSwitchCombatSeconds") && root.Get("m_iSwitchCombatSeconds", value) && value >= 0)
		{
			m_iSwitchCombatSeconds = value;
			changed++;
		}

		if (root.IsVariableSetDirectly("m_iStartingBank") && root.Get("m_iStartingBank", value) && value >= -1)
		{
			m_iStartingBank = value;
			changed++;
		}

		return changed;
	}

	//------------------------------------------------------------------------------------------------
	//! Seconds after player-on-player damage during which a character switch is refused.
	static int SwitchCombatSeconds()
	{
		LF_CharacterConfig config = Get();
		if (!config || config.m_iSwitchCombatSeconds < 0)
			return DEFAULT_SWITCH_COMBAT_SECONDS;

		return config.m_iSwitchCombatSeconds;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests: forget the loaded config so the next Get() reads the file and its pack layers again.
	static void Reload()
	{
		s_Config = null;
		s_bLoadAttempted = false;
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
	//! The opening bank balance a pack sets, or `fallback` (the game mode's own value) when none does.
	static int StartingBank(int fallback)
	{
		LF_CharacterConfig config = Get();
		if (!config || config.m_iStartingBank < 0)
			return fallback;

		return config.m_iStartingBank;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests only: 0 goes back to the config file.
	static void SetMaxPerAccountOverride(int maxPerAccount)
	{
		s_iMaxOverride = Math.Max(0, maxPerAccount);
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_CharacterConfig's statics (LF_StaticReset finds it by reflection): the next
//! world may run under another mission header, whose pack overlays differ.
class LF_CharacterConfig_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_CharacterConfig";
	}

	override void Apply()
	{
		LF_CharacterConfig.Reload();
	}
}
