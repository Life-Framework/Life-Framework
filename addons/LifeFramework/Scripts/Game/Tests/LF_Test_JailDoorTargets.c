// red-proof: with `RedProof()` on the door targets are never set, so "the cell door is remembered", "matches only its
// own id" and "the outer door is its own target" fail.

//! police/jail-door-targets: the breach target ids the jailbreak reads (LF_Jail.SetDoorTarget, SetOuterDoorTarget,
//! DoorTargetId, OuterDoorTargetId, IsDoorTarget, IsOuterDoorTarget). An empty cell door id is refused and keeps the
//! old one, an id matches only itself, the empty id matches nothing (an unset door must never be breachable by an
//! unnamed target), the cell and the outer door are separate targets, and an empty outer id clears the outer door.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_JailDoorTargets : LF_Test
{
	protected static const string CELL = "test-jail-cell-door";
	protected static const string OUTER = "test-jail-outer-door";

	override string GetName()
	{
		return "police/jail-door-targets";
	}

	override void Run(LF_TestContext ctx)
	{
		string cellBefore = "?";
		string cellAfterEmpty = "?";
		bool cellMatch = false;
		bool otherMatch = true;
		bool emptyMatches = true;
		bool cellIsOuter = true;
		string outerSet = "?";
		bool outerMatch = false;
		bool outerIsCell = true;
		bool emptyOuterMatches = true;
		bool outerCleared = false;
		bool clearedStillMatches = true;
		if (!RedProof())
		{
			LF_Jail.SetOuterDoorTarget("");
			LF_Jail.SetDoorTarget(CELL);
			cellBefore = LF_Jail.DoorTargetId();
			LF_Jail.SetDoorTarget("");
			cellAfterEmpty = LF_Jail.DoorTargetId();
			cellMatch = LF_Jail.IsDoorTarget(CELL);
			otherMatch = LF_Jail.IsDoorTarget("some-other-door");
			emptyMatches = LF_Jail.IsDoorTarget("");
			cellIsOuter = LF_Jail.IsOuterDoorTarget(CELL);

			LF_Jail.SetOuterDoorTarget(OUTER);
			outerSet = LF_Jail.OuterDoorTargetId();
			outerMatch = LF_Jail.IsOuterDoorTarget(OUTER);
			outerIsCell = LF_Jail.IsDoorTarget(OUTER);
			emptyOuterMatches = LF_Jail.IsOuterDoorTarget("");

			LF_Jail.SetOuterDoorTarget("");
			outerCleared = LF_Jail.OuterDoorTargetId().IsEmpty();
			clearedStillMatches = LF_Jail.IsOuterDoorTarget(OUTER);
		}

		ctx.EqualStr(CELL, cellBefore, "the cell door is remembered");
		ctx.EqualStr(CELL, cellAfterEmpty, "an empty cell door id is refused and keeps the old one");
		ctx.True(cellMatch, "the cell door matches its own id");
		ctx.False(otherMatch, "the cell door matches only its own id");
		ctx.False(emptyMatches, "the empty id matches no door");
		ctx.False(cellIsOuter, "the cell door is not the outer door");
		ctx.EqualStr(OUTER, outerSet, "the outer door is remembered");
		ctx.True(outerMatch, "the outer door is its own target");
		ctx.False(outerIsCell, "the outer door is not the cell door");
		ctx.False(emptyOuterMatches, "the empty id matches no outer door");
		ctx.True(outerCleared, "an empty outer id clears the outer door");
		ctx.False(clearedStillMatches, "a cleared outer door no longer matches its old id");

		// Back to the placed cell's own ids, as the other jail tests leave it.
		LF_Jail.SyncPlacedCell();
	}
}
