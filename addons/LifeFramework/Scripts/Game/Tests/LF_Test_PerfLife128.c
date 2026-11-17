// red-proof: 2026-11-17 `tools/cli test --no-build --red LF_Test_PerfLife128`: RED-OK perf/life-128, with the synthetic population not installed "after: one uid compare per racer per tick (expected 6400, got 0)" failed. `--tier perf` green, 3 of 3 (numbers in docs/architecture/performance.md, #2404).
// tier: PERF
//! perf/life-128 (#2404): the four everyday-life costs the life review found at 128 players, each
//! measured the old way and the new way in the same run, on the same machine. "Before" replays the
//! old loop with the same primitives; "after" runs the shipped code. Prints one
//! `[LFDebug:Perf] BENCH <name> value=<n> budget=none unit=<u>` line per number; the counts are the
//! gate (deterministic), the times are reported.
//!
//! 1. Drunk sway: frame callbacks armed per client frame (127 proxies before, the local body only after).
//! 2. Race tick: uid lookups to find 16 racers among 128 players (a scan per racer before, one compare after).
//! 3. Logout bodies: snapshots written for one looted item while many bodies are kept (all before, one after).
//! 4. Police call: inventory walks to find scanners (every player before, the scanner registry after).
class LF_Test_PerfLife128 : LF_Test
{
	static const int PLAYERS = 128;
	static const int RACERS = 16;
	//! 100 s of the 250 ms race clock.
	static const int RACE_TICKS = 400;
	static const int SWAY_FRAMES = 1000;
	static const int KEPT_BODIES = 16;
	static const int LOOT_ITEMS = 20;
	static const int POLICE_CALLS = 50;

	override string GetName()
	{
		return "perf/life-128";
	}

	//------------------------------------------------------------------------------------------------
	protected void Bench(string name, float value, string unit)
	{
		LF_Debug.SetAllEnabled(true);
		LF_Debug.Info(LF_Perf.FEATURE, string.Format("BENCH %1 value=%2 budget=none unit=%3", name, value, unit));
		LF_Debug.SetAllEnabled(false);
	}

	//------------------------------------------------------------------------------------------------
	protected static float PerCallUs(int ms, int calls)
	{
		if (calls <= 0)
			return 0;

		return (ms * 1000.0) / calls;
	}

	override void Run(LF_TestContext ctx)
	{
		LF_Debug.SetAllEnabled(false);
		Sway(ctx);
		Race(ctx);
		Logout(ctx);
		Scanner(ctx);
		LF_Debug.SetAllEnabled(true);
	}

	//------------------------------------------------------------------------------------------------
	//! A client at 128 players: body 0 is the local player, half the server is drunk.
	protected void Sway(LF_TestContext ctx)
	{
		int beforeArmed = PLAYERS; // the old rule armed every survival component on a rendering client
		int afterArmed;
		for (int i = 0; i < PLAYERS; i++)
		{
			int drunk = i % 2;
			float level = drunk * 30;
			if (LF_ImpairmentView.WantsFrame(false, i == 0, level, false))
				afterArmed++;
		}

		// The script side of one early-return frame, as each proxy paid it.
		IEntity body = LF_TestCharacterFixture.Spawn(ctx);
		if (!body)
			return;

		LF_CharacterSurvivalComponent survival = LF_CharacterSurvivalComponent.Cast(body.FindComponent(LF_CharacterSurvivalComponent));
		ctx.NotNull(survival, "the fixture carries LF_CharacterSurvivalComponent");
		if (survival)
		{
			int skipped;
			vector applied = vector.Zero;
			int t = System.GetTickCount();
			for (int frame = 0; frame < SWAY_FRAMES; frame++)
			{
				for (int p = 1; p < PLAYERS; p++)
				{
					float viewLevel = LF_ImpairmentView.ViewLevel(survival.GetReplicatedImpairment());
					if (viewLevel <= 0 && applied == vector.Zero)
						skipped++;
				}
			}
			int ms = System.GetTickCount(t);
			ctx.Equal(SWAY_FRAMES * (PLAYERS - 1), skipped, "every replayed proxy frame returned early");
			Bench("sway-proxy-call-us", PerCallUs(ms, SWAY_FRAMES * (PLAYERS - 1)), "us/call");
			Bench("sway-frame-script-us-before", PerCallUs(ms, SWAY_FRAMES), "us/frame");
		}

		Bench("sway-armed-before", beforeArmed, "callbacks/frame");
		Bench("sway-armed-after", afterArmed, "callbacks/frame");
		ctx.Equal(0, afterArmed, "a sober local player and 127 proxies arm no sway frame");
		ctx.True(LF_ImpairmentView.WantsFrame(false, true, 30, false), "a drunk local player arms exactly its own");
		LF_TestCharacterFixture.Delete(body);
	}

	//------------------------------------------------------------------------------------------------
	protected void Race(LF_TestContext ctx)
	{
		LF_RaceRunner.ResetStatics();
		IEntity body = LF_TestCharacterFixture.Spawn(ctx);
		if (!body)
			return;

		LF_TestRacePlayers players = LF_TestRacePlayers.Create(PLAYERS, "perf-racer-", body);
		if (!RedProof())
			LF_RaceRunner.SetPlayers(players);
		LF_RaceCourseRecord course = LF_Test_RaceRacerIndex.BuildCourse("perf_life_race");
		array<string> uids = {};
		for (int r = 0; r < RACERS; r++)
		{
			// Racers spread over the server: player ids 8, 16, ... 128.
			string uid = players.UidAt((r + 1) * (PLAYERS / RACERS));
			uids.Insert(uid);
			LF_RaceRunner.Join(course, uid, body);
		}
		ctx.True(LF_RaceRunner.CloseLobby(course.m_sId), "16 racers start");

		// Before: each racer's body found by a uid scan over every player, every tick.
		players.m_iUidLookups = 0;
		array<int> ids = {};
		int t = System.GetTickCount();
		for (int tick = 0; tick < RACE_TICKS; tick++)
		{
			foreach (string racerUid : uids)
			{
				players.GetPlayers(ids);
				foreach (int playerId : ids)
				{
					if (players.UidOf(playerId) != racerUid)
						continue;

					players.ControlledBy(playerId);
					break;
				}
			}
		}
		int beforeMs = System.GetTickCount(t);
		int beforeLookups = players.m_iUidLookups;

		// After: the shipped tick (body, path and checkpoint test included).
		players.m_iUidLookups = 0;
		t = System.GetTickCount();
		for (int tick2 = 0; tick2 < RACE_TICKS; tick2++)
			LF_RaceRunner.Tick();
		int afterMs = System.GetTickCount(t);
		int afterLookups = players.m_iUidLookups;

		Bench("race-lookups-per-tick-before", beforeLookups / RACE_TICKS, "lookups/tick");
		Bench("race-lookups-per-tick-after", afterLookups / RACE_TICKS, "lookups/tick");
		Bench("race-lookup-us-per-tick-before", PerCallUs(beforeMs, RACE_TICKS), "us/tick");
		Bench("race-tick-us-after", PerCallUs(afterMs, RACE_TICKS), "us/tick");
		ctx.Equal(RACERS * RACE_TICKS, afterLookups, "after: one uid compare per racer per tick");
		ctx.True(afterLookups * 4 < beforeLookups, "after: at least four times fewer uid lookups than the scan");

		LF_RaceRunner.AbandonAll("perf end");
		LF_RaceRunner.SetPlayers(null);
		LF_RaceRunner.ResetStatics();
		LF_TestCharacterFixture.Delete(body);
	}

	//------------------------------------------------------------------------------------------------
	protected void Logout(LF_TestContext ctx)
	{
		LF_LogoutBody.ResetStatics();
		int kept;
		for (int i = 0; i < KEPT_BODIES; i++)
		{
			IEntity body = LF_TestCarriedInventoryKit.SpawnLoaded(ctx);
			if (!body)
				break;

			if (LF_LogoutBody.KeepBody(LogoutUid(i), LogoutCharacter(i), body, LF_LogoutBody.Now() + 600))
				kept++;
		}
		ctx.Equal(KEPT_BODIES, kept, "every synthetic logout body is kept");

		// Before: every item change re-captured every kept body.
		int captures = LF_LogoutBody.GetCaptureCount();
		int t = System.GetTickCount();
		for (int item = 0; item < LOOT_ITEMS; item++)
			LF_LogoutBody.RecaptureAll();
		int beforeMs = System.GetTickCount(t);
		int beforeCaptures = LF_LogoutBody.GetCaptureCount() - captures;

		// After: the looted body's mark captures that body only.
		captures = LF_LogoutBody.GetCaptureCount();
		t = System.GetTickCount();
		for (int item2 = 0; item2 < LOOT_ITEMS; item2++)
		{
			LF_LogoutBody.ScheduleRecapture(LogoutUid(0));
			LF_LogoutBody.RecaptureDirty();
		}
		int afterMs = System.GetTickCount(t);
		int afterCaptures = LF_LogoutBody.GetCaptureCount() - captures;

		Bench("logout-captures-per-item-before", beforeCaptures / LOOT_ITEMS, "snapshots/item");
		Bench("logout-captures-per-item-after", afterCaptures / LOOT_ITEMS, "snapshots/item");
		Bench("logout-us-per-item-before", PerCallUs(beforeMs, LOOT_ITEMS), "us/item");
		Bench("logout-us-per-item-after", PerCallUs(afterMs, LOOT_ITEMS), "us/item");
		ctx.Equal(KEPT_BODIES * LOOT_ITEMS, beforeCaptures, "before: every kept body per item");
		ctx.Equal(LOOT_ITEMS, afterCaptures, "after: one body per item");

		for (int j = 0; j < KEPT_BODIES; j++)
		{
			LF_LogoutBody.Release(LogoutUid(j), "perf end");
			LF_StorageSnapshot.Clear(LogoutCharacter(j), LF_StorageSnapshot.SLOT_CARRIED);
		}
		LF_LogoutBody.ResetStatics();
	}

	//------------------------------------------------------------------------------------------------
	protected void Scanner(LF_TestContext ctx)
	{
		IEntity body = LF_TestCarriedInventoryKit.SpawnLoaded(ctx);
		if (!body)
			return;

		IEntity scannerEntity = LF_Utils.SpawnEntityPrefab(LF_Scanner.PREFAB, body.GetOrigin());
		InventoryStorageManagerComponent manager = LF_InventoryUtils.GetResponsibleStorageManager(body);
		if (scannerEntity && manager)
			ctx.True(manager.TryInsertItem(scannerEntity), "the scanner is carried");

		// Before: every police call walked every player's inventory (here 128 walks of a loaded body).
		int walks;
		int t = System.GetTickCount();
		for (int call = 0; call < POLICE_CALLS; call++)
		{
			for (int p = 0; p < PLAYERS; p++)
			{
				array<LF_ScannerComponent> found = {};
				LF_Scanner.FindCarried(body, found);
				walks++;
			}
		}
		int beforeMs = System.GetTickCount(t);

		// After: the registry of scanners.
		array<int> receivers = {};
		int visited;
		t = System.GetTickCount();
		for (int call2 = 0; call2 < POLICE_CALLS; call2++)
		{
			array<string> uids = {};
			array<ref array<LF_ScannerComponent>> carried = {};
			array<IEntity> bodies = {};
			visited += LF_Scanner.CollectHolders(receivers, uids, carried, bodies);
		}
		int afterMs = System.GetTickCount(t);

		Bench("scanner-inventory-walks-per-call-before", walks / POLICE_CALLS, "walks/call");
		Bench("scanner-registry-visits-per-call-after", visited / POLICE_CALLS, "scanners/call");
		Bench("scanner-us-per-call-before", PerCallUs(beforeMs, POLICE_CALLS), "us/call");
		Bench("scanner-us-per-call-after", PerCallUs(afterMs, POLICE_CALLS), "us/call");
		ctx.True(visited / POLICE_CALLS < PLAYERS, "after: a call visits the scanners, not every player");

		if (scannerEntity)
			SCR_EntityHelper.DeleteEntityAndChildren(scannerEntity);

		LF_TestCharacterFixture.Delete(body);
	}

	//------------------------------------------------------------------------------------------------
	protected static string LogoutUid(int i)
	{
		return "perf-logout-uid-" + i.ToString();
	}

	//------------------------------------------------------------------------------------------------
	protected static string LogoutCharacter(int i)
	{
		return "perf-logout-character-" + i.ToString();
	}
}
