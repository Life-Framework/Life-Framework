// red-proof: Observed 2026-11-16 with `tools/cli test --no-build --red LF_Test_FirstSpawn,LF_Test_FirstSpawnPersist` (RED PROOF OK): with the Consume call skipped, failed "the new character's first spawn is first"; green on the real code.
// tier: LOGIC
//! #1964: a character the creation flow marked spawns as new exactly once; the second spawn
//! (after a death or a rejoin) is not; a character that was never marked (an existing one) never is.
class LF_Test_FirstSpawn : LF_Test
{
	protected static const string NEW_ID = "firstspawn-new-character";
	protected static const string OLD_ID = "firstspawn-existing-character";
	protected static const string OTHER_NEW_ID = "firstspawn-second-new-character";

	override string GetName()
	{
		return "character/first-spawn";
	}

	override void Run(LF_TestContext ctx)
	{
		array<ref LF_FirstSpawnRow> before = LF_FirstSpawn.ExportAll();
		LF_FirstSpawn.ApplyAll(null);

		ctx.False(LF_FirstSpawn.IsPending(NEW_ID), "no character is new before the creation flow marks one");
		ctx.True(LF_FirstSpawn.Mark(NEW_ID), "the creation flow marks the new character");
		ctx.True(LF_FirstSpawn.Mark(NEW_ID), "marking twice is accepted");
		ctx.Equal(1, LF_FirstSpawn.Count(), "marking twice is still one mark");
		ctx.True(LF_FirstSpawn.IsPending(NEW_ID), "the mark waits for the first spawn");

		bool first;
		if (!RedProof())
			first = LF_FirstSpawn.Consume(NEW_ID);
		ctx.True(first, "the new character's first spawn is first");
		ctx.False(LF_FirstSpawn.IsPending(NEW_ID), "the first spawn cleared the mark");
		ctx.False(LF_FirstSpawn.Consume(NEW_ID), "the second spawn is not first");
		ctx.False(LF_FirstSpawn.Consume(NEW_ID), "no later spawn is first either");

		ctx.False(LF_FirstSpawn.Consume(OLD_ID), "an existing character, never marked, is never first");

		// One character's spawn leaves another's mark alone.
		LF_FirstSpawn.Mark(OTHER_NEW_ID);
		ctx.False(LF_FirstSpawn.Consume(OLD_ID), "another character spawning does not clear a mark");
		ctx.True(LF_FirstSpawn.IsPending(OTHER_NEW_ID), "the second new character still waits for its first spawn");
		ctx.True(LF_FirstSpawn.Consume(OTHER_NEW_ID), "and gets it");

		// A broken input is refused, not crashed on.
		ctx.False(LF_FirstSpawn.Mark(""), "a character with no id cannot be marked");
		ctx.False(LF_FirstSpawn.Consume(""), "an empty id is never first");
		ctx.Equal(0, LF_FirstSpawn.Count(), "nothing is left waiting");

		LF_FirstSpawn.ApplyAll(before);
	}
}
