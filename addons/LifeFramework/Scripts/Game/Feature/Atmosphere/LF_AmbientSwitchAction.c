//------------------------------------------------------------------------------------------------
//! "Switch off" / "Switch on" on a radio or cassette player. The server flips the replicated
//! LF_AmbientSwitchComponent; every machine's emitter follows it. Anyone may use it: it is a radio
//! on a counter.
class LF_AmbientSwitchAction : ScriptedUserAction
{
	protected LF_AmbientSwitchComponent m_Switch;

	//------------------------------------------------------------------------------------------------
	override void Init(IEntity pOwnerEntity, GenericComponent pManagerComponent)
	{
		m_Switch = LF_AmbientSwitchComponent.Cast(pOwnerEntity.FindComponent(LF_AmbientSwitchComponent));
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return m_Switch != null && LF_Systems.IsEnabled(LF_Atmosphere.SYSTEM);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		return m_Switch != null;
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		if (!m_Switch)
			return false;

		if (m_Switch.IsOn())
			outName = "#LF-Atmosphere_SwitchOff";
		else
			outName = "#LF-Atmosphere_SwitchOn";

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;

		if (LF_Systems.Refuses(LF_Atmosphere.SYSTEM, "radio switch"))
			return;

		if (!m_Switch)
		{
			LF_Debug.Error(LF_Atmosphere.FEATURE, "switch action on an entity with no LF_AmbientSwitchComponent");
			return;
		}

		m_Switch.SetOn(!m_Switch.IsOn());
	}
}
