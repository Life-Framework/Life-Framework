// red-proof: 2026-11-23, before LF_ResidentActivity.Apply reached the work loop, `hammer` was an unknown word: it idled with a
//   warning, so `a hammer line starts the work loop` and `the activity says hammer` failed. With the act skipped
//   (RedProof() on, `cli test --no-build --red LF_Test_ResidentWorkApply`) the same checks fail. The run that went red
//   and then green is in the PR.
//! residents/work-apply (#4366): a timetable line that says carry, hammer or sweep plays the work loop. Apply on a
//! character starts the loop and faces the anchor, a later line (serve) or Release ends it, a static body stands with
//! one warning, and the spawner starts the loop on a spawned resident and ends it when it despawns the resident.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_ResidentWorkApply : LF_Test
{
	protected static const ResourceName MANNEQUIN = "{6E8EF026940F3841}Prefabs/Characters/Npcs/Npc_Civilian_01.et";
	protected static const ResourceName CHARACTER = "{9B443DD4C1F6DCAD}Prefabs/Characters/Factions/CIV/Character_CIV_baseLoadout.et";
	protected static const ResourceName PLACE_PREFAB = "{25C8B889A777399D}Prefabs/Props/Infrastructure/ConeTraffic/ConeTraffic_01_red.et";
	protected static const ResourceName ANCHOR_PREFAB = "{CCED18E36D627B1D}Prefabs/Systems/Residents/LF_ResidentAnchor.et";
	protected static const string SHOP_KIND = "test_residents_workapply_shop";
	protected static const int WORK_MINUTE = 450;	// 07:30

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "residents/work-apply";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		bool known;
		ctx.EqualStr("hammer", LF_ResidentActivity.Normalise(" Hammer ", known), "a work word is trimmed and lower-cased");
		ctx.True(known, "hammer is a known activity");

		LF_ResidentWorkLoop.ResetStatics();
		LF_ResidentActivity.ResetStatics();
		IEntity worker = LF_TestCharacterFixture.Spawn(ctx);
		if (worker)
			RunApply(ctx, worker);

		LF_ResidentWorkLoop.ResetStatics();
		LF_ResidentActivity.ResetStatics();
		LF_TestCharacterFixture.Delete(worker);

		RunSpawned(ctx);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunApply(LF_TestContext ctx, IEntity worker)
	{
		LF_ResidentSpot spot = new LF_ResidentSpot();
		spot.m_vOrigin = worker.GetOrigin();
		spot.m_vAngles = "90 0 0";

		string did;
		if (!RedProof())
			did = LF_ResidentActivity.Apply(worker, "hammer", spot);

		ctx.EqualStr("hammer", did, "the activity says hammer");
		ctx.True(LF_ResidentWorkLoop.IsRunning(worker), "a hammer line starts the work loop");
		ctx.Equal(0, LF_ResidentWorkLoop.StepOf(worker), "the hammer loop starts on step 0");

		// The same line again keeps one loop; a different work word swaps it.
		LF_ResidentActivity.Apply(worker, "hammer", spot);
		ctx.Equal(1, LF_ResidentWorkLoop.Count(), "the same work line keeps one loop");
		LF_ResidentActivity.Apply(worker, "sweep", spot);
		ctx.True(LF_ResidentWorkLoop.IsRunning(worker), "a sweep line takes over the loop");
		ctx.Equal(1, LF_ResidentWorkLoop.Count(), "a new work word replaces the old loop");

		// A later line that is not work ends it.
		ctx.EqualStr("serve", LF_ResidentActivity.Apply(worker, "serve", spot), "a serve line follows the work");
		ctx.False(LF_ResidentWorkLoop.IsRunning(worker), "a line that is not work ends the loop");

		// Release (a walk, a despawn) ends it too.
		LF_ResidentActivity.Apply(worker, "carry", spot);
		ctx.True(LF_ResidentWorkLoop.IsRunning(worker), "a carry line starts the loop");
		LF_ResidentActivity.Release(worker);
		ctx.False(LF_ResidentWorkLoop.IsRunning(worker), "Release ends the loop");

		// A static body has no controller: it stands, with one warning however often.
		IEntity mannequin = LF_Utils.SpawnEntityPrefab(MANNEQUIN, worker.GetOrigin() + "0 0 4", "0 0 0", false);
		ctx.NotNull(mannequin, "a static NPC body spawns");
		if (mannequin)
		{
			int before = LF_ResidentActivity.WarnCount();
			ctx.EqualStr("idle", LF_ResidentActivity.Apply(mannequin, "carry", spot), "a static body cannot work and idles");
			LF_ResidentActivity.Apply(mannequin, "carry", spot);
			ctx.False(LF_ResidentWorkLoop.IsRunning(mannequin), "a static body has no loop");
			ctx.Equal(before + 1, LF_ResidentActivity.WarnCount(), "a static body warns once");
			SCR_EntityHelper.DeleteEntityAndChildren(mannequin);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The spawner applies the line: a resident due to hammer is working once spawned, and not once despawned.
	protected void RunSpawned(LF_TestContext ctx)
	{
		IEntity vault = LF_Places.Find(LF_Bank.PLACE_KIND, LF_TestAnnex.BANK_VAULT);
		ctx.NotNull(vault, "the annex vault is placed, to spawn the shop near");
		if (!vault)
			return;

		vector shopAt = vault.GetOrigin() + "0 -20 0";
		IEntity shop = LF_Utils.SpawnEntityPrefab(PLACE_PREFAB, shopAt, "0 0 0", false);
		IEntity bench = LF_Utils.SpawnEntityPrefab(ANCHOR_PREFAB, shopAt + "3 0 4", "90 0 0", false);
		ctx.True(shop && bench, "the shop and its work anchor spawn");
		if (!shop || !bench)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(shop);
			SCR_EntityHelper.DeleteEntityAndChildren(bench);
			return;
		}

		LF_ResidentAnchorComponent comp = LF_ResidentAnchorComponent.Cast(bench.FindComponent(LF_ResidentAnchorComponent));
		if (comp)
			comp.SetAnchorId("bench");

		ctx.True(LF_Places.Register(SHOP_KIND, "workapply_shop", shop), "the shop is placed");

		LF_ResidentDef def = LF_ResidentDef.Make("aaa_builder", SHOP_KIND, SHOP_KIND);
		def.m_sPrefab = CHARACTER;
		def.Add("06:00", "work", "bench", "hammer");
		LF_ResidentsConfig config = new LF_ResidentsConfig();
		config.m_aResidents = {};
		config.m_aResidents.Insert(def);
		LF_ResidentsConfig.UseForTest(config);
		LF_ResidentTimetable.Load(config);
		LF_ResidentSpawner.ResetStatics();
		LF_ResidentWorkLoop.ResetStatics();

		LF_Test_ResidentSpawner_Players players = new LF_Test_ResidentSpawner_Players();
		players.m_aAt.Insert(shopAt + "30 0 0");
		LF_ResidentSpawner.SweepAt(WORK_MINUTE, players);
		IEntity builder = LF_ResidentSpawner.GetLive("aaa_builder");
		ctx.NotNull(builder, "the hammering resident spawns");
		if (builder)
			ctx.True(LF_ResidentWorkLoop.IsRunning(builder), "the spawned resident is working");

		// Everyone leaves: the sweep despawns the resident and the loop goes with them.
		LF_Test_ResidentSpawner_Players nobody = new LF_Test_ResidentSpawner_Players();
		nobody.m_aAt.Insert(shopAt + "900 0 0");
		LF_ResidentSpawner.SweepAt(WORK_MINUTE, nobody);
		ctx.True(LF_ResidentSpawner.GetLive("aaa_builder") == null, "the resident despawns when nobody is near");
		ctx.Equal(0, LF_ResidentWorkLoop.Count(), "a despawned resident leaves no work loop");

		LF_ResidentSpawner.DespawnAll();
		LF_ResidentSpawner.ResetStatics();
		LF_ResidentWorkLoop.ResetStatics();
		LF_ResidentsConfig.Reload();
		LF_ResidentTimetable.ResetStatics();
		SCR_EntityHelper.DeleteEntityAndChildren(shop);
		SCR_EntityHelper.DeleteEntityAndChildren(bench);
	}
}
