// red-proof: RedProof() drops the pack table, so the Cadet draws no car and every assertion below fails (`tools\cli test --no-build --red LF_Test_EveronRankFleet`).
//! PO-R1 (#4205) in game: under the Everon 1989 header (the pack leg of `tools\cli test`), the police car pool
//! hands the Cadet the beige S105 and Patrol and up the UAZ469 police car, and refuses the Cadet the UAZ in words.
//! The ladder is the core's layered with `LF_Police_Everon1989.conf`; the cars are `LF_RankFleet_Everon1989.conf`.
[LF_TestInfo(LF_TestTier.LOGIC, pack: true)]
class LF_Test_EveronRankFleet : LF_Test
{
	protected static const ResourceName SMALL_CAR = "{E1A105BE1A000001}Prefabs/Vehicles/Wheeled/S105/EL_S105_beige.et";
	protected static const ResourceName PATROL_CAR = "{EFEB161F9BC2031D}Prefabs/Vehicles/Wheeled/UAZ469/LF_UAZ469_Police.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/rank-fleet";
	}

	//------------------------------------------------------------------------------------------------
	protected static int RankIndex(string id)
	{
		array<ref LF_PoliceRank> ranks = {};
		LF_PoliceConfig.Ranks(ranks);
		foreach (int i, LF_PoliceRank rank : ranks)
		{
			if (rank.m_sId == id)
				return i;
		}

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_RankFleet.Reload();
		// RedProof(): the pack table is replaced by an empty one, so no rank draws a car (`--red`).
		if (RedProof())
			LF_RankFleet.UseRowsForTest(new array<ref LF_RankFleetRow>());

		int cadet = RankIndex("patrol");
		int patrol = RankIndex("senior");
		ctx.True(cadet >= 0 && patrol > cadet, "the pack ladder has the Cadet and Patrol rungs");
		ctx.True(LF_RankFleet.CarFor(cadet) == SMALL_CAR, "the Cadet draws the beige S105");
		ctx.True(LF_RankFleet.CarFor(patrol) == PATROL_CAR, "Patrol draws the UAZ469 police car");
		array<string> upper = { "senior_patrol", "sergeant", "lieutenant" };
		foreach (string id : upper)
		{
			ctx.True(LF_RankFleet.CarFor(RankIndex(id)) == PATROL_CAR, string.Format("%1 draws the UAZ469 police car", id));
		}

		string refusal;
		string needed;
		ctx.False(LF_RankFleet.MayDraw(cadet, PATROL_CAR, refusal, needed), "the Cadet is refused the UAZ469");
		ctx.EqualStr(LF_RankFleet.REFUSED_NEEDS_RANK, refusal, "the refusal says the rank is too low");
		ctx.EqualStr("LF-Rank_Patrol", needed, "the refusal names Patrol");
		ctx.True(LF_RankFleet.MayDraw(patrol, PATROL_CAR, refusal, needed), "Patrol draws the UAZ469");

		LF_RankFleet.Reload();
	}
}
