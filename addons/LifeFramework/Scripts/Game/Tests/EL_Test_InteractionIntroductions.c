// red-proof: Observed 2026-11-08 with `tools\cli test --no-build --only` (all four broken in one run, 0/4 passed), then restored; green. Dropping the duplicate check in LF_Introductions.Introduce failed
// `[ELTEST] FAIL interaction/introductions`, `- one pair after two identical introductions (expected 1, got 2)`.
// tier: LOGIC
class EL_Test_InteractionIntroductions : EL_Test
{
	override string GetName()
	{
		return "interaction/introductions";
	}

	override void Run(EL_TestContext ctx)
	{
		array<ref LF_IntroductionRecord> before = LF_Introductions.ExportAll();
		LF_Introductions.ResetStatics();

		// Memory is one way: Bea knows Ada after Ada introduces herself, not the other way round.
		ctx.True(LF_Introductions.Introduce("bea", "ada"), "Ada introduces herself to Bea");
		ctx.True(LF_Introductions.Knows("bea", "ada"), "Bea knows Ada");
		ctx.False(LF_Introductions.Knows("ada", "bea"), "Ada does not know Bea yet");
		ctx.False(LF_Introductions.Introduce("bea", "ada"), "a second introduction adds nothing");
		ctx.Equal(1, LF_Introductions.PairCount(), "one pair after two identical introductions");
		ctx.False(LF_Introductions.Introduce("ada", "ada"), "introducing yourself to yourself is refused");
		ctx.False(LF_Introductions.Introduce("", "ada"), "an empty uid is refused");
		ctx.True(LF_Introductions.Introduce("cy", "ada"), "Ada introduces herself to Cy");

		array<string> knowers = {};
		LF_Introductions.GetKnowersOf("ada", knowers);
		ctx.Equal(2, knowers.Count(), "two people know Ada");

		// Export / apply: the save carries every pair, and a second apply ends on the same state.
		array<ref LF_IntroductionRecord> saved = LF_Introductions.ExportAll();
		ctx.Equal(2, saved.Count(), "the export carries both pairs");
		LF_Introductions.ResetStatics();
		ctx.False(LF_Introductions.Knows("bea", "ada"), "a fresh session knows nobody");
		LF_Introductions.ApplyAll(saved);
		ctx.True(LF_Introductions.Knows("bea", "ada"), "Bea still knows Ada after the apply");
		ctx.True(LF_Introductions.Knows("cy", "ada"), "Cy still knows Ada after the apply");
		ctx.False(LF_Introductions.Knows("ada", "bea"), "the apply does not invent the reverse pair");
		LF_Introductions.ApplyAll(saved);
		ctx.Equal(2, LF_Introductions.PairCount(), "a second apply converges on the same 2 pairs");

		array<ref LF_IntroductionRecord> broken = {};
		broken.Insert(LF_IntroductionRecord.Create("", "ada"));
		broken.Insert(null);
		broken.Insert(LF_IntroductionRecord.Create("dee", "ada"));
		LF_Introductions.ApplyAll(broken);
		ctx.Equal(1, LF_Introductions.PairCount(), "a bad record is skipped, the good one applied");

		// Up close only: a known name shows within a few metres and while looked at.
		ctx.True(LF_KnownNames.ShouldShow(2.0, 1.0), "looked at from 2 m: the name shows");
		ctx.True(LF_KnownNames.ShouldShow(LF_KnownNames.NAME_RANGE, 0.99), "looked at from the edge of range: the name shows");
		ctx.False(LF_KnownNames.ShouldShow(6.0, 1.0), "looked at from 6 m: no name");
		ctx.False(LF_KnownNames.ShouldShow(25.0, 1.0), "across the street: no name");
		ctx.False(LF_KnownNames.ShouldShow(2.0, 0.7), "45 degrees off: no name");

		LF_Introductions.ApplyAll(before);
	}
}
