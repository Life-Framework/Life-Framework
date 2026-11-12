// red-proof: dropped RELEASE_MARGIN from LF_WantedPosterFaces.Decide (`distance > radius`) and ran
// `tools\cli test --no-build --only LF_Test_WantedPosterFace` (2026-11-12). Observed:
//   [LFTEST]   - just outside, a built face stays (hysteresis) (expected 0, got -1)
// Restored the margin and the test passed (13 assertions).

// tier: LOGIC
//! The face's budget rules (docs/architecture/visual-boards-budget.md): build within the draw
//! radius, release beyond it plus the margin, keep in between; posters sit in one centred row up
//! to four and two rows above that; the owner's `wantedBoard` section sets the count and radius
//! inside their limits.
class LF_Test_WantedPosterFace : LF_Test
{
	override string GetName()
	{
		return "police/wanted-poster-face";
	}

	override void Run(LF_TestContext ctx)
	{
		ctx.Equal(1, LF_WantedPosterFaces.Decide(29, 30, false), "inside the radius a released face builds");
		ctx.Equal(0, LF_WantedPosterFaces.Decide(33, 30, true), "just outside, a built face stays (hysteresis)");
		ctx.Equal(-1, LF_WantedPosterFaces.Decide(36, 30, true), "beyond the margin it is released");
		ctx.Equal(0, LF_WantedPosterFaces.Decide(33, 30, false), "outside, a released face stays released");

		vector a = LF_WantedPosterFace.SlotPosition(0, 4);
		vector d = LF_WantedPosterFace.SlotPosition(3, 4);
		ctx.EqualFloat(LF_WantedPosterFace.FACE_WIDTH - d[0] - LF_WantedPosterFace.POSTER_WIDTH, a[0], 0.01, "four posters sit in one centred row");
		ctx.EqualFloat(a[1], d[1], 0.01, "four posters share a row");
		vector one = LF_WantedPosterFace.SlotPosition(0, 1);
		ctx.EqualFloat((LF_WantedPosterFace.FACE_WIDTH - LF_WantedPosterFace.POSTER_WIDTH) * 0.5, one[0], 0.01, "one poster is centred");
		vector fifth = LF_WantedPosterFace.SlotPosition(4, 5);
		ctx.True(fifth[1] > a[1], "a fifth poster starts a second row");
		ctx.True(fifth[1] + LF_WantedPosterFace.POSTER_HEIGHT <= LF_WantedPosterFace.FACE_HEIGHT, "two rows fit the face");

		LF_ConfigService.Reset();
		LF_ConfigService.LoadOwnerString("{\"schemaVersion\":1,\"wantedBoard\":{\"posters\":9,\"drawRadius\":50}}", "test");
		LF_WantedPostersConfig config = new LF_WantedPostersConfig();
		config.m_iPosters = 4;
		config.m_fDrawRadius = 30;
		config.ApplyLayers();
		ctx.Equal(4, config.m_iPosters, "more posters than the layout holds is refused; the shipped 4 stays");
		ctx.EqualFloat(50, config.m_fDrawRadius, 0.01, "the owner's draw radius applies");
		LF_ConfigService.Reset();
		LF_WantedPostersConfig.Reload();
		ctx.Equal(LF_WantedPostersConfig.DEFAULT_POSTERS, LF_WantedPostersConfig.Posters(), "the shipped board holds 4 posters");
		ctx.EqualFloat(LF_WantedPostersConfig.DEFAULT_DRAW_RADIUS, LF_WantedPostersConfig.DrawRadius(), 0.01, "the shipped draw radius is 30 m");
	}
}
