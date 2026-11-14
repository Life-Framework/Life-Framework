// red-proof: observed 2026-11-14 with `tools\cli test --no-build --only LF_Test_Quiz,LF_Test_QuizPersist`, the stored-pass read in
// LF_Quiz.HasPassed replaced by `return false;`:
//   SUMMARY tier=only passed=0 failed=2 total=2 skipped=0
//   [LFTEST] FAIL rules/quiz
//   [LFTEST]   - a later session does not ask a passed account
// Restored; green.
// tier: LOGIC
//! Rules orientation quiz (#915): who is asked, scoring, the retry limit, the once-per-account pass,
//! the owner switches and what the client is sent. No world.
class LF_Test_Quiz : LF_Test
{
	protected static const string ACCOUNT = "quiz-test-account";
	protected static const string OTHER = "quiz-test-other";

	override string GetName()
	{
		return "rules/quiz";
	}

	//------------------------------------------------------------------------------------------------
	static LF_QuizConfig MakeConfig()
	{
		LF_QuizConfig config = new LF_QuizConfig();
		config.m_bEnabled = true;
		config.m_bRepeatOnConnect = false;
		config.m_iPassPercent = 80;
		config.m_iMaxAttempts = 3;
		return config;
	}

	//------------------------------------------------------------------------------------------------
	//! Questions with the right option at 0, 1, 2, 0, 1 and a distinct rule text each.
	static LF_QuizBank MakeBank(int count)
	{
		LF_QuizBank bank = new LF_QuizBank();
		bank.m_aQuestions = new array<ref LF_QuizQuestion>();
		for (int i = 0; i < count; i++)
		{
			LF_QuizQuestion question = new LF_QuizQuestion();
			question.m_sId = "q" + i.ToString();
			question.m_sQuestion = "Question " + i.ToString();
			question.m_aOptions = new array<string>();
			question.m_aOptions.Insert("first");
			question.m_aOptions.Insert("second");
			question.m_aOptions.Insert("third");
			question.m_iCorrect = i % 3;
			question.m_sExplain = "Rule " + i.ToString();
			bank.m_aQuestions.Insert(question);
		}

		return bank;
	}

	//------------------------------------------------------------------------------------------------
	//! The shipped switches and `count` questions.
	static void Use(int count)
	{
		LF_QuizConfig.UseForTest(MakeConfig());
		LF_QuizBank.UseForTest(MakeBank(count));
	}

	//------------------------------------------------------------------------------------------------
	static array<int> Answers(int count, int wrongFrom)
	{
		array<int> answers = {};
		for (int i = 0; i < count; i++)
		{
			int answer = i % 3;
			if (i >= wrongFrom)
				answer = (i + 1) % 3;

			answers.Insert(answer);
		}

		return answers;
	}

	//------------------------------------------------------------------------------------------------
	static void Clean()
	{
		LF_Progression.Forget(ACCOUNT);
		LF_Progression.Forget(OTHER);
		LF_Quiz.ResetStatics();
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		Clean();
		Use(5);

		// Who is asked.
		ctx.True(LF_Quiz.Enabled(), "five usable questions switch the quiz on");
		ctx.True(LF_Quiz.Required(ACCOUNT), "an account that has not passed is asked");
		ctx.False(LF_Quiz.Required(""), "no account id is never asked (the flow waits for the identity)");
		ctx.Equal(4, LF_Quiz.Needed(5), "80 percent of five is four");
		ctx.Equal(1, LF_Quiz.Needed(1), "one question needs one right");

		// Scoring: wrong answers are explained, the attempt is counted, the quiz stays required.
		LF_QuizResult third = LF_Quiz.Submit(ACCOUNT, Answers(5, 3));
		ctx.False(third.m_bPassed, "3 of 5 right is not a pass");
		ctx.Equal(3, third.m_iCorrect, "three answers were right");
		ctx.Equal(2, third.m_aNotes.Count(), "each wrong answer brings its rule");
		ctx.EqualStr("Rule 3", third.m_aNotes[0], "the first rule is the first question missed");
		ctx.Equal(2, third.m_iAttemptsLeft, "one of three attempts is used");
		ctx.True(LF_Quiz.Required(ACCOUNT), "a failed account is still asked");
		ctx.False(LF_Quiz.HasPassed(ACCOUNT), "a failed account has not passed");

		// A malformed answer list is refused and not counted as an attempt.
		int attemptsBefore = LF_Quiz.GetAttempts();
		array<int> shortList = {0, 1};
		ctx.True(LF_Quiz.Submit(ACCOUNT, shortList).m_bRefused, "too few answers are refused");
		array<int> outOfRange = {0, 1, 2, 0, 500};
		ctx.True(LF_Quiz.Submit(ACCOUNT, outOfRange).m_bRefused, "an option that does not exist is refused");
		ctx.Equal(attemptsBefore, LF_Quiz.GetAttempts(), "a refused list is not an attempt");
		ctx.Equal(2, LF_Quiz.AttemptsLeft(ACCOUNT), "a refused list costs no attempt");

		// The retry limit: the third failure is the kick, and the count starts again after it.
		ctx.False(LF_Quiz.Submit(ACCOUNT, Answers(5, 0)).m_bKick, "the second failure does not kick");
		LF_QuizResult last = LF_Quiz.Submit(ACCOUNT, Answers(5, 0));
		ctx.True(last.m_bKick, "the third failure kicks");
		ctx.Equal(0, last.m_iAttemptsLeft, "no attempts are left on the kick");
		ctx.Equal(3, LF_Quiz.AttemptsLeft(ACCOUNT), "after the kick the account has its attempts back");

		// A pass is remembered per account.
		LF_QuizResult pass = LF_Quiz.Submit(ACCOUNT, Answers(5, 5));
		ctx.True(pass.m_bPassed, "all five right passes");
		ctx.True(pass.m_aNotes.IsEmpty(), "a pass lists no rules");
		ctx.True(LF_Quiz.HasPassed(ACCOUNT), "the account has passed");
		ctx.False(LF_Quiz.Required(ACCOUNT), "a passed account is not asked again");
		ctx.True(LF_Quiz.Required(OTHER), "another account is still asked");
		ctx.True(LF_Progression.HasFlag(ACCOUNT, LF_Quiz.PASS_FLAG), "the pass is in the progression store, which is saved");

		// Next session: the session counters are gone, the store is not.
		LF_Quiz.ResetStatics();
		Use(5);
		ctx.False(LF_Quiz.Required(ACCOUNT), "a later session does not ask a passed account");

		// Staff reset forces a retake.
		ctx.True(LF_Quiz.Reset(ACCOUNT), "the reset finds a pass to forget");
		ctx.True(LF_Quiz.Required(ACCOUNT), "a reset account is asked again");
		ctx.False(LF_Quiz.Reset(""), "a reset with no account is refused");

		// Owner switch: ask on every connect.
		LF_QuizConfig repeat = MakeConfig();
		repeat.m_bRepeatOnConnect = true;
		LF_Quiz.Submit(ACCOUNT, Answers(5, 5));
		LF_Quiz.ResetStatics();
		Use(5);
		LF_QuizConfig.UseForTest(repeat);
		ctx.True(LF_Quiz.Required(ACCOUNT), "repeat on connect asks an account with a stored pass");
		LF_Quiz.Submit(ACCOUNT, Answers(5, 5));
		ctx.False(LF_Quiz.Required(ACCOUNT), "repeat on connect does not ask twice in one session");

		// Owner switch: off, and no questions, ask nobody.
		LF_QuizConfig off = MakeConfig();
		off.m_bEnabled = false;
		Use(5);
		LF_QuizConfig.UseForTest(off);
		ctx.False(LF_Quiz.Required(OTHER), "a quiz the owner switched off asks nobody");
		Use(0);
		ctx.False(LF_Quiz.Required(OTHER), "a core-only server with no questions asks nobody");

		// A broken question is left out, the rest still run.
		LF_QuizBank broken = MakeBank(2);
		broken.m_aQuestions[1].m_iCorrect = 9;
		LF_QuizBank.UseForTest(broken);
		array<LF_QuizQuestion> usable = {};
		LF_QuizBank.GetQuestions(usable);
		ctx.Equal(1, usable.Count(), "a question whose right answer is not an option is skipped");

		// What the client is sent: questions and options, never a rule text or the right answer.
		Use(3);
		string payload = LF_Quiz.BuildPayload();
		array<ref array<string>> rows = {};
		ctx.Equal(3, LF_Quiz.ParsePayload(payload, rows), "the payload parses back to three questions");
		ctx.Equal(4, rows[0].Count(), "a row is the question and its three options");
		ctx.False(payload.Contains("Rule 0"), "the rule text stays on the server");

		// The answer list a client sends.
		array<int> parsed = {};
		ctx.True(LF_Quiz.ParseAnswers("2, 0,1", parsed), "a list of numbers parses");
		ctx.Equal(3, parsed.Count(), "three answers parsed");
		ctx.False(LF_Quiz.ParseAnswers("1,x,2", parsed), "a list with a word in it is refused");
		ctx.False(LF_Quiz.ParseAnswers("", parsed), "an empty list is refused");

		// Owner file section: switches apply, a number outside its range keeps the lower value.
		LF_ConfigService.LoadOwnerString("{\"schemaVersion\":1,\"rulesQuiz\":{\"enabled\":false,\"repeatOnConnect\":true,\"passPercent\":50,\"maxAttempts\":150}}", "test");
		LF_QuizConfig fromOwner = MakeConfig();
		LF_ConfigReader reader = LF_ConfigService.OpenOwnerSection(LF_QuizConfig.SECTION);
		ctx.NotNull(reader, "the rulesQuiz section opens");
		if (reader)
			fromOwner.ApplyOwner(reader);

		ctx.False(fromOwner.m_bEnabled, "the owner switches the quiz off");
		ctx.True(fromOwner.m_bRepeatOnConnect, "the owner asks on every connect");
		ctx.Equal(50, fromOwner.m_iPassPercent, "the owner sets the pass percent");
		ctx.Equal(3, fromOwner.m_iMaxAttempts, "an attempt limit above 20 is rejected and the shipped 3 stays");
		LF_ConfigService.LoadOwnerString("{\"schemaVersion\":1}", "test-end");

		Clean();
	}
}
