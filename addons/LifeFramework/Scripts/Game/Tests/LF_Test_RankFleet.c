// red-proof: observed 2026-11-23 `tools\cli test --no-build --red LF_Test_RankFleet`: RED PROOF OK; RedProof() installs an empty fleet table, so `the small car needs the lowest rank (expected 0, got -1)` and every draw assertion fail. Green with `--only`.
//! Rank fleet (PO-R1, #4205): each police rank draws its configured car from the pool, a rank draws any car
//! at or below its own, and a lower rank is refused the higher car with the rank it needs named in the
//! refusal. Uses the ladder's first two rungs (the built-in patrol and senior, or the core file's).
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_RankFleet : LF_Test
{
	protected static const ResourceName SMALL_CAR = "{E1A105BE1A000001}Prefabs/Vehicles/Wheeled/S105/EL_S105_beige.et";
	protected static const ResourceName PATROL_CAR = "{EFEB161F9BC2031D}Prefabs/Vehicles/Wheeled/UAZ469/LF_UAZ469_Police.et";
	protected static const ResourceName OTHER_CAR = "{E1A105BE1A000002}Prefabs/Vehicles/Wheeled/Nowhere/Nothing.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "police/rank-fleet";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		array<ref LF_PoliceRank> ranks = {};
		LF_PoliceConfig.Ranks(ranks);
		ctx.True(ranks.Count() >= 2, "the ladder has the two rungs the fleet test needs");
		if (ranks.Count() < 2)
			return;

		array<ref LF_RankFleetRow> rows = new array<ref LF_RankFleetRow>();
		rows.Insert(LF_RankFleetRow.Create(ranks[0].m_sId, SMALL_CAR));
		rows.Insert(LF_RankFleetRow.Create(ranks[1].m_sId, PATROL_CAR));
		// RedProof(): the table is never installed, so no rank draws a car and every assertion below fails (`--red`).
		if (!RedProof())
			LF_RankFleet.UseRowsForTest(rows);
		else
			LF_RankFleet.UseRowsForTest(new array<ref LF_RankFleetRow>());

		// Each rank gets its configured prefab.
		ctx.True(LF_RankFleet.CarFor(0) == SMALL_CAR, "the lowest rank draws the small car");
		ctx.True(LF_RankFleet.CarFor(1) == PATROL_CAR, "the next rank draws the patrol UAZ");
		ctx.True(LF_RankFleet.CarFor(ranks.Count() + 3) == CarOfTop(ranks, rows), "a rank past the ladder reads as the top rung");
		ResourceName drawn;
		string refusal;
		ctx.True(LF_RankFleet.Request(0, drawn, refusal), "the lowest rank may request its car");
		ctx.True(drawn == SMALL_CAR, "the pool hands the lowest rank the small car");
		ctx.EqualStr("", refusal, "an allowed request has no refusal");
		ctx.True(LF_RankFleet.Request(1, drawn, refusal), "the next rank may request its car");
		ctx.True(drawn == PATROL_CAR, "the pool hands the next rank the patrol UAZ");

		// A lower rank is refused the higher car in words; a higher rank may still take the lower one.
		string needed;
		ctx.False(LF_RankFleet.MayDraw(0, PATROL_CAR, refusal, needed), "the lowest rank is refused the patrol UAZ");
		ctx.EqualStr(LF_RankFleet.REFUSED_NEEDS_RANK, refusal, "the refusal says the rank is too low");
		ctx.EqualStr(LF_PoliceRanks.TitleKey(1), needed, "the refusal names the rank that may draw the car");
		ctx.True(LF_RankFleet.MayDraw(1, PATROL_CAR, refusal, needed), "the next rank draws the patrol UAZ");
		ctx.EqualStr("", refusal, "an allowed draw has no refusal");
		ctx.True(LF_RankFleet.MayDraw(0, SMALL_CAR, refusal, needed), "the lowest rank draws the small car");
		ctx.True(LF_RankFleet.MayDraw(1, SMALL_CAR, refusal, needed), "a higher rank may still take the lower rank's car");
		ctx.Equal(0, LF_RankFleet.RankNeeded(SMALL_CAR), "the small car needs the lowest rank");
		ctx.Equal(1, LF_RankFleet.RankNeeded(PATROL_CAR), "the patrol UAZ needs the next rank");

		// A car on no rank's row is never issued.
		ctx.False(LF_RankFleet.MayDraw(1, OTHER_CAR, refusal, needed), "a car on no row is refused");
		ctx.EqualStr(LF_RankFleet.REFUSED_NOT_ISSUED, refusal, "the refusal says the car is not issued");
		ctx.Equal(-1, LF_RankFleet.RankNeeded(OTHER_CAR), "a car on no row needs no rank");

		// A rank with no row draws nothing, and the request says so.
		array<ref LF_RankFleetRow> partial = new array<ref LF_RankFleetRow>();
		partial.Insert(LF_RankFleetRow.Create(ranks[1].m_sId, PATROL_CAR));
		LF_RankFleet.UseRowsForTest(partial);
		ctx.False(LF_RankFleet.Request(0, drawn, refusal), "a rank with no row draws no car");
		ctx.True(drawn.IsEmpty(), "a refused request hands out no prefab");
		ctx.EqualStr(LF_RankFleet.REFUSED_NOT_ISSUED, refusal, "a rank with no row is told no car is issued");

		LF_RankFleet.Reload();
	}

	//------------------------------------------------------------------------------------------------
	//! The car of the last rung in the test table that the ladder's top rung maps to.
	protected ResourceName CarOfTop(notnull array<ref LF_PoliceRank> ranks, notnull array<ref LF_RankFleetRow> rows)
	{
		string topId = ranks[ranks.Count() - 1].m_sId;
		foreach (LF_RankFleetRow row : rows)
		{
			if (row.m_sRankId == topId)
				return row.m_sPrefab;
		}

		return ResourceName.Empty;
	}
}
