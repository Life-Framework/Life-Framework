// red-proof: 2026-11-27, cli test --no-build --red LF_Test_MapIconQuads: RED-OK (RedProof() skips the missing-quad question, so the "a miss is never offered to a widget" check fails).

//------------------------------------------------------------------------------------------------
//! map/icon-quads (#5127): the icon lookup asks the imageset whether a quad exists before it loads it into a
//! widget, because a load of a missing quad makes the engine log `Can't find image` (60 lines per map open
//! while the fallback quad drew fine). Reads the real core imageset: its own quads answer yes, a quad it does
//! not hold answers no, and an empty imageset name never matches.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_MapIconQuads : LF_Test
{
	override string GetName()
	{
		return "map/icon-quads";
	}

	override void Run(LF_TestContext ctx)
	{
		ctx.True(LF_MapPlaceIcons.SetMayHave(LF_MapPlaceIcons.CORE_IMAGESET, "cat_civic"), "a quad the core imageset holds is offered to the widget");
		ctx.True(LF_MapPlaceIcons.SetMayHave(LF_MapPlaceIcons.CORE_IMAGESET, "cat_civic"), "the second ask (cached) answers the same");

		bool missing = true;
		if (!RedProof())
			missing = LF_MapPlaceIcons.SetMayHave(LF_MapPlaceIcons.CORE_IMAGESET, "poi_NoSuchQuad");

		ctx.False(missing, "a quad the imageset does not hold is never offered to a widget (no GUI error)");
		ctx.False(LF_MapPlaceIcons.SetMayHave("", "cat_civic"), "an empty imageset name matches nothing");
	}
}
