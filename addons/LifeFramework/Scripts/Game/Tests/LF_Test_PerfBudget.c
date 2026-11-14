// red-proof: 2026-11-14, reference-op budgets: with the money publish tripled in the measured path (a planted 3x), `perf-budget` failed `perf money-event-us 23.4375 us/event over budget 20.72 (63 ref ops against 56, one ref op 370 ns)` on this PC and `... 124.219 us/event over budget 89.6 (77 ref ops against 56, one ref op 1600 ns)` on lf-cloud. Removed the plant; green on this PC, lf-testbox and lf-cloud. Earlier: Observed 2026-11-11: with `m_iMaxMoneyEventUs 1` the run failed `perf money-event-us 33 us/event over budget 1` and exited 1.
//! One row of the calibration workload in LF_Test_PerfBudget.
class LF_PerfRefItem
{
	string m_sKey;
	int m_iValue;
}

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

	//! Calibration. Every time limit in LF_PerfBudget is a count of reference ops, not microseconds: one
	//! ref op is one turn of a small pure-script loop (allocate a managed row, build a string key, append
	//! to an array, set a map entry), the same kind of work the bus, ledger and serializer do. The loop is
	//! timed on this run, on this machine, right before each measure (best of three), so a slow box or a
	//! busy one stretches the limit by the same ratio it stretches the code under test, and a measure that
	//! needs 3x the budgeted ref ops fails on any machine. The limit has no scale cap and no floor.
	protected static const int REF_OPS = 100000;
	//! Rows the reference loop keeps alive before it drops them.
	protected static const int REF_RETAIN = 1000;
	//! One export is under a millisecond, so time a batch of them.
	protected static const int EXPORT_BATCH = 20;
	protected float m_fRefNs = 250;
	protected bool m_bMuted;

	override string GetName()
	{
		return "perf/budget-128";
	}

	//------------------------------------------------------------------------------------------------
	//! Bytes are deterministic: a plain limit.
	protected void CheckBytes(LF_TestContext ctx, string name, int value, int limit, string unit)
	{
		Say(string.Format("BENCH %1 value=%2 budget=%3 unit=%4", name, value, limit, unit));
		ctx.True(value <= limit, string.Format("perf %1 %2 %3 over budget %4", name, value, unit, limit));
	}

	//------------------------------------------------------------------------------------------------
	//! One measure of time against a limit in ref ops. costUs the measured cost in microseconds
	//! (per event, or per pass for the ms measures); unit "ms" prints milliseconds, anything else microseconds.
	protected void CheckRef(LF_TestContext ctx, string name, float costUs, int maxRef, string unit)
	{
		float used = (costUs * 1000) / m_fRefNs;
		float limitUs = (maxRef * m_fRefNs) / 1000;
		float value = costUs;
		float limit = limitUs;
		if (unit == "ms")
		{
			value = costUs / 1000;
			limit = limitUs / 1000;
		}

		int shownValue = Math.Floor(value);
		int shownLimit = Math.Floor(limit);
		int shownUsed = Math.Floor(used);
		int shownRefNs = Math.Floor(m_fRefNs);
		Say(string.Format("BENCH %1 value=%2 budget=%3 unit=%4 refOps=%5 maxRefOps=%6 refNs=%7", name, shownValue, shownLimit, unit, shownUsed, maxRef, shownRefNs));
		ctx.True(used <= maxRef, string.Format("perf %1 %2 %3 over budget %4 (%5 ref ops against %6, one ref op %7 ns)", name, value, unit, limit, shownUsed, maxRef, m_fRefNs));
	}

	//------------------------------------------------------------------------------------------------
	protected void Report(string name, int value, string unit)
	{
		Say(string.Format("BENCH %1 value=%2 budget=none unit=%3", name, value, unit));
	}

	//------------------------------------------------------------------------------------------------
	//! The measures run with the LF_Debug console chatter off: every event logs a line or two, and the
	//! cost of writing a line is the box's disk and console, not the code under test. The BENCH lines
	//! print through here, which lifts the mute for one line.
	protected void Say(string message)
	{
		LF_Debug.SetAllEnabled(true);
		LF_Debug.Info(LF_Perf.FEATURE, message);
		LF_Debug.SetAllEnabled(!m_bMuted);
	}

	//------------------------------------------------------------------------------------------------
	protected void Mute(bool muted)
	{
		m_bMuted = muted;
		LF_Debug.SetAllEnabled(!muted);
	}

	//------------------------------------------------------------------------------------------------
	//! Times the reference loop (best of three) and stores the cost of one ref op in ns.
	protected void Calibrate()
	{
		int best = 0;
		for (int run = 0; run < 3; run++)
		{
			int t0 = System.GetTickCount();
			array<ref LF_PerfRefItem> rows = new array<ref LF_PerfRefItem>();
			map<string, int> index = new map<string, int>();
			for (int i = 0; i < REF_OPS; i++)
			{
				LF_PerfRefItem item = new LF_PerfRefItem();
				item.m_sKey = UID_PREFIX + (i % 128).ToString();
				item.m_iValue = i;
				rows.Insert(item);
				index.Set(item.m_sKey, i);
				if (rows.Count() >= REF_RETAIN)
					rows.Clear();
			}

			int ms = System.GetTickCount(t0);
			if (run == 0 || ms < best)
				best = ms;
		}

		m_fRefNs = (best * 1000000.0) / REF_OPS;
		if (m_fRefNs < 1)
			m_fRefNs = 1;
	}

	//------------------------------------------------------------------------------------------------
	protected static float PerOpUs(int ms, int ops)
	{
		if (ops <= 0)
			return 0;

		return (ms * 1000.0) / ops;
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

		Mute(true);

		// Crime history through the bus (the record and the incident log both listen). Each section is
		// timed in chunks of players and the cheapest chunk counts: a load spike on a shared box hits some
		// chunks, a real regression hits all of them.
		Calibrate();
		float bestUs = -1;
		int chunks = 2;
		int per = players / chunks;
		int ch;
		for (ch = 0; ch < chunks; ch++)
		{
			int from = ch * per;
			int to = from + per;
			if (ch == chunks - 1)
				to = players;

			int crimeOps = 0;
			t = System.GetTickCount();
			for (index = from; index < to; index++)
			{
				for (int c = 0; c < budget.m_iCrimeEventsPerPlayer; c++)
				{
					LF_EventBus.Publish(LF_OnCrimeCommitted.Create(UID_PREFIX + index.ToString(), LF_WantedReports.OFFENCE_ROBBERY));
					crimeOps++;
				}
			}
			float crimeUs = PerOpUs(System.GetTickCount(t), crimeOps);
			if (bestUs < 0 || crimeUs < bestUs)
				bestUs = crimeUs;
		}
		CheckRef(ctx, "crime-event-us", bestUs, budget.m_iMaxCrimeEventRef, "us/event");

		// Money log, below the cap: every subscriber (ledger, incident log) runs.
		Calibrate();
		bestUs = -1;
		chunks = 4;
		per = players / chunks;
		for (ch = 0; ch < chunks; ch++)
		{
			int moneyFrom = ch * per;
			int moneyTo = moneyFrom + per;
			if (ch == chunks - 1)
				moneyTo = players;

			int moneyOps = 0;
			t = System.GetTickCount();
			for (index = moneyFrom; index < moneyTo; index++)
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
			float moneyUs = PerOpUs(System.GetTickCount(t), moneyOps);
			if (bestUs < 0 || moneyUs < bestUs)
				bestUs = moneyUs;
		}
		CheckRef(ctx, "money-event-us", bestUs, budget.m_iMaxMoneyEventRef, "us/event");
		Report("ledger-rows", LF_Ledger.Count(), "rows");
		Report("incident-rows", LF_IncidentLog.Count(), "rows");

		// What one character costs in the save: the same store with the two shared logs emptied.
		LF_Ledger.ApplyAll(null);
		LF_IncidentLog.ApplyAll(null);
		JsonSaveContext baseContext = new JsonSaveContext(false);
		LF_PersistenceComponentSerializer.SerializeStore(persistence, baseContext);
		int baseBytes = baseContext.SaveToString().Length();
		CheckBytes(ctx, "save-bytes-per-player", baseBytes / players, budget.m_iMaxSaveBytesPerPlayer, "bytes/player");

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

		Calibrate();
		int capOps = 500;
		bestUs = -1;
		for (ch = 0; ch < 4; ch++)
		{
			t = System.GetTickCount();
			for (index = 0; index < capOps; index++)
			{
				LF_EventBus.Publish(LF_OnMoneyMoved.CreateFull(UID_PREFIX + (index % players).ToString(), LF_Ledger.ACCOUNT_BANK, 50, true, "perf-bench", 1050, ""));
			}
			float capUs = PerOpUs(System.GetTickCount(t), capOps);
			if (bestUs < 0 || capUs < bestUs)
				bestUs = capUs;
		}
		CheckRef(ctx, "money-event-at-cap-us", bestUs, budget.m_iMaxMoneyEventAtCapRef, "us/event");
		Report("ledger-rows-at-cap", LF_Ledger.Count(), "rows");

		// One query per character against the full ledger (min of three passes).
		Calibrate();
		array<ref LF_LedgerEntry> found = new array<ref LF_LedgerEntry>();
		int bestMs = -1;
		int rep;
		for (rep = 0; rep < 3; rep++)
		{
			t = System.GetTickCount();
			for (index = 0; index < players; index++)
			{
				LF_Ledger.Last(UID_PREFIX + index.ToString(), LF_Ledger.ACCOUNT_BANK, 20, found);
			}
			int queryMs = System.GetTickCount(t);
			if (bestMs < 0 || queryMs < bestMs)
				bestMs = queryMs;
		}
		CheckRef(ctx, "ledger-query-all-ms", bestMs * 1000.0, budget.m_iMaxLedgerQueryAllRef, "ms");

		// The save: export, then the store write and read through the real serializer.
		Calibrate();
		array<ref LF_PlayerAccountRecord> exported;
		bestMs = -1;
		for (rep = 0; rep < 3; rep++)
		{
			t = System.GetTickCount();
			for (int batch = 0; batch < EXPORT_BATCH; batch++)
			{
				exported = LF_PlayerAccountManager.ExportAll();
			}
			int exportMs = System.GetTickCount(t);
			if (bestMs < 0 || exportMs < bestMs)
				bestMs = exportMs;
		}
		CheckRef(ctx, "account-export-us", (bestMs * 1000.0) / EXPORT_BATCH, budget.m_iMaxAccountExportRef, "us");
		ctx.Equal(players, exported.Count(), "the export carries every account");

		Calibrate();
		string saved;
		bestMs = -1;
		for (rep = 0; rep < 3; rep++)
		{
			JsonSaveContext saveContext = new JsonSaveContext(false);
			t = System.GetTickCount();
			ESerializeResult written = LF_PersistenceComponentSerializer.SerializeStore(persistence, saveContext);
			int writeMs = System.GetTickCount(t);
			ctx.True(written == ESerializeResult.OK, "the store serializes");
			saved = saveContext.SaveToString();
			if (bestMs < 0 || writeMs < bestMs)
				bestMs = writeMs;
		}
		CheckRef(ctx, "save-write-ms", bestMs * 1000.0, budget.m_iMaxSaveWriteRef, "ms");
		CheckBytes(ctx, "save-bytes-at-cap", saved.Length(), budget.m_iMaxSaveBytesAtCap, "bytes");

		JsonLoadContext loadContext = new JsonLoadContext(false);
		t = System.GetTickCount();
		bool parsed = loadContext.LoadFromString(saved);
		int parseMs = System.GetTickCount(t);
		ctx.True(parsed, "the save parses");
		Report("save-parse-ms", parseMs, "ms");

		Calibrate();
		bestMs = -1;
		for (rep = 0; rep < 3; rep++)
		{
			JsonLoadContext readContext = new JsonLoadContext(false);
			ctx.True(readContext.LoadFromString(saved), "the save parses again");
			t = System.GetTickCount();
			bool read = LF_PersistenceComponentSerializer.DeserializeStore(persistence, readContext);
			int readMs = System.GetTickCount(t);
			ctx.True(read, "the store reads");
			if (bestMs < 0 || readMs < bestMs)
				bestMs = readMs;
		}
		CheckRef(ctx, "save-read-ms", bestMs * 1000.0, budget.m_iMaxSaveReadRef, "ms");
		ctx.Equal(players, LF_PlayerAccountManager.ExportAll().Count(), "the read leaves every account in place");

		Mute(false);
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
