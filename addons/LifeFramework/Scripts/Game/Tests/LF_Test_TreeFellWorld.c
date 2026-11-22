// red-proof: 2026-11-19, `--only LF_Test_TreeFellWorld` before the change: LF_FellAction, LF_FelledTrees and LF_OnTreeFelled did not exist, so the test could not compile. `--red LF_Test_TreeFellWorld` skips the first fell, so the hidden, ledger, logs, XP and already-felled checks fail. 2026-11-22: the far-side log check fails on the old ring drop (half its logs lay on the feller side), passes with LF_TreeFall.
//! lumber/tree-fell-world (#3260): a character with an axe fells a real vanilla tree. The tree is hidden
//! and in the ledger, its logs lie on the ground along the line it fell, away from the feller, the LUMBERJACK XP rose, a second fell is
//! refused ALREADY_FELLED, and a fell without the axe is refused NO_AXE with nothing changed.
class LF_TestTreeFellRecorder : LF_EventSubscriber
{
	ref array<ref LF_OnTreeFelled> m_aSeen = {};

	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnTreeFelled felled = LF_OnTreeFelled.Cast(busEvent);
		if (felled)
			m_aSeen.Insert(felled);
		return true;
	}
}

[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_TreeFellWorld : LF_Test
{
	protected static const ResourceName PINE = "Prefabs/Vegetation/Tree/t_pinus_sylvestris/t_pinus_sylvestris_2f.et";
	protected static const string UID = "lf-test-tree-fell";
	//! Metres between candidate spots, and how far out the search goes.
	protected static const float SEARCH_STEP_M = 60;
	protected static const int SEARCH_RINGS = 8;

	override string GetName()
	{
		return "lumber/tree-fell-world";
	}

	//------------------------------------------------------------------------------------------------
	//! A ground point with no building, road or safe zone near it, searched outward from `centre`.
	protected bool FindOpenSpot(vector centre, out vector spot)
	{
		LF_TreeFellingConfig config = LF_TreeFellingConfig.Get();
		BaseWorld world = GetGame().GetWorld();
		for (int ring = 0; ring <= SEARCH_RINGS; ring++)
		{
			int points = 1;
			if (ring > 0)
				points = 8;

			for (int i = 0; i < points; i++)
			{
				float angle = i * 45 * Math.DEG2RAD;
				vector at = centre + Vector(Math.Sin(angle) * ring * SEARCH_STEP_M, 0, Math.Cos(angle) * ring * SEARCH_STEP_M);
				at[1] = world.GetSurfaceY(at[0], at[2]);
				if (at[1] <= world.GetOceanBaseHeight() + 1)
					continue;

				if (LF_Zones.IsSafe(at))
					continue;

				if (LF_FelledTrees.BuildingDistance(at) >= 0 && LF_FelledTrees.BuildingDistance(at) < config.m_fBuildingClearM + 10)
					continue;

				if (LF_FelledTrees.RoadDistance(at, config.m_fRoadClearM + 6) >= 0)
					continue;

				spot = at;
				return true;
			}
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_FelledTrees.ResetStatics();
		LF_JobLevelSources.EnsureSubscribed();
		LF_TestTreeFellRecorder recorder = new LF_TestTreeFellRecorder();
		LF_EventBus.Subscribe(LF_OnTreeFelled, recorder);

		IEntity character = LF_TestCharacterFixture.Spawn(ctx);
		IEntity tree;
		IEntity bystander;
		array<IEntity> logs = {};
		if (character)
		{
			LF_Utils.SetUidForTests(character, UID);
			RunChecks(ctx, character, recorder, tree, bystander, logs);
		}

		foreach (IEntity log : logs)
		{
			LF_TestCharacterFixture.Delete(log);
		}

		LF_TreeFall.ClearLocal();
		LF_TreeFall.ResetStatics();
		LF_TestCharacterFixture.Delete(tree);
		LF_TestCharacterFixture.Delete(bystander);
		LF_TestCharacterFixture.Delete(character);
		LF_Utils.SetUidForTests(null, "");
		LF_EventBus.Unsubscribe(LF_OnTreeFelled, recorder);
		LF_FelledTrees.ResetStatics();
	}

	//------------------------------------------------------------------------------------------------
	protected void RunChecks(LF_TestContext ctx, IEntity character, LF_TestTreeFellRecorder recorder, out IEntity tree, out IEntity bystander, out array<IEntity> logs)
	{
		LF_TreeFellingConfig config = LF_TreeFellingConfig.Get();
		ctx.NotNull(config, "the tree species config loads");
		if (!config)
			return;

		vector spot;
		if (!FindOpenSpot(character.GetOrigin(), spot))
		{
			ctx.Fail("no open ground (no building, road or safe zone) within reach of the spawn to fell on");
			return;
		}

		tree = LF_Utils.SpawnEntityPrefab(PINE, spot);
		bystander = LF_Utils.SpawnEntityPrefab(PINE, spot + "4 0 0");
		ctx.NotNull(tree, "the pine spawns");
		ctx.NotNull(bystander, "the second pine spawns");
		if (!tree || !bystander)
			return;

		character.SetOrigin(spot + "1.5 0 0");
		string key = LF_FellAction.KeyOfTree(tree);
		ctx.False(key.IsEmpty(), "the tree has a ledger key");
		ctx.False(LF_FellAction.HoldsAxe(character), "an empty-handed character does not hold the axe");
		int xpBefore = LF_JobLevels.XpOf(UID, "LUMBERJACK");

		// A fell without the axe is refused and changes nothing.
		array<IEntity> none = {};
		ctx.Equal(LF_FellRefusal.NO_AXE, LF_FellAction.TryFell(character, bystander, false, none), "no axe: refused NO_AXE");
		ctx.False(LF_FelledTrees.IsHidden(bystander), "a refused fell leaves the tree standing");
		ctx.Equal(0, LF_FelledTrees.Count(), "a refused fell writes nothing to the ledger");

		// The fell.
		if (!RedProof())
			ctx.Equal(LF_FellRefusal.OK, LF_FellAction.TryFell(character, tree, true, logs, 0), "the fell is accepted");

		ctx.True(LF_FelledTrees.IsHidden(tree), "the felled tree is hidden");
		ctx.True(LF_FelledTrees.IsFelled(key), "the felled tree is in the ledger");
		ctx.Equal(1, LF_FelledTrees.Count(), "one tree is down");
		ctx.Equal(2, logs.Count(), "a size 2 pine drops 2 logs");
		// The tree fell away from the character (standing at +x): every log lies on the far side, along the line, on the ground.
		float height = LF_TreeFall.HeightOf(tree);
		vector stump = tree.GetOrigin();
		foreach (IEntity log : logs)
		{
			vector at = log.GetOrigin();
			ctx.True(at[0] < stump[0], "a log lies on the far side of the stump from the feller");
			ctx.True(vector.DistanceXZ(at, stump) <= height + 1, "a log lies within the tree's height of the stump");
			ctx.InRange(at[1] - LF_TreeFall.GroundY(at), -0.6, 0.8, "a log rests on the ground, neither floating nor sunk");
		}

		ctx.True(LF_JobLevels.XpOf(UID, "LUMBERJACK") > xpBefore, "the fell paid LUMBERJACK XP");
		ctx.Equal(1, recorder.m_aSeen.Count(), "the fell published one LF_OnTreeFelled");
		if (recorder.m_aSeen.Count() > 0)
		{
			ctx.EqualStr(key, recorder.m_aSeen[0].m_sTreeKey, "the event names the tree key");
			ctx.Equal(2, recorder.m_aSeen[0].m_iLogs, "the event carries the log count");
		}

		// A second fell of the same tree is refused.
		ctx.Equal(LF_FellRefusal.ALREADY_FELLED, LF_FellAction.TryFell(character, tree, true, none), "a second fell: refused ALREADY_FELLED");
		ctx.Equal(1, LF_FelledTrees.Count(), "a repeat fell does not add to the ledger");
		ctx.Equal(0, none.Count(), "a refused fell drops no logs");
	}
}
