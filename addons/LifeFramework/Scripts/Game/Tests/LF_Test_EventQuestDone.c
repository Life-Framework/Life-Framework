// red-proof: with `tools\cli test --no-build --red LF_Test_EventQuestDone` the before-the-delay check is skipped and the test fails (`red proof: before-delay check skipped`).
//! The QUEST_DONE event condition (#3784): it holds nothing until a player turned the quest in, not
//! before its delay has passed, and stops after its window; a missing quest id or measurement holds
//! nothing; the log keeps the latest turn-in. No world.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_EventQuestDone : LF_Test
{
	override string GetName()
	{
		return "events/quest-done-condition";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_QuestDoneLog.ResetStatics();

		LF_EventPrecondition tip = LF_EventPrecondition.MakeQuestDone("shipment_report", 120, 600);
		LF_EventContext none = LF_EventContext.Make(12, 1, 0, 1000);
		ctx.False(tip.Holds(none), "no turn-in yet holds nothing");

		// The turn-in at 1000 s: closed for 120 s, open for the next 600 s, then closed again.
		LF_EventContext cx = LF_EventContext.Make(12, 1, 0, 1000);
		cx.NoteQuestDone("shipment_report", 1000);
		if (!RedProof())
			ctx.False(tip.Holds(cx), "the turn-in second itself is before the delay");
		else
			ctx.False(true, "red proof: before-delay check skipped");
		cx.m_fNow = 1119;
		ctx.False(tip.Holds(cx), "one second short of the delay holds nothing");
		cx.m_fNow = 1120;
		ctx.True(tip.Holds(cx), "the delay has passed");
		cx.m_fNow = 1719;
		ctx.True(tip.Holds(cx), "still inside the window");
		cx.m_fNow = 1720;
		ctx.False(tip.Holds(cx), "the window has closed");

		// No window: holds until the restart. No delay: holds at once.
		LF_EventPrecondition open = LF_EventPrecondition.MakeQuestDone("shipment_report", 0, 0);
		cx.m_fNow = 1000;
		ctx.True(open.Holds(cx), "no delay holds at the turn-in");
		cx.m_fNow = 999999;
		ctx.True(open.Holds(cx), "no window never closes");

		// Another quest id, an empty id and a missing measurement hold nothing.
		LF_EventPrecondition other = LF_EventPrecondition.MakeQuestDone("other_quest", 0, 0);
		ctx.False(other.Holds(cx), "a different quest's turn-in does not count");
		LF_EventPrecondition blank = LF_EventPrecondition.MakeQuestDone("", 0, 0);
		ctx.False(blank.Holds(cx), "a condition with no quest id holds nothing");
		ctx.False(tip.Holds(null), "no measurements hold nothing");

		// The log feeds a measurement and the latest turn-in wins.
		LF_QuestDoneLog.Note("shipment_report", 500);
		LF_QuestDoneLog.Note("shipment_report", 900);
		LF_QuestDoneLog.Note("", 5);
		ctx.EqualFloat(900, LF_QuestDoneLog.DoneAt("shipment_report"), 0.01, "the latest turn-in is kept");
		ctx.EqualFloat(-1, LF_QuestDoneLog.DoneAt("other_quest"), 0.01, "an unseen quest is -1");
		ctx.EqualFloat(-1, LF_QuestDoneLog.DoneAt(""), 0.01, "an empty id is never noted");
		LF_EventContext filled = LF_EventContext.Make(12, 1, 0, 1000);
		LF_QuestDoneLog.Fill(filled);
		ctx.EqualFloat(900, filled.QuestDoneAt("shipment_report"), 0.01, "a measurement carries the logged turn-in");
		ctx.False(tip.Holds(filled), "100 s after the turn-in is short of a 120 s delay");

		LF_QuestDoneLog.ResetStatics();
		ctx.EqualFloat(-1, LF_QuestDoneLog.DoneAt("shipment_report"), 0.01, "a world reset forgets the tip-off");
	}
}
