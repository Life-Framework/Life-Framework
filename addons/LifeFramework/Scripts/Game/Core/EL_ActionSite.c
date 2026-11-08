//------------------------------------------------------------------------------------------------
//! Server-side "is the player standing at one of these" check for RPCs that a menu sends after
//! the player used a world action (ATM, organization desk). The server looks for the nearest
//! entity that carries the named user action within range of the position. Nothing the client
//! sends names the site: a client-chosen RplId would still need proof the entity is an ATM, and
//! a client can name its own character to pass a distance check. The site also needs no
//! RplComponent, which the placed organization desk does not declare.
//------------------------------------------------------------------------------------------------
class EL_ActionSite
{
	protected static string s_sWantedClass;
	protected static vector s_vOrigin;
	protected static float s_fRange;
	protected static IEntity s_pFound;
	protected static float s_fBestDist;

	//------------------------------------------------------------------------------------------------
	//! \param origin World position to search from, usually the player's origin.
	//! \param range Maximum distance from origin to the site entity's origin, in metres.
	//! \param actionClass Class name of the ScriptedUserAction that marks the site.
	//! \return The nearest entity carrying that action within range, or null.
	static IEntity FindNear(vector origin, float range, string actionClass)
	{
		if (actionClass.IsEmpty() || range <= 0)
			return null;

		if (!GetGame() || !GetGame().GetWorld())
		{
			EL_Debug.Error("ActionSite", string.Format("site lookup skipped for %1: no world", actionClass));
			return null;
		}

		s_sWantedClass = actionClass;
		s_vOrigin = origin;
		s_fRange = range;
		s_pFound = null;
		s_fBestDist = range;
		GetGame().GetWorld().QueryEntitiesBySphere(origin, range, CollectSite, null, EQueryEntitiesFlags.ALL);

		IEntity found = s_pFound;
		s_pFound = null;
		return found;
	}

	//------------------------------------------------------------------------------------------------
	//! \return true when an entity carrying actionClass stands within range of origin.
	static bool IsNear(vector origin, float range, string actionClass)
	{
		return FindNear(origin, range, actionClass) != null;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool CollectSite(IEntity entity)
	{
		if (!entity)
			return true;

		float dist = vector.Distance(entity.GetOrigin(), s_vOrigin);
		if (dist > s_fRange)
			return true;

		if (s_pFound && dist >= s_fBestDist)
			return true;

		if (!HasAction(entity, s_sWantedClass))
			return true;

		s_pFound = entity;
		s_fBestDist = dist;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool HasAction(IEntity entity, string actionClass)
	{
		BaseActionsManagerComponent manager = BaseActionsManagerComponent.Cast(entity.FindComponent(BaseActionsManagerComponent));
		if (!manager)
			return false;

		array<BaseUserAction> actions = {};
		manager.GetActionsList(actions);
		foreach (BaseUserAction action : actions)
		{
			if (action && action.Type().ToString() == actionClass)
				return true;
		}

		return false;
	}
}
