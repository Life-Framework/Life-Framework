//------------------------------------------------------------------------------------------------
//! Harness stand-in for the rules quiz (#1567, gap named in #1542). A proof that wants a rejoin to
//! see the quiz again has to forget the pass first, and the pass sits in the server's memory and in the
//! progression store (LF_Progression.NoteFlag), where a client cannot reach:
//!   quizclear   (server, behind the harness gate) forgets the sending client's own account's pass,
//!               in memory and in the store, so the next connect asks the quiz again; replies
//!               `ok quiz pass cleared for <uid> had=<0|1>`
//! The clearing itself is LF_Quiz.Reset (the staff reset). This class only reaches it.
//! HARNESS ONLY: Run refuses in any build without -scrDefine LF_HARNESS, on top of the gate the RPC
//! already passed (LF_DevGate.CheckHarnessRequest), so a real server never runs it, an administrator
//! included. The define changes the script checksum, so a real player's client cannot join a harness
//! build either.
//------------------------------------------------------------------------------------------------
class LF_QuizHarness
{
	static const string FEATURE = "AgentHarness";
	static const string VERB = "quizclear";

	//------------------------------------------------------------------------------------------------
	//! Pure: whether a harness tool string is this verb ("quizclear", with or without a trailing space).
	static bool IsVerb(string tool)
	{
		return tool == VERB || tool == VERB + " ";
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: forgets one account's quiz pass in memory (this session's pass, the failed-attempt count)
	//! and in the progression store, the way the staff reset does. Not gated by the build: the gate is
	//! Run, the one entry a client can reach.
	//! \return Whether there was a pass or attempts to forget.
	static bool Clear(string accountUid)
	{
		return LF_Quiz.Reset(accountUid);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the `quizclear` verb for this character's player.
	//! \param tool "quizclear"
	//! \return "ok ..." or "error <reason>"
	static string Run(IEntity character, string tool)
	{
		if (!LF_DevGate.HarnessBuild())
		{
			LF_Debug.Warn(FEATURE, "quizclear refused: not a LF_HARNESS build");
			return "error harness_only";
		}

		if (!IsVerb(tool))
			return "error usage_quizclear";

		if (!character)
			return "error no_character";

		PlayerManager manager = GetGame().GetPlayerManager();
		int playerId = 0;
		if (manager)
			playerId = manager.GetPlayerIdFromControlledEntity(character);

		if (playerId <= 0)
			return "error no_player";

		// The account the quiz keys on (LF_QuizController), resolved by the server, never from the payload.
		string accountUid = LF_Utils.GetAccountUID(playerId);
		if (accountUid.IsEmpty())
			return "error no_account";

		bool had = Clear(accountUid);
		int hadFlag = 0;
		if (had)
			hadFlag = 1;

		LF_Debug.Info(FEATURE, string.Format("quizclear by player %1 (%2): had=%3", playerId, accountUid, hadFlag));
		return string.Format("ok quiz pass cleared for %1 had=%2", accountUid, hadFlag);
	}
}
