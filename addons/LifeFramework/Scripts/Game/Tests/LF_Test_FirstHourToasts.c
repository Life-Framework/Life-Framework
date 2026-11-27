// red-proof: Observed 2026-11-27 with `tools\cli test --no-build --red LF_Test_FirstHourToasts`: RED-OK quests/first-hour-toasts, `a row that names a turn-in title shows it on the closing card (expected 'next-title', got 'step-title')`; green without the hook.
//! quests/first-hour-toasts (playtest 2026-11-27, findings 4 and 5): a chain step's closing card carries the title
//! its row names (the last arrival step looks ahead to "find steady work" instead of keeping "Sell your juice"), and
//! the two cards the garage shows for a car coming out share one slot, so the player sees one car card, not two.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_FirstHourToasts : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "quests/first-hour-toasts";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_QuestDefinition plain = new LF_QuestDefinition();
		plain.m_sTitleKey = "step-title";
		ctx.EqualStr("step-title", plain.TurnInTitle(), "a row with no turn-in title keeps the step's own title");

		LF_QuestDefinition ahead = new LF_QuestDefinition();
		ahead.m_sTitleKey = "step-title";
		if (!RedProof())
			ahead.m_sTurnInTitleKey = "next-title";

		ctx.EqualStr("next-title", ahead.TurnInTitle(), "a row that names a turn-in title shows it on the closing card");

		ctx.False(LF_NotificationManagerComponent.SLOT_VEHICLE.IsEmpty(), "the garage cards have a slot");
		ctx.True(LF_NotificationManagerComponent.SLOT_VEHICLE != LF_NotificationManagerComponent.SLOT_STEP, "the car card does not replace the step card");
	}
}
