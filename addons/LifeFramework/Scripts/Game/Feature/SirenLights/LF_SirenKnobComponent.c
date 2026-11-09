class LF_SirenKnobComponentClass : ScriptComponentClass
{
}

/**
Responsible for ticking the animation. If the animation is ticked by the vehicle, its RigidBody must be set to AlwaysActive and that causes issues
**/
class LF_SirenKnobComponent : ScriptComponent
{
	// The animation being played
	protected ref LF_LightAnimation m_Animation;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		SetEventMask(owner, EntityEvent.SIMULATE);
		Register();
	}

	//------------------------------------------------------------------------------------------------
	/**
	\brief Register itself to LF_SirenManagerComponent
	**/
	void Register()
	{
		IEntity parent = GetOwner().GetParent();
		LF_SirenManagerComponent manager;

		// Gets the parent component until it finds one with an LF_SirenManagerComponent
		// or until there is no more parent entity
		while (parent && !manager)
		{
			manager = LF_SirenManagerComponent.Cast(parent.FindComponent(LF_SirenManagerComponent));
			parent = parent.GetParent();
		}
		if (manager) manager.RegisterKnob(this);
		else Print("Siren knob component with no manager", LogLevel.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	void SetAnimation(LF_LightAnimation anim)
	{
		m_Animation = anim;
	}

	//------------------------------------------------------------------------------------------------
	override event protected void EOnSimulate(IEntity owner, float timeSlice)
	{
		if (m_Animation) m_Animation.Tick(timeSlice);
	}
}