//------------------------------------------------------------------------------------------------
//! The one thing the server replicates for an ambient sound: on or off. A radio a player switches
//! (LF_AmbientSwitchAction), or a machine that runs for some seconds after it was used (Pulse).
//! The sound itself is local to each machine (LF_AmbientEmitterComponent with
//! LF_AmbientConditionSwitch). The entity needs an RplComponent; a late joiner gets the current
//! value with the entity.
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "LifeFramework/Feature/Atmosphere", description: "Replicated on/off for an ambient sound: a radio switch, a machine that runs after use.")]
class LF_AmbientSwitchComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_AmbientSwitchComponent : ScriptComponent
{
	[Attribute("1", UIWidgets.CheckBox, "On when the world starts")]
	protected bool m_bStartsOn;

	[RplProp()]
	protected bool m_bOn;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (Replication.IsServer())
			m_bOn = m_bStartsOn;
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (Replication.IsServer() && GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(PulseEnd);

		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	bool IsOn()
	{
		return m_bOn;
	}

	//------------------------------------------------------------------------------------------------
	//! Server only. Returns true when the value changed.
	bool SetOn(bool on)
	{
		if (!Replication.IsServer())
			return false;

		if (m_bOn == on)
			return false;

		m_bOn = on;
		Replication.BumpMe();
		LF_Debug.Info(LF_Atmosphere.FEATURE, string.Format("switch %1 on=%2", LF_Atmosphere.LabelOf(GetOwner()), on));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server only: on now, off `seconds` after the last pulse. A second pulse restarts the wait.
	void Pulse(float seconds)
	{
		if (!Replication.IsServer() || seconds <= 0)
			return;

		ScriptCallQueue queue = GetGame().GetCallqueue();
		queue.Remove(PulseEnd);
		queue.CallLater(PulseEnd, seconds * 1000, false);
		SetOn(true);
	}

	//------------------------------------------------------------------------------------------------
	protected void PulseEnd()
	{
		SetOn(false);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: pulse the switch on `entity` when it has one. What a feature calls when its machine
	//! was used; an entity without the component is not an error.
	static void PulseOn(IEntity entity, float seconds)
	{
		if (!entity)
			return;

		LF_AmbientSwitchComponent ambient = LF_AmbientSwitchComponent.Cast(entity.FindComponent(LF_AmbientSwitchComponent));
		if (ambient)
			ambient.Pulse(seconds);
	}
}
