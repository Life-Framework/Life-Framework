// red-proof: RedProof() drops the pack table, so no rank draws an ambulance and the table assertions fail, and it puts the core M997 stretcher list back so the fleet ambulances stop being stretcher vehicles (`tools\cli test --no-build --red LF_Test_EveronMedicFleet`).
//! #4432 in game: under the Everon 1989 header (the pack leg of `tools\cli test`), the medic car pool hands the
//! Orderly and EMT the UAZ452 ambulance and the Paramedic and Doctor the S1203 ambulance, refuses the Orderly the S1203
//! in words, and hands nobody the old M997. The cars carry the lock, store-here and siren a service ambulance needs.
[LF_TestInfo(LF_TestTier.LOGIC, pack: true)]
class LF_Test_EveronMedicFleet : LF_Test
{
	protected static const ResourceName UAZ_CAR = "{CD9E4E997EA1DFEF}Prefabs/Vehicles/Medic/LF_UAZ452_AmbulanceFleet.et";
	protected static const ResourceName S1203_CAR = "{6645B582BBD5F2CD}Prefabs/Vehicles/Medic/LF_S1203_AmbulanceFleet.et";
	protected static const ResourceName M997 = "{D8C109C681E7AAF1}Prefabs/Vehicles/Wheeled/M997/LF_M997_Ambulance.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/medic-fleet";
	}

	//------------------------------------------------------------------------------------------------
	protected static int RankIndex(string id)
	{
		array<ref LF_MedicRank> ranks = {};
		LF_MedicRanks.Ranks(ranks);
		foreach (int i, LF_MedicRank rank : ranks)
		{
			if (rank.m_sId == id)
				return i;
		}

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_MedicFleet.Reload();
		// RedProof(): the pack table is replaced by an empty one, so no rank draws a car (`--red`).
		if (RedProof())
			LF_MedicFleet.UseRowsForTest(new array<ref LF_MedicFleetRow>());

		int orderly = RankIndex("orderly");
		int paramedic = RankIndex("paramedic");
		ctx.True(orderly >= 0 && paramedic > orderly, "the pack ladder has the Orderly and Paramedic rungs");
		ctx.True(LF_MedicFleet.CarFor(orderly) == UAZ_CAR, "the Orderly draws the UAZ452 ambulance");
		ctx.True(LF_MedicFleet.CarFor(RankIndex("emt")) == UAZ_CAR, "the EMT draws the UAZ452 ambulance");
		ctx.True(LF_MedicFleet.CarFor(paramedic) == S1203_CAR, "the Paramedic draws the S1203 ambulance");
		ctx.True(LF_MedicFleet.CarFor(RankIndex("doctor")) == S1203_CAR, "the Doctor draws the S1203 ambulance");

		string refusal;
		string needed;
		ctx.False(LF_MedicFleet.MayDraw(orderly, S1203_CAR, refusal, needed), "the Orderly is refused the S1203");
		ctx.EqualStr(LF_RankFleet.REFUSED_NEEDS_RANK, refusal, "the refusal says the rank is too low");
		ctx.EqualStr("LF-MedicRank_Paramedic", needed, "the refusal names the Paramedic");
		ctx.True(LF_MedicFleet.MayDraw(paramedic, UAZ_CAR, refusal, needed), "the Paramedic may also draw the lower car");
		ctx.False(LF_MedicFleet.MayDraw(RankIndex("doctor"), M997, refusal, needed), "no rank is handed the old M997");

		array<ResourceName> board = {};
		LF_MedicCarPool.Cars(board);
		ctx.True(board.Count() == 2, string.Format("the board offers two ambulances (%1)", board.Count()));
		foreach (ResourceName car : board)
		{
			ctx.False(LF_Utils.SamePrefab(car, M997), "the board does not offer the M997");
		}

		// #4566: the fleet ambulances are the stretcher vehicles, the old M997 is not (the pack overlay replaces the list).
		LF_MedicConfig.Reload();
		// RedProof(): the core list stands (the M997 only), as if the pack overlay did not set the stretcher vehicles.
		if (RedProof())
		{
			array<ResourceName> coreList = new array<ResourceName>();
			coreList.Insert(M997);
			LF_MedicConfig.Get().m_aStretcherVehicles = coreList;
		}

		ctx.True(LF_BedDelivery.IsStretcherPrefab(UAZ_CAR), "the UAZ452 fleet ambulance is a stretcher vehicle");
		ctx.True(LF_BedDelivery.IsStretcherPrefab(S1203_CAR), "the S1203 fleet ambulance is a stretcher vehicle");
		ctx.False(LF_BedDelivery.IsStretcherPrefab(M997), "the M997 is not a stretcher vehicle under the pack");
		LF_MedicConfig.Reload();
		LF_MedicFleet.Reload();
	}
}
