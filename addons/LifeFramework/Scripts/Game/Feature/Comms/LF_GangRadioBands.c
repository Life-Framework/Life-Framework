//------------------------------------------------------------------------------------------------
//! One gang's band (GA-C7, #4215): a frequency in kHz and an encryption key. The key is drawn fresh
//! on every assignment, so a rival who dials the same frequency by hand hears nothing, and a band
//! given to a new gang after a release does not carry the old gang's radios with it.
class LF_GangRadioBand
{
	string m_sOrgId;
	int m_iFrequency;
	string m_sKey;
}

//------------------------------------------------------------------------------------------------
//! Pure band allocator: the channels from min to max in steps, less the reserved ones. A gang gets
//! a random free channel (a rival cannot guess it from founding order) and keeps it until Release.
//! Two gangs never hold one channel; a channel goes back to the free set only on Release.
//! Server state, in memory: after a restart every gang is assigned again on first use.
class LF_GangRadioBands
{
	protected int m_iMin;
	protected int m_iMax;
	protected int m_iStep;
	protected ref array<int> m_aReserved = {};
	protected ref map<string, ref LF_GangRadioBand> m_mByOrg = new map<string, ref LF_GangRadioBand>();
	protected ref map<int, string> m_mByFrequency = new map<int, string>();

	//------------------------------------------------------------------------------------------------
	void LF_GangRadioBands(int minKHz, int maxKHz, int stepKHz, array<int> reserved = null)
	{
		m_iMin = minKHz;
		m_iMax = maxKHz;
		m_iStep = LF_Utils.MaxInt(stepKHz, 1);
		if (reserved)
			m_aReserved.Copy(reserved);
	}

	//------------------------------------------------------------------------------------------------
	//! Every channel a gang may be given, held or free.
	void Channels(notnull array<int> outChannels)
	{
		outChannels.Clear();
		if (m_iMin <= 0 || m_iMax < m_iMin)
			return;

		for (int frequency = m_iMin; frequency <= m_iMax; frequency += m_iStep)
		{
			if (m_aReserved.Find(frequency) < 0)
				outChannels.Insert(frequency);
		}
	}

	//------------------------------------------------------------------------------------------------
	int FreeCount()
	{
		array<int> channels = {};
		Channels(channels);
		return channels.Count() - m_mByFrequency.Count();
	}

	//------------------------------------------------------------------------------------------------
	LF_GangRadioBand Find(string orgId)
	{
		return m_mByOrg.Get(orgId);
	}

	//------------------------------------------------------------------------------------------------
	//! The gang holding a frequency, "" when it is free.
	string HolderOf(int frequency)
	{
		return m_mByFrequency.Get(frequency);
	}

	//------------------------------------------------------------------------------------------------
	//! The gang's band, assigning a random free one on first use.
	//! \return null (and an Error) for an empty id or when every channel is held.
	LF_GangRadioBand Assign(string orgId)
	{
		if (orgId.IsEmpty())
		{
			LF_Debug.Error(LF_GangRadio.FEATURE, "band refused: no organization id");
			return null;
		}

		LF_GangRadioBand held = m_mByOrg.Get(orgId);
		if (held)
			return held;

		array<int> channels = {};
		Channels(channels);
		array<int> free = {};
		foreach (int frequency : channels)
		{
			if (!m_mByFrequency.Contains(frequency))
				free.Insert(frequency);
		}

		if (free.IsEmpty())
		{
			LF_Debug.Error(LF_GangRadio.FEATURE, string.Format("band refused for org=%1: all %2 gang channels are held", orgId, channels.Count()));
			return null;
		}

		LF_GangRadioBand band = new LF_GangRadioBand();
		band.m_sOrgId = orgId;
		band.m_iFrequency = free.Get(Math.RandomInt(0, free.Count()));
		band.m_sKey = string.Format("LF_gang_%1_%2", Math.RandomInt(100000, 999999), Math.RandomInt(100000, 999999));
		m_mByOrg.Set(orgId, band);
		m_mByFrequency.Set(band.m_iFrequency, orgId);
		LF_Debug.Info(LF_GangRadio.FEATURE, string.Format("band assigned org=%1 frequency=%2 free=%3", orgId, band.m_iFrequency, free.Count() - 1));
		return band;
	}

	//------------------------------------------------------------------------------------------------
	//! Frees the gang's band. \return False when the gang held none.
	bool Release(string orgId, string reason)
	{
		LF_GangRadioBand band = m_mByOrg.Get(orgId);
		if (!band)
			return false;

		m_mByFrequency.Remove(band.m_iFrequency);
		m_mByOrg.Remove(orgId);
		LF_Debug.Info(LF_GangRadio.FEATURE, string.Format("band released org=%1 frequency=%2 (%3)", orgId, band.m_iFrequency, reason));
		return true;
	}
}
