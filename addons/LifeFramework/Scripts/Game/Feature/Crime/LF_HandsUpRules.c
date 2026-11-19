//------------------------------------------------------------------------------------------------
//! Why a hands-up ended. NONE while it still holds.
enum LF_HandsUpEndReason
{
	NONE,
	MOVED,
	ARMED,
	TIMEOUT,
	LOWERED
}

//------------------------------------------------------------------------------------------------
//! The hands-up state of one target as pure rules (#3394, #782): no world, no clock, no UI.
//! The target chooses: it starts only on the target's own accepted answer to the prompt, never on
//! another player's request, and it is never a forced pose. It ends when the target moves past a
//! small radius from where they raised their hands, draws a weapon, lowers them, or the timeout
//! runs out. The caller feeds in the position, whether a weapon is drawn, and the clock in ms.
//! LOGIC-tested (LF_Test_HandsUpRules).
class LF_HandsUpRules
{
	//! Metres the target may drift from the spot where they raised their hands (horizontal).
	static const float MOVE_RADIUS_M = 1.5; // lf-fixed: rule default; the prompt card (#782) owns the config row and passes its own value
	//! Seconds the hands stay up with no further input.
	static const int TIMEOUT_SECONDS = 30; // lf-fixed: rule default; the prompt card (#782) owns the config row and passes its own value

	protected bool m_bActive;
	protected vector m_vOrigin;
	protected int m_iStartMs;
	protected int m_iTargetId;
	protected LF_HandsUpEndReason m_eEndReason;

	//------------------------------------------------------------------------------------------------
	//! \return True while the hands are up.
	bool IsActive()
	{
		return m_bActive;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Why the last hands-up ended, or NONE while it holds or before any began.
	LF_HandsUpEndReason GetEndReason()
	{
		return m_eEndReason;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The player id the hands-up belongs to; -1 when none.
	int GetTargetId()
	{
		if (!m_bActive)
			return -1;

		return m_iTargetId;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when an answer may start hands-up: the answer comes from the target themselves
	//! and accepts. Another player's request or answer, a refusal, and an invalid id never do.
	static bool AnswerStarts(int targetId, int answeringId, bool accepted)
	{
		if (targetId < 0 || answeringId < 0)
			return false;

		return accepted && targetId == answeringId;
	}

	//------------------------------------------------------------------------------------------------
	//! Starts hands-up for \p targetId at \p origin when \p answeringId is the target and accepted.
	//! \return True when it started. A refused or foreign answer changes nothing; an already raised
	//! pair of hands is left as it is (no timer reset by repeating the answer).
	bool Answer(int targetId, int answeringId, bool accepted, vector origin, int nowMs)
	{
		if (!AnswerStarts(targetId, answeringId, accepted))
			return false;

		if (m_bActive)
			return false;

		m_bActive = true;
		m_vOrigin = origin;
		m_iStartMs = nowMs;
		m_iTargetId = targetId;
		m_eEndReason = LF_HandsUpEndReason.NONE;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return What would end the hands-up at \p pos, \p armed and \p nowMs, without changing state.
	//! A drawn weapon wins over moving, which wins over the timeout.
	LF_HandsUpEndReason Check(vector pos, bool armed, int nowMs)
	{
		if (!m_bActive)
			return LF_HandsUpEndReason.NONE;

		if (armed)
			return LF_HandsUpEndReason.ARMED;

		float dx = pos[0] - m_vOrigin[0]; // lf-float: metres of a short walk, compared to the radius
		float dz = pos[2] - m_vOrigin[2]; // lf-float: metres of a short walk, compared to the radius
		if (dx * dx + dz * dz > MOVE_RADIUS_M * MOVE_RADIUS_M)
			return LF_HandsUpEndReason.MOVED;

		float elapsedMs = nowMs - m_iStartMs; // lf-float: a short span in ms, compared to the timeout
		if (elapsedMs >= TIMEOUT_SECONDS * 1000)
			return LF_HandsUpEndReason.TIMEOUT;

		return LF_HandsUpEndReason.NONE;
	}

	//------------------------------------------------------------------------------------------------
	//! Ticks the state. \return The reason it just ended, or NONE when it still holds (or was not up).
	LF_HandsUpEndReason Tick(vector pos, bool armed, int nowMs)
	{
		LF_HandsUpEndReason reason = Check(pos, armed, nowMs);
		if (reason != LF_HandsUpEndReason.NONE)
			End(reason);

		return reason;
	}

	//------------------------------------------------------------------------------------------------
	//! The target lowers their hands by choice. \return True when they were up.
	bool Lower(int answeringId)
	{
		if (!m_bActive || answeringId != m_iTargetId)
			return false;

		End(LF_HandsUpEndReason.LOWERED);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void End(LF_HandsUpEndReason reason)
	{
		m_bActive = false;
		m_eEndReason = reason;
	}
}
