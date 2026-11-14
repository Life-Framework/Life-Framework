//------------------------------------------------------------------------------------------------
//! The owner's `persistence` section: how the autosave runs, and which fields survive a restart (#689). The shipped values are the
//! LF_PersistenceManagerComponent attributes on the game mode prefab, so with no config.json
//! nothing changes: Resolve starts from them and the owner's keys replace only what they name.
//! Server only; no client reads it, so it is not in the config stream. The autosave timer is
//! scheduled once at game start, so a change needs a restart (/configreload does not move it).
//! There is no retention setting: an autosave overwrites the one AUTO slot of the playthrough
//! (LF_SaveSelection), so there is nothing to keep or prune.
//! This class has no `.conf`: the shipped values live on the component, the one place they are
//! already edited.
[BaseContainerProps(configRoot: true)]
class LF_PersistenceConfig
{
	//! The owner config.json section this class reads (LF_ConfigService).
	static const string SECTION = "persistence";

	static const float INTERVAL_MIN = 30;	// lf-fixed: the schema limit, mirrors the attribute params
	static const float INTERVAL_MAX = 86400;	// lf-fixed: the schema limit, one day

	[Attribute("1", UIWidgets.CheckBox, "Save the campaign periodically while it runs. Off leaves saving to the save points and the shutdown save")]
	bool m_bEnableAutosave;

	[Attribute("600", UIWidgets.Auto, "Seconds between autosaves", params: "30 86400")]
	float m_fAutosaveInterval;

	// Persistence switches (#689). Each one says whether a field survives a restart. Off: the save
	// still carries the field (same shape, same version), and the load resets it to its default
	// (LF_PersistenceKeep). The attribute defaults are on, and this class has no
	// .conf; ResolveKeep starts them on (no member initializer: it would beat a .conf value of 0).
	[Attribute("1", UIWidgets.CheckBox, "Cash on hand survives a restart. Off: carried cash is gone after a restart; the bank is kept")]
	bool m_bKeepCash;

	[Attribute("1", UIWidgets.CheckBox, "Carried gear survives a restart. Off: a character respawns with the starting loadout, cash on hand included")]
	bool m_bKeepCarriedGear;

	[Attribute("1", UIWidgets.CheckBox, "Hunger and thirst survive a restart. Off: both start full")]
	bool m_bKeepSurvival;

	[Attribute("1", UIWidgets.CheckBox, "Wanted levels survive a restart. Off: every warrant is cleared and its poster comes down")]
	bool m_bKeepWanted;

	[Attribute("1", UIWidgets.CheckBox, "Jail sentences survive a restart. Off: every prisoner is free after a restart")]
	bool m_bKeepJail;

	[Attribute("1", UIWidgets.CheckBox, "Earned licences survive a restart. Off: characters keep only the starter licences")]
	bool m_bKeepLicences;

	//------------------------------------------------------------------------------------------------
	//! The autosave settings after the owner's layer: starts from the component's own values.
	//! \param[inout] enabled The component's autosave flag in, the effective one out.
	//! \param[inout] interval The component's interval in seconds in, the effective one out.
	static void Resolve(inout bool enabled, inout float interval)
	{
		LF_PersistenceConfig config = new LF_PersistenceConfig();
		config.m_bEnableAutosave = enabled;
		config.m_fAutosaveInterval = interval;
		config.ApplyLayers();
		enabled = config.m_bEnableAutosave;
		interval = config.m_fAutosaveInterval;
	}

	//------------------------------------------------------------------------------------------------
	//! The persistence switches after the owner's layer (#689). Read at each store load, so a
	//! config.json change takes effect at the next restart.
	//! \return A config whose m_bKeep* members are the effective switches.
	static LF_PersistenceConfig ResolveKeep()
	{
		LF_PersistenceConfig config = new LF_PersistenceConfig();
		config.m_bKeepCash = true;
		config.m_bKeepCarriedGear = true;
		config.m_bKeepSurvival = true;
		config.m_bKeepWanted = true;
		config.m_bKeepJail = true;
		config.m_bKeepLicences = true;
		config.ApplyLayers();
		return config;
	}

	//------------------------------------------------------------------------------------------------
	//! Merges the owner's "persistence" section over this instance.
	void ApplyLayers()
	{
		LF_ConfigReader reader = LF_ConfigService.OpenOwnerSection(SECTION);
		if (!reader)
			return;

		ApplyOwner(reader);
		LF_Debug.Info(LF_ConfigService.FEATURE, string.Format("persistence: owner config read %1 fields, rejected %2", reader.GetAppliedCount(), reader.GetRejectedCount()));
	}

	//------------------------------------------------------------------------------------------------
	//! Applies the owner's "persistence" section field by field; a key left out keeps the shipped value.
	//! \param[in] reader A reader on the section.
	void ApplyOwner(notnull LF_ConfigReader reader)
	{
		array<string> ownerKeys = {"enableAutosave", "autosaveInterval", "keepCash", "keepCarriedGear", "keepSurvival", "keepWanted", "keepJail", "keepLicences"};
		reader.WarnUnknownKeys(ownerKeys);
		reader.ReadBool("enableAutosave", m_bEnableAutosave);
		reader.ReadFloat("autosaveInterval", m_fAutosaveInterval, INTERVAL_MIN, INTERVAL_MAX);
		reader.ReadBool("keepCash", m_bKeepCash);
		reader.ReadBool("keepCarriedGear", m_bKeepCarriedGear);
		reader.ReadBool("keepSurvival", m_bKeepSurvival);
		reader.ReadBool("keepWanted", m_bKeepWanted);
		reader.ReadBool("keepJail", m_bKeepJail);
		reader.ReadBool("keepLicences", m_bKeepLicences);
	}
}
