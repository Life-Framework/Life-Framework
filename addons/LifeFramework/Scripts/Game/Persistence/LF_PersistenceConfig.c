//------------------------------------------------------------------------------------------------
//! The owner's `persistence` section: how the autosave runs. The shipped values are the
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
	bool m_bEnableAutosave = true;

	[Attribute("600", UIWidgets.Auto, "Seconds between autosaves", params: "30 86400")]
	float m_fAutosaveInterval = 600;

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
		array<string> ownerKeys = {"enableAutosave", "autosaveInterval"};
		reader.WarnUnknownKeys(ownerKeys);
		reader.ReadBool("enableAutosave", m_bEnableAutosave);
		reader.ReadFloat("autosaveInterval", m_fAutosaveInterval, INTERVAL_MIN, INTERVAL_MAX);
	}
}
