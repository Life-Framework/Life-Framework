// red-proof: 2026-11-15, `--tier perf` (PERF tier, 15 ref runs and repeats, 6 windows, cheapest cost over cheapest ref op, limits at 70 %): with the money publish tripled in the measured path (3 publishes per counted event, about 2.8x of the whole measure) 3 of 3 runs failed `perf money-event-us 42.1875 us/event over budget 26.52 (62 ref ops against 39, one ref op 680 ns)` (65 and 62 ref ops in the other two); restored, 3 of 3 passed with money-event-us at 18-19 ref ops against 39. The same plant at the old shared-suite settings failed only 2 runs of 3 (PR #1244: ref op 370-700 ns between runs, money-event baseline 21-44 of 56 ref ops), which is why this test left `fast` and `all`. Earlier: 2026-11-14, reference-op budgets: with the money publish tripled in the measured path, `perf-budget` failed `perf money-event-us 23.4375 us/event over budget 20.72 (63 ref ops against 56, one ref op 370 ns)` on this PC and `... 124.219 us/event over budget 89.6 (77 ref ops against 56, one ref op 1600 ns)` on lf-cloud. Observed 2026-11-11: with `m_iMaxMoneyEventUs 1` the run failed `perf money-event-us 33 us/event over budget 1` and exited 1.
//! One row of the calibration workload in LF_Test_PerfBudget.
class LF_PerfRefItem
{
	string m_sKey;
	int m_iValue;
}

// tier: PERF
//! perf/budget-128 (#335): the server cost gate. Runs only under `tools\cli test --tier perf` (a timing
//! measure is flaky in the shared suite and blunt there); `--only` runs it too, at the lenient settings below. Builds a synthetic population (accounts, bank
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
	//! timed on this run, on this machine, right before each measure (best of five), so a slow box or a
	//! busy one stretches the limit by the same ratio it stretches the code under test, and a measure that
	//! needs 3x the budgeted ref ops fails on any machine. The limit has no scale cap and no floor.
	protected static const int REF_OPS = 100000;
	//! Runs of the reference loop per calibration; the cheapest counts. A busy box spoils some runs of a window,
	//! never all five, so the best-of estimate of one ref op stays near the quiet value.
	protected static const int REF_RUNS = 5;
	protected static const int PERF_REF_RUNS = 15;
	//! Repeats of a timed measure whose cheapest run counts (the chunk and pass loops below).
	protected static const int REPEATS = 5;
	protected static const int PERF_REPEATS = 15;
	//! Rows the reference loop keeps alive before it drops them.
	protected static const int REF_RETAIN = 1000;
	//! One export is under a millisecond, so time a batch of them.
	protected static const int EXPORT_BATCH = 20;
	protected float m_fRefNs = 250;
	//! Windows a time measure may take before it counts as over its limit (see Settle).
	protected static const int ATTEMPTS = 3;
	protected static const int PERF_ATTEMPTS = 6;
	//! Percent of each conf limit that --tier perf allows.
	protected static const int PERF_LIMIT_PCT = 70;
	protected float m_fKeepUs = -1;
	protected float m_fKeepRefNs = -1;
	protected bool m_bMuted;
	//! Set from LF_TestManager.PerfRun() at the start of Run(): `--tier perf` takes more of everything and
	//! reads the cheapest cost and the cheapest reference op over ALL windows, so the ratio converges on the
	//! quiet machine instead of stopping at the first window that passes.
	protected bool m_bPerf;
	protected int m_iRefRuns = REF_RUNS;
	protected int m_iRepeats = REPEATS;
	protected int m_iAttempts = ATTEMPTS;

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
	//! The limit in ref ops: the conf's number in the shared suite and under --only, PERF_LIMIT_PCT of it
	//! under --tier perf (the sharper settings can hold a tighter limit).
	protected int Limit(int maxRef)
	{
		if (!m_bPerf)
			return maxRef;

		return Math.Round(maxRef * PERF_LIMIT_PCT / 100.0);
	}

	//------------------------------------------------------------------------------------------------
	//! One measure of time against a limit in ref ops. costUs the measured cost in microseconds
	//! (per event, or per pass for the ms measures); unit "ms" prints milliseconds, anything else microseconds.
	protected void CheckRef(LF_TestContext ctx, string name, float costUs, int maxRef, string unit)
	{
		maxRef = Limit(maxRef);
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
	//! A time measure that is over its limit is measured again with a fresh calibration, up to ATTEMPTS
	//! windows; the attempt with the lowest cost in ref ops counts. A noisy window passes one attempt, a real
	//! regression is over the limit in every window. Returns true when the measure is within its limit.
	protected bool Settle(float costUs, int maxRef)
	{
		if (m_bPerf)
		{
			// --tier perf: the cheapest cost and the cheapest reference op over every window, each the best the
			// machine showed, so noise on either side cannot move the ratio by more than the quiet spread. Never
			// stops early: all windows run.
			if (m_fKeepUs < 0 || costUs < m_fKeepUs)
				m_fKeepUs = costUs;

			if (m_fKeepRefNs < 0 || m_fRefNs < m_fKeepRefNs)
				m_fKeepRefNs = m_fRefNs;

			return false;
		}

		float used = (costUs * 1000) / m_fRefNs;
		float keptUsed = 0;
		if (m_fKeepUs >= 0)
			keptUsed = (m_fKeepUs * 1000) / m_fKeepRefNs;
		if (m_fKeepUs < 0 || used < keptUsed)
		{
			m_fKeepUs = costUs;
			m_fKeepRefNs = m_fRefNs;
		}

		return used <= maxRef;
	}

	//------------------------------------------------------------------------------------------------
	//! Checks the best attempt Settle kept.
	protected void CheckKept(LF_TestContext ctx, string name, int maxRef, string unit)
	{
		m_fRefNs = m_fKeepRefNs;
		CheckRef(ctx, name, m_fKeepUs, maxRef, unit);
		m_fKeepUs = -1;
		m_fKeepRefNs = -1;
	}

	//------------------------------------------------------------------------------------------------
	//! Times the reference loop (best of five) and stores the cost of one ref op in ns.
	protected void Calibrate()
	{
		int best = 0;
		for (int run = 0; run < m_iRefRuns; run++)
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
		m_bPerf = LF_TestManager.PerfRun();
		if (m_bPerf)
		{
			m_iRefRuns = PERF_REF_RUNS;
			m_iRepeats = PERF_REPEATS;
			m_iAttempts = PERF_ATTEMPTS;
		}

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
		float bestUs = -1;
		int chunks = 4;
		int per = players / chunks;
		int ch;
		int attempt;
		for (attempt = 0; attempt < m_iAttempts; attempt++)
		{
		if (attempt > 0)
		{
			LF_CrimeRecord.ApplyAll(null);
			LF_IncidentLog.ApplyAll(null);
		}

		Calibrate();
		bestUs = -1;
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
		if (Settle(bestUs, budget.m_iMaxCrimeEventRef))
			break;
		}
		CheckKept(ctx, "crime-event-us", budget.m_iMaxCrimeEventRef, "us/event");

		// Money log, below the cap: every subscriber (ledger, incident log) runs.
		chunks = 8;
		per = players / chunks;
		for (attempt = 0; attempt < m_iAttempts; attempt++)
		{
		if (attempt > 0)
		{
			LF_Ledger.ApplyAll(null);
			LF_IncidentLog.ApplyAll(null);
		}

		Calibrate();
		bestUs = -1;
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
		if (Settle(bestUs, budget.m_iMaxMoneyEventRef))
			break;
		}
		CheckKept(ctx, "money-event-us", budget.m_iMaxMoneyEventRef, "us/event");
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
		for (ch = 0; ch < m_iRepeats; ch++)
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

		// One query per character against the full ledger (min of five passes).
		Calibrate();
		array<ref LF_LedgerEntry> found = new array<ref LF_LedgerEntry>();
		int bestMs = -1;
		int rep;
		for (rep = 0; rep < m_iRepeats; rep++)
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
		for (rep = 0; rep < m_iRepeats; rep++)
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
		for (rep = 0; rep < m_iRepeats; rep++)
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
		for (rep = 0; rep < m_iRepeats; rep++)
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
