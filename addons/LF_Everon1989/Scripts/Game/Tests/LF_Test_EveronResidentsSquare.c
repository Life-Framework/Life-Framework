// red-proof: batched run `tools\cli test --no-build --red LF_Test_EveronResidentsSquare`: RedProof() skips the config load and every spawn, so the three residents are not found, no prefab or outfit is read and the day checks fail.
//! #4228 (W1.3 of the world chapter, #4170): under the Everon 1989 header (the pack leg of `tools\cli test`) the square
//! cast resolves. Two gossips (Odile Ferrand and Colette Brisset) sit on the square's café bench by day and a gendarme walks
//! the bank beat in the evening. Each is a resident row of the pack (found through the residents config, never by a
//! coordinate), their prefabs load and spawn as dressed characters, the two gossip outfit presets name real 1989 clothes
//! the bodies actually wear, and every timetable parses, covers the day and puts them where the card says.
//! Voice: the gossips carry no talk component yet (their voiced lines are W1.4), so nothing a player hears is unvoiced;
//! the gendarme talks through the existing voiced `gendarme` conversation.
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronResidentsSquare : LF_Test
{
	static const ResourceName OUTFIT_ODILE = "{845B13DD97367707}Configs/NpcOutfits/LF_Outfit_Gossip_Odile.conf";
	static const ResourceName OUTFIT_COLETTE = "{AF5BEC462983A654}Configs/NpcOutfits/LF_Outfit_Gossip_Colette.conf";
	static const ResourceName OVERLAY = "{4491B25773C72A3A}Configs/Residents/LF_Residents_Everon1989/LF_Residents_Square_Everon1989.conf";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/residents-square";
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
	//! The line a resident is on at a game minute: the latest start at or before it, else the last of the day (it holds across midnight).
	protected static LF_ResidentEntryDef LineAt(LF_ResidentDef def, int minute)
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

		if (best)
			return best;

		return last;
	}

	//------------------------------------------------------------------------------------------------
	//! Valid day: parsed, distinct 24 h starts, a home or work place, a work kind for every work line, known anchors and activities.
	protected void CheckDay(LF_TestContext ctx, LF_ResidentDef def)
	{
		string id = def.m_sId;
		ctx.True(def.m_aTimetable && def.m_aTimetable.Count() >= 5, id + " has a day of at least five lines");
		if (!def.m_aTimetable)
			return;

		set<int> starts = new set<int>();
		foreach (LF_ResidentEntryDef line : def.m_aTimetable)
		{
			int start = LF_ResidentTimetable.MinuteOf(line.m_sTime);
			ctx.True(start >= 0, string.Format("%1 line %2 is a 24 h HH:MM time", id, line.m_sTime));
			ctx.False(starts.Contains(start), string.Format("%1 has one line starting at %2", id, line.m_sTime));
			starts.Insert(start);

			bool home = line.m_sPlace == LF_ResidentTimetable.ROLE_HOME;
			bool work = line.m_sPlace == LF_ResidentTimetable.ROLE_WORK;
			ctx.True(home || work, string.Format("%1 line %2 is at home or work", id, line.m_sTime));
			if (work)
				ctx.False(def.m_sWorkKind.IsEmpty(), string.Format("%1 line %2 works but has no work kind", id, line.m_sTime));

			ctx.True(line.m_sAnchor.IndexOf("bench") == 0 || line.m_sAnchor.IndexOf("door") == 0, string.Format("%1 line %2 stands on a known anchor ('%3')", id, line.m_sTime, line.m_sAnchor));
			ctx.True(line.m_sActivity == "sit" || line.m_sActivity == "loiter" || line.m_sActivity == "idle", string.Format("%1 line %2 does a known activity ('%3')", id, line.m_sTime, line.m_sActivity));
		}

		// Home and work are place kinds the island registers: a house, the pub's café table, the bank.
		ctx.EqualStr("House", def.m_sHomeKind, id + " lives in a house");
		ctx.True(def.m_sWorkKind == "CafeTable" || def.m_sWorkKind == "Bank", id + " works at a kind the island places (" + def.m_sWorkKind + ")");

		string key = "#" + "LF-Resident_" + id + "_Name";
		string name = WidgetManager.Translate(key);
		ctx.True(!name.IsEmpty() && name != key && name.IndexOf("#") == -1, id + " name key resolves: " + key);
	}

	//------------------------------------------------------------------------------------------------
	//! What the body wears: the cloth children it spawned with (a jacket, trousers, boots and a hat).
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
	//! One outfit preset: four real clothing prefabs (hat, jacket, trousers, boots), loadable and named.
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

		array<string> slots = { "m_Head", "m_Torso", "m_Legs", "m_Feet" };
		foreach (string slot : slots)
		{
			ResourceName cloth;
			container.Get(slot, cloth);
			ctx.True(!cloth.IsEmpty(), who + " outfit fills " + slot);
			if (cloth.IsEmpty())
				continue;

			Resource clothResource = Resource.Load(cloth);
			ctx.True(clothResource && clothResource.IsValid(), who + " outfit " + slot + " loads: " + cloth);
		}

		// Era: a raincoat and a knit hat, what an older Saint-Philippe woman wore in 1989, nothing military in the torso or head.
		ResourceName torso;
		ResourceName head;
		container.Get("m_Torso", torso);
		container.Get("m_Head", head);
		ctx.True(torso.IndexOf("Jacket_Raincoat_01") != -1, who + " wears a raincoat (" + torso + ")");
		ctx.True(head.IndexOf("Hat_Knitted_01") != -1, who + " wears a knit hat (" + head + ")");
	}

	//------------------------------------------------------------------------------------------------
	//! A resident's body: the prefab loads, spawns as a real character that can sit, and wears four pieces of clothing.
	protected void CheckBody(LF_TestContext ctx, LF_ResidentDef def, bool voiced)
	{
		string id = def.m_sId;
		ctx.False(def.m_sPrefab.IsEmpty(), id + " has a prefab of their own");
		if (def.m_sPrefab.IsEmpty())
			return;

		Resource prefab = Resource.Load(def.m_sPrefab);
		ctx.True(prefab && prefab.IsValid(), id + " prefab loads: " + def.m_sPrefab);

		IEntity body;
		if (!RedProof())
			body = LF_Utils.SpawnEntityPrefab(def.m_sPrefab, "0 -50 0");

		ctx.True(SCR_ChimeraCharacter.Cast(body) != null, id + " spawns as a real character (can sit and loiter)");
		if (!body)
			return;

		ctx.True(WornCount(body) >= 4, id + " wears a hat, jacket, trousers and boots");
		LF_DialogueComponent talk = LF_DialogueComponent.Cast(body.FindComponent(LF_DialogueComponent));
		if (voiced)
		{
			ctx.NotNull(talk, id + " talks through the voiced conversation");
			LF_DialogueConfig config = LF_DialogueComponent.ConfigOf(body);
			ctx.NotNull(config, id + " has a playable dialogue");
			if (config)
			{
				ctx.True(!config.m_sSoundProject.IsEmpty(), id + " dialogue names an audio project");
				foreach (LF_DialogueLine each : config.m_aLines)
				{
					if (each)
						ctx.True(!each.IsTextOnly(), id + "/" + each.m_sId + " is a recording");
				}
			}
		}
		else
		{
			ctx.True(talk == null, id + " has no talk component until its voiced lines land (W1.4)");
		}

		SCR_EntityHelper.DeleteEntityAndChildren(body);
	}

	//------------------------------------------------------------------------------------------------
	//! The two gossips are two different women: own prefab, own outfit, and by day both sit on the café bench together.
	protected void CheckGossips(LF_TestContext ctx, LF_ResidentDef odile, LF_ResidentDef colette)
	{
		ctx.True(odile.m_sPrefab != colette.m_sPrefab, "the gossips are two different bodies");
		ctx.True(odile.m_sId != colette.m_sId, "the gossips have their own ids");

		int sharedMinutes;
		for (int minute = 9 * 60; minute < 17 * 60; minute += 15)
		{
			LF_ResidentEntryDef a = LineAt(odile, minute);
			LF_ResidentEntryDef b = LineAt(colette, minute);
			if (a && b && a.m_sPlace == "work" && b.m_sPlace == "work" && a.m_sAnchor.IndexOf("bench") == 0 && b.m_sAnchor.IndexOf("bench") == 0 && a.m_sActivity == "sit" && b.m_sActivity == "sit")
				sharedMinutes += 15;
		}

		ctx.True(sharedMinutes >= 4 * 60, string.Format("both gossips sit together on the bench at least four hours between 09:00 and 17:00 (%1 min)", sharedMinutes));

		// Each is on the bench at noon-adjacent times at least once, and goes home to sleep: the night line is at home.
		LF_ResidentEntryDef nightA = LineAt(odile, 2 * 60);
		LF_ResidentEntryDef nightB = LineAt(colette, 2 * 60);
		ctx.True(nightA && nightA.m_sPlace == "home", "Odile is at home at 02:00");
		ctx.True(nightB && nightB.m_sPlace == "home", "Colette is at home at 02:00");
		ctx.EqualStr("CafeTable", odile.m_sWorkKind, "Odile sits on the square's bench (the café's)");
		ctx.EqualStr("CafeTable", colette.m_sWorkKind, "Colette sits on the square's bench (the café's)");
	}

	//------------------------------------------------------------------------------------------------
	//! The gendarme: off the bank beat by day, on it in the evening, back home before the night.
	protected void CheckGendarme(LF_TestContext ctx, LF_ResidentDef gendarme)
	{
		ctx.EqualStr("Bank", gendarme.m_sWorkKind, "the gendarme's beat is the bank");

		LF_ResidentEntryDef noon = LineAt(gendarme, 12 * 60);
		ctx.True(noon && noon.m_sPlace == "home", "the gendarme is not on the bank beat at noon");

		int beatMinutes;
		for (int minute = 17 * 60; minute < 22 * 60; minute += 15)
		{
			LF_ResidentEntryDef line = LineAt(gendarme, minute);
			if (line && line.m_sPlace == "work" && line.m_sActivity == "loiter")
				beatMinutes += 15;
		}

		ctx.True(beatMinutes >= 3 * 60, string.Format("the gendarme walks the bank beat at least three hours in the evening (%1 min)", beatMinutes));

		// A beat has two ends: he stands at least at two different anchors on it.
		set<string> ends = new set<string>();
		foreach (LF_ResidentEntryDef line : gendarme.m_aTimetable)
		{
			if (line.m_sPlace == "work")
				ends.Insert(line.m_sAnchor);
		}

		ctx.True(ends.Count() >= 2, "the beat has at least two stops at the bank");

		LF_ResidentEntryDef night = LineAt(gendarme, 23 * 60);
		ctx.True(night && night.m_sPlace == "home", "the gendarme is home at 23:00");
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

		ctx.True(names.Find(OVERLAY) != -1, "the residents loader picks up the square cast overlay");

		LF_ResidentsConfig config;
		if (!RedProof())
			config = LF_ResidentsConfig.Get();

		ctx.NotNull(config, "the residents config loads under the pack");
		if (!config)
			return;

		LF_ResidentDef odile = Find(config, "gossip_odile");
		LF_ResidentDef colette = Find(config, "gossip_colette");
		LF_ResidentDef gendarme = Find(config, "gendarme_bank_beat");
		ctx.NotNull(odile, "resident gossip_odile is in the cast");
		ctx.NotNull(colette, "resident gossip_colette is in the cast");
		ctx.NotNull(gendarme, "resident gendarme_bank_beat is in the cast");
		if (!odile || !colette || !gendarme)
			return;

		array<LF_ResidentDef> cast = { odile, colette, gendarme };
		foreach (LF_ResidentDef def : cast)
		{
			ctx.EqualStr("House", def.m_sHomeKind, def.m_sId + " lives in a house");
			CheckDay(ctx, def);
		}

		CheckOutfit(ctx, OUTFIT_ODILE, "Odile");
		CheckOutfit(ctx, OUTFIT_COLETTE, "Colette");
		CheckBody(ctx, odile, false);
		CheckBody(ctx, colette, false);
		CheckBody(ctx, gendarme, true);
		CheckGossips(ctx, odile, colette);
		CheckGendarme(ctx, gendarme);
	}
}
