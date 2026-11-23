// red-proof: batched run `tools\cli test --no-build --red LF_Test_EveronResidentsShops`: RedProof() skips the overlay load, every outfit load and every spawn, so the keepers are not in the config, the plate text is empty and the clothes checks fail. Talk wiring (#4578): on the prefabs before this card the run failed exactly the six `shop_baker|shop_butcher|shop_pharmacist has a playable dialogue / a talk action` lines.
//! #4475 (W4.5 of the world chapter, #4170): under the Everon 1989 header (the pack leg of `tools\cli test`) the shop
//! cast resolves. Six keepers (a baker, a butcher, a pharmacist, a barber, a newsagent and a postmaster; every body is
//! male) each have an outfit preset of real 1989 working clothes the body actually wears, a prefab that loads and spawns
//! as a dressed character, and a name key. The one keeper whose work kind has a place of his own on the island (the barber's chair)
//! has a resident row: its timetable parses, is at work only inside the hours the
//! opening-hours plate (#4236) prints for it, and stands on a home and a work kind the island registers. The four
//! StoreCounter keepers and the postmaster have rows too (#4560): each names the one shop they work at
//! (`m_sWorkPlaceId`), so four keepers of one kind stand at their own counters and each shop's plate prints its own
//! keeper's hours. The postmaster and neighbour_anselme share the one depot counter by shift (validate-npc-spots).
//! The bartender is the
//! placed barkeeper of #4439, not a second resident: his prefab and outfit are checked here.
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronResidentsShops : LF_Test
{
	static const ResourceName OVERLAY = "{C0B790996D5743CF}Configs/Residents/LF_Residents_Everon1989/LF_Residents_Shops_Everon1989.conf";
	static const ResourceName BARKEEPER_PREFAB = "{4439B0A5C0000003}Configs/Npcs/LF_Npc_Townsfolk_Barkeeper.et";
	static const ResourceName BARKEEPER_OUTFIT = "{4439B0A5C0000004}Configs/NpcOutfits/LF_Outfit_Barkeeper.conf";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/residents-shops";
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
	//! The place kinds the island registers for a shop (its place layers), as LF_Test_EveronResidents lists them.
	protected static bool IslandKind(string kind)
	{
		array<string> kinds = { "StoreCounter", "BarberChair", "ParcelDepot", "PubBar", "House" };
		return kinds.Find(kind) != -1;
	}

	//------------------------------------------------------------------------------------------------
	//! What the body wears: the cloth children it spawned with.
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
	//! One outfit preset: a jacket, trousers and boots (and a hat when it has one), every piece a loadable 1989 garment.
	protected void CheckOutfit(LF_TestContext ctx, ResourceName outfit, string who)
	{
		Resource resource;
		if (!RedProof())
			resource = BaseContainerTools.LoadContainer(outfit);

		ctx.True(resource != null && resource.IsValid() && resource.GetResource() != null, who + " outfit preset loads");
		if (!resource || !resource.IsValid() || !resource.GetResource())
			return;

		BaseContainer container = resource.GetResource().ToBaseContainer();
		ctx.EqualStr("LF_NpcOutfitPreset", container.GetClassName(), who + " outfit is an LF_NpcOutfitPreset");

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

			Resource clothResource = Resource.Load(cloth);
			ctx.True(clothResource && clothResource.IsValid(), who + " outfit " + slot + " loads: " + cloth);
			// Era: nothing military in a shop keeper's work clothes (a boot is the one vanilla pair we have).
			if (slot != "m_Feet")
				ctx.True(cloth.IndexOf("_M70") == -1 && cloth.IndexOf("_M88") == -1 && cloth.IndexOf("Vest_") == -1, who + " " + slot + " is civilian work wear (" + cloth + ")");
		}
	}

	//------------------------------------------------------------------------------------------------
	//! A keeper's body: the prefab loads, spawns as a real character and wears at least a jacket, trousers and boots.
	protected void CheckBody(LF_TestContext ctx, ResourceName prefab, string who, bool talks = false)
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
		if (talks)
			CheckTalk(ctx, body, who);

		SCR_EntityHelper.DeleteEntityAndChildren(body);
	}

	//------------------------------------------------------------------------------------------------
	//! The placed clerk the keeper replaced (#4578) carried a talk action: the keeper keeps it, with the shopkeeper dialogue.
	protected void CheckTalk(LF_TestContext ctx, IEntity body, string who)
	{
		LF_DialogueConfig dialogue = LF_DialogueComponent.ConfigOf(body);
		ctx.NotNull(dialogue, who + " has a playable dialogue");
		if (dialogue)
			ctx.EqualStr("shopkeeper", dialogue.m_sId, who + " talks with the shopkeeper dialogue");

		bool talk;
		BaseActionsManagerComponent manager = BaseActionsManagerComponent.Cast(body.FindComponent(BaseActionsManagerComponent));
		if (manager)
		{
			array<BaseUserAction> actions = {};
			manager.GetActionsList(actions);
			foreach (BaseUserAction action : actions)
			{
				if (LF_DialogueTalkAction.Cast(action))
					talk = true;
			}
		}

		ctx.True(talk, who + " has a talk action");
	}

	//------------------------------------------------------------------------------------------------
	//! Game minutes this resident's lines put them at work, counted independently of the plate (the latest line at or before the minute).
	protected static bool WorksAt(LF_ResidentDef def, int minute)
	{
		LF_ResidentEntryDef best;
		LF_ResidentEntryDef last;
		int bestStart = -1;
		int lastStart = -1;
		foreach (LF_ResidentEntryDef line : def.m_aTimetable)
		{
			int start = LF_ResidentTimetable.MinuteOf(line.m_sTime);
			if (start <= minute && start > bestStart)
			{
				best = line;
				bestStart = start;
			}

			if (start > lastStart)
			{
				last = line;
				lastStart = start;
			}
		}

		if (!best)
			best = last;

		return best && best.m_sPlace == LF_ResidentTimetable.ROLE_WORK;
	}

	//------------------------------------------------------------------------------------------------
	//! A keeper's row: kinds the island registers, a valid distinct day, and the shop's plate says what the timetable says.
	protected void CheckRow(LF_TestContext ctx, LF_ResidentDef def, string plate, string shopId)
	{
		string id = def.m_sId;
		ctx.True(IslandKind(def.m_sHomeKind), id + " home kind '" + def.m_sHomeKind + "' is a kind the island registers");
		ctx.True(IslandKind(def.m_sWorkKind), id + " work kind '" + def.m_sWorkKind + "' is a kind the island registers");
		ctx.False(def.m_sHomePlaceId.IsEmpty(), id + " lives in one named house");
		ctx.EqualStr(shopId, def.m_sWorkPlaceId, id + " works at the one named shop");

		set<int> starts = new set<int>();
		foreach (LF_ResidentEntryDef line : def.m_aTimetable)
		{
			int start = LF_ResidentTimetable.MinuteOf(line.m_sTime);
			ctx.True(start >= 0, string.Format("%1 line %2 is a 24 h HH:MM time", id, line.m_sTime));
			ctx.False(starts.Contains(start), string.Format("%1 has one line starting at %2", id, line.m_sTime));
			starts.Insert(start);
			ctx.True(line.m_sAnchor.IsEmpty() || line.m_sAnchor == "counter" || line.m_sAnchor == "bench" || line.m_sAnchor.IndexOf("door") == 0, string.Format("%1 line %2 stands on a known anchor ('%3')", id, line.m_sTime, line.m_sAnchor));
			ctx.True(line.m_sActivity == "serve" || line.m_sActivity == "sit" || line.m_sActivity == "loiter" || line.m_sActivity == "idle", string.Format("%1 line %2 does a known activity ('%3')", id, line.m_sTime, line.m_sActivity));
			if (line.m_sPlace == LF_ResidentTimetable.ROLE_WORK)
				ctx.EqualStr("counter", line.m_sAnchor, id + " works at the counter");
		}

		// Every shop keeper serves a few hours of the day, at home to sleep at night.
		LF_ResidentTimetable day = LF_ResidentTimetable.Build(def, new LF_Test_AnyKinds());
		ctx.NotNull(day, id + " timetable builds");
		if (!day)
			return;

		array<int> spans = {};
		LF_HoursPlate.Spans(day, spans);
		ctx.True(spans.Count() >= 2, id + " has opening hours");
		ctx.EqualStr(plate, LF_HoursPlate.Text(day), id + " opening-hours plate text");
		ctx.EqualStr(shopId, LF_HoursPlate.WorkPlaceId(day), id + " plate finds its own shop by the work place id");

		int mismatches;
		int openMinutes;
		for (int minute = 0; minute < LF_ResidentTimetable.MINUTES_PER_DAY; minute++)
		{
			bool works = WorksAt(def, minute);
			if (works)
				openMinutes++;

			if (works != LF_HoursPlate.IsInside(spans, minute))
				mismatches++;
		}

		ctx.Equal(0, mismatches, id + ": the plate and the timetable agree on every minute");
		ctx.True(openMinutes >= 6 * 60 && openMinutes <= 12 * 60, string.Format("%1 keeps the shop six to twelve hours (%2 min)", id, openMinutes));
		ctx.False(WorksAt(def, 2 * 60), id + " is not at the counter at 02:00");
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

		ctx.True(names.Find(OVERLAY) != -1, "the residents loader picks up the shop cast overlay");

		LF_ResidentsConfig config;
		if (!RedProof())
			config = LF_ResidentsConfig.Get();

		ctx.NotNull(config, "the residents config loads under the pack");
		if (!config)
			return;

		// id, outfit, plate text of the shop's own hours ("" while the keeper has no row), display name.
		array<string> ids = { "shop_baker", "shop_butcher", "shop_pharmacist", "shop_barber", "shop_newsagent", "shop_postmaster" };
		array<string> outfits = {
			"{17EB5E84FD1C1A3E}Configs/NpcOutfits/LF_Outfit_ShopBaker.conf",
			"{B7DAF66838F6B0C4}Configs/NpcOutfits/LF_Outfit_ShopButcher.conf",
			"{812E1B66861F0D39}Configs/NpcOutfits/LF_Outfit_ShopPharmacist.conf",
			"{507DFD3E96314E06}Configs/NpcOutfits/LF_Outfit_ShopBarber.conf",
			"{AA4FD40424DCE78D}Configs/NpcOutfits/LF_Outfit_ShopNewsagent.conf",
			"{8531C4F1F9F11B48}Configs/NpcOutfits/LF_Outfit_ShopPostmaster.conf"
		};
		array<string> prefabs = {
			"{5A6F878F7DC4CA7D}Configs/Residents/Npcs/LF_Resident_ShopBaker.et",
			"{F2BB0F17CE5C6F50}Configs/Residents/Npcs/LF_Resident_ShopButcher.et",
			"{8DD9F44A94D28623}Configs/Residents/Npcs/LF_Resident_ShopPharmacist.et",
			"{8FD9179354919E23}Configs/Residents/Npcs/LF_Resident_ShopBarber.et",
			"{C3E6B21FB417F1BF}Configs/Residents/Npcs/LF_Resident_ShopNewsagent.et",
			"{8416E7C7F06BD33D}Configs/Residents/Npcs/LF_Resident_ShopPostmaster.et"
		};
		array<string> plates = { "OPEN 6 AM - 2 PM", "OPEN 7:30 AM - 5 PM", "OPEN 9 AM - 6 PM", "OPEN 9 AM - 12:30 PM, 1:30 PM - 6 PM", "OPEN 7 AM - 7 PM", "OPEN 8 AM - 2 PM" };
		// The LF_Places id of the one shop each keeper works at (the barber's chair has no id of its own).
		array<string> shops = { "saint-philippe.bakery", "saint-philippe.butcher", "saint-philippe.pharmacy", "", "saint-philippe.newsagent", "saint-philippe.parcel" };

		set<string> distinctPrefabs = new set<string>();
		for (int i = 0; i < ids.Count(); i++)
		{
			string id = ids[i];
			CheckOutfit(ctx, outfits[i], id);
			// The baker, butcher and pharmacist replaced a placed clerk that could be talked to (#4578).
			CheckBody(ctx, prefabs[i], id, id == "shop_baker" || id == "shop_butcher" || id == "shop_pharmacist");
			distinctPrefabs.Insert(prefabs[i]);

			string key = "#" + "LF-Resident_" + id + "_Name";
			string name = WidgetManager.Translate(key);
			ctx.True(!name.IsEmpty() && name != key && name.IndexOf("#") == -1, id + " name key resolves: " + key);

			LF_ResidentDef def = Find(config, id);
			if (plates[i].IsEmpty())
				continue;

			ctx.NotNull(def, "resident " + id + " is in the cast");
			if (!def)
				continue;

			ctx.EqualStr(prefabs[i], def.m_sPrefab, id + " row names their own prefab");
			CheckRow(ctx, def, plates[i], shops[i]);
		}

		ctx.Equal(ids.Count(), distinctPrefabs.Count(), "six keepers are six bodies");

		// The bartender is the placed barkeeper of #4439 (the crime outlet), never a second resident: no resident row works the pub bar.
		foreach (LF_ResidentDef each : config.m_aResidents)
		{
			if (each)
				ctx.True(each.m_sWorkKind != "PubBar", each.m_sId + " does not double the placed barkeeper");
		}

		CheckOutfit(ctx, BARKEEPER_OUTFIT, "barkeeper");
		CheckBody(ctx, BARKEEPER_PREFAB, "barkeeper");
	}
}
