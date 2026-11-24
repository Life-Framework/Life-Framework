// red-proof: 2026-11-22 `tools/cli.mjs test --no-build --red LF_Test_NextActivities,LF_Test_NextActivitiesWorld,LF_Test_EveronActivities`: RED-OK everon/activities (fails with the hook on: RedProof() skips loading the table).
//! everon/activities: under the Everon 1989 header (the pack leg) the "What to do next" table is the pack's: the
//! seven activities (and the four island starters, #4709) load in order, every one stands (LF_ActivityConfig.Problem), every step names a target, every
//! item a check or an objective names loads, and the mining starter is the maintainer's route: miner job, pickaxe,
//! car, the mine, ore, smelt, sell. Its quests are registered with the quest manager, and a new civilian is offered
//! the five civilian activities and the crew starter but not police or medic duty or the gated starters (#4771).
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronActivities : LF_Test
{
	override string GetName()
	{
		return "everon/activities";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_ActivityTable.ResetStatics();
		LF_ActivityConfig config;
		if (!RedProof())
			config = LF_ActivityTable.Get();

		ctx.NotNull(config, "the activity table loads");
		if (!config)
			return;

		string ids = string.Empty;
		foreach (LF_ActivityDef def : config.m_aActivities)
		{
			if (!ids.IsEmpty())
				ids += ",";

			ids += def.m_sId;
			ctx.EqualStr(string.Empty, LF_ActivityConfig.Problem(def), "the activity stands: " + def.m_sId);
			ctx.True(def.m_iPayPerHour > 0, "the activity says what it pays: " + def.m_sId);
			ctx.False(def.m_sPlaceKind.IsEmpty(), "the activity has a place for its distance: " + def.m_sId);
			foreach (LF_ActivityStep step : def.m_aSteps)
			{
				string where = def.m_sId + "/" + step.m_sStepId;
				ctx.False(step.m_sTargetKind.IsEmpty(), "the step has a waypoint target: " + where);
				ctx.False(step.m_sNoteKey.IsEmpty(), "the step has a note: " + where);
				if (step.m_eCheck == LF_EActivityCheck.ITEM)
					ctx.True(Resource.Load(step.m_sCheckParam).IsValid(), "the step's item loads: " + where);

				foreach (LF_QuestObjective objective : step.m_aObjectives)
				{
					if (objective.m_eType == LF_EQuestEventType.PROCESS || objective.m_eType == LF_EQuestEventType.GATHER)
						ctx.True(Resource.Load(objective.m_sFilter).IsValid(), "the step's objective item loads: " + where);
				}
			}
		}

		ctx.EqualStr("mining,postal,lumber,fishing,apples,police,medic,crew,truckcheck,perkpick,rumor", ids, "the pack lists its seven activities in order, then the four island starters (#4709)");

		LF_ActivityDef mining = config.Find("mining");
		ctx.NotNull(mining, "mining is listed");
		if (mining)
		{
			string steps = string.Empty;
			foreach (LF_ActivityStep miningStep : mining.m_aSteps)
			{
				if (!steps.IsEmpty())
					steps += ",";

				steps += miningStep.m_sStepId;
			}

			ctx.EqualStr("job,pickaxe,car,drive,ore,smelt,sell", steps, "the mining starter is the maintainer's route");
			ctx.Equal(LF_EActivityCheck.JOB, mining.m_aSteps[0].m_eCheck, "step one checks the miner job");
			ctx.EqualStr("job", mining.m_aSteps[3].m_sTargetKind, "the mine is the nearest miner workplace, found by its job");
			ctx.EqualStr("MINER", mining.m_aSteps[3].m_sTargetId, "of the MINER job");
		}

		ctx.True(LF_ActivityTable.EnsureRegistered(), "the activity quests are registered");
		ctx.NotNull(LF_QuestManager.GetInstance().GetDefinition(LF_ActivityTable.QuestIdOf("mining", "sell")), "the last mining step is a quest");

		array<LF_ActivityDef> listed = {};
		LF_ActivityRules.Listed(config.m_aActivities, new LF_ActivityFacts(), listed);
		ctx.Equal(6, listed.Count(), "a new civilian is offered the five civilian activities and the crew starter, not police or medic duty, and not the gated starters (#4771)");
	}
}
