class LF_FixtureTicker : ScriptComponent
{
	override void OnPostInit(IEntity owner)
	{
		GetGame().GetCallqueue().CallLater(OnTick, 1000, true);
		EventHandlerManagerComponent ev = EventHandlerManagerComponent.Cast(owner.FindComponent(EventHandlerManagerComponent));
		if (ev)
			ev.RegisterScriptHandler("OnCompartmentLeft", this, OnCompartmentLeft);
	}

	protected void OnTick() {}
	protected void OnCompartmentLeft(IEntity vehicle) {}
}
