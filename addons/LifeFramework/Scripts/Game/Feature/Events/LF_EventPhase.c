//------------------------------------------------------------------------------------------------
//! One phase of an event (#1948, shape from Overthrow's objective phase, adapted). The NAME is the
//! save key (docs/features/events.md): phases can be added or reordered without breaking a save,
//! so never rename a shipped phase. A phase has no code; its modules are ids the event shell
//! (#757) resolves to condition, operation and abort modules.
[BaseContainerProps()]
class LF_EventPhase
{
	[Attribute("", UIWidgets.EditBox, "Stable phase name, unique inside the event. The save key: never rename a shipped phase")]
	string m_sName;

	[Attribute("5", UIWidgets.Auto, "Seconds between two runs of this phase's modules. 0 runs them on every tick", params: "0 3600")]
	float m_fCadenceSeconds;

	[Attribute("0", UIWidgets.Auto, "Seconds the phase may pass with no module acting before the event ends into cooldown. 0 never times out", params: "0 86400")]
	float m_fIdleTimeoutSeconds;

	[Attribute("", UIWidgets.Auto, "Module ids the shell runs in this phase, in authored order")]
	ref array<string> m_aModules;

	//------------------------------------------------------------------------------------------------
	static LF_EventPhase Make(string name, float cadenceSeconds, float idleTimeoutSeconds, array<string> modules = null)
	{
		LF_EventPhase phase = new LF_EventPhase();
		phase.m_sName = name;
		phase.m_fCadenceSeconds = cadenceSeconds;
		phase.m_fIdleTimeoutSeconds = idleTimeoutSeconds;
		phase.m_aModules = {};
		if (modules)
			phase.m_aModules.Copy(modules);
		return phase;
	}
}

//------------------------------------------------------------------------------------------------
//! What one running event remembers between ticks. Plain data, so the rules read and write it
//! with no world. m_sPhaseName is "" while the event is not running (idle or cooling down).
//! Times are server seconds. The persistence slice saves these four fields under the phase name.
class LF_EventRunState
{
	string m_sPhaseName;
	float m_fPhaseStartedAt;
	float m_fLastActivityAt;
	float m_fCooldownUntil;
}
