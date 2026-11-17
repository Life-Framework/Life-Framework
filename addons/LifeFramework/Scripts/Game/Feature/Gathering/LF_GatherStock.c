//------------------------------------------------------------------------------------------------
//! The stock of one gather spot (#2409): it starts full, each gather takes one, and one comes
//! back every `regrowMs` while the spot is below its cap. Pure arithmetic on a caller-supplied
//! clock (ms), so a test drives time without a world. The spot owns the instance on the server;
//! nothing here replicates or saves, a restart refills every spot (the mining rocks do the same).
class LF_GatherStock : Managed
{
	protected int m_iMax;
	protected float m_fRegrowMs;
	protected int m_iStock;
	protected float m_fAnchorMs;

	//------------------------------------------------------------------------------------------------
	//! \param max Gathers a full spot holds. Below 1 makes a spot that is always empty.
	//! \param regrowMs Milliseconds for one gather to grow back. 0 or less never regrows.
	//! \param nowMs The caller's clock when the spot is first touched.
	static LF_GatherStock Create(int max, float regrowMs, float nowMs)
	{
		LF_GatherStock stock = new LF_GatherStock();
		stock.m_iMax = max;
		stock.m_fRegrowMs = regrowMs;
		stock.m_iStock = max;
		stock.m_fAnchorMs = nowMs;
		return stock;
	}

	//------------------------------------------------------------------------------------------------
	//! Gathers left right now, after regrowth.
	int Available(float nowMs)
	{
		Regrow(nowMs);
		return m_iStock;
	}

	//------------------------------------------------------------------------------------------------
	//! Takes one gather. False (and nothing taken) when the spot is empty.
	bool Take(float nowMs)
	{
		Regrow(nowMs);
		if (m_iStock <= 0)
			return false;

		// A full spot has no regrowth running: the clock starts with this take.
		if (m_iStock >= m_iMax)
			m_fAnchorMs = nowMs;

		m_iStock--;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Puts a taken gather back (the grant that followed it failed). Never past the cap.
	void GiveBack()
	{
		if (m_iStock < m_iMax)
			m_iStock++;
	}

	//------------------------------------------------------------------------------------------------
	//! Milliseconds until the next gather grows back. 0 when the spot is full or never regrows.
	float MsUntilNext(float nowMs)
	{
		Regrow(nowMs);
		if (m_iStock >= m_iMax || m_fRegrowMs <= 0)
			return 0;

		float left = m_fAnchorMs + m_fRegrowMs - nowMs;
		if (left < 0)
			return 0;

		return left;
	}

	//------------------------------------------------------------------------------------------------
	protected void Regrow(float nowMs)
	{
		if (m_iStock >= m_iMax || m_fRegrowMs <= 0)
			return;

		int grown = Math.Floor((nowMs - m_fAnchorMs) / m_fRegrowMs);
		if (grown <= 0)
			return;

		m_iStock += grown;
		if (m_iStock >= m_iMax)
		{
			m_iStock = m_iMax;
			m_fAnchorMs = nowMs;
			return;
		}

		m_fAnchorMs += grown * m_fRegrowMs;
	}
}
