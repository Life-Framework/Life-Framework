// red-proof: observed 2026-11-14 with `tools\cli test --no-build --only LF_Test_PackPatchOverlays`: making LF_DialogueNodeList.RemoveAt a no-op failed `- remove: the work node is gone` and `- one node added, one removed (expected 5, got 6)`; dropping the m_iBasePrice copy in LF_VehiclePriceList.PatchFromContainer failed `- patch: the S105 base price is the pack's (expected 1200, got 1000)`; making vehicle RemoveAt a no-op failed `- remove: the UAZ is unlisted (expected -1, got 2500)`. Restored; green.

//------------------------------------------------------------------------------------------------
//! A stand-in for the D4 pack source: patches the clerk's dialogue (the good or the bad fixture)
//! and the vehicle prices core file.
class LF_Test_PackPatchSourceStub : LF_ConfigPackSource
{
	static const ResourceName DIALOGUE_GOOD = "{38D29E0E9DC24207}Configs/Tests/LF_Test_DialogueClerkPatch.conf";
	static const ResourceName DIALOGUE_BAD = "{B34964F9C0AB3BB7}Configs/Tests/LF_Test_DialogueClerkBadPatch.conf";
	static const ResourceName VEHICLES = "{1C85EEE80FBE2238}Configs/Tests/LF_Test_VehiclePricesOverlay.conf";

	bool m_bBadDialogue;

	override void GetOverlays(ResourceName coreConf, notnull array<ResourceName> overlays)
	{
		if (coreConf == LF_Test_PackPatchOverlays.CLERK)
		{
			if (m_bBadDialogue)
				overlays.Insert(DIALOGUE_BAD);
			else
				overlays.Insert(DIALOGUE_GOOD);
		}
		else if (coreConf == LF_VehiclePricesConfig.CONFIG_PATH)
		{
			overlays.Insert(VEHICLES);
		}
	}

	override string GetName()
	{
		return "test-stub-patches";
	}
};

// tier: WORLD
//! #1109, #957: a pack `.conf` patches the clerk's dialogue by node and line id (replaced line text,
//! a replaced, an added and a removed node, a new start node, a patch of a missing node refused) and
//! the vehicle prices by prefab GUID (patch, remove, add, a patch of a missing vehicle refused). A
//! pack patch that leaves the dialogue broken is dropped and the core clerk plays.
class LF_Test_PackPatchOverlays : LF_Test
{
	static const ResourceName CLERK = "{65C163E3C4868E76}Configs/Dialogue/LF_Dialogue_Clerk.conf";

	override string GetName()
	{
		return "config/pack-patch-overlays";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_ConfigService.Reset();
		LF_ConfigService.LoadOwnerString("{\"schemaVersion\":1}", "test");
		LF_Test_PackPatchSourceStub source = new LF_Test_PackPatchSourceStub();
		LF_ConfigService.SetPackSource(source);

		// The clerk as shipped.
		LF_DialogueConfig core = LF_DialogueConfig.Load(CLERK);
		ctx.NotNull(core, "the core clerk dialogue loads");
		if (!core)
		{
			LF_ConfigService.Reset();
			return;
		}
		int coreNodes = core.m_aNodes.Count();
		ctx.EqualStr("greet", core.m_sStart, "the core clerk starts on greet");

		// The good patch.
		LF_DialogueConfig patched = LF_DialogueConfig.Layered(CLERK);
		ctx.NotNull(patched, "the patched clerk is playable");
		if (patched)
		{
			ctx.EqualStr("menu", patched.m_sStart, "the patch sets the start node");
			LF_DialogueLine bye = LF_DialogueFlow.FindLine(patched, "bye");
			ctx.NotNull(bye, "the bye line is still there");
			if (bye)
			{
				ctx.EqualStr("#LF-Dialogue_Clerk_Greet", bye.m_sTextKey, "patch: the line's text key is replaced");
				ctx.EqualStr("SOUND_DLG_CLERK_BYE", bye.m_sEvent, "patch: a field the pack leaves out keeps the core recording");
			}
			// `reload dialogue` (#970) overrides a caption by its text key, so it reaches the patched line too.
			if (bye)
			{
				map<string, string> captions = new map<string, string>();
				captions.Set(bye.m_sTextKey, "Reloaded text");
				LF_DialogueOverrides.Replace(captions);
				ctx.EqualStr("Reloaded text", LF_DialogueOverrides.Text(bye.m_sTextKey), "reload dialogue: a dev caption override applies to the patched line");
				LF_DialogueOverrides.Reset();
			}
			LF_DialogueNode town = LF_DialogueFlow.FindNode(patched, "town");
			ctx.NotNull(town, "the town node is still there");
			if (town)
			{
				ctx.EqualStr("bye", town.m_sLine, "replace: the node says the pack's line");
				ctx.True(!town.m_aOptions || town.m_aOptions.Count() == 0, "replace: the core replies are gone");
			}
			LF_DialogueNode extra = LF_DialogueFlow.FindNode(patched, "extra");
			ctx.NotNull(extra, "add: the pack's node is there");
			if (extra)
				ctx.Equal(1, extra.m_aOptions.Count(), "add: the node has its reply");
			ctx.True(LF_DialogueFlow.FindNode(patched, "work") == null, "remove: the work node is gone");
			ctx.True(LF_DialogueFlow.FindNode(patched, "ghost") == null, "a patch of a missing node adds nothing");
			ctx.Equal(coreNodes, patched.m_aNodes.Count(), "one node added, one removed");
			LF_DialogueNode menu = LF_DialogueFlow.FindNode(patched, "menu");
			if (menu)
			{
				ctx.Equal(2, menu.m_aOptions.Count(), "patch: the reply list is the pack's");
				ctx.EqualStr("extra", menu.m_aOptions[0].m_sNext, "patch: the new reply leads to the added node");
			}
		}

		// The same core file again is untouched: the patch is applied to a fresh copy each time.
		LF_DialogueConfig again = LF_DialogueConfig.Load(CLERK);
		if (again)
		{
			ctx.EqualStr("greet", again.m_sStart, "the shipped file is not changed by the patch");
			ctx.NotNull(LF_DialogueFlow.FindNode(again, "work"), "the shipped file still has the work node");
		}

		// The bad patch (a reply to a node that does not exist): dropped whole, the core plays.
		source.m_bBadDialogue = true;
		LF_DialogueConfig fallback = LF_DialogueConfig.Layered(CLERK);
		ctx.NotNull(fallback, "a broken patch leaves a playable clerk");
		if (fallback)
		{
			ctx.EqualStr("greet", fallback.m_sStart, "a broken patch is dropped: the core start node");
			LF_DialogueLine fallbackBye = LF_DialogueFlow.FindLine(fallback, "bye");
			if (fallbackBye)
				ctx.EqualStr("#LF-Dialogue_Clerk_Bye", fallbackBye.m_sTextKey, "a broken patch is dropped whole: the core line text, not the patch's");
		}

		// Vehicle prices: patch, remove, add and a refused patch of a vehicle no layer lists.
		Resource resource = BaseContainerTools.LoadContainer(LF_VehiclePricesConfig.CONFIG_PATH);
		ctx.True(resource && resource.IsValid(), "the core vehicle prices load");
		if (resource && resource.IsValid())
		{
			LF_VehiclePricesConfig vehicles = LF_VehiclePricesConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(resource.GetResource().ToBaseContainer()));
			int coreVehicles = vehicles.m_aVehicles.Count();
			int coreImpound = vehicles.m_iImpoundPercent;
			vehicles.ApplyLayers();
			ctx.Equal(1200, vehicles.PriceFor(LF_EVehiclePriceAction.BUY, LF_VehicleCatalog.S105) * 100 / Math.Max(vehicles.m_iBuyPercent, 1), "patch: the S105 base price is the pack's");
			ctx.Equal(-1, vehicles.PriceFor(LF_EVehiclePriceAction.BUY, LF_VehicleCatalog.UAZ469), "remove: the UAZ is unlisted");
			ctx.Equal(coreVehicles, vehicles.m_aVehicles.Count(), "one vehicle removed, one added");
			ctx.Equal(900, vehicles.PriceFor(LF_EVehiclePriceAction.BUY, "{8FABDE609C994BC2}") * 100 / Math.Max(vehicles.m_iBuyPercent, 1), "add: the pack's vehicle is priced");
			ctx.Equal(-1, vehicles.PriceFor(LF_EVehiclePriceAction.BUY, "{EA882144DF2D4BEC}"), "a patch of a vehicle no layer lists adds nothing");
			ctx.Equal(20, vehicles.m_iImpoundPercent, "a percent the pack sets wins");
			ctx.True(coreImpound != 20, "the core impound percent was different");
			LF_VehiclePriceEntry s105 = vehicles.Find(LF_VehicleCatalog.S105);
			if (s105)
				ctx.EqualStr(LF_VehicleCatalog.S105_LOCKED, s105.m_SpawnPrefab, "patch: a field the pack leaves out keeps the core value");
		}

		LF_ConfigService.Reset();
	}
}
