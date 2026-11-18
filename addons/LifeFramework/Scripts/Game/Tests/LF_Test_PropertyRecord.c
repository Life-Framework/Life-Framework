// red-proof: Observed 2026-11-17 with `tools\cli test --no-build --red LF_Test_PropertyRecord,LF_Test_PropertyPersist`: `RED-OK property/record` with SetStage/SetOwner skipped, `- the plot is at stage 2 (expected 2, got 0)`, `- the plot has its owner (expected 'property-record-owner', got '')`. Green with them.
//! property/record (#2646): a property row is created by its stable id, takes a build stage and an
//! owner, exports and applies back to the same rows; a row whose property is not in the world waits
//! in the book and is exported again. The test puts back the book it found.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_PropertyRecord : LF_Test
{
	protected static const string PLOT = "property-record-plot";
	protected static const string UNKNOWN = "property-record-unknown";
	protected static const string OWNER = "property-record-owner";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "property/record";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		array<ref LF_PropertyRecord> before = LF_Properties.ExportAll();
		LF_Properties.ApplyAll(null);

		ctx.True(LF_Properties.Create("") == null, "a property with no id is refused");

		LF_PropertyRecord created = LF_Properties.Create(PLOT);
		ctx.NotNull(created, "a plot row is created by its key");
		if (created)
		{
			created.m_sOrgId = "property-record-org";
			created.m_sLayoutId = "cabin-small";
			created.m_iWorkDone = 40;
			created.m_sDeposited = "planks=12;nails=30";
			created.m_iPurchasedAt = 1800000000;
			created.m_iRentPaidUntil = 1800086400;
			created.m_bOwnerSpawnsHere = true;
		}

		ctx.True(LF_Properties.Create(PLOT) == created, "creating the same id again returns the same row");
		if (!RedProof())
		{
			LF_Properties.SetStage(PLOT, 2);
			LF_Properties.SetOwner(PLOT, OWNER);
		}

		ctx.False(LF_Properties.SetStage("property-record-none", 1), "a stage for an unknown property is refused");
		ctx.Equal(2, LF_Properties.Get(PLOT).m_iBuildStage, "the plot is at stage 2");
		ctx.EqualStr(OWNER, LF_Properties.GetOwnerKey(PLOT), "the plot has its owner");

		array<ref LF_PropertyRecord> exported = LF_Properties.ExportAll();
		ctx.Equal(1, exported.Count(), "one row is exported");

		// An unknown property (no house, no plot in the world) arrives with the save: it waits.
		LF_PropertyRecord waiting = LF_PropertyRecord.Create(UNKNOWN);
		waiting.m_iBuildStage = 1;
		exported.Insert(waiting);

		ctx.Equal(2, LF_Properties.ApplyAll(exported), "both rows are applied");
		ctx.Equal(2, LF_Properties.ApplyAll(exported), "applying again keeps both rows");

		array<ref LF_PropertyRecord> again = LF_Properties.ExportAll();
		ctx.Equal(2, again.Count(), "both rows are exported again");
		if (again.Count() == 2)
		{
			ctx.True(again.Get(0).SameAs(exported.Get(0)), "the plot row is the same after apply");
			ctx.EqualStr(UNKNOWN, again.Get(1).m_sPropertyId, "the waiting row is saved again");
			ctx.Equal(1, again.Get(1).m_iBuildStage, "the waiting row keeps its stage");
		}

		LF_PropertyRecord plot = LF_Properties.Get(PLOT);
		ctx.NotNull(plot, "the plot is back");
		if (plot)
		{
			ctx.EqualStr("planks=12;nails=30", plot.m_sDeposited, "the deposits are back");
			ctx.Equal(1800086400, plot.m_iRentPaidUntil, "the rent clock is back");
			ctx.True(plot.m_bOwnerSpawnsHere, "the spawn choice is back");
		}

		LF_Properties.ApplyAll(before);
	}
}
