//------------------------------------------------------------------------------------------------
//! What a conversation can ask of fishing (#3451). The dialogue file names the hook "CoveTip"; the
//! class name is the registration (LF_DialogueHook.Find). Server only, no state.
//------------------------------------------------------------------------------------------------
//! How many fish a character has landed, read from the FISHER xp the catches booked (the
//! `fish_catch` row of the jobs config), so no new counter is saved.
class LF_FishingLanded
{
	//------------------------------------------------------------------------------------------------
	//! \return Fish landed by the character; 0 when none, or when the jobs config pays no xp per catch.
	static int Count(string playerUid)
	{
		LF_JobsConfig jobs = LF_JobsConfig.Get();
		if (!jobs)
			return 0;

		LF_JobXpAwardDef award = jobs.FindAward(LF_FishingSession.XP_ACTION);
		if (!award || award.m_iXp <= 0)
			return 0;

		return LF_JobLevels.XpOf(playerUid, LF_FishingSession.JOB) / award.m_iXp;
	}

	//------------------------------------------------------------------------------------------------
	//! Splits the hook param "<place id>:<fish landed>" ("fishermans-bay:3").
	//! \return False for a param with no place or no count above 0.
	static bool ParseTip(string param, out string placeId, out int fish)
	{
		placeId = string.Empty;
		fish = 0;
		int cut = param.IndexOf(":");
		if (cut <= 0)
			return false;

		placeId = param.Substring(0, cut); // lf-bounded: a short hook param
		fish = param.Substring(cut + 1, param.Length() - cut - 1).ToInt(); // lf-bounded: a short hook param
		return fish > 0;
	}
}

//------------------------------------------------------------------------------------------------
//! A reply that tells the player about a cove. As a condition it shows once the character has landed
//! the fish the param names; as an action it marks the cove on their map (LF_MapKnown.Hear). The
//! param is "<place id>:<fish landed>".
class LF_DialogueHook_CoveTip : LF_DialogueHook
{
	//------------------------------------------------------------------------------------------------
	override bool Allows(IEntity player, IEntity npc, string param)
	{
		string placeId;
		int fish;
		if (!LF_FishingLanded.ParseTip(param, placeId, fish))
		{
			LF_Debug.Error(LF_FishingConfig.FEATURE, string.Format("cove tip param '%1' is not <place>:<fish landed>", param));
			return false;
		}

		return LF_FishingLanded.Count(LF_Utils.GetPlayerUid(player)) >= fish;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(IEntity player, IEntity npc, string param, notnull array<string> spoken)
	{
		string placeId;
		int fish;
		if (!LF_FishingLanded.ParseTip(param, placeId, fish))
		{
			LF_Debug.Error(LF_FishingConfig.FEATURE, string.Format("cove tip param '%1' is not <place>:<fish landed>", param));
			return;
		}

		// The server checks again what a client could lie about: the count, then the player.
		if (LF_FishingLanded.Count(LF_Utils.GetPlayerUid(player)) < fish)
		{
			LF_Debug.Warn(LF_FishingConfig.FEATURE, string.Format("cove tip %1 refused: fewer than %2 fish landed", placeId, fish));
			return;
		}

		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(player);
		if (playerId <= 0)
		{
			LF_Debug.Warn(LF_FishingConfig.FEATURE, string.Format("cove tip %1 refused: no player controls the speaker's listener", placeId));
			return;
		}

		if (LF_MapKnown.Hear(playerId, placeId))
			LF_Debug.Info(LF_FishingConfig.FEATURE, string.Format("player %1 was told of the cove %2", playerId, placeId));
	}
}
