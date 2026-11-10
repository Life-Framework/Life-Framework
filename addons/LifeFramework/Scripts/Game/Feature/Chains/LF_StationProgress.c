//------------------------------------------------------------------------------------------------
//! The progress of one station (one barrel, one still, one press). Pure: no entities, no time.
//! Progress belongs to the station, not to a worker: every accepted piece of hands-on work adds
//! to the same number, so two workers on one barrel finish it in half the time of one.
class LF_StationProgress
{
	//! Percent while empty.
	static const int EMPTY = -1;
	//! Percent when the batch is finished and waits to be collected.
	static const int READY = 100;

	protected bool m_bLoaded;
	protected int m_iWork;
	protected int m_iNeeded;

	//------------------------------------------------------------------------------------------------
	bool IsLoaded()
	{
		return m_bLoaded;
	}

	//------------------------------------------------------------------------------------------------
	bool IsDone()
	{
		return m_bLoaded && m_iWork >= m_iNeeded;
	}

	//------------------------------------------------------------------------------------------------
	int GetWork()
	{
		return m_iWork;
	}

	//------------------------------------------------------------------------------------------------
	int GetNeeded()
	{
		return m_iNeeded;
	}

	//------------------------------------------------------------------------------------------------
	//! Starts a batch needing `needed` units of work. Refused while a batch is loaded.
	bool Load(int needed)
	{
		if (m_bLoaded || needed < 1)
			return false;

		m_bLoaded = true;
		m_iWork = 0;
		m_iNeeded = needed;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Adds units of work. \return True when this call finished the batch.
	bool AddWork(int units)
	{
		if (!m_bLoaded || IsDone() || units < 1)
			return false;

		m_iWork = Math.Min(m_iWork + units, m_iNeeded);
		return IsDone();
	}

	//------------------------------------------------------------------------------------------------
	//! Empties the station, taking the finished batch out.
	void Clear()
	{
		m_bLoaded = false;
		m_iWork = 0;
		m_iNeeded = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Persistence seam: puts a saved batch back. Work is clamped to 0..needed; an unloaded or
	//! unsized batch restores as an empty station.
	void Restore(bool loaded, int work, int needed)
	{
		if (!loaded || needed < 1)
		{
			Clear();
			return;
		}

		m_bLoaded = true;
		m_iNeeded = needed;
		m_iWork = Math.Clamp(work, 0, needed);
	}

	//------------------------------------------------------------------------------------------------
	//! -1 empty, 0 to 99 being worked, 100 ready. This is the number that replicates; a client
	//! shows the state of the barrel in words from it.
	int Percent()
	{
		if (!m_bLoaded)
			return EMPTY;

		if (IsDone())
			return READY;

		return Math.Clamp(m_iWork * 100 / m_iNeeded, 0, READY - 1);
	}

	//------------------------------------------------------------------------------------------------
	//! Rounds of work (one unit per worker per round) a batch of `needed` takes with `workers`
	//! hands on it. One worker takes `needed` rounds; N workers take about 1/N of that.
	static int Rounds(int needed, int workers)
	{
		if (needed < 1 || workers < 1)
			return 0;

		return (needed + workers - 1) / workers;
	}
}
