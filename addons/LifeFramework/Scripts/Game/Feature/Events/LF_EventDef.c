//------------------------------------------------------------------------------------------------
//! An event is data (#1948): an id and an ordered list of named phases, then a cooldown. The
//! core ships the shell and no place names; a pack's config says what a phase means on this
//! server. The pure rules over a definition live in LF_EventPhaseRules. It is an LF_ConfigRecord
//! so a pack overlay adds, replaces, patches or removes it by id (LF_EventOverlay, #1952).
[BaseContainerProps()]
class LF_EventDef : LF_ConfigRecord
{
	[Attribute("", UIWidgets.EditBox, "Event id, unique across the config. Never rename a shipped id: saves key on it")]
	string m_sEventId;

	[Attribute("", UIWidgets.Object, "The phases in order. The first runs when the event starts; finishing the last ends it into cooldown")]
	ref array<ref LF_EventPhase> m_aPhases;

	[Attribute("600", UIWidgets.Auto, "Seconds after the event ends (done, timed out or aborted) before it may start again", params: "0 604800")]
	float m_fCooldownSeconds;

	[Attribute("0", UIWidgets.CheckBox, "Off by default: the shell runs only an event a server or pack turns on (a pack patch sets m_bEnabled 1)")]
	bool m_bEnabled;

	//------------------------------------------------------------------------------------------------
	static LF_EventDef Make(string eventId, notnull array<ref LF_EventPhase> phases, float cooldownSeconds)
	{
		LF_EventDef def = new LF_EventDef();
		def.m_sEventId = eventId;
		def.m_aPhases = {};
		foreach (LF_EventPhase phase : phases)
		{
			def.m_aPhases.Insert(phase);
		}
		def.m_fCooldownSeconds = cooldownSeconds;
		// Built in code (tests, a module that owns its event): meant to run.
		def.m_bEnabled = true;
		return def;
	}

	//------------------------------------------------------------------------------------------------
	//! \return the index of the phase called `name`, or -1.
	int FindPhase(string name)
	{
		if (!m_aPhases || name == "")
			return -1;

		for (int i = 0; i < m_aPhases.Count(); i++)
		{
			if (m_aPhases[i] && m_aPhases[i].m_sName == name)
				return i;
		}
		return -1;
	}
}

//------------------------------------------------------------------------------------------------
//! The event config file root (Configs/Events/LF_Events.conf). The shell (#757) loads it.
[BaseContainerProps(configRoot: true)]
class LF_EventsConfig
{
	[Attribute("", UIWidgets.Object, "Every event this server can run")]
	ref array<ref LF_EventDef> m_aEvents;
}
