//------------------------------------------------------------------------------------------------
//! When an ambient emitter may sound. Set on LF_AmbientEmitterComponent in the prefab; no
//! condition means always. Every machine with a renderer asks its own copy, so a condition reads
//! only state that is already replicated to clients. A feature adds its own by extending this
//! class in its own folder.
[BaseContainerProps()]
class LF_AmbientCondition
{
	//------------------------------------------------------------------------------------------------
	//! Called once when the emitter starts up, on machines that render.
	void Init(IEntity owner)
	{
	}

	//------------------------------------------------------------------------------------------------
	bool IsMet(IEntity owner)
	{
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Follows the LF_AmbientSwitchComponent on the same entity: a radio a player switched off, a saw
//! that runs for a while after each log.
[BaseContainerProps()]
class LF_AmbientConditionSwitch : LF_AmbientCondition
{
	protected LF_AmbientSwitchComponent m_Switch;

	//------------------------------------------------------------------------------------------------
	override void Init(IEntity owner)
	{
		m_Switch = LF_AmbientSwitchComponent.Cast(owner.FindComponent(LF_AmbientSwitchComponent));
		if (!m_Switch)
			LF_Debug.Error(LF_Atmosphere.FEATURE, string.Format("%1 has a switch condition and no LF_AmbientSwitchComponent; it stays silent", LF_Atmosphere.LabelOf(owner)));
	}

	//------------------------------------------------------------------------------------------------
	override bool IsMet(IEntity owner)
	{
		return m_Switch && m_Switch.IsOn();
	}
}

//------------------------------------------------------------------------------------------------
//! A hands-on chain station (LF_ChainStationComponent: the moonshine still) while a batch is in it
//! and not yet ready. The percent is the station's own replicated state.
[BaseContainerProps()]
class LF_AmbientConditionChainWorking : LF_AmbientCondition
{
	protected LF_ChainStationComponent m_Station;

	//------------------------------------------------------------------------------------------------
	override void Init(IEntity owner)
	{
		m_Station = LF_ChainStationComponent.Cast(owner.FindComponent(LF_ChainStationComponent));
		if (!m_Station)
			LF_Debug.Error(LF_Atmosphere.FEATURE, string.Format("%1 has a chain condition and no LF_ChainStationComponent; it stays silent", LF_Atmosphere.LabelOf(owner)));
	}

	//------------------------------------------------------------------------------------------------
	override bool IsMet(IEntity owner)
	{
		if (!m_Station)
			return false;

		return Working(m_Station.GetPercent());
	}

	//------------------------------------------------------------------------------------------------
	//! -1 is empty and 100 is ready to collect; between them the batch is cooking.
	static bool Working(int percent)
	{
		return percent >= 0 && percent < LF_StationProgress.READY;
	}
}
