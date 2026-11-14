// LF_Quiz.c — the rules orientation quiz (#915). Server: decides who is asked, scores the answers,
// counts the failures, remembers the pass. The client sheet (LF_QuizSheet) only shows what the
// server sends and sends back the option each question was answered with.
//
// THE PASS is kept per account uid in the progression store (LF_Progression.NoteFlag, the
// "progression" payload), so it survives a restart with no new saved field. It is not kept per
// character: a player who registers a second character is not asked again.

//------------------------------------------------------------------------------------------------
//! What one submitted attempt came to.
class LF_QuizResult
{
	//! The answers were malformed (wrong count, an option that does not exist): not scored, not counted.
	bool m_bRefused;
	bool m_bPassed;
	int m_iCorrect;
	int m_iTotal;
	int m_iNeeded;
	//! Failed attempts left before the kick, -1 when the owner allows unlimited retries.
	int m_iAttemptsLeft = -1;
	//! The player failed their last allowed attempt.
	bool m_bKick;
	//! The explanation (a #key or plain text) of each question answered wrong, in question order.
	ref array<string> m_aNotes = {};
}

class LF_Quiz
{
	static const string FEATURE = "Rules";
	static const string PASS_FLAG = "rules-quiz";
	static const string FIELD_SEP = "\t";
	static const string ROW_SEP = "\n";

	//! Failed attempts since the last pass, per account uid. Session only: a kick clears it.
	protected static ref map<string, int> s_mFailed;
	//! Accounts that passed in this session (the gate when the owner asks on every connect).
	protected static ref map<string, bool> s_mPassedNow;
	protected static int s_iAttempts;
	protected static int s_iPasses;
	protected static int s_iFails;
	protected static int s_iKicks;

	//------------------------------------------------------------------------------------------------
	protected static map<string, int> Failed()
	{
		if (!s_mFailed)
			s_mFailed = new map<string, int>();

		return s_mFailed;
	}

	//------------------------------------------------------------------------------------------------
	protected static map<string, bool> PassedNow()
	{
		if (!s_mPassedNow)
			s_mPassedNow = new map<string, bool>();

		return s_mPassedNow;
	}

	//------------------------------------------------------------------------------------------------
	//! \return "" when the question can be asked, else why not.
	static string RefuseQuestion(LF_QuizQuestion question)
	{
		if (!question)
			return "empty entry";

		if (question.m_sQuestion.IsEmpty())
			return string.Format("question '%1' has no text", question.m_sId);

		if (!question.m_aOptions || question.m_aOptions.Count() < 2)
			return string.Format("question '%1' needs at least two options", question.m_sId);

		foreach (string option : question.m_aOptions)
		{
			if (option.IsEmpty())
				return string.Format("question '%1' has a blank option", question.m_sId);
		}

		if (question.m_iCorrect < 0 || question.m_iCorrect >= question.m_aOptions.Count())
			return string.Format("question '%1' names option %2 as right but has %3", question.m_sId, question.m_iCorrect, question.m_aOptions.Count());

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the quiz can run at all: switched on and at least one usable question. With no
	//! questions nobody is asked (a core-only server).
	static bool Enabled()
	{
		if (!LF_QuizConfig.Enabled())
			return false;

		array<LF_QuizQuestion> questions = {};
		LF_QuizBank.GetQuestions(questions);
		return !questions.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	//! Whether this account has passed: the stored pass, or in this session when the owner asks on
	//! every connect.
	static bool HasPassed(string accountUid)
	{
		if (accountUid.IsEmpty())
			return false;

		if (PassedNow().Contains(accountUid))
			return true;

		if (LF_QuizConfig.RepeatOnConnect())
			return false;

		return LF_Progression.HasFlag(accountUid, PASS_FLAG);
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the server must ask this account before it plays.
	static bool Required(string accountUid)
	{
		if (accountUid.IsEmpty() || !Enabled())
			return false;

		return !HasPassed(accountUid);
	}

	//------------------------------------------------------------------------------------------------
	//! Correct answers needed to pass: the pass percent of the questions, rounded up, at least 1.
	static int Needed(int total)
	{
		if (total <= 0)
			return 0;

		int needed = (total * LF_QuizConfig.PassPercent() + 99) / 100;
		return Math.ClampInt(needed, 1, total);
	}

	//------------------------------------------------------------------------------------------------
	//! Failed attempts left before the kick, -1 when unlimited.
	static int AttemptsLeft(string accountUid)
	{
		int max = LF_QuizConfig.MaxAttempts();
		if (max <= 0)
			return -1;

		int failed;
		Failed().Find(accountUid, failed);
		return Math.Max(0, max - failed);
	}

	//------------------------------------------------------------------------------------------------
	//! The questions as the client gets them: one row per question, fields split by FIELD_SEP, the
	//! question then each option. The right answer and the explanation are not in it.
	static string BuildPayload()
	{
		array<LF_QuizQuestion> questions = {};
		LF_QuizBank.GetQuestions(questions);
		string payload;
		foreach (int i, LF_QuizQuestion question : questions)
		{
			if (i > 0)
				payload += ROW_SEP;

			payload += question.m_sQuestion;
			foreach (string option : question.m_aOptions)
			{
				payload += FIELD_SEP + option;
			}
		}

		return payload;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the payload back into rows of question then options.
	//! \return The number of questions.
	static int ParsePayload(string payload, notnull array<ref array<string>> outRows)
	{
		outRows.Clear();
		array<string> rows = {};
		payload.Split(ROW_SEP, rows, true);
		foreach (string row : rows)
		{
			array<string> fields = {};
			row.Split(FIELD_SEP, fields, false);
			if (fields.Count() < 3)
				continue;

			array<string> copy = {};
			copy.Copy(fields);
			outRows.Insert(copy);
		}

		return outRows.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the answers a client sent ("2,0,1": the option picked for each question) as numbers.
	//! \return false when the text is not a list of whole numbers.
	static bool ParseAnswers(string text, notnull array<int> outAnswers)
	{
		outAnswers.Clear();
		// Split on an empty string is a VME in the engine, and the text is the client's.
		if (text.IsEmpty())
			return false;

		array<string> parts = {};
		text.Split(",", parts, false);
		foreach (string part : parts)
		{
			string trimmed = part.Trim();
			int value = trimmed.ToInt();
			if (trimmed.IsEmpty() || trimmed != value.ToString())
			{
				outAnswers.Clear();
				return false;
			}

			outAnswers.Insert(value);
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: scores one attempt, counts it, and remembers a pass. Every attempt is logged.
	static LF_QuizResult Submit(string accountUid, notnull array<int> answers)
	{
		LF_QuizResult result = new LF_QuizResult();
		array<LF_QuizQuestion> questions = {};
		LF_QuizBank.GetQuestions(questions);
		result.m_iTotal = questions.Count();
		result.m_iNeeded = Needed(result.m_iTotal);
		result.m_iAttemptsLeft = AttemptsLeft(accountUid);

		if (accountUid.IsEmpty() || questions.IsEmpty() || answers.Count() != questions.Count())
		{
			result.m_bRefused = true;
			LF_Debug.Warn(FEATURE, string.Format("quiz answers refused for %1: %2 answer(s) for %3 question(s)", accountUid, answers.Count(), questions.Count()));
			return result;
		}

		foreach (int answer : answers)
		{
			if (answer < 0 || answer > 99)
			{
				result.m_bRefused = true;
				LF_Debug.Warn(FEATURE, string.Format("quiz answers refused for %1: option %2 is out of range", accountUid, answer));
				return result;
			}
		}

		foreach (int i, LF_QuizQuestion question : questions)
		{
			if (answers[i] == question.m_iCorrect)
			{
				result.m_iCorrect++;
				continue;
			}

			string note = question.m_sExplain;
			if (note.IsEmpty())
				note = question.m_sQuestion;

			result.m_aNotes.Insert(note);
		}

		s_iAttempts++;
		result.m_bPassed = result.m_iCorrect >= result.m_iNeeded;
		if (result.m_bPassed)
		{
			s_iPasses++;
			Failed().Remove(accountUid);
			PassedNow().Set(accountUid, true);
			LF_Progression.NoteFlag(accountUid, PASS_FLAG);
			result.m_iAttemptsLeft = AttemptsLeft(accountUid);
			result.m_aNotes.Clear();
			LF_Debug.Info(FEATURE, string.Format("quiz passed by %1: %2 of %3 right (needed %4)", accountUid, result.m_iCorrect, result.m_iTotal, result.m_iNeeded));
			return result;
		}

		s_iFails++;
		int failed;
		Failed().Find(accountUid, failed);
		failed++;
		Failed().Set(accountUid, failed);
		result.m_iAttemptsLeft = AttemptsLeft(accountUid);
		LF_Debug.Info(FEATURE, string.Format("quiz failed by %1: %2 of %3 right (needed %4), failed attempt %5, %6 left", accountUid, result.m_iCorrect, result.m_iTotal, result.m_iNeeded, failed, result.m_iAttemptsLeft));
		if (result.m_iAttemptsLeft == 0)
		{
			result.m_bKick = true;
			s_iKicks++;
			Failed().Remove(accountUid);
			LF_Debug.Warn(FEATURE, string.Format("quiz: %1 used every attempt and is kicked", accountUid));
		}

		return result;
	}

	//------------------------------------------------------------------------------------------------
	//! Staff: forget one account's pass so it takes the quiz again. Logged.
	//! \return Whether there was anything to forget.
	static bool Reset(string accountUid)
	{
		if (accountUid.IsEmpty())
		{
			LF_Debug.Warn(FEATURE, "quiz reset refused: no account");
			return false;
		}

		bool had = LF_Progression.HasFlag(accountUid, PASS_FLAG) || PassedNow().Contains(accountUid) || Failed().Contains(accountUid);
		LF_Progression.ClearFlag(accountUid, PASS_FLAG);
		PassedNow().Remove(accountUid);
		Failed().Remove(accountUid);
		LF_Debug.Info(FEATURE, string.Format("quiz reset for %1 (had a pass or attempts: %2)", accountUid, had));
		return had;
	}

	//------------------------------------------------------------------------------------------------
	//! Staff: the counts since the world started, one line.
	static string StatsLine()
	{
		return string.Format("rules quiz: %1 attempt(s), %2 passed, %3 failed, %4 kicked", s_iAttempts, s_iPasses, s_iFails, s_iKicks);
	}

	//------------------------------------------------------------------------------------------------
	static int GetAttempts()
	{
		return s_iAttempts;
	}

	//------------------------------------------------------------------------------------------------
	//! Called from LF_StaticReset.ResetAll when a world ends.
	static void ResetStatics()
	{
		s_mFailed = null;
		s_mPassedNow = null;
		s_iAttempts = 0;
		s_iPasses = 0;
		s_iFails = 0;
		s_iKicks = 0;
		LF_QuizConfig.Reload();
		LF_QuizBank.Reload();
	}
}
