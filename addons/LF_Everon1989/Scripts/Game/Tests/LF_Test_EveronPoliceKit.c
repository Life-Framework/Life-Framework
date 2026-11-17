// red-proof: observed 2026-11-16 (#1942): before the kit overlay and the catalog line, `tools\cli test --no-build --only LF_Test_EveronPoliceKit` failed `- senior is issued the roadblock kit` and `- the senior officer carries one roadblock kit (expected 1, got 0)`; with them it passes. `--red LF_Test_EveronPoliceKit` (RedProof() drops the ladder): RED PROOF OK.
// tier: WORLD
//! #1942 in game: under the Everon 1989 header (the pack leg of `tools\cli test`), the police armory
//! hands out the spike strip and the roadblock kit by rank. The ladder is the core's layered with
//! `LF_Police_Everon1989.conf` and `LF_PoliceKit_Everon1989.conf`. The patrol rank draws a spike strip
//! and no roadblock kit; the senior rank draws the roadblock kit; a civilian cannot draw and carries
//! nothing out.
class LF_Test_EveronPoliceKit : LF_Test
{
	static const ResourceName KIT_OVERLAY = "{6ED5DDC9D2DAF591}Configs/Police/LF_PoliceKit_Everon1989.conf";
	protected static const string PATROL_UID = "everon-kit-patrol";
	protected static const string SENIOR_UID = "everon-kit-senior";
	protected static const string CIVILIAN_UID = "everon-kit-civilian";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/police-kit";
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
	protected static bool KitHas(int rank, ResourceName prefab)
	{
		array<ResourceName> kit = {};
		LF_PoliceRanks.CollectKit(rank, kit);
		return kit.Find(prefab) != -1;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// The leg boots under the pack header, and that header lists the kit overlay.
		LF_MissionHeader header = LF_MissionHeader.GetCurrent();
		ctx.NotNull(header, "the pack leg runs under an LF mission header");
		if (header)
			ctx.True(header.m_aLF_ConfigOverlays && header.m_aLF_ConfigOverlays.Find(KIT_OVERLAY) != -1, "the header lists the Everon police kit overlay");

		// The act: the running header's overlays layered over the core ladder.
		LF_PoliceConfig.Reload();
		if (RedProof())
			LF_PoliceConfig.UseForTest(null);

		int patrol = RankIndex("patrol");
		int senior = RankIndex("senior");
		int lieutenant = RankIndex("lieutenant");
		ctx.True(patrol >= 0 && senior > patrol, "the pack ladder has a patrol rung under a senior rung");

		// The kit lines per rank.
		ctx.True(KitHas(patrol, LF_PoliceRanks.SPIKE_STRIP), "patrol is issued the spike strip");
		ctx.False(KitHas(patrol, LF_Roadblocks.KIT), "patrol is not issued the roadblock kit");
		ctx.True(KitHas(senior, LF_Roadblocks.KIT), "senior is issued the roadblock kit");
		ctx.True(KitHas(senior, LF_PoliceRanks.SPIKE_STRIP), "senior keeps the spike strip");
		ctx.True(lieutenant >= 0 && KitHas(lieutenant, LF_Roadblocks.KIT), "a higher rung keeps the roadblock kit");

		// The draw itself.
		IEntity patrolOfficer = LF_TestCharacterFixture.Spawn(ctx);
		IEntity seniorOfficer = LF_TestCharacterFixture.Spawn(ctx);
		IEntity civilian = LF_TestCharacterFixture.Spawn(ctx);
		if (!patrolOfficer || !seniorOfficer || !civilian)
		{
			LF_PoliceConfig.Reload();
			return;
		}

		LF_PlayerAccount patrolAccount = LF_TestPoliceKit.Account(PATROL_UID, true);
		patrolAccount.SetFaction(LF_Faction.POLICE);
		patrolAccount.SetOnDuty(true);
		patrolAccount.SetPoliceRank(patrol);
		LF_PoliceRanks.BindOfficer(patrolOfficer, PATROL_UID);

		// The roadblock kit is bulky (6000 cm3): a plain test character has no room for it beside the rifle.
		InventoryStorageManagerComponent inventory = LF_InventoryUtils.GetResponsibleStorageManager(seniorOfficer);
		IEntity pack = LF_Utils.SpawnEntityPrefab("{06B68C58B72EAAC6}Prefabs/Items/Equipment/Backpacks/Backpack_ALICE_Medium.et", seniorOfficer.GetOrigin());
		ctx.True(pack != null && inventory && inventory.TryInsertItem(pack, EStoragePurpose.PURPOSE_LOADOUT_PROXY), "the senior officer wears a pack");

		LF_PlayerAccount seniorAccount = LF_TestPoliceKit.Account(SENIOR_UID, true);
		seniorAccount.SetFaction(LF_Faction.POLICE);
		seniorAccount.SetOnDuty(true);
		seniorAccount.SetPoliceRank(senior);
		LF_PoliceRanks.BindOfficer(seniorOfficer, SENIOR_UID);

		LF_TestPoliceKit.Account(CIVILIAN_UID, false);
		LF_PoliceRanks.BindOfficer(civilian, CIVILIAN_UID);

		ctx.True(LF_PoliceRanks.Draw(PATROL_UID, patrolOfficer), "the patrol officer draws a kit");
		ctx.Equal(1, LF_InventoryUtils.GetAmount(patrolOfficer, LF_PoliceRanks.SPIKE_STRIP), "the patrol officer carries one spike strip");
		ctx.Equal(0, LF_InventoryUtils.GetAmount(patrolOfficer, LF_Roadblocks.KIT), "the patrol officer carries no roadblock kit");

		ctx.True(LF_PoliceRanks.Draw(SENIOR_UID, seniorOfficer), "the senior officer draws a kit");
		ctx.Equal(1, LF_InventoryUtils.GetAmount(seniorOfficer, LF_Roadblocks.KIT), "the senior officer carries one roadblock kit");

		ctx.False(LF_PoliceRanks.CanDraw(CIVILIAN_UID), "a civilian cannot draw");
		ctx.False(LF_PoliceRanks.Draw(CIVILIAN_UID, civilian), "a civilian's draw is refused");
		ctx.Equal(0, LF_InventoryUtils.GetAmount(civilian, LF_PoliceRanks.SPIKE_STRIP), "a civilian carries no spike strip");
		ctx.Equal(0, LF_InventoryUtils.GetAmount(civilian, LF_Roadblocks.KIT), "a civilian carries no roadblock kit");

		LF_TestPoliceKit.Release(PATROL_UID);
		LF_TestPoliceKit.Release(SENIOR_UID);
		LF_TestPoliceKit.Release(CIVILIAN_UID);
		LF_PoliceConfig.Reload();
	}
}
