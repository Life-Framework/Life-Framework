// red-proof: 2026-11-19 `tools/cli test --no-build --red LF_Test_EventDispatchSeize`: RED-OK (`RedProof()` skips the run, so the crate is never seized and the second run reads as 4 instead of ALREADY_RAN). Green without it.
//! events/dispatch-seize (#3785, card B of the #3443 raid split): the "dispatch and seize" scene
//! operation files a police call at its place (LF_Dispatch.Report), then finds the named item lying
//! there and logs it in the evidence locker (LF_EvidenceLocker.LogSeizure). An item the smugglers
//! moved out of its radius is not seized (the call still goes out); a phase run acts once; a bad
//! config is refused by the module loader. The place is a test override; the scene markers are the
//! island's (#3786).
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_EventDispatchSeize : LF_Test
{
	static const ResourceName CRATE_PREFAB = "{C8C36ADC9DFC5FBB}Prefabs/Props/Crates/Crate_01_red.et";
	static const ResourceName OTHER_PREFAB = "{9B5BB216CC7FF18E}Prefabs/Characters/Core/Character_Roleplay.et";
	static const string MODULE_ID = "test_dispatch_seize";
	static const string PLACE = "Test Warehouse";
	static const float RADIUS = 6;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "events/dispatch-seize";
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_EventDispatchSeize Config()
	{
		LF_EventDispatchSeize config = new LF_EventDispatchSeize();
		config.m_sSceneId = "test_dispatch_seize_scene";
		config.m_Item = CRATE_PREFAB;
		config.m_sOffence = "smuggling";
		config.m_sReporterUid = "event:test";
		config.m_sOfficerUid = "event:test-officer";
		config.m_sPlace = PLACE;
		config.m_fRadius = RADIUS;
		config.m_bRemoveItem = true;
		return config;
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_EventDispatchSeizeOperation Operation(vector site)
	{
		LF_EventDispatchSeizeOperation operation = new LF_EventDispatchSeizeOperation(MODULE_ID, Config());
		operation.m_bSiteOverride = true;
		operation.m_vSiteOverride = site;
		return operation;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		if (!Replication.IsServer())
			return;

		// Only this feature's and its two sinks' statics; LF_StaticReset.ResetAll() would wipe every feature's.
		LF_EvidenceLocker.ResetStatics();
		LF_Dispatch.ClearLog();

		float x = 40;
		float z = 40;
		vector site = Vector(x, GetGame().GetWorld().GetSurfaceY(x, z) + 0.2, z);

		// Refused configs: a missing scene, a missing item.
		LF_EventDispatchSeize noScene = Config();
		noScene.m_sSceneId = "";
		ctx.False(noScene.Validate().IsEmpty(), "a config with no scene id is refused");
		LF_EventDispatchSeize noItem = Config();
		noItem.m_Item = "";
		ctx.False(noItem.Validate().IsEmpty(), "a config with no item is refused");
		ctx.True(Config().Validate().IsEmpty(), "a complete config is accepted");

		// The loader registers a good module as an operation and refuses a bad one.
		LF_EventModuleLoader.UnregisterLoaded();
		ctx.True(LF_EventModuleLoader.RegisterModule(MODULE_ID, Config(), "test"), "the loader takes a dispatch and seize module");
		ctx.True(LF_EventShell.IsRegistered(MODULE_ID), "the module is registered under its id");
		ctx.False(LF_EventModuleLoader.RegisterModule("test_dispatch_seize_bad", noScene, "test"), "the loader refuses a module with no scene");
		LF_EventModuleLoader.UnregisterLoaded();

		// 1. The crate lies at its place: called, found, logged, taken.
		IEntity crate = LF_ExploitGuardsWorldFixture.Spawn(CRATE_PREFAB, site);
		ctx.NotNull(crate, "the crate spawns");
		if (!crate)
			return;

		EntityID crateId = crate.GetID();
		LF_EventContext phaseA = LF_EventContext.Make(5, 1, 1, 100);
		LF_EventDispatchSeizeOperation operation = Operation(site);
		int calls = LF_Dispatch.CallCount();
		int rows = LF_EvidenceLocker.Count();
		bool acted;
		if (!RedProof())
			acted = operation.Run("test_event", "raid", phaseA);

		ctx.True(acted, "the run acted");
		ctx.Equal(LF_EDispatchSeizeOutcome.SEIZED, operation.m_eLast, "the crate at its place is seized");
		ctx.Equal(calls + 1, LF_Dispatch.CallCount(), "one police call was filed");
		ctx.Equal(rows + 1, LF_EvidenceLocker.Count(), "one locker row was logged");
		LF_DispatchRecord record = LF_Dispatch.Last();
		if (record)
			ctx.EqualStr("smuggling", record.m_sOffence, "the call carries the configured offence");
		array<ref LF_EvidenceLockerRow> lockerRows = LF_EvidenceLocker.ExportAll();
		if (lockerRows.Count() > 0)
		{
			LF_EvidenceLockerRow row = lockerRows[lockerRows.Count() - 1];
			ctx.EqualStr(PLACE, row.m_sPlace, "the row names the place");
			ctx.EqualStr("event:test-officer", row.m_sOfficerUid, "the row names the configured officer");
			ctx.True(LF_Utils.SamePrefab(CRATE_PREFAB, row.m_sPrefab), "the row holds the crate prefab");
		}
		ctx.True(GetGame().GetWorld().FindEntityByID(crateId) == null, "the seized crate is taken out of the world");

		// 2. The same phase run does not call again; a new phase run does (and finds nothing left).
		acted = operation.Run("test_event", "raid", phaseA);
		ctx.False(acted, "the same phase run does nothing twice");
		ctx.Equal(LF_EDispatchSeizeOutcome.ALREADY_RAN, operation.m_eLast, "the second run in a phase run is ALREADY_RAN");
		ctx.Equal(calls + 1, LF_Dispatch.CallCount(), "no second call in one phase run");

		// 3. The crate was moved out of the radius: the police are told, nothing is seized.
		IEntity moved = LF_ExploitGuardsWorldFixture.Spawn(CRATE_PREFAB, site + Vector(RADIUS + 4, 0, 0));
		LF_EventContext phaseB = LF_EventContext.Make(5, 1, 1, 400);
		acted = operation.Run("test_event", "raid", phaseB);
		ctx.Equal(LF_EDispatchSeizeOutcome.MOVED, operation.m_eLast, "a crate moved out of its place is not seized");
		ctx.Equal(calls + 2, LF_Dispatch.CallCount(), "the police were still called");
		ctx.Equal(rows + 1, LF_EvidenceLocker.Count(), "nothing more was logged");
		ctx.True(moved != null && GetGame().GetWorld().FindEntityByID(moved.GetID()) != null, "the moved crate is left alone");
		LF_ExploitGuardsWorldFixture.Despawn(moved);

		// 4. Something else at the place is not the crate.
		IEntity other = LF_ExploitGuardsWorldFixture.Spawn(OTHER_PREFAB, site);
		LF_EventContext phaseC = LF_EventContext.Make(5, 1, 1, 800);
		acted = operation.Run("test_event", "raid", phaseC);
		ctx.Equal(LF_EDispatchSeizeOutcome.MOVED, operation.m_eLast, "a different item at the place is not seized");
		LF_ExploitGuardsWorldFixture.Despawn(other);

		// 5. A scene with no placed target: no call, no seizure.
		LF_EventDispatchSeizeOperation lost = new LF_EventDispatchSeizeOperation(MODULE_ID, Config());
		int before = LF_Dispatch.CallCount();
		lost.Run("test_event", "raid", LF_EventContext.Make(5, 1, 1, 1200));
		ctx.Equal(LF_EDispatchSeizeOutcome.NO_SITE, lost.m_eLast, "no placed event_target: no site");
		ctx.Equal(before, LF_Dispatch.CallCount(), "no site, no call");

		LF_Dispatch.StopAllAlarms();
		LF_Dispatch.ClearLog();
		LF_EvidenceLocker.ResetStatics();
	}
}
