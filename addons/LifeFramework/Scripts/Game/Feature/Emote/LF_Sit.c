// LF_Sit.c — sitting on chairs and benches (#982). The rule and the seat position; the action
// (LF_SitAction) runs them on every machine and the base game loiter carries the pose to others.

//! Why a sit request was refused; NONE means it may go ahead.
enum LF_ESitRefusal
{
	NONE,
	NO_CHARACTER,
	DEAD,
	RESTRAINED,
	STUNNED,
	OCCUPIED,
	BUSY
}

class LF_Sit
{
	static const string FEATURE = "Emote";

	//------------------------------------------------------------------------------------------------
	//! The rule, with no world: who may sit on a seat. Checked in that order so the reason a player
	//! reads is the one that matters most.
	static LF_ESitRefusal Decide(bool hasCharacter, bool alive, bool restrained, bool stunned, bool occupiedByOther, bool busy)
	{
		if (!hasCharacter)
			return LF_ESitRefusal.NO_CHARACTER;

		if (!alive)
			return LF_ESitRefusal.DEAD;

		if (restrained)
			return LF_ESitRefusal.RESTRAINED;

		if (stunned)
			return LF_ESitRefusal.STUNNED;

		if (occupiedByOther)
			return LF_ESitRefusal.OCCUPIED;

		if (busy)
			return LF_ESitRefusal.BUSY;

		return LF_ESitRefusal.NONE;
	}

	//------------------------------------------------------------------------------------------------
	//! The string key a player reads for a refusal.
	static string ReasonKey(LF_ESitRefusal refusal)
	{
		if (refusal == LF_ESitRefusal.OCCUPIED)
			return "#LF-Sit_Occupied";

		if (refusal == LF_ESitRefusal.RESTRAINED)
			return "#LF-Emote_Restrained";

		return "#LF-Sit_Denied";
	}

	//------------------------------------------------------------------------------------------------
	//! The rule for a real character on this machine. Uses only replicated state (life state, the
	//! restrained and stunned flags, the loiter), so a client and the server reach the same answer.
	static LF_ESitRefusal DecideFor(IEntity user, bool occupiedByOther)
	{
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(user);
		if (!character)
			return Decide(false, false, false, false, occupiedByOther, false);

		bool alive = true;
		bool busy = false;
		SCR_CharacterControllerComponent controller = SCR_CharacterControllerComponent.Cast(character.GetCharacterController());
		if (controller)
		{
			alive = controller.GetLifeState() == ECharacterLifeState.ALIVE;
			busy = controller.IsPlayingGesture();
		}

		CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
		if (access && access.IsInCompartment())
			busy = true;

		return Decide(true, alive, LF_Restraint.IsRestrained(character), LF_Stun.IsStunned(character), occupiedByOther, busy);
	}

	//------------------------------------------------------------------------------------------------
	//! Where a sitter stands before the sit pose plays: the seat's offset in the seat's own space, turned
	//! by yawDegrees about the seat's up axis (0 faces the seat's +z). Pure math, so a LOGIC test covers it.
	static void SeatTransform(vector seat[4], vector offset, float yawDegrees, out vector result[4])
	{
		float rad = yawDegrees * Math.DEG2RAD;
		vector localForward = Vector(Math.Sin(rad), 0, Math.Cos(rad));

		vector worldForward = localForward.Multiply3(seat);
		vector worldUp = seat[1];
		Math3D.DirectionAndUpMatrix(worldForward, worldUp, result);
		result[3] = seat[3] + offset.Multiply3(seat);
	}
}
