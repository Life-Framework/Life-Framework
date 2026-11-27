//------------------------------------------------------------------------------------------------
//! What a stage waits for before the heist moves on (#3409).
enum LF_EHeistGate
{
	TARGET_OPENED,	//!< a target (vault, van box, door) was opened
	CREW_IN_AREA,	//!< the crew is inside an area
	TIME_HELD		//!< the crew holds the spot for a stage-specific time
}

//------------------------------------------------------------------------------------------------
enum LF_EHeistState
{
	IDLE,
	RUNNING,
	SUCCEEDED,
	FAILED
}

//------------------------------------------------------------------------------------------------
//! A heist as pure logic: ordered stages, each with a gate, one clock for the whole job and a
//! fail state. A signal for any stage but the current one does nothing. The clock running out or
//! the crew dying fails the heist; a fail (and a success) is final. Times are world milliseconds
//! handed in by the caller, so this class reads no world and no clock of its own.
class LF_HeistStages
{
	static const string FAIL_CLOCK = "clock";
	static const string FAIL_CREW = "crew";

	protected ref array<int> m_aGates = {};
	protected ref array<int> m_aHoldMs = {};
	protected int m_iState = LF_EHeistState.IDLE;
	protected int m_iStage;
	protected int m_iStartMs;
	protected int m_iClockMs;
	protected int m_iHoldSinceMs = -1;
	protected string m_sFailReason;

	//------------------------------------------------------------------------------------------------
	//! Append a stage. Only before Start. \param holdMs the hold time of a TIME_HELD gate, else 0.
	//! \return the new stage's index, or -1 once the heist has started.
	int AddStage(LF_EHeistGate gate, int holdMs)
	{
		if (m_iState != LF_EHeistState.IDLE)
			return -1;

		m_aGates.Insert(gate);
		m_aHoldMs.Insert(holdMs);
		return m_aGates.Count() - 1;
	}

	//------------------------------------------------------------------------------------------------
	//! Start the clock. \param clockMs the whole job's time limit in ms; 0 or less means no limit.
	//! \return false with no stages or when the heist already started (a failed one is not restarted).
	bool Start(int nowMs, int clockMs)
	{
		if (m_iState != LF_EHeistState.IDLE)
			return false;

		if (m_aGates.IsEmpty())
		{
			LF_Debug.Warn("Heist", "Start refused: no stages");
			return false;
		}

		m_iState = LF_EHeistState.RUNNING;
		m_iStage = 0;
		m_iStartMs = nowMs;
		m_iClockMs = clockMs;
		m_iHoldSinceMs = -1;
		LF_Debug.Info("Heist", "started: " + m_aGates.Count() + " stages, clock " + clockMs + " ms");
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! A target was opened. Advances only when \p stage is the current stage and its gate is TARGET_OPENED.
	//! \return true when the heist advanced.
	bool TargetOpened(int stage, int nowMs)
	{
		if (!Accepts(stage, LF_EHeistGate.TARGET_OPENED, nowMs))
			return false;

		Advance(nowMs);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The crew is (or is not) in the stage's area. Advances on true for a CREW_IN_AREA stage.
	bool CrewInArea(int stage, bool inArea, int nowMs)
	{
		if (!Accepts(stage, LF_EHeistGate.CREW_IN_AREA, nowMs))
			return false;

		if (!inArea)
			return false;

		Advance(nowMs);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The crew holds (or has left) the stage's spot. A TIME_HELD stage completes once the hold has
	//! lasted its time; leaving resets it. \return true when the heist advanced.
	bool Hold(int stage, bool holding, int nowMs)
	{
		if (!Accepts(stage, LF_EHeistGate.TIME_HELD, nowMs))
			return false;

		if (!holding)
		{
			m_iHoldSinceMs = -1;
			return false;
		}

		if (m_iHoldSinceMs < 0)
			m_iHoldSinceMs = nowMs;

		return CheckHold(nowMs);
	}

	//------------------------------------------------------------------------------------------------
	//! Periodic check: the clock, the crew and a running hold. \param crewAlive crew members still alive.
	//! \return true when the heist advanced or finished with success on this tick.
	bool Tick(int nowMs, int crewAlive)
	{
		if (m_iState != LF_EHeistState.RUNNING)
			return false;

		if (ClockExpired(nowMs))
		{
			Fail(FAIL_CLOCK);
			return false;
		}

		if (crewAlive <= 0)
		{
			Fail(FAIL_CREW);
			return false;
		}

		if (m_aGates[m_iStage] == LF_EHeistGate.TIME_HELD && m_iHoldSinceMs >= 0)
			return CheckHold(nowMs);

		return false;
	}

	//------------------------------------------------------------------------------------------------
	int GetState()
	{
		return m_iState;
	}

	//------------------------------------------------------------------------------------------------
	//! Index of the current stage (the last one reached once the heist succeeded or failed).
	int GetStage()
	{
		return m_iStage;
	}

	//------------------------------------------------------------------------------------------------
	int GetStageCount()
	{
		return m_aGates.Count();
	}

	//------------------------------------------------------------------------------------------------
	bool IsFailed()
	{
		return m_iState == LF_EHeistState.FAILED;
	}

	//------------------------------------------------------------------------------------------------
	string GetFailReason()
	{
		return m_sFailReason;
	}

	//------------------------------------------------------------------------------------------------
	protected bool ClockExpired(int nowMs)
	{
		return m_iClockMs > 0 && (nowMs - m_iStartMs) >= m_iClockMs;
	}

	//------------------------------------------------------------------------------------------------
	//! Common gate for every signal: the heist runs, the clock has not run out, \p stage is the
	//! current one and has this gate. An expired clock fails the heist here, so a late signal cannot win.
	protected bool Accepts(int stage, LF_EHeistGate gate, int nowMs)
	{
		if (m_iState != LF_EHeistState.RUNNING)
			return false;

		if (ClockExpired(nowMs))
		{
			Fail(FAIL_CLOCK);
			return false;
		}

		if (stage != m_iStage)
			return false;

		return m_aGates[stage] == gate;
	}

	//------------------------------------------------------------------------------------------------
	protected bool CheckHold(int nowMs)
	{
		if (nowMs - m_iHoldSinceMs < m_aHoldMs[m_iStage])
			return false;

		Advance(nowMs);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void Advance(int nowMs)
	{
		m_iHoldSinceMs = -1;
		if (m_iStage + 1 >= m_aGates.Count())
		{
			m_iState = LF_EHeistState.SUCCEEDED;
			LF_Debug.Info("Heist", "succeeded after " + (nowMs - m_iStartMs) + " ms");
			return;
		}

		m_iStage++;
		LF_Debug.Info("Heist", "stage " + m_iStage + " reached");
	}

	//------------------------------------------------------------------------------------------------
	protected void Fail(string reason)
	{
		if (m_iState != LF_EHeistState.RUNNING)
			return;

		m_iState = LF_EHeistState.FAILED;
		m_sFailReason = reason;
		LF_Debug.Info("Heist", "failed: " + reason + " at stage " + m_iStage);
	}
}
