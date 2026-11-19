// red-proof: RedProof() skips the turn-in marks, so the "first handed in, the rest not" answers fail; batched run `tools\cli test --no-build --red LF_Test_QuestTurnedInOnly`.
//! The dialogue condition QuestTurnedInOnly (#3809): a comma list whose first quest is handed in and whose
//! others are not. It is what keeps a go-between from offering the sell intro to a player who has already
//! reported or sold what was in the crate (one reply takes one condition, so QuestTurnedIn alone cannot).
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_QuestTurnedInOnly : LF_Test
{
	protected static const string FIRST = "turned_in_only_first";
	protected static const string SECOND = "turned_in_only_second";
	protected static const string THIRD = "turned_in_only_third";
	protected static const string LIST = "turned_in_only_first,turned_in_only_second, turned_in_only_third";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "quests/turned-in-only";
	}

	//------------------------------------------------------------------------------------------------
	protected static bool Answer(LF_QuestTrackerComponent tracker, string param)
	{
		return LF_DialogueHook_QuestTurnedInOnly.Evaluate(tracker, param);
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		ctx.True(LF_DialogueHook.Find("QuestTurnedInOnly") != null, "the QuestTurnedInOnly condition has a class");

		IEntity player = LF_TestCharacterFixture.Spawn(ctx);
		if (!player)
			return;

		LF_QuestTrackerComponent tracker = LF_Component<LF_QuestTrackerComponent>.Find(player);
		ctx.NotNull(tracker, "the test character carries the quest tracker");
		if (!tracker)
		{
			LF_TestCharacterFixture.Delete(player);
			return;
		}

		ctx.False(Answer(tracker, LIST), "nothing handed in: the list is not met");
		ctx.False(Answer(tracker, ""), "an empty list is never met");

		// The first quest handed in, the others not: met.
		array<string> marks = {};
		if (!RedProof())
			marks.Insert(FIRST);

		tracker.RestoreTurnedIn(marks);
		ctx.True(Answer(tracker, LIST), "the first handed in and the others not: the list is met");
		ctx.True(Answer(tracker, FIRST), "a one-quest list is the same as QuestTurnedIn");

		// A later quest handed in: no longer met (the player reported or sold what the crate held).
		marks.Clear();
		marks.Insert(SECOND);
		tracker.RestoreTurnedIn(marks);
		ctx.False(Answer(tracker, LIST), "a later quest of the list handed in: the list is not met");

		// The first not handed in but a later one is: not met either.
		ctx.False(Answer(tracker, "turned_in_only_never," + SECOND), "the first not handed in: the list is not met");

		LF_TestCharacterFixture.Delete(player);
	}
}
