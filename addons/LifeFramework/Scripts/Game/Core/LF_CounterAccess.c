//------------------------------------------------------------------------------------------------
//! Who a counter screen (ATM terminal, shop price list) opens for: the player who used the counter,
//! on that player's own machine. HasLocalEffectOnlyScript keeps PerformAction on the acting
//! machine; this is the second guard, so a listen host never opens a screen for someone else.
//------------------------------------------------------------------------------------------------
class LF_CounterAccess
{
	//------------------------------------------------------------------------------------------------
	//! \param user The entity that performed the action.
	//! \param localCharacter The character this machine controls (null on a dedicated server).
	//! \return true when the user is this machine's own character.
	static bool IsLocalActor(IEntity user, IEntity localCharacter)
	{
		if (!user || !localCharacter)
			return false;

		return user == localCharacter;
	}
}
