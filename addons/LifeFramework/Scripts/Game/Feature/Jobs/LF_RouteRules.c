//------------------------------------------------------------------------------------------------
//! Route job rules (#3324): a route is an ordered list of stops. Pure logic, no world and no statics:
//! the caller (a route job, later the bus driver job) owns one instance per run and reports each stop
//! reached by its index. The rules say which stop is next, refuse a stop out of order or twice, count
//! the stops done, and give the pay: stops x rate + a round bonus at the end (int math), or half the
//! rate per done stop when the run is abandoned.
class LF_RouteRules
{
	protected int m_iStopCount;
	protected int m_iRatePerStop;
	protected int m_iRoundBonus;
	protected int m_iDone;

	//------------------------------------------------------------------------------------------------
	//! \param[in] stopCount How many stops the route has; one or more.
	//! \param[in] ratePerStop Cash paid for each stop done; zero or more.
	//! \param[in] roundBonus Cash added when every stop is done; zero or more.
	//! \return The rules for a fresh run, or null (logged) when the config is bad: no stops, or a
	//! negative rate or bonus.
	static LF_RouteRules Create(int stopCount, int ratePerStop, int roundBonus)
	{
		if (stopCount <= 0)
		{
			LF_Debug.Error("Jobs", string.Format("route has %1 stops: a route needs at least one", stopCount));
			return null;
		}

		if (ratePerStop < 0 || roundBonus < 0)
		{
			LF_Debug.Error("Jobs", string.Format("route pay is negative (rate %1, bonus %2)", ratePerStop, roundBonus));
			return null;
		}

		LF_RouteRules rules = new LF_RouteRules();
		rules.m_iStopCount = stopCount;
		rules.m_iRatePerStop = ratePerStop;
		rules.m_iRoundBonus = roundBonus;
		return rules;
	}

	//------------------------------------------------------------------------------------------------
	int StopCount()
	{
		return m_iStopCount;
	}

	//------------------------------------------------------------------------------------------------
	int StopsDone()
	{
		return m_iDone;
	}

	//------------------------------------------------------------------------------------------------
	bool IsComplete()
	{
		return m_iDone >= m_iStopCount;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The index of the stop to reach next, or -1 once every stop is done.
	int NextStop()
	{
		if (IsComplete())
			return -1;

		return m_iDone;
	}

	//------------------------------------------------------------------------------------------------
	//! Records that stop `index` was reached. \return False (logged) when it is not the next stop: out
	//! of order, already done, out of range, or after the route ended. A refusal changes nothing.
	bool CompleteStop(int index)
	{
		if (index != NextStop())
		{
			LF_Debug.Warn("Jobs", string.Format("route stop %1 refused: next is %2 (%3 of %4 done)", index, NextStop(), m_iDone, m_iStopCount));
			return false;
		}

		m_iDone++;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The pay for finishing the route: stops x rate + the round bonus. 0 (logged) while a stop
	//! is still to do; use AbandonPay for a run that stops early.
	int PayAtEnd()
	{
		if (!IsComplete())
		{
			LF_Debug.Warn("Jobs", string.Format("route end pay asked at %1 of %2 stops: nothing paid", m_iDone, m_iStopCount));
			return 0;
		}

		return m_iStopCount * m_iRatePerStop + m_iRoundBonus;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The pay for abandoning the route: half the rate for each stop done, in whole cash
	//! (rounded down), and no round bonus.
	int AbandonPay()
	{
		return (m_iDone * m_iRatePerStop) / 2;
	}
}
