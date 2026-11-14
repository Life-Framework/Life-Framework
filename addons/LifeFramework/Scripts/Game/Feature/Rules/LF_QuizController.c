//------------------------------------------------------------------------------------------------
//! Client <-> server bridge of the rules quiz (#915), hosted on the PlayerController like the
//! registration form: it exists before any character does.
//!   server -> owner : LF_ShowQuiz (the questions, never the answers), LF_ShowQuizResult
//!   owner -> server : LF_AskQuizAnswers, LF_AskQuizRetry, LF_AskQuizContinue
//! The server identifies the player by this controller's id and scores every answer itself
//! (LF_Quiz.Submit); a retry or a continue is honoured only while the account still has to pass or
//! has passed, which LF_CharacterCreationManager checks again before it opens the next screen.
modded class SCR_PlayerController
{
	//------------------------------------------------------------------------------------------------
	//! Server: open the quiz on this player's machine.
	void LF_ShowQuiz(string payload, int attemptsLeft)
	{
		if (LF_IsLocalController())
			RpcDo_LF_ShowQuiz(payload, attemptsLeft);
		else
			Rpc(RpcDo_LF_ShowQuiz, payload, attemptsLeft);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: show the player how an attempt went.
	void LF_ShowQuizResult(bool passed, int correct, int total, int needed, int attemptsLeft, string notes)
	{
		if (LF_IsLocalController())
			RpcDo_LF_ShowQuizResult(passed, correct, total, needed, attemptsLeft, notes);
		else
			Rpc(RpcDo_LF_ShowQuizResult, passed, correct, total, needed, attemptsLeft, notes);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the option picked for each question, "2,0,1".
	void LF_AskQuizAnswers(string answers)
	{
		if (Replication.IsServer())
			RpcAsk_LF_QuizAnswers(answers);
		else
			Rpc(RpcAsk_LF_QuizAnswers, answers);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: ask for a fresh set of the questions after a failed attempt.
	void LF_AskQuizRetry()
	{
		if (Replication.IsServer())
			RpcAsk_LF_QuizRetry();
		else
			Rpc(RpcAsk_LF_QuizRetry);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the player read the pass page and goes on to registration.
	void LF_AskQuizContinue()
	{
		if (Replication.IsServer())
			RpcAsk_LF_QuizContinue();
		else
			Rpc(RpcAsk_LF_QuizContinue);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_ShowQuiz(string payload, int attemptsLeft)
	{
		if (!GetGame().GetWorkspace())
		{
			LF_Debug.Error(LF_Quiz.FEATURE, "quiz not opened: no workspace on this machine");
			return;
		}

		LF_DestroySpawnSplash();
		if (!LF_QuizSheet.Open(this, payload, attemptsLeft))
			LF_Debug.Error(LF_Quiz.FEATURE, "failed to open the quiz sheet");
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_ShowQuizResult(bool passed, int correct, int total, int needed, int attemptsLeft, string notes)
	{
		if (!GetGame().GetWorkspace())
			return;

		if (!LF_QuizSheet.ShowResult(this, passed, correct, total, needed, attemptsLeft, notes))
			LF_Debug.Error(LF_Quiz.FEATURE, "failed to open the quiz result");
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_QuizAnswers(string answers)
	{
		LF_QuizFlow.ServerAnswers(GetPlayerId(), answers);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_QuizRetry()
	{
		LF_QuizFlow.ServerRetry(GetPlayerId());
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_QuizContinue()
	{
		LF_QuizFlow.ServerContinue(GetPlayerId());
	}
}

//------------------------------------------------------------------------------------------------
//! Server side of the quiz conversation: who is asked, what an answer does. The decisions are
//! LF_Quiz's; this only moves a player between the sheet, the kick and the next step of the
//! first-join flow (LF_CharacterCreationManager).
class LF_QuizFlow
{
	static const int KICK_DELAY_MS = 6000;	// lf-fixed: long enough to read the last page, not a balance number

	//------------------------------------------------------------------------------------------------
	protected static SCR_PlayerController Find(int playerId)
	{
		return SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: whether this player may answer now: their account is known and has not passed.
	static bool CanUseQuiz(int playerId)
	{
		return LF_Quiz.Required(LF_Utils.GetAccountUID(playerId));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: send the player the questions.
	static void Offer(int playerId)
	{
		SCR_PlayerController controller = Find(playerId);
		if (!controller)
		{
			LF_Debug.Error(LF_Quiz.FEATURE, string.Format("quiz not sent: player %1 has no controller", playerId));
			return;
		}

		string uid = LF_Utils.GetAccountUID(playerId);
		LF_Debug.Info(LF_Quiz.FEATURE, string.Format("quiz sent to player %1 (%2), %3 attempt(s) left", playerId, uid, LF_Quiz.AttemptsLeft(uid)));
		controller.LF_ShowQuiz(LF_Quiz.BuildPayload(), LF_Quiz.AttemptsLeft(uid));
	}

	//------------------------------------------------------------------------------------------------
	static void ServerAnswers(int playerId, string text)
	{
		string uid = LF_Utils.GetAccountUID(playerId);
		SCR_PlayerController controller = Find(playerId);
		if (!controller || !CanUseQuiz(playerId))
		{
			LF_Debug.Warn(LF_Quiz.FEATURE, string.Format("quiz answers from player %1 dropped: nothing to answer", playerId));
			return;
		}

		array<int> answers = {};
		if (!LF_Quiz.ParseAnswers(text, answers))
		{
			LF_Debug.Warn(LF_Quiz.FEATURE, string.Format("quiz answers from player %1 refused: not a list of numbers", playerId));
			Offer(playerId);
			return;
		}

		LF_QuizResult result = LF_Quiz.Submit(uid, answers);
		if (result.m_bRefused)
		{
			Offer(playerId);
			return;
		}

		string notes;
		foreach (int i, string note : result.m_aNotes)
		{
			if (i > 0)
				notes += LF_Quiz.ROW_SEP;

			notes += note;
		}

		controller.LF_ShowQuizResult(result.m_bPassed, result.m_iCorrect, result.m_iTotal, result.m_iNeeded, result.m_iAttemptsLeft, notes);
		if (result.m_bKick)
			GetGame().GetCallqueue().CallLater(Kick, KICK_DELAY_MS, false, playerId);
	}

	//------------------------------------------------------------------------------------------------
	static void ServerRetry(int playerId)
	{
		string uid = LF_Utils.GetAccountUID(playerId);
		if (!LF_Quiz.Required(uid) || LF_Quiz.AttemptsLeft(uid) == 0)
		{
			LF_Debug.Warn(LF_Quiz.FEATURE, string.Format("quiz retry from player %1 dropped", playerId));
			return;
		}

		Offer(playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! The pass page's Continue: back into the first-join flow, which asks again whether the quiz is
	//! still required before it opens registration.
	static void ServerContinue(int playerId)
	{
		LF_CharacterCreationManager manager = LF_CharacterCreationManager.GetInstance();
		if (!manager)
		{
			LF_Debug.Error(LF_Quiz.FEATURE, "quiz continue dropped: no creation manager on the server");
			return;
		}

		manager.ContinueAfterQuiz(playerId);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Kick(int playerId)
	{
		LF_Debug.Warn(LF_Quiz.FEATURE, string.Format("kicking player %1: failed the rules quiz", playerId));
		GetGame().GetPlayerManager().KickPlayer(playerId, PlayerManagerKickReason.KICK, 10);
	}
}
