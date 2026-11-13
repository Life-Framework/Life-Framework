// tier: LOGIC
// red-proof: 2026-11-13, HandsetUp as `state != IDLE`: `--only LF_Test_AmbientTown` failed "a ringing phone has its receiver down". Restored, green.
//! The conditions behind the second batch of town sounds (#897): the payphone hum only while the
//! receiver is up, and the pub's outdoor radio taking the music from the building while it is on.
class LF_Test_AmbientTown : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "atmosphere/town";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// A payphone hums with the receiver up (BUSY), not while idle or ringing.
		ctx.True(LF_AmbientConditionPhoneLine.HandsetUp(LF_EPhoneLineState.BUSY), "a phone in a call has its receiver up");
		ctx.False(LF_AmbientConditionPhoneLine.HandsetUp(LF_EPhoneLineState.IDLE), "an idle phone is silent");
		ctx.False(LF_AmbientConditionPhoneLine.HandsetUp(LF_EPhoneLineState.RINGING), "a ringing phone has its receiver down");

		// Pub: the radio (priority 3) and the building (priority 2) share a group; one voice.
		array<ref LF_AmbientCandidate> rows = {};
		rows.Insert(Row(0, 10, 33, 2, "pub_music", true));
		rows.Insert(Row(1, 25, 33, 3, "pub_music", true));
		array<int> ids = {};
		LF_AtmosphereMix.Select(rows, 6, ids);
		ctx.Equal(1, ids.Count(), "the pub's radio and building make one voice");
		if (ids.Count() == 1)
			ctx.Equal(1, ids[0], "the radio wins even though the building is nearer");

		rows[1].m_bAllowed = false;
		LF_AtmosphereMix.Select(rows, 6, ids);
		ctx.Equal(1, ids.Count(), "a radio that is switched off leaves the building's music");
		if (ids.Count() == 1)
			ctx.Equal(0, ids[0], "the building plays when the radio is off");
	}

	//------------------------------------------------------------------------------------------------
	protected LF_AmbientCandidate Row(int id, float distance, float radius, int priority, string group, bool allowed)
	{
		LF_AmbientCandidate c = new LF_AmbientCandidate();
		c.m_iId = id;
		c.m_fDistance = distance;
		c.m_fRadius = radius;
		c.m_fMargin = 3;
		c.m_iPriority = priority;
		c.m_sGroup = group;
		c.m_bAllowed = allowed;
		return c;
	}
}
