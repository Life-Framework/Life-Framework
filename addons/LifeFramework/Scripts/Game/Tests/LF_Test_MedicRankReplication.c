// red-proof: RedProof() drops the medic rank from the packed state, so the board offers a low rank every slot and the rank round trip fails (`tools\cli test --no-build --red LF_Test_MedicRankReplication`).
//! #4566: the medic rank rides in the replicated account state beside the police rank, in its own bits: it
//! round trips, clamps without spilling into the roster bit or the police rank, and the car pool board's
//! slot rule (`LF_MedicFleet.MayDraw` on the decoded rank) shows a medic only the ambulances the rank may draw.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_MedicRankReplication : LF_Test
{
	protected static const ResourceName LOW_CAR = "{E1A105BE1A000001}Prefabs/Vehicles/Wheeled/S105/EL_S105_beige.et";
	protected static const ResourceName HIGH_CAR = "{EFEB161F9BC2031D}Prefabs/Vehicles/Wheeled/UAZ469/LF_UAZ469_Police.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "medic/rank-replication";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		int medicRank = 2;
		if (RedProof())
			medicRank = 0;

		int medic = LF_AccountStateBits.Encode(true, LF_Faction.MEDIC, true, 1, 3, true, medicRank);
		ctx.Equal(2, LF_AccountStateBits.GetMedicRank(medic), "the medic rank round trips");
		ctx.Equal(3, LF_AccountStateBits.GetPoliceRank(medic), "the police rank is untouched by the medic rank");
		ctx.True(LF_AccountStateBits.IsOnRoster(medic), "the roster bit is untouched by the medic rank");
		ctx.Equal(1, LF_AccountStateBits.GetWantedLevel(medic), "wanted is untouched by the medic rank");
		ctx.True(LF_MedicDuty.IsOnDutyState(medic), "the medic duty reading is untouched");

		int clamped = LF_AccountStateBits.Encode(true, LF_Faction.MEDIC, true, 0, 0, false, 999);
		ctx.Equal(LF_AccountStateBits.MEDIC_RANK_MASK, LF_AccountStateBits.GetMedicRank(clamped), "the medic rank clamps to its field");
		ctx.False(LF_AccountStateBits.IsOnRoster(clamped), "a large medic rank does not set the roster bit");
		ctx.Equal(0, LF_AccountStateBits.GetPoliceRank(clamped), "a large medic rank does not spill into the police rank");
		ctx.Equal(0, LF_AccountStateBits.GetMedicRank(LF_AccountStateBits.Encode(true, LF_Faction.POLICE, true, 0, 5)), "no medic rank given reads 0");

		LF_PlayerAccount account = LF_PlayerAccount.Create("medic-rank-replication");
		account.SetFaction(LF_Faction.MEDIC);
		account.SetOnDuty(true);
		if (!RedProof())
			account.SetMedicRank(1);

		ctx.Equal(1, LF_AccountStateBits.GetMedicRank(LF_AccountStateBits.FromAccount(account)), "FromAccount carries the account's medic rank");

		array<ref LF_MedicRank> ranks = {};
		LF_MedicRanks.Ranks(ranks);
		ctx.True(ranks.Count() >= 2, "the medic ladder has the two rungs the slot rule needs");
		if (ranks.Count() < 2)
			return;

		array<ref LF_MedicFleetRow> rows = new array<ref LF_MedicFleetRow>();
		rows.Insert(LF_MedicFleetRow.Create(ranks[0].m_sId, LOW_CAR));
		rows.Insert(LF_MedicFleetRow.Create(ranks[1].m_sId, HIGH_CAR));
		LF_MedicFleet.UseRowsForTest(rows);

		string refusal;
		string needed;
		int low = LF_AccountStateBits.GetMedicRank(LF_AccountStateBits.Encode(true, LF_Faction.MEDIC, true, 0, 0, false, 0));
		int high = LF_AccountStateBits.GetMedicRank(LF_AccountStateBits.Encode(true, LF_Faction.MEDIC, true, 0, 0, false, 1));
		ctx.True(LF_MedicFleet.MayDraw(low, LOW_CAR, refusal, needed), "the lowest rank is offered the first car");
		ctx.False(LF_MedicFleet.MayDraw(low, HIGH_CAR, refusal, needed), "the lowest rank is not offered the second car");
		ctx.True(LF_MedicFleet.MayDraw(high, HIGH_CAR, refusal, needed), "the higher rank is offered the second car");
		LF_MedicFleet.Reload();
	}
}
