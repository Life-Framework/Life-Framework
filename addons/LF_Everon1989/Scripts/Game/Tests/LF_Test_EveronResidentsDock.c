// red-proof: batched run `tools\cli test --no-build --red LF_Test_EveronResidentsDock`: RedProof() skips the overlay load, every outfit load and every spawn, so the five dock residents are not in the config and the clothes and dialogue checks fail.
//! #4466 (W4.2 of the world chapter, #4170): under the Everon 1989 header (the pack leg of `tools\cli test`) the Saint-Philippe
//! dock has its people at work by day: the harbour master at his cabin, two dockers on the work loop in hard hats (#4230), a
//! fishmonger at the market's fish stall and a chandler at the dock's wholesale counter. Each of the five has an outfit preset of
//! real 1989 working clothes the body wears, a prefab that loads and spawns as a dressed character, a name key, a timetable that
//! parses and a home place of a kind (and, for each, the one named place) the island registers. The harbour master and the dockers
//! talk with their voiced dialogues; the fishmonger and the chandler have no talk lines yet (voices are W4.7).
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronResidentsDock : LF_Test
{
	static const ResourceName OVERLAY = "{36D706AEDAB483EC}Configs/Residents/LF_Residents_Everon1989/LF_Residents_Dock_Everon1989.conf";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/residents-dock";
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_ResidentDef Find(LF_ResidentsConfig config, string id)
	{
		foreach (LF_ResidentDef def : config.m_aResidents)
		{
			if (def && def.m_sId == id)
				return def;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! The place kinds the island registers for the dock cast (its place layers).
	protected static bool IslandKind(string kind)
	{
		array<string> kinds = { "HarbourMaster", "Warehouse", "MarketTrader", "WholesaleCounter" };
		return kinds.Find(kind) != -1;
	}

	//------------------------------------------------------------------------------------------------
	protected static int WornCount(IEntity npc)
	{
		int worn;
		IEntity child = npc.GetChildren();
		while (child)
		{
			if (child.FindComponent(BaseLoadoutClothComponent))
				worn++;

			child = child.GetSibling();
		}

		return worn;
	}

	//------------------------------------------------------------------------------------------------
	//! One outfit preset: a jacket, trousers and boots (and a hat when it has one), every piece a loadable civilian garment. \return the head piece.
	protected string CheckOutfit(LF_TestContext ctx, ResourceName outfit, string who)
	{
		Resource resource;
		if (!RedProof())
			resource = BaseContainerTools.LoadContainer(outfit);

		ctx.True(resource != null && resource.IsValid() && resource.GetResource() != null, who + " outfit preset loads");
		if (!resource || !resource.IsValid() || !resource.GetResource())
			return "";

		BaseContainer container = resource.GetResource().ToBaseContainer();
		ctx.EqualStr("LF_NpcOutfitPreset", container.GetClassName(), who + " outfit is an LF_NpcOutfitPreset");

		string head;
		array<string> slots = { "m_Torso", "m_Legs", "m_Feet", "m_Head" };
		foreach (string slot : slots)
		{
			ResourceName cloth;
			container.Get(slot, cloth);
			if (cloth.IsEmpty())
			{
				ctx.True(slot == "m_Head", who + " outfit fills " + slot);
				continue;
			}

			if (slot == "m_Head")
				head = cloth;

			Resource clothResource = Resource.Load(cloth);
			ctx.True(clothResource && clothResource.IsValid(), who + " outfit " + slot + " loads: " + cloth);
			if (slot != "m_Feet")
				ctx.True(cloth.IndexOf("_M70") == -1 && cloth.IndexOf("_M88") == -1 && cloth.IndexOf("Vest_") == -1, who + " " + slot + " is civilian work wear (" + cloth + ")");
		}

		return head;
	}

	//------------------------------------------------------------------------------------------------
	//! A resident's body: the prefab loads, spawns as a real character, wears a jacket, trousers and boots, and talks with `dialogue` when one is named.
	protected void CheckBody(LF_TestContext ctx, ResourceName prefab, string who, string dialogueId)
	{
		ctx.False(prefab.IsEmpty(), who + " has a prefab of their own");
		if (prefab.IsEmpty())
			return;

		Resource resource = Resource.Load(prefab);
		ctx.True(resource && resource.IsValid(), who + " prefab loads: " + prefab);

		IEntity body;
		if (!RedProof())
			body = LF_Utils.SpawnEntityPrefab(prefab, "0 -50 0");

		ctx.True(SCR_ChimeraCharacter.Cast(body) != null, who + " spawns as a real character (can stand a shift)");
		if (!body)
			return;

		ctx.True(WornCount(body) >= 3, who + " wears a jacket, trousers and boots");
		if (!dialogueId.IsEmpty())
		{
			LF_DialogueConfig dialogue = LF_DialogueComponent.ConfigOf(body);
			ctx.NotNull(dialogue, who + " has a playable dialogue");
			if (dialogue)
				ctx.EqualStr(dialogueId, dialogue.m_sId, who + " talks with the '" + dialogueId + "' dialogue");
		}

		SCR_EntityHelper.DeleteEntityAndChildren(body);
	}

	//------------------------------------------------------------------------------------------------
	//! A resident's row: a kind and one named place the island registers, a valid distinct day of known activities.
	//! \return how many of its lines use a work activity (carry, hammer, sweep).
	protected int CheckRow(LF_TestContext ctx, LF_ResidentDef def, string kind, string placeId)
	{
		string id = def.m_sId;
		ctx.EqualStr(kind, def.m_sHomeKind, id + " stands at the '" + kind + "' place kind");
		ctx.True(IslandKind(def.m_sHomeKind), id + " place kind '" + def.m_sHomeKind + "' is a kind the island registers");
		ctx.EqualStr(placeId, def.m_sHomePlaceId, id + " is bound to its own place");
		ctx.True(def.m_aTimetable != null && def.m_aTimetable.Count() >= 3, id + " has a day of at least three lines");
		if (!def.m_aTimetable)
			return 0;

		int workLines;
		set<int> starts = new set<int>();
		foreach (LF_ResidentEntryDef line : def.m_aTimetable)
		{
			int start = LF_ResidentTimetable.MinuteOf(line.m_sTime);
			ctx.True(start >= 0, string.Format("%1 line %2 is a 24 h HH:MM time", id, line.m_sTime));
			ctx.False(starts.Contains(start), string.Format("%1 has one line starting at %2", id, line.m_sTime));
			starts.Insert(start);
			ctx.True(line.m_sPlace == LF_ResidentTimetable.ROLE_HOME, string.Format("%1 line %2 is at the place itself", id, line.m_sTime));
			bool work = LF_ResidentWorkLoop.IsWork(line.m_sActivity);
			if (work)
				workLines++;

			ctx.True(work || line.m_sActivity == "serve" || line.m_sActivity == "loiter" || line.m_sActivity == "idle", string.Format("%1 line %2 does a known activity ('%3')", id, line.m_sTime, line.m_sActivity));
		}

		// By day the resident is at their post: the day builds, and the first line is in the morning hours.
		LF_ResidentTimetable timetable = LF_ResidentTimetable.Build(def, new LF_Test_AnyKinds());
		ctx.NotNull(timetable, id + " timetable builds");
		int first = 24 * 60;
		foreach (LF_ResidentEntryDef begin : def.m_aTimetable)
			first = Math.Min(first, LF_ResidentTimetable.MinuteOf(begin.m_sTime));

		ctx.True(first >= 5 * 60 && first <= 8 * 60, string.Format("%1 starts the day between 05:00 and 08:00 (%2 min)", id, first));
		return workLines;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_MissionHeader header = LF_MissionHeader.GetCurrent();
		ctx.NotNull(header, "the pack leg runs under an LF mission header");

		// The new file is a folder overlay of the residents overlay, loaded after it in path order (no header line).
		array<ref Resource> resources = {};
		array<ResourceName> names = {};
		if (!RedProof())
			LF_ConfigOverlay.LoadPackOverlays(LF_ResidentsConfig.CONFIG_PATH, LF_ResidentsConfig, "residents", resources, names);

		ctx.True(names.Find(OVERLAY) != -1, "the residents loader picks up the dock cast overlay");

		LF_ResidentsConfig config;
		if (!RedProof())
			config = LF_ResidentsConfig.Get();

		ctx.NotNull(config, "the residents config loads under the pack");
		if (!config)
			return;

		array<string> ids = { "dock_harbour_master", "dock_docker_1", "dock_docker_2", "dock_fishmonger", "dock_chandler" };
		array<string> outfits = {
			"{45949C0D22AF0B6F}Configs/NpcOutfits/LF_Outfit_DockHarbourMaster.conf",
			"{F2C48A895452DDCF}Configs/NpcOutfits/LF_Outfit_DockDocker1.conf",
			"{74E4DC6E4843B82C}Configs/NpcOutfits/LF_Outfit_DockDocker2.conf",
			"{34ACF2F97F1257CA}Configs/NpcOutfits/LF_Outfit_DockFishmonger.conf",
			"{08DD053B904442FB}Configs/NpcOutfits/LF_Outfit_DockChandler.conf"
		};
		array<string> prefabs = {
			"{27F4DCF1ED60D6AD}Configs/Residents/Npcs/LF_Resident_DockHarbourMaster.et",
			"{7ADEE9AA26430699}Configs/Residents/Npcs/LF_Resident_DockDocker1.et",
			"{C19715E28D61D614}Configs/Residents/Npcs/LF_Resident_DockDocker2.et",
			"{D57E9062A0FC2341}Configs/Residents/Npcs/LF_Resident_DockFishmonger.et",
			"{73110EC1F87E9441}Configs/Residents/Npcs/LF_Resident_DockChandler.et"
		};
		array<string> kinds = { "HarbourMaster", "Warehouse", "Warehouse", "MarketTrader", "WholesaleCounter" };
		array<string> places = { "saint-philippe.harbour", "saint-philippe-dock.warehouse", "saint-philippe-dock.warehouse", "saint-philippe.market", "saint-philippe-dock.wholesale" };
		// The dialogue each body talks with; the fishmonger and the chandler have no lines yet (W4.7).
		array<string> dialogues = { "harbour_master", "dockworker", "dockworker", "", "" };

		set<string> distinctPrefabs = new set<string>();
		for (int i = 0; i < ids.Count(); i++)
		{
			string id = ids[i];
			string head = CheckOutfit(ctx, outfits[i], id);
			CheckBody(ctx, prefabs[i], id, dialogues[i]);
			distinctPrefabs.Insert(prefabs[i]);

			string key = "#" + "LF-Resident_" + id + "_Name";
			string name = WidgetManager.Translate(key);
			ctx.True(!name.IsEmpty() && name != key && name.IndexOf("#") == -1, id + " name key resolves: " + key);

			LF_ResidentDef def = Find(config, id);
			ctx.NotNull(def, "resident " + id + " is in the cast");
			if (!def)
				continue;

			ctx.EqualStr(prefabs[i], def.m_sPrefab, id + " row names their own prefab");
			int workLines = CheckRow(ctx, def, kinds[i], places[i]);

			// The dockers wear a hard hat and work the loop (#4230); nobody else does.
			if (id.IndexOf("dock_docker") == 0)
			{
				ctx.True(head.IndexOf("Hat_Hard_01") != -1, id + " wears a hard hat (" + head + ")");
				ctx.True(workLines >= 2, id + " works the carry and hammer loop on at least two lines");
			}
			else
			{
				ctx.Equal(0, workLines, id + " does no loop work");
			}
		}

		ctx.Equal(ids.Count(), distinctPrefabs.Count(), "five dock people are five bodies");
	}
}
