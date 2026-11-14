// LF_Chain.c — Chain (#794).
// TODO(scaffold): one line on what this does, who calls it and on which side (server or client).

class LF_Chain
{
	protected static int s_iUses;

	//------------------------------------------------------------------------------------------------
	//! The rule, decided on the server. TODO(scaffold): replace the parameter with the real inputs
	//! (account, faction, distance, cooldown) and log every refusal with its reason.
	static bool CanUse(bool hasCharacter)
	{
		if (!hasCharacter)
		{
			LF_Debug.Warn("Onboarding", "chain refused: no character");
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server. Returns false and logs when refused; a silent no-op is a bug.
	static bool Use(bool hasCharacter)
	{
		if (!CanUse(hasCharacter))
			return false;

		s_iUses = s_iUses + 1;
		LF_Debug.Info("Onboarding", string.Format("chain used (%1)", s_iUses));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	static int GetUses()
	{
		return s_iUses;
	}

	//------------------------------------------------------------------------------------------------
	//! Called from LF_StaticReset.ResetAll when a world ends.
	static void ResetStatics()
	{
		s_iUses = 0;
	}
}
