// red-proof: pending `tools\cli test --no-build --red LF_Test_ShortageNotice`; RedProof() skips the shortage start, so the notice rows stay empty and the "short" assertions fail.
//! signs/shortage-notice (#4448): the paper on the clinic door follows the clinic's shortage state. Short: three
//! rows (why, prices, until) and the paper hangs; stocked again: no rows and the paper is gone. The row source
//! that a prefab names exists and works locally, so no server ask is needed.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_ShortageNotice : LF_Test
{
	static const string CABINET_ID = "shortage-notice-test-cabinet";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "signs/shortage-notice";
	}

	//------------------------------------------------------------------------------------------------
	protected int RowCount()
	{
		array<ref LF_SignRow> rows = {};
		LF_ShortageNotice.BuildRows(LF_ShortageNotice.IsShort(), rows);
		return rows.Count();
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_ClinicShortage.ResetStatics();
		LF_ClinicShortageConfig config = LF_ClinicShortageConfig.Defaults();
		config.m_iDurationSeconds = 600;
		LF_ClinicShortageConfig.UseForTest(config);
		LF_ClinicShortage.s_iNowOverride = 5000;

		LF_SignRowSource source = LF_SignRowSources.Find(LF_ShortageNotice.SOURCE_KIND);
		ctx.NotNull(source, "the clinic_shortage row source is registered");
		if (source)
			ctx.True(source.IsLocal(), "the notice works its rows out on the client, no server ask");

		// Stocked: nothing on the door.
		ctx.Equal(0, RowCount(), "a stocked clinic prints no notice rows");
		ctx.False(LF_ShortageNotice.Hangs(LF_ShortageNotice.IsShort()), "a stocked clinic has no paper on the door");

		// Short: the paper says why.
		if (!RedProof())
			LF_ClinicShortage.Start(CABINET_ID, "shortage-notice-robber");

		ctx.Equal(3, RowCount(), "a short clinic prints why, prices and until");
		ctx.True(LF_ShortageNotice.Hangs(LF_ShortageNotice.IsShort()), "a short clinic hangs the paper");
		array<ref LF_SignRow> rows = {};
		LF_ShortageNotice.BuildRows(true, rows);
		foreach (LF_SignRow row : rows)
			ctx.True(!row.m_sLabel.IsEmpty() && !row.m_sValue.IsEmpty(), "every notice row has a label and a value");

		// The time runs out: the paper comes down.
		LF_ClinicShortage.s_iNowOverride = 6000;
		LF_ClinicShortage.Expire();
		ctx.Equal(0, RowCount(), "after the shortage the notice prints nothing");
		ctx.False(LF_ShortageNotice.Hangs(LF_ShortageNotice.IsShort()), "after the shortage the paper is gone");

		LF_ClinicShortage.ResetStatics();
		LF_ClinicShortage.Install();
	}
}
