class LF_FixtureTicker : ScriptComponent
{
	protected EventHandlerManagerComponent m_Events;

	override void OnPostInit(IEntity owner)
	{
		GetGame().GetCallqueue().CallLater(OnTick, 1000, true);
		GetGame().GetCallqueue().CallLater(OnOnce, 1000, false);
		m_Events = EventHandlerManagerComponent.Cast(owner.FindComponent(EventHandlerManagerComponent));
		if (m_Events)
			m_Events.RegisterScriptHandler("OnCompartmentLeft", this, OnCompartmentLeft);
	}

	override void OnDelete(IEntity owner)
	{
		GetGame().GetCallqueue().Remove(OnTick);
		if (m_Events)
			m_Events.RemoveScriptHandler("OnCompartmentLeft", this, OnCompartmentLeft);
	}

	protected void OnTick() {}
	protected void OnOnce() {}
	protected void OnCompartmentLeft(IEntity vehicle) {}
}

class LF_FixtureMenu
{
	void Open(LF_FixtureSource source)
	{
		source.GetOnChanged().Insert(OnChanged);
	}

	void Close(LF_FixtureSource source)
	{
		source.GetOnChanged().Remove(OnChanged);
	}

	protected void OnChanged() {}
}
