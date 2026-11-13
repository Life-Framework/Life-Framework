// tier: LOGIC
// red-proof: 2026-11-13, LF_AtmosphereMix.InRange without the stop margin: `--only LF_Test_AtmosphereMix` failed "a playing emitter keeps playing inside radius plus margin (expected '0', got '')". Restored, green.
// red-proof: 2026-11-13, Select looping while `outIds.Count() <= cap`: failed "over the cap the nearest emitters sound" and "a cap of zero silences everything". Restored, green.
//! Which ambient emitters sound (LF_AtmosphereMix, Feature/Atmosphere): the start radius and the
//! wider stop radius, conditions and hours, one voice per group, the voice cap by priority and
//! distance, and the hour window that wraps past midnight. Pure data in, ids out.
class LF_Test_AtmosphereMix : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "atmosphere/mix";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_AmbientCandidate Row(int id, float distance, float radius, bool playing = false, int priority = 0, string group = "", bool allowed = true)
	{
		LF_AmbientCandidate c = new LF_AmbientCandidate();
		c.m_iId = id;
		c.m_fDistance = distance;
		c.m_fRadius = radius;
		c.m_fMargin = 3;
		c.m_iPriority = priority;
		c.m_sGroup = group;
		c.m_bAllowed = allowed;
		c.m_bPlaying = playing;
		return c;
	}

	//------------------------------------------------------------------------------------------------
	protected string Ids(array<int> ids)
	{
		string text;
		foreach (int id : ids)
		{
			if (!text.IsEmpty())
				text += ",";

			text += id.ToString();
		}

		return text;
	}

	//------------------------------------------------------------------------------------------------
	protected string Pick(array<ref LF_AmbientCandidate> rows, int cap)
	{
		array<int> ids = {};
		LF_AtmosphereMix.Select(rows, cap, ids);
		return Ids(ids);
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// Start and stop distance.
		array<ref LF_AmbientCandidate> rows = {Row(0, 19.9, 20)};
		ctx.EqualStr("0", Pick(rows, 6), "a silent emitter starts inside its radius");

		rows = {Row(0, 20.1, 20)};
		ctx.EqualStr("", Pick(rows, 6), "a silent emitter does not start just outside its radius");

		rows = {Row(0, 22.9, 20, true)};
		ctx.EqualStr("0", Pick(rows, 6), "a playing emitter keeps playing inside radius plus margin");

		rows = {Row(0, 23.1, 20, true)};
		ctx.EqualStr("", Pick(rows, 6), "a playing emitter stops past radius plus margin");

		// Condition and hours: not allowed is silent at any distance.
		rows = {Row(0, 1, 20, true, 0, "", false)};
		ctx.EqualStr("", Pick(rows, 6), "an emitter whose condition is off stops although the listener stands on it");

		// One voice per group: the nearest; the incumbent holds against a rival that is barely nearer.
		rows = {Row(0, 12, 20, false, 0, "farm"), Row(1, 6, 20, false, 0, "farm"), Row(2, 9, 20, false, 0, "farm")};
		ctx.EqualStr("1", Pick(rows, 6), "of a group only the nearest emitter sounds");

		rows = {Row(0, 8, 20, true, 0, "farm"), Row(1, 6, 20, false, 0, "farm")};
		ctx.EqualStr("0", Pick(rows, 6), "the playing emitter of a group holds against one 2 m nearer");

		rows = {Row(0, 12, 20, true, 0, "farm"), Row(1, 6, 20, false, 0, "farm")};
		ctx.EqualStr("1", Pick(rows, 6), "the group's voice moves to an emitter 6 m nearer");

		rows = {Row(0, 4, 20, false, 0, "farm"), Row(1, 5, 20), Row(2, 6, 20, false, 0, "phones")};
		ctx.EqualStr("0,1,2", Pick(rows, 6), "different groups and ungrouped emitters each get a voice");

		// The cap: priority first, then distance.
		rows = {Row(0, 5, 30), Row(1, 10, 30), Row(2, 15, 30), Row(3, 20, 30)};
		ctx.EqualStr("0,1", Pick(rows, 2), "over the cap the nearest emitters sound");

		rows = {Row(0, 5, 30), Row(1, 10, 30), Row(2, 25, 30, false, 5)};
		ctx.EqualStr("2,0", Pick(rows, 2), "a higher priority takes a voice before a nearer emitter");

		rows = {Row(0, 5, 30), Row(1, 10, 30)};
		ctx.EqualStr("", Pick(rows, 0), "a cap of zero silences everything");

		rows = {Row(0, 5, 30, false, 0, "", false), Row(1, 10, 30), Row(2, 15, 30)};
		ctx.EqualStr("1,2", Pick(rows, 2), "an emitter that is off does not use up the cap");

		// Hours.
		ctx.True(LF_AtmosphereMix.InHours(3, 0, 0), "from equal to to is always");
		ctx.True(LF_AtmosphereMix.InHours(6, 6, 21), "a window includes its first hour");
		ctx.False(LF_AtmosphereMix.InHours(21, 6, 21), "a window ends at its last hour");
		ctx.False(LF_AtmosphereMix.InHours(5.9, 6, 21), "before the window is outside");
		ctx.True(LF_AtmosphereMix.InHours(23, 22, 5), "a window that wraps midnight includes the late evening");
		ctx.True(LF_AtmosphereMix.InHours(2, 22, 5), "and the small hours");
		ctx.False(LF_AtmosphereMix.InHours(12, 22, 5), "and not midday");

		// The still's condition reads the station's replicated percent.
		ctx.False(LF_AmbientConditionChainWorking.Working(LF_StationProgress.EMPTY), "an empty still is silent");
		ctx.True(LF_AmbientConditionChainWorking.Working(0), "a loaded still bubbles");
		ctx.True(LF_AmbientConditionChainWorking.Working(99), "it bubbles until the batch is ready");
		ctx.False(LF_AmbientConditionChainWorking.Working(LF_StationProgress.READY), "a finished batch is silent");
	}
}
