// red-proof: #4869 2026-11-24: RedProof() adds 1000 to the level every real row is checked at, so every row that names a gate (a licence, a machine or a step, a perk pick, a fish, an activity, a delivery job, the export board) fails to find it; the batched `tools\cli test --no-build --red LF_Test_EveronTradeUnlocks` run records the RED-OK line. Earlier: RedProof() swapped the table for an empty one; #4861: the SMELTING step rows are checked against the smelter's steps.
// red-proof: observed 2026-11-23: with the header line removed from LF_Everon1989.conf, `tools\cli test --no-build --only LF_Test_EveronTradeUnlocks` failed `- the header lists the trade unlocks table` and `- LF_TradeUnlocks.Get returns every row under the pack header (expected 261, got 0)`; with the line it passes.
//! #4185, #4869: the flagship names one line for every level 2 to 30 of every trade, so the level-up card
//! and F2 can show it, and the pack header lists the table so Get() returns it. Every row is either a
//! MILESTONE (it opens nothing and carries no reference) or names a gate that really exists at that trade
//! and level: the row's structured kind and reference are looked up in the jobs, machine, fishing, activity,
//! perk and export configs, never matched by text. The other way round, every gate those configs hold for
//! a trade has its row. Each row's key has pack text, and no trade repeats a line.
[LF_TestInfo(LF_TestTier.LOGIC, pack: true)]
class LF_Test_EveronTradeUnlocks : LF_Test
{
	static const ResourceName CONFIG_PATH = "{2AD4170BBC219196}Configs/Jobs/LF_TradeUnlocks_Everon1989.conf";
	static const string KEY_PREFIX = "LF-Unlock_";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/trade-unlocks";
	}

	//------------------------------------------------------------------------------------------------
	//! \return The machine ids whose level gate or steps a trade row can name (the Smelting work).
	protected void MachineIds(notnull array<string> into)
	{
		into.Insert("smelter-iron");
		into.Insert("cement-works");
	}

	//------------------------------------------------------------------------------------------------
	//! \return "" when the row's gate exists in the config at the row's trade and level, else what is missing.
	//! \param shift 0; the red proof adds 1000 so no real gate matches.
	protected string Backs(LF_TradeUnlockRow row, int shift)
	{
		int level = row.m_iLevel + shift;
		string gateRef = row.m_sRef;
		LF_ETradeUnlockKind kind = row.m_eKind;

		if (kind == LF_ETradeUnlockKind.MILESTONE)
		{
			if (!gateRef.IsEmpty())
				return "a milestone names no reference, got " + gateRef;
			return "";
		}

		if (kind == LF_ETradeUnlockKind.LICENSE)
		{
			LF_JobsConfig jobs = LF_JobsConfig.Get();
			LF_JobLicenceGateDef gate = null;
			if (jobs)
				gate = jobs.FindLicenceGate(gateRef);
			if (!gate)
				return "no licence gate " + gateRef;
			if (gate.m_sJob != row.m_sJob || gate.m_iMinLevel != level)
				return string.Format("licence %1 gates %2 level %3, not this row", gateRef, gate.m_sJob, gate.m_iMinLevel);
			return "";
		}

		if (kind == LF_ETradeUnlockKind.MACHINE || kind == LF_ETradeUnlockKind.MACHINE_STEP)
		{
			LF_ResourceMachineDef machine = LF_ResourceChainCatalog.FindMachine(gateRef);
			if (!machine)
				return "no machine " + gateRef;
			if (machine.m_sLevelJob != row.m_sJob)
				return string.Format("machine %1 gates %2, not %3", gateRef, machine.m_sLevelJob, row.m_sJob);

			if (kind == LF_ETradeUnlockKind.MACHINE)
			{
				if (machine.m_iMinLevel != level)
					return string.Format("machine %1 opens at level %2", gateRef, machine.m_iMinLevel);
				return "";
			}

			if (!machine.m_aSteps)
				return "machine " + gateRef + " has no steps";
			foreach (LF_MachineStep step : machine.m_aSteps)
			{
				if (!step || step.m_iLevel != level)
					continue;

				// The step's number is in the text the player reads: bars a run, else seconds a run.
				int number = step.m_iMaxBatches;
				if (number <= 0)
					number = step.m_iProcessSeconds;

				string text = WidgetManager.Translate(row.m_sTextKey);
				if (!text.Contains(number.ToString()))
					return string.Format("the text names no %1: %2", number, text);
				return "";
			}

			return string.Format("machine %1 has no step at level %2", gateRef, level);
		}

		if (kind == LF_ETradeUnlockKind.PERK_PICK)
		{
			array<string> trades = {};
			LF_PerkPick.Trades(trades);
			if (trades.Find(row.m_sJob) < 0)
				return row.m_sJob + " has no perk pick";

			array<string> pool = {};
			LF_PerkPick.Pool(row.m_sJob, level, pool);
			if (pool.IsEmpty())
				return string.Format("%1 offers no perk at level %2", row.m_sJob, level);
			return "";
		}

		if (kind == LF_ETradeUnlockKind.FISH)
		{
			if (row.m_sJob != "FISHER")
				return "a fish row belongs to FISHER";

			LF_FishingConfig fishing = LF_FishingConfig.Get();
			if (!fishing || !fishing.m_aSpecies)
				return "no fishing config";
			foreach (LF_FishSpeciesDef species : fishing.m_aSpecies)
			{
				if (species && species.m_sId == gateRef)
				{
					if (species.m_iMinFisherLevel != level)
						return string.Format("%1 needs fisher level %2", gateRef, species.m_iMinFisherLevel);
					return "";
				}
			}

			return "no species " + gateRef;
		}

		if (kind == LF_ETradeUnlockKind.ACTIVITY)
		{
			LF_ActivityConfig activities = LF_ActivityTable.Get();
			LF_ActivityDef def = null;
			if (activities)
				def = activities.Find(gateRef);
			if (!def)
				return "no activity " + gateRef;
			if (def.m_sTrade != row.m_sJob || def.m_iMinTradeLevel != level)
				return string.Format("activity %1 needs %2 level %3", gateRef, def.m_sTrade, def.m_iMinTradeLevel);
			return "";
		}

		if (kind == LF_ETradeUnlockKind.DELIVERY_JOB)
		{
			if (row.m_sJob != "DELIVERY")
				return "a delivery job row belongs to DELIVERY";

			int jobValue = typename.StringToEnum(LF_EJobType, gateRef);
			if (jobValue < 0)
				return "no delivery job " + gateRef;
			LF_EJobType jobType = jobValue;
			if (LF_CivilianJob.RequiredLevel(jobType) != level)
				return string.Format("%1 needs level %2", gateRef, LF_CivilianJob.RequiredLevel(jobType));
			return "";
		}

		if (kind == LF_ETradeUnlockKind.EXPORT_ORDERS)
		{
			if (row.m_sJob != "DELIVERY")
				return "the export board belongs to DELIVERY";

			LF_ExportOrderConfig exportConfig = LF_ExportOrderConfig.Get();
			if (!exportConfig || exportConfig.m_iMinLevel != level)
				return "the export board opens at another level";
			return "";
		}

		return "unknown row kind";
	}

	//------------------------------------------------------------------------------------------------
	//! The other way round: a gate the configs hold for a trade has its row at that level.
	protected void CheckGatesHaveRows(LF_TestContext ctx, LF_TradeUnlocksConfig config, notnull array<string> trades)
	{
		LF_JobsConfig jobs = LF_JobsConfig.Get();
		if (jobs && jobs.m_aLicenceGates)
		{
			foreach (LF_JobLicenceGateDef gate : jobs.m_aLicenceGates)
			{
				if (!gate || trades.Find(gate.m_sJob) < 0 || gate.m_iMinLevel < LF_TradeUnlocksConfig.FIRST_LEVEL)
					continue;

				LF_TradeUnlockRow row = config.RowFor(gate.m_sJob, gate.m_iMinLevel);
				ctx.True(row && row.m_eKind == LF_ETradeUnlockKind.LICENSE && row.m_sRef == gate.m_sLicence, string.Format("the %1 licence at %2 level %3 has its row", gate.m_sLicence, gate.m_sJob, gate.m_iMinLevel));
			}
		}

		array<string> machines = {};
		MachineIds(machines);
		foreach (string id : machines)
		{
			LF_ResourceMachineDef machine = LF_ResourceChainCatalog.FindMachine(id);
			if (!machine || trades.Find(machine.m_sLevelJob) < 0)
				continue;

			if (machine.m_iMinLevel >= LF_TradeUnlocksConfig.FIRST_LEVEL)
			{
				LF_TradeUnlockRow gateRow = config.RowFor(machine.m_sLevelJob, machine.m_iMinLevel);
				ctx.True(gateRow && gateRow.m_eKind == LF_ETradeUnlockKind.MACHINE && gateRow.m_sRef == id, string.Format("machine %1 at level %2 has its row", id, machine.m_iMinLevel));
			}

			if (!machine.m_aSteps)
				continue;
			foreach (LF_MachineStep step : machine.m_aSteps)
			{
				LF_TradeUnlockRow stepRow = config.RowFor(machine.m_sLevelJob, step.m_iLevel);
				ctx.True(stepRow && stepRow.m_eKind == LF_ETradeUnlockKind.MACHINE_STEP && stepRow.m_sRef == id, string.Format("the %1 step at level %2 has its row", id, step.m_iLevel));
			}
		}

		LF_FishingConfig fishing = LF_FishingConfig.Get();
		if (fishing && fishing.m_aSpecies)
		{
			foreach (LF_FishSpeciesDef species : fishing.m_aSpecies)
			{
				if (!species || species.m_iMinFisherLevel < LF_TradeUnlocksConfig.FIRST_LEVEL)
					continue;

				LF_TradeUnlockRow fishRow = config.RowFor("FISHER", species.m_iMinFisherLevel);
				ctx.True(fishRow && fishRow.m_eKind == LF_ETradeUnlockKind.FISH && fishRow.m_sRef == species.m_sId, string.Format("%1 at fisher level %2 has its row", species.m_sId, species.m_iMinFisherLevel));
			}
		}

		array<string> pickTrades = {};
		LF_PerkPick.Trades(pickTrades);
		foreach (string pickTrade : pickTrades)
		{
			if (trades.Find(pickTrade) < 0)
				continue;

			LF_TradeUnlockRow first = config.RowFor(pickTrade, LF_PerkPick.LEVEL_FIRST);
			LF_TradeUnlockRow second = config.RowFor(pickTrade, LF_PerkPick.LEVEL_SECOND);
			ctx.True(first && first.m_eKind == LF_ETradeUnlockKind.PERK_PICK, pickTrade + " names its first perk pick");
			ctx.True(second && second.m_eKind == LF_ETradeUnlockKind.PERK_PICK, pickTrade + " names its second perk pick");
		}

		LF_ExportOrderConfig exportConfig = LF_ExportOrderConfig.Get();
		if (exportConfig)
		{
			LF_TradeUnlockRow exportRow = config.RowFor("DELIVERY", exportConfig.m_iMinLevel);
			ctx.True(exportRow && exportRow.m_eKind == LF_ETradeUnlockKind.EXPORT_ORDERS, "the export board has its row");
		}
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		array<string> trades = { "FARMER", "MINER", "SMELTING", "LUMBERJACK", "FISHER", "DELIVERY", "SALVAGE", "POLICE", "MEDIC" };

		LF_TradeUnlocksConfig config = LF_TradeUnlocksConfig.Load(CONFIG_PATH);
		ctx.NotNull(config, "the flagship trade unlock table loads");
		if (!config)
			return;

		// Red proof: every real row is checked at a level that no gate has.
		int shift = 0;
		if (RedProof())
			shift = 1000;

		int milestones = 0;
		foreach (string trade : trades)
		{
			array<string> seen = {};
			for (int level = LF_TradeUnlocksConfig.FIRST_LEVEL; level <= LF_TradeUnlocksConfig.LAST_LEVEL; level++)
			{
				string where = string.Format("%1 level %2", trade, level);
				LF_TradeUnlockRow row = config.RowFor(trade, level);
				ctx.NotNull(row, where + " has an unlock row");
				if (!row)
					continue;

				string key = row.m_sTextKey;
				ctx.True(key.StartsWith("#" + KEY_PREFIX), where + " names an unlock key");
				string text = WidgetManager.Translate(key);
				ctx.True(!text.IsEmpty() && text != key, where + " has pack text for its key");
				ctx.True(seen.Find(text) < 0, where + " does not repeat a line of its trade");
				seen.Insert(text);

				// A row is real (its gate exists at this trade and level) or a milestone that names nothing.
				string problem = Backs(row, shift);
				ctx.True(problem.IsEmpty(), string.Format("%1 (%2) is backed: %3", where, typename.EnumToString(LF_ETradeUnlockKind, row.m_eKind), problem));
				if (row.m_eKind == LF_ETradeUnlockKind.MILESTONE)
					milestones++;
			}

			ctx.True(config.KeyFor(trade, 1).IsEmpty(), trade + " has no row for level 1");
			ctx.True(config.KeyFor(trade, LF_TradeUnlocksConfig.LAST_LEVEL + 1).IsEmpty(), trade + " has no row past level 30");
		}

		int total = trades.Count() * (LF_TradeUnlocksConfig.LAST_LEVEL - LF_TradeUnlocksConfig.FIRST_LEVEL + 1);
		ctx.Equal(total, config.m_aRows.Count(), "the table holds exactly 29 rows per trade");
		ctx.True(milestones > 0 && milestones < total, "the table mixes real rows and milestones");

		CheckGatesHaveRows(ctx, config, trades);

		// The header: the layered table the game reads (LF_TradeUnlocks.Get) holds those rows only when
		// the pack header lists the file (#4185 follow-up: the flagship header left it out and the level-up card was empty).
		LF_MissionHeader header = LF_MissionHeader.GetCurrent();
		ctx.NotNull(header, "the pack leg runs under an LF mission header");
		if (header && header.m_aLF_ConfigOverlays)
			ctx.True(header.m_aLF_ConfigOverlays.Find(CONFIG_PATH) != -1, "the header lists the trade unlocks table");
		else
			ctx.Fail("the header lists no config overlays");

		LF_TradeUnlocksConfig.Reload();
		LF_TradeUnlocksConfig layered = LF_TradeUnlocksConfig.Get();
		ctx.NotNull(layered, "the layered trade unlock table loads");
		if (layered)
			ctx.Equal(config.m_aRows.Count(), layered.m_aRows.Count(), "LF_TradeUnlocks.Get returns every row under the pack header");
		LF_TradeUnlocksConfig.Reload();
	}
}
