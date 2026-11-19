//------------------------------------------------------------------------------------------------
//! The one gate a search or a robbery of a PLAYER target goes through (#3399, parent #1024, #782):
//! the target must have given up, by having their hands up or by being restrained. Otherwise the
//! action says "They haven't given up." (`#LF-HandsUp_NotGivenUp`) and does nothing.
//!
//! The hands-up flag is read only from a target that is alive, controlled by a player and not
//! downed. A dead body, a downed player or an AI never counts as "hands up", so a stale flag on a
//! character cannot open a search. Restraint (handcuffs) stands on its own.
//! LOGIC-tested (LF_Test_HandsUpGate) through the pure `Allows`.
class LF_HandsUpGate
{
	static const string FEATURE = "HandsUp";
	static const string NOT_GIVEN_UP = "#LF-HandsUp_NotGivenUp";

	//------------------------------------------------------------------------------------------------
	//! Pure. True when the target has given up. The hands-up flag counts only for a target that is
	//! alive, player-controlled and not downed; restraint counts on its own.
	static bool Allows(bool handsUp, bool restrained, bool alive, bool playerControlled, bool downed)
	{
		if (restrained)
			return true;

		return handsUp && alive && playerControlled && !downed;
	}

	//------------------------------------------------------------------------------------------------
	//! Client and server: has \p target given up? Reads the replicated hands-up and restraint flags,
	//! so the offer and the server decision agree. A null or non-character target has not.
	static bool GivenUp(IEntity target)
	{
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(target);
		if (!character)
			return false;

		bool alive = LF_PlayerInteraction.IsAlive(target);
		bool player = LF_PlayerInteraction.PlayerIdOf(target) > 0;
		return Allows(character.LF_IsHandsUp(), character.LF_IsRestrained(), alive, player, character.LF_IsDowned());
	}

	//------------------------------------------------------------------------------------------------
	//! Client and server: the localization key of why \p target may not be searched or robbed, or
	//! empty when they have given up. Reads only, logs nothing (an offer asks it).
	static string Refusal(IEntity target)
	{
		if (GivenUp(target))
			return string.Empty;

		return NOT_GIVEN_UP;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: true when \p user may go on against \p target. Otherwise logs, tells the user "They
	//! haven't given up." and returns false.
	static bool Check(IEntity user, IEntity target, string what)
	{
		string refusal = Refusal(target);
		if (refusal.IsEmpty())
			return true;

		LF_Debug.Info(FEATURE, string.Format("%1 refused: the target has not given up", what));
		LF_Utils.NotifyEntity(user, refusal, "#LF-Interact_Title", 3.0);
		return false;
	}
}
