//------------------------------------------------------------------------------------------------
//! How far a break-in has come, as a word the player reads.
enum LF_EBreakInProgress
{
	NOT_STARTED,
	BARELY_STARTED,
	HALFWAY,
	NEARLY_THROUGH,
	THROUGH
}

//------------------------------------------------------------------------------------------------
//! One break-in's progress: the stage it is on, how far into that stage, how long work has been
//! stopped, and whether a reset is waiting for the defenders. Plain data; LF_BreakInRules moves it.
class LF_BreakInState
{
	//! Stages finished so far.
	int m_iStage;
	//! 0..1 into the current stage.
	float m_fProgress;
	//! Seconds since every work point was last worked.
	float m_fIdleSeconds;
	//! True once the stage fell back; the caller resets the defenders and calls ConsumeReset.
	bool m_bResetPending;
	//! True while this stop in work has already fallen back, so one gap resets once.
	bool m_bGapHandled;
	//! True when the last stage finished.
	bool m_bThrough;
}

//------------------------------------------------------------------------------------------------
//! The staged break-in rules, pure logic (#3368): a stage advances only while every one of its work
//! points is worked at the same time; if work stops for longer than the grace, the current stage
//! falls back to its start and a reset is flagged for the defenders. The reset fires on stopped
//! work, never on a player's approach. Time comes in as seconds elapsed, so no world clock is read.
class LF_BreakInRules
{
	//------------------------------------------------------------------------------------------------
	//! Moves \p state by \p dt seconds. \p worked is how many of the \p required work points are
	//! being worked right now. Below \p required nothing advances and the idle clock runs.
	static void Tick(notnull LF_BreakInState state, int worked, int required, float dt, notnull LF_BreakInConfig config)
	{
		if (state.m_bThrough || required <= 0 || dt <= 0)
			return;

		if (worked >= required)
		{
			state.m_fIdleSeconds = 0;
			state.m_bGapHandled = false;
			state.m_fProgress += dt / Math.Max(config.m_fSecondsPerStage, 0.001);
			while (state.m_fProgress >= 1 && !state.m_bThrough)
			{
				state.m_fProgress -= 1;
				state.m_iStage++;
				if (state.m_iStage >= config.m_iStageCount)
				{
					state.m_iStage = config.m_iStageCount;
					state.m_fProgress = 0;
					state.m_bThrough = true;
				}
			}

			return;
		}

		state.m_fIdleSeconds += dt;
		if (state.m_bGapHandled || state.m_fIdleSeconds <= config.m_fGraceSeconds)
			return;

		state.m_bGapHandled = true;
		if (state.m_fProgress <= 0)
			return;

		state.m_fProgress = 0;
		state.m_bResetPending = true;
		LF_Debug.Info(LF_BreakInConfig.FEATURE, string.Format("work stopped %1 s, stage %2 falls back, defenders reset", state.m_fIdleSeconds, state.m_iStage + 1));
	}

	//------------------------------------------------------------------------------------------------
	//! \return True once after a fall-back, then false until the next one.
	static bool ConsumeReset(notnull LF_BreakInState state)
	{
		bool pending = state.m_bResetPending;
		state.m_bResetPending = false;
		return pending;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Overall progress 0..1 across every stage.
	static float Overall(notnull LF_BreakInState state, notnull LF_BreakInConfig config)
	{
		if (state.m_bThrough)
			return 1;

		return Math.Clamp((state.m_iStage + state.m_fProgress) / Math.Max(config.m_iStageCount, 1), 0, 1);
	}

	//------------------------------------------------------------------------------------------------
	//! \return The band the \p overall progress (0..1) falls in.
	static LF_EBreakInProgress Band(float overall, bool through, notnull LF_BreakInConfig config)
	{
		if (through)
			return LF_EBreakInProgress.THROUGH;

		if (overall <= 0)
			return LF_EBreakInProgress.NOT_STARTED;

		if (overall >= config.m_fNearlyFrom)
			return LF_EBreakInProgress.NEARLY_THROUGH;

		if (overall >= config.m_fHalfwayFrom)
			return LF_EBreakInProgress.HALFWAY;

		return LF_EBreakInProgress.BARELY_STARTED;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The band of this break-in.
	static LF_EBreakInProgress ProgressOf(notnull LF_BreakInState state, notnull LF_BreakInConfig config)
	{
		return Band(Overall(state, config), state.m_bThrough, config);
	}

	//------------------------------------------------------------------------------------------------
	//! \return The localization key of the word for a band; empty for NOT_STARTED, which shows nothing.
	static string WordKey(LF_EBreakInProgress band)
	{
		switch (band)
		{
			case LF_EBreakInProgress.BARELY_STARTED:
				return "#LF-BreakIn_Barely";
			case LF_EBreakInProgress.HALFWAY:
				return "#LF-BreakIn_Halfway";
			case LF_EBreakInProgress.NEARLY_THROUGH:
				return "#LF-BreakIn_Nearly";
			case LF_EBreakInProgress.THROUGH:
				return "#LF-BreakIn_Through";
		}

		return "";
	}
}
