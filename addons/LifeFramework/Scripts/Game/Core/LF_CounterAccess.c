//------------------------------------------------------------------------------------------------
//! Who a counter screen (ATM terminal, shop price list) opens for: the player who used the counter,
//! on that player's own machine. HasLocalEffectOnlyScript keeps PerformAction on the acting
//! machine; this is the second guard, so a listen host never opens a screen for someone else.
//------------------------------------------------------------------------------------------------
class LF_CounterAccess
{
	//! A player this far behind a clerk's back plane still counts as at the counter (the end of it).
	static const float BACK_SLACK = 0.3;	// lf-fixed: a step round the counter end, not a balance number

	//------------------------------------------------------------------------------------------------
	//! Pure: whether a spot is on the customer side of a clerk who faces the customers: in front of the
	//! plane across the clerk's body, on the ground plane. A player behind the counter stands behind
	//! the clerk's back, so the prompt and the talk stay on the public side.
	//! \param npcPos The clerk's position.
	//! \param npcForward The way the clerk faces (any length; a zero vector counts as the customer side).
	//! \param userPos The player's position.
	static bool InFront(vector npcPos, vector npcForward, vector userPos)
	{
		vector forward = Vector(npcForward[0], 0, npcForward[2]);
		if (forward.Length() < 0.001)
			return true;

		forward.Normalize();
		vector delta = Vector(userPos[0] - npcPos[0], 0, userPos[2] - npcPos[2]);
		return vector.Dot(forward, delta) >= -BACK_SLACK;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether `user` stands on the customer side of the counter clerk `npc` (see InFront).
	static bool OnCustomerSide(IEntity user, IEntity npc)
	{
		if (!user || !npc)
			return false;

		return InFront(npc.GetOrigin(), npc.GetWorldTransformAxis(2), user.GetOrigin());
	}

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
