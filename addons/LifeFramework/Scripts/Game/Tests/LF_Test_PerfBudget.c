// red-proof: 2026-11-14, load tolerance: with `m_iMaxMoneyEventUs 5` (a 10x regression against the ~50 us measured) the run failed `perf money-event-us 85 us/event over budget 11 (limit 5, load scale 231 pct)`; the calibration loop had scaled the limit for the loaded box and the regression still failed. Restored to 200; green. Earlier: Observed 2026-11-11: with `m_iMaxMoneyEventUs 1` in Configs/Perf/LF_PerfBudget.conf, `tools\cli perf --local` failed `perf money-event-us 33 us/event over budget 1` and exited 1. Restored to 200; green.
// tier: PERSISTENCE
//! perf/budget-128 (#335): the server cost gate. Builds a synthetic population (accounts, bank
//! balances, a crime history, a money log) of `m_iPlayers` characters through the public
//! managers and the bus, times the save write and read through the real serializer, and fails
//! when a cost passes its limit in Configs/Perf/LF_PerfBudget.conf. Prints one
//! `[LFDebug:Perf] BENCH <name> value=<n> budget=<n>` line per measure; `tools\cli perf` reads
//! those into the report.
//!
//! What it cannot show: network cost (replication, RPC fan-out), real character entities and
//! per-frame ticks over live players. Those are counted by LF_Perf on a real server and read
//! in a multi-client run (docs/architecture/performance.md).
class LF_Test_PerfBudget : LF_Test
{
	protected static const string UID_PREFIX = "perf-bench-";

	//! Calibration: a fixed CPU loop timed on this run. The budgets were set on an idle box; when the loop
	//! takes longer than CALIBRATION_REF_MS the box is loaded (other sessions, builds) and every time limit
	//! scales by the same ratio, capped at CALIBRATION_MAX_PCT. A real regression slows the code under test
	//! but not this loop, so the ratio stays 1 and the unscaled budget catches it.
	protected static const int CALIBRATION_REF_MS = 16;
	protected static const int CALIBRATION_MAX_PCT = 400;
	protected int m_iScalePct = 100;

	override string GetName()
	{
		return "perf/budget-128";
	}

	//------------------------------------------------------------------------------------------------
	//! One measure against one limit. The BENCH line prints even when it passes.
	protected void Check(LF_TestContext ctx, string name, int value, int limit, string unit)
	{
		LF_Debug.Info(LF_Perf.FEATURE, string.Format("BENCH %1 value=%2 budget=%3 unit=%4", name, value, limit, unit));
		int scaled = limit;
		if (unit != "bytes" && unit != "bytes/player")
			scaled = (limit * m_iScalePct) / 100;

		ctx.True(value <= scaled, string.Format("perf %1 %2 %3 over budget %4 (limit %5, load scale %6 pct)", name, value, unit, scaled, limit, m_iScalePct));
	}

	//------------------------------------------------------------------------------------------------
	protected void Report(string name, int value, string unit)
	{
		LF_Debug.Info(LF_Perf.FEATURE, string.Format("BENCH %1 value=%2 budget=none unit=%3", name, value, unit));
	}

	//------------------------------------------------------------------------------------------------
	//! Best of three timings of a fixed loop; returns the load scale in percent (100 = idle box).
	protected static int Calibrate()
	{
		int best = 0;
		for (int run = 0; run < 3; run++)
		{
			int t0 = System.GetTickCount();
			int acc = 0;
			array<int> sink = new array<int>();
			for (int i = 0; i < 400000; i++)
			{
				acc = (acc + i * 7) % 100003;
				if ((i % 64) == 0)
					sink.Insert(acc);
			}
			int ms = System.GetTickCount(t0);
			if (run == 0 || ms < best)
				best = ms;
		}

		int pct = (best * 100) / CALIBRATION_REF_MS;
		if (pct < 100)
			pct = 100;
		if (pct > CALIBRATION_MAX_PCT)
			pct = CALIBRATION_MAX_PCT;

		LF_Debug.Info(LF_Perf.FEATURE, string.Format("BENCH calibration-ms value=%1 budget=none unit=ms scalePct=%2", best, pct));
		return pct;
	}

	//------------------------------------------------------------------------------------------------
	protected static int PerOpUs(int ms, int ops)
	{
		if (ops <= 0)
			return 0;

		return (ms * 1000) / ops;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		ctx.NotNull(gameMode, "the DebugWorld game mode resolves");
		if (!gameMode)
			return;

		LF_PersistenceComponent persistence = LF_PersistenceComponent.Cast(gameMode.FindComponent(LF_PersistenceComponent));
		ctx.NotNull(persistence, "the game mode carries LF_PersistenceComponent");
		if (!persistence)
			return;

		m_iScalePct = Calibrate();
		LF_PerfBudget budget = LF_PerfBudget.Get();
		int players = budget.m_iPlayers;
		ctx.True(players >= 128, "the budget is set for at least 128 players");

		// Keep what the session holds, through the real save path: account records alone lose the character
		// slots and standings (#331), which the e2e save boot seeds before this runs and saves at shutdown.
		JsonSaveContext beforeContext = new JsonSaveContext(false);
		LF_PersistenceComponentSerializer.SerializeStore(persistence, beforeContext);
		string before = beforeContext.SaveToString();

		LF_Perf.SetEnabled(true);
		LF_Perf.Reset();
		LF_Ledger.EnsureSubscribed();
		LF_CrimeRecord.EnsureSubscribed();
		LF_IncidentLog.Install();
		ctx.True(LF_EventBus.SubscriberCount(LF_OnMoneyMoved) > 0, "the ledger listens for money");

		LF_PlayerAccountManager.Reset();
		LF_ATMManager.Reset();
		LF_Ledger.ApplyAll(null);
		LF_CrimeRecord.ApplyAll(null);
		LF_IncidentLog.ApplyAll(null);

		// The population: one account with one character, a faction, a wanted level and a bank
		// balance each.
		int t = System.GetTickCount();
		int index;
		for (index = 0; index < players; index++)
		{
			string uid = UID_PREFIX + index.ToString();
			LF_PlayerAccount account = LF_PlayerAccountManager.GetOrCreate(uid);
			LF_PlayerCharacter character = LF_PlayerCharacter.Create("Prefabs/Characters/Core/Character_Roleplay.et", "Perf", "Bench" + index.ToString(), 20 + (index % 40));
			account.AddCharacter(character, true);
			account.SetWantedLevel(index % 6);
			LF_ATMManager.GetOrCreate(character.GetId()).SetBalance(1000 + index);
		}
		Report("populate-ms", System.GetTickCount(t), "ms");
		ctx.Equal(players, LF_PlayerAccountManager.ExportAll().Count(), "every synthetic account is resident");

		// Crime history through the bus (the record and the incident log both listen).
		t = System.GetTickCount();
		int crimeOps = 0;
		for (index = 0; index < players; index++)
		{
			for (int c = 0; c < budget.m_iCrimeEventsPerPlayer; c++)
			{
				LF_EventBus.Publish(LF_OnCrimeCommitted.Create(UID_PREFIX + index.ToString(), LF_WantedReports.OFFENCE_ROBBERY));
				crimeOps++;
			}
		}
		Check(ctx, "crime-event-us", PerOpUs(System.GetTickCount(t), crimeOps), budget.m_iMaxCrimeEventUs, "us/event");

		// Money log, below the cap: every subscriber (ledger, incident log) runs.
		t = System.GetTickCount();
		int moneyOps = 0;
		for (index = 0; index < players; index++)
		{
			int balance = 1000 + index;
			for (int m = 0; m < budget.m_iMoneyEventsPerPlayer; m++)
			{
				bool credit = (m % 2) == 0;
				if (credit)
					balance += 50;
				else
					balance -= 50;

				LF_EventBus.Publish(LF_OnMoneyMoved.CreateFull(UID_PREFIX + index.ToString(), LF_Ledger.ACCOUNT_BANK, 50, credit, "perf-bench", balance, ""));
				moneyOps++;
			}
		}
		Check(ctx, "money-event-us", PerOpUs(System.GetTickCount(t), moneyOps), budget.m_iMaxMoneyEventUs, "us/event");
		Report("ledger-rows", LF_Ledger.Count(), "rows");
		Report("incident-rows", LF_IncidentLog.Count(), "rows");

		// What one character costs in the save: the same store with the two shared logs emptied.
		LF_Ledger.ApplyAll(null);
		LF_IncidentLog.ApplyAll(null);
		JsonSaveContext baseContext = new JsonSaveContext(false);
		LF_PersistenceComponentSerializer.SerializeStore(persistence, baseContext);
		int baseBytes = baseContext.SaveToString().Length();
		Check(ctx, "save-bytes-per-player", baseBytes / players, budget.m_iMaxSaveBytesPerPlayer, "bytes/player");

		// Money log at its retention cap: the steady state of a busy server. Fill it with rows
		// of the same shape, then publish more.
		int cap = LF_LedgerConfig.MaxEntries();
		array<ref LF_LedgerEntry> fill = new array<ref LF_LedgerEntry>();
		int now = System.GetUnixTime();
		for (index = 0; index < cap; index++)
		{
			LF_LedgerEntry row = new LF_LedgerEntry();
			row.m_iId = index + 1;
			row.m_iTime = now - 60;
			row.m_sPlayerUid = UID_PREFIX + (index % players).ToString();
			row.m_sAccount = LF_Ledger.ACCOUNT_BANK;
			row.m_iDelta = 50;
			row.m_sReason = "perf-bench";
			row.m_iBalanceAfter = 1000;
			fill.Insert(row);
		}
		LF_Ledger.ApplyAll(fill);

		int capOps = 2000;
		t = System.GetTickCount();
		for (index = 0; index < capOps; index++)
		{
			LF_EventBus.Publish(LF_OnMoneyMoved.CreateFull(UID_PREFIX + (index % players).ToString(), LF_Ledger.ACCOUNT_BANK, 50, true, "perf-bench", 1050, ""));
		}
		Check(ctx, "money-event-at-cap-us", PerOpUs(System.GetTickCount(t), capOps), budget.m_iMaxMoneyEventAtCapUs, "us/event");
		Report("ledger-rows-at-cap", LF_Ledger.Count(), "rows");

		// One query per character against the full ledger.
		array<ref LF_LedgerEntry> found = new array<ref LF_LedgerEntry>();
		t = System.GetTickCount();
		for (index = 0; index < players; index++)
		{
			LF_Ledger.Last(UID_PREFIX + index.ToString(), LF_Ledger.ACCOUNT_BANK, 20, found);
		}
		Check(ctx, "ledger-query-all-ms", System.GetTickCount(t), budget.m_iMaxLedgerQueryAllMs, "ms");

		// The save: export, then the store write and read through the real serializer.
		t = System.GetTickCount();
		array<ref LF_PlayerAccountRecord> exported = LF_PlayerAccountManager.ExportAll();
		Check(ctx, "account-export-ms", System.GetTickCount(t), budget.m_iMaxAccountExportMs, "ms");
		ctx.Equal(players, exported.Count(), "the export carries every account");

		JsonSaveContext saveContext = new JsonSaveContext(false);
		t = System.GetTickCount();
		ESerializeResult written = LF_PersistenceComponentSerializer.SerializeStore(persistence, saveContext);
		int writeMs = System.GetTickCount(t);
		ctx.True(written == ESerializeResult.OK, "the store serializes");
		string saved = saveContext.SaveToString();
		Check(ctx, "save-write-ms", writeMs, budget.m_iMaxSaveWriteMs, "ms");
		Check(ctx, "save-bytes-at-cap", saved.Length(), budget.m_iMaxSaveBytesAtCap, "bytes");

		JsonLoadContext loadContext = new JsonLoadContext(false);
		t = System.GetTickCount();
		bool parsed = loadContext.LoadFromString(saved);
		int parseMs = System.GetTickCount(t);
		ctx.True(parsed, "the save parses");
		Report("save-parse-ms", parseMs, "ms");

		t = System.GetTickCount();
		ctx.True(LF_PersistenceComponentSerializer.DeserializeStore(persistence, loadContext), "the store reads");
		Check(ctx, "save-read-ms", System.GetTickCount(t), budget.m_iMaxSaveReadMs, "ms");
		ctx.Equal(players, LF_PlayerAccountManager.ExportAll().Count(), "the read leaves every account in place");

		LF_Perf.Report();

		// Put the session back.
		LF_PlayerAccountManager.Reset();
		LF_ATMManager.Reset();
		JsonLoadContext beforeLoad = new JsonLoadContext(false);
		ctx.True(beforeLoad.LoadFromString(before) && LF_PersistenceComponentSerializer.DeserializeStore(persistence, beforeLoad), "the session is put back");
		LF_Perf.Reset();
		LF_Perf.SetEnabled(false);
	}
}
