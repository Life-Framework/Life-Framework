// red-proof: 2026-11-18, before LF_ResidentActivity existed the test did not compile; with the act skipped
//   (RedProof() on, `cli test --no-build --red LF_Test_ResidentActivity`) nobody sits, faces the anchor or
//   warns, so `a resident on a bench anchor sits`, `a resident on a counter faces the anchor's facing` and
//   `an unknown activity warns once` fail. The run that went red and then green is in the PR.
//! residents/activity (#2907): a resident does what their timetable line says. On a bench anchor with a
//! free seat beside it, `sit` seats them (the seat and pose a player's Sit uses); on a counter anchor
//! `serve` turns them to the anchor's facing and they stay on their feet; an unknown activity idles with
//! one warning however often it comes; `sit` with no seat near idles with one warning; and the spawner
//! applies the line when it spawns a resident (a counter resident faces the anchor).
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_ResidentActivity : LF_Test
{
	protected static const ResourceName CHAIR = "{69D6D9BFDFEC3225}Prefabs/Props/Seats/LF_Seat_Chair_02_green.et";
	protected static const ResourceName MANNEQUIN = "{6E8EF026940F3841}Prefabs/Characters/Npcs/Npc_Civilian_01.et";
	protected static const ResourceName CHARACTER = "{9B443DD4C1F6DCAD}Prefabs/Characters/Factions/CIV/Character_CIV_baseLoadout.et";
	protected static const ResourceName PLACE_PREFAB = "{25C8B889A777399D}Prefabs/Props/Infrastructure/ConeTraffic/ConeTraffic_01_red.et";
	protected static const ResourceName ANCHOR_PREFAB = "{CCED18E36D627B1D}Prefabs/Systems/Residents/LF_ResidentAnchor.et";
	protected static const string SHOP_KIND = "test_residents_activity_shop";
	protected static const int WORK_MINUTE = 450;	// 07:30

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "residents/activity";
	}

	//------------------------------------------------------------------------------------------------
	protected static float YawGap(float a, float b)
	{
		float gap = Math.AbsFloat(a - b);
		while (gap > 360)
			gap -= 360;

		if (gap > 180)
			gap = 360 - gap;

		return gap;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// The rule: names are normalised, empty and unknown ones idle.
		bool known;
		ctx.EqualStr("sit", LF_ResidentActivity.Normalise(" Sit ", known), "a name is trimmed and lower-cased");
		ctx.True(known, "sit is a known activity");
		ctx.EqualStr("idle", LF_ResidentActivity.Normalise("", known), "an empty activity is idle");
		ctx.True(known, "an empty activity is not an unknown one");
		ctx.EqualStr("idle", LF_ResidentActivity.Normalise("moonwalk", known), "an unknown activity is idle");
		ctx.False(known, "an unknown activity reads unknown");

		LF_NpcLoiter.ResetStatics();
		LF_ResidentActivity.ResetStatics();
		IEntity npc = LF_TestCharacterFixture.Spawn(ctx);
		IEntity other = LF_TestCharacterFixture.Spawn(ctx);
		if (npc && other)
			RunActivities(ctx, npc, other);

		LF_NpcLoiter.ResetStatics();
		LF_ResidentActivity.ResetStatics();
		LF_TestCharacterFixture.Delete(npc);
		LF_TestCharacterFixture.Delete(other);

		RunSpawned(ctx);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunActivities(LF_TestContext ctx, IEntity npc, IEntity other)
	{
		LF_ResidentSpot spot = new LF_ResidentSpot();
		spot.m_vOrigin = npc.GetOrigin();
		spot.m_vAngles = "90 0 0";

		// serve: faces the anchor's facing and stays on its feet.
		if (!RedProof())
			LF_ResidentActivity.Apply(npc, "serve", spot);

		ctx.EqualFloat(0, YawGap(LF_ResidentActivity.YawOf(npc), 90), 2, "a resident on a counter faces the anchor's facing");
		ctx.False(LF_ResidentActivity.IsSeated(npc), "a serving resident is not seated");

		// An unknown activity idles and warns once, however often it comes.
		int warnsBefore = LF_ResidentActivity.WarnCount();
		string ended;
		if (!RedProof())
			ended = LF_ResidentActivity.Apply(other, "moonwalk", spot);

		LF_ResidentActivity.Apply(other, "moonwalk", spot);
		ctx.EqualStr("idle", ended, "an unknown activity falls back to idle");
		ctx.Equal(warnsBefore + 1, LF_ResidentActivity.WarnCount(), "an unknown activity warns once");

		// sit with no seat near: idle, one warning.
		string noSeat = LF_ResidentActivity.Apply(other, "sit", spot);
		ctx.EqualStr("idle", noSeat, "sit with no seat near falls back to idle");
		ctx.False(LF_ResidentActivity.IsSeated(other), "no seat, not seated");
		ctx.Equal(warnsBefore + 2, LF_ResidentActivity.WarnCount(), "sit with no seat warns once");

		// A static NPC body cannot sit: it stands and one warning says why.
		IEntity mannequin = LF_Utils.SpawnEntityPrefab(MANNEQUIN, npc.GetOrigin() + "0 0 4", "0 0 0", false);
		ctx.NotNull(mannequin, "a static NPC body spawns");
		if (mannequin)
		{
			int staticBefore = LF_ResidentActivity.WarnCount();
			ctx.EqualStr("idle", LF_ResidentActivity.Apply(mannequin, "sit", spot), "a static NPC body cannot sit and idles");
			ctx.Equal(staticBefore + 1, LF_ResidentActivity.WarnCount(), "a static NPC body warns once");
			SCR_EntityHelper.DeleteEntityAndChildren(mannequin);
		}

		// sit with a free seat beside the anchor: seated.
		Resource res = Resource.Load(CHAIR);
		ctx.True(res.IsValid(), "the chair loads");
		if (!res.IsValid())
			return;

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[0] = "1 0 0";
		params.Transform[1] = "0 1 0";
		params.Transform[2] = "0 0 1";
		params.Transform[3] = npc.GetOrigin() + "1.5 0 0";
		IEntity chair = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
		ctx.NotNull(chair, "the chair spawns beside the resident");
		if (!chair)
			return;

		string sat;
		if (!RedProof())
			sat = LF_ResidentActivity.Apply(npc, "sit", spot);

		ctx.EqualStr("sit", sat, "sit with a seat beside the anchor reports sit");
		ctx.True(LF_ResidentActivity.IsSeated(npc), "a resident on a bench anchor with sit reports seated");
		LF_ResidentActivity.Release(npc);
		ctx.False(LF_ResidentActivity.IsSeated(npc), "releasing stands the resident up");
		SCR_EntityHelper.DeleteEntityAndChildren(chair);
	}

	//------------------------------------------------------------------------------------------------
	//! The spawner applies the activity: a resident due at a counter anchor facing 90 degrees stands facing 90.
	protected void RunSpawned(LF_TestContext ctx)
	{
		IEntity vault = LF_Places.Find(LF_Bank.PLACE_KIND, LF_TestAnnex.BANK_VAULT);
		ctx.NotNull(vault, "the annex vault is placed, to spawn the shop near");
		if (!vault)
			return;

		vector shopAt = vault.GetOrigin() + "0 -20 0";
		IEntity shop = LF_Utils.SpawnEntityPrefab(PLACE_PREFAB, shopAt, "0 0 0", false);
		vector counterAt = shopAt + "3 0 4";
		IEntity counter = LF_Utils.SpawnEntityPrefab(ANCHOR_PREFAB, counterAt, "90 0 0", false);
		ctx.True(shop && counter, "the shop and its counter anchor spawn");
		if (!shop || !counter)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(shop);
			SCR_EntityHelper.DeleteEntityAndChildren(counter);
			return;
		}

		LF_ResidentAnchorComponent comp = LF_ResidentAnchorComponent.Cast(counter.FindComponent(LF_ResidentAnchorComponent));
		if (comp)
			comp.SetAnchorId("counter");

		ctx.True(LF_Places.Register(SHOP_KIND, "activity_shop", shop), "the shop is placed");

		LF_ResidentDef def = LF_ResidentDef.Make("aaa_server", SHOP_KIND, SHOP_KIND);
		def.m_sPrefab = CHARACTER; // a real character, so the role's settle has a body it could move (#2948)
		def.Add("06:00", "work", "counter", "serve");
		LF_ResidentsConfig config = new LF_ResidentsConfig();
		config.m_aResidents = {};
		config.m_aResidents.Insert(def);
		LF_ResidentsConfig.UseForTest(config);
		LF_ResidentTimetable.Load(config);
		LF_ResidentSpawner.ResetStatics();

		LF_Test_ResidentSpawner_Players players = new LF_Test_ResidentSpawner_Players();
		players.m_aAt.Insert(shopAt + "30 0 0");
		LF_ResidentSpawner.SweepAt(WORK_MINUTE, players);
		IEntity server = LF_ResidentSpawner.GetLive("aaa_server");
		ctx.NotNull(server, "the serving resident spawns");
		if (server)
		{
			ctx.EqualFloat(0, YawGap(LF_ResidentActivity.YawOf(server), 90), 2, "the spawned counter resident faces the anchor's facing");

			// A townsfolk-role resident is not settled onto a seat or a lean by the role (#2948): the timetable owns them.
			bool settled = true;
			if (!RedProof())
				settled = LF_NpcLoiter.Settle(server) != LF_ENpcLoiter.NONE;

			ctx.False(settled, "the role's settle leaves a live resident to their timetable line");
			ctx.False(LF_ResidentActivity.IsSeated(server), "a serving resident stays unseated after the role's settle");
		}

		LF_ResidentSpawner.DespawnAll();
		LF_ResidentSpawner.ResetStatics();
		LF_ResidentsConfig.Reload();
		LF_ResidentTimetable.ResetStatics();
		SCR_EntityHelper.DeleteEntityAndChildren(shop);
		SCR_EntityHelper.DeleteEntityAndChildren(counter);
	}
}
