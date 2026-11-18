// red-proof: with the act skipped (RedProof() on, `cli test --no-build --red LF_Test_ResidentWitness`) no
//   scene is recorded, so `the resident near the crime is a named witness` fails.
//! residents/witness: a live resident within sight of a crime is listed on the report's scene under
//! `resident:<id>` and remembers the suspect (`crime_seen`); one out of sight remembers nothing; one
//! who is not spawned is never a witness (#2860).
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_ResidentWitness : LF_Test
{
	protected static const string SHOP_KIND = "test_residents_witness_shop";
	protected static const string HOME_KIND = "test_residents_witness_home";
	protected static const ResourceName PLACE_PREFAB = "{25C8B889A777399D}Prefabs/Props/Infrastructure/ConeTraffic/ConeTraffic_01_red.et";
	protected static const ResourceName ANCHOR_PREFAB = "{CCED18E36D627B1D}Prefabs/Systems/Residents/LF_ResidentAnchor.et";
	protected static const int WORK_MINUTE = 450;	// 07:30
	protected static const string SUSPECT = "test-resident-witness-uid";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "residents/witness";
	}

	//------------------------------------------------------------------------------------------------
	protected bool Listed(LF_WitnessScene scene, string key)
	{
		if (!scene)
			return false;

		foreach (LF_WitnessEntry entry : scene.m_aWitnesses)
		{
			if (entry.m_sKey == key)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		IEntity vault = LF_Places.Find(LF_Bank.PLACE_KIND, LF_TestAnnex.BANK_VAULT);
		ctx.NotNull(vault, "the annex vault is placed, to spawn the stand-ins near");
		if (!vault)
			return;

		vector shopAt = vault.GetOrigin() + "0 -20 0";
		vector homeAt = shopAt + "600 0 0";
		IEntity shop = LF_Utils.SpawnEntityPrefab(PLACE_PREFAB, shopAt, "0 0 0", false);
		IEntity home = LF_Utils.SpawnEntityPrefab(PLACE_PREFAB, homeAt, "0 0 0", false);
		ctx.True(shop && home, "the test shop and home spawn");
		if (!shop || !home)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(shop);
			SCR_EntityHelper.DeleteEntityAndChildren(home);
			return;
		}

		ctx.True(LF_Places.Register(SHOP_KIND, "witness_shop", shop), "the shop is placed");
		ctx.True(LF_Places.Register(HOME_KIND, "witness_home", home), "the home is placed");
		vector counterAt = shopAt + "3 0 4";
		IEntity counter = LF_Utils.SpawnEntityPrefab(ANCHOR_PREFAB, counterAt, "0 0 0", false);
		ctx.NotNull(counter, "the counter anchor spawns");
		LF_ResidentAnchorComponent comp;
		if (counter)
			comp = LF_ResidentAnchorComponent.Cast(counter.FindComponent(LF_ResidentAnchorComponent));
		if (comp)
			comp.SetAnchorId("counter");

		LF_ResidentsConfig config = new LF_ResidentsConfig();
		config.m_iSpawnRadiusM = 150;
		config.m_iDespawnRadiusM = 200;
		config.m_iMaxLive = 1;	// the cap keeps the second resident dormant
		config.m_aResidents = {};
		LF_ResidentDef baker = LF_ResidentDef.Make("aaa_baker", HOME_KIND, SHOP_KIND);
		baker.Add("06:00", "home", "bench");
		baker.Add("07:00", "work", "counter", "wave");
		config.m_aResidents.Insert(baker);
		LF_ResidentDef clerk = LF_ResidentDef.Make("bbb_clerk", HOME_KIND, SHOP_KIND);
		clerk.Add("06:00", "home", "bench");
		clerk.Add("07:00", "work", "counter", "wave");
		config.m_aResidents.Insert(clerk);
		LF_ResidentsConfig.UseForTest(config);
		LF_ResidentTimetable.Load(config);
		LF_ResidentSpawner.ResetStatics();

		LF_ResidentMemory.ResetStatics();
		LF_ResidentMemoryConfig memory = new LF_ResidentMemoryConfig();
		memory.m_iCooldownSeconds = 600;
		memory.m_iGossipPercent = 0;
		memory.m_iRefusesBelow = -60;
		memory.m_iWaryBelow = -20;
		memory.m_iFriendlyFrom = 20;
		memory.m_iRegularFrom = 60;
		memory.m_aReasons = {};
		memory.m_aReasons.Insert(LF_ResidentMemoryReason.Make("crime_seen", -30, true));
		memory.m_aKnows = {};
		LF_ResidentMemoryConfig.UseForTest(memory);

		LF_Test_ResidentSpawner_Players players = new LF_Test_ResidentSpawner_Players();
		players.m_aAt.Insert(shopAt + "30 0 0");
		LF_ResidentSpawner.SweepAt(WORK_MINUTE, players);
		IEntity live = LF_ResidentSpawner.GetLive("aaa_baker");
		ctx.NotNull(live, "the baker is live");
		ctx.True(LF_ResidentSpawner.GetLive("bbb_clerk") == null, "the clerk stays dormant");

		if (live)
		{
			// 10 m from the baker: a named witness, and the baker thinks less of the suspect.
			IEntity near = LF_Utils.SpawnEntityPrefab(PLACE_PREFAB, live.GetOrigin() + "10 0 0", "0 0 0", false);
			LF_WitnessScene scene;
			if (!RedProof())
				scene = LF_Witness.RecordCrime(9001, SUSPECT, LF_WantedReports.OFFENCE_SHOPLIFTING, near, "", "", "");

			ctx.True(Listed(scene, "resident:aaa_baker"), "the resident near the crime is a named witness");
			ctx.True(LF_Witness.NpcKey(live) == "resident:aaa_baker", "the key is the resident id, not the generic npc key");
			ctx.True(!Listed(scene, "resident:bbb_clerk"), "a dormant resident is never a witness");
			ctx.Equal(-30, LF_ResidentMemory.Score("aaa_baker", SUSPECT), "the baker's memory drops by the crime_seen award");
			ctx.Equal(0, LF_ResidentMemory.Score("bbb_clerk", SUSPECT), "the dormant clerk remembers nothing");
			SCR_EntityHelper.DeleteEntityAndChildren(near);

			// 80 m away: out of sight, listed nowhere, remembers nothing about this other suspect.
			string other = SUSPECT + "-far";
			IEntity far = LF_Utils.SpawnEntityPrefab(PLACE_PREFAB, live.GetOrigin() + "80 0 0", "0 0 0", false);
			LF_WitnessScene farScene = LF_Witness.RecordCrime(9002, other, LF_WantedReports.OFFENCE_SHOPLIFTING, far, "", "", "");
			ctx.True(!Listed(farScene, "resident:aaa_baker"), "a resident 80 m away is not a witness");
			ctx.Equal(0, LF_ResidentMemory.Score("aaa_baker", other), "and remembers nothing");
			SCR_EntityHelper.DeleteEntityAndChildren(far);
		}

		LF_ResidentSpawner.DespawnAll();
		LF_ResidentSpawner.ResetStatics();
		LF_ResidentTimetable.ResetStatics();
		LF_ResidentMemory.ResetStatics();
		LF_ResidentMemoryConfig.ResetStatics();
		LF_ResidentsConfig.Reload();
		LF_Places.Unregister(SHOP_KIND, "witness_shop");
		LF_Places.Unregister(HOME_KIND, "witness_home");
		SCR_EntityHelper.DeleteEntityAndChildren(counter);
		SCR_EntityHelper.DeleteEntityAndChildren(shop);
		SCR_EntityHelper.DeleteEntityAndChildren(home);
	}
}
