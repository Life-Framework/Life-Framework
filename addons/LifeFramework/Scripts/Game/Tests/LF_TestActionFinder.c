//------------------------------------------------------------------------------------------------
//! Test helper: the user action of a spawned prop, found the way the engine holds it (the prefab's
//! ActionsManagerComponent), so a test drives `PerformAction` on the instance the engine already
//! initialised (`Init` ran against the real owner). Cast the result to the action class.
class LF_TestActionFinder
{
	//------------------------------------------------------------------------------------------------
	//! \return The first action on `prop` whose class is `wanted` or inherits it, or null.
	static BaseUserAction Find(IEntity prop, typename wanted)
	{
		if (!prop)
			return null;

		BaseActionsManagerComponent manager = BaseActionsManagerComponent.Cast(prop.FindComponent(BaseActionsManagerComponent));
		if (!manager)
			return null;

		array<BaseUserAction> actions = {};
		manager.GetActionsList(actions);
		foreach (BaseUserAction action : actions)
		{
			if (action && action.Type().IsInherited(wanted))
				return action;
		}

		return null;
	}
}
