// red-proof: before Configs/Residents/LF_Residents_Everon1989.conf and its header line existed, the overlay
//   was not listed, the two core examples were still in the config and no resident carried a pack prefab,
//   so `the header lists the residents overlay` and `the core example residents are gone` failed. The
//   run that went red and then green is in the PR; `RedProof()` skips the overlay merge for the batched run.
//! everon/residents (#2906): under the Everon 1989 header (the pack leg) the residents overlay loads and
//! merges, the core's two neutral examples are gone, Saint-Philippe has six to eight named residents, every
//! resident's prefab loads, every home and work kind is a kind this pack's places register or its place
//! templates name, every timetable parses and covers the whole 24 h, and the names are pack `#` keys.
//! A site resident (a work-site foreman, #4237: home kind `Workplace`, a place id, no anchor) stands at the
//! site's place origin, so it needs no anchor and does not count toward the Saint-Philippe cast of six to twenty (the shop cast of #4475 and #4560 grew it).
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronResidents : LF_Test
{
	//! The home kind of a site resident: a foreman who stands at one work site all day.
	static const string SITE_KIND = "Workplace";
	static const ResourceName OVERLAY = "{742981AD457A5EDC}Configs/Residents/LF_Residents_Everon1989.conf";

	//! The place kinds of the 1989 pack: the ones its place layers register plus the ones its town and
	//! village place templates (tools/town/templates) name. A resident's home or work is one of these.
	protected static bool IsPackKind(string kind)
	{
		array<string> kinds = {
			"Spawn", "Clerk", "LicenceDesk", "Bank", "BankVault", "BankCounter", "TownBoard", "MapBoard",
			"ClothingCounter", "GunCounter", "StoreCounter", "CafeTable", "HostelBed", "House", "FuelTill",
			"ParcelDepot", "PubBar", "Hall", "Dock", "FishCounter", "Garage", "BarberChair", SITE_KIND,
			"HarbourMaster", "Warehouse", "MarketTrader", "WholesaleCounter"
		};
		return kinds.Find(kind) != -1;
	}

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/residents";
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
	override void Run(LF_TestContext ctx)
	{
		LF_MissionHeader header = LF_MissionHeader.GetCurrent();
		ctx.NotNull(header, "the pack leg runs under an LF mission header");
		if (header && header.m_aLF_ConfigOverlays)
			ctx.True(header.m_aLF_ConfigOverlays.Find(OVERLAY) != -1, "the header lists the residents overlay");
		else
			ctx.Fail("the header lists no config overlays");

		array<ref Resource> resources = {};
		array<ResourceName> names = {};
		if (!RedProof())
			LF_ConfigOverlay.LoadPackOverlays(LF_ResidentsConfig.CONFIG_PATH, LF_ResidentsConfig, "residents", resources, names);

		ctx.True(names.Find(OVERLAY) != -1, "the residents loader picks up the pack overlay");

		// The act: the merged config, as the spawner and the timetable read it.
		LF_ResidentsConfig config = LF_ResidentsConfig.Get();
		ctx.NotNull(config, "the residents config loads under the pack");
		if (!config)
			return;

		ctx.True(Find(config, "example_shopkeeper") == null, "the core example shopkeeper is removed");
		ctx.True(Find(config, "example_neighbour") == null, "the core example neighbour is removed");
		int townCast = 0;
		foreach (LF_ResidentDef counted : config.m_aResidents)
		{
			if (counted && counted.m_sHomeKind != SITE_KIND && counted.m_sId.IndexOf("dock_") != 0)
				townCast++;
		}

		ctx.True(townCast >= 6 && townCast <= 20, string.Format("Saint-Philippe has six to twenty town residents, site foremen apart (has %1)", townCast));

		// The named cast the card asks for.
		array<string> cast = { "cafe_owner", "old_man", "night_shift", "teller", "store_clerk", "neighbour_anselme", "neighbour_mireille" };
		foreach (string castId : cast)
			ctx.NotNull(Find(config, castId), "resident '" + castId + "' is in the cast");

		set<string> seen = new set<string>();
		foreach (LF_ResidentDef def : config.m_aResidents)
		{
			if (!def)
				continue;

			string id = def.m_sId;
			ctx.False(seen.Contains(id), "resident id '" + id + "' is unique");
			seen.Insert(id);

			// The prefab loads and spawns nothing: the container resolves.
			ctx.False(def.m_sPrefab.IsEmpty(), id + " has a prefab of their own");
			if (!def.m_sPrefab.IsEmpty())
			{
				Resource prefab = Resource.Load(def.m_sPrefab);
				ctx.True(prefab && prefab.IsValid(), id + " prefab loads: " + def.m_sPrefab);

				// A resident is a real character, so a `sit` or `loiter` line can act (#2948): a static NPC body has no
				// character controller and would stand with a warning.
				IEntity body;
				if (!RedProof())
					body = LF_Utils.SpawnEntityPrefab(def.m_sPrefab, "0 -50 0");

				ctx.True(SCR_ChimeraCharacter.Cast(body) != null, id + " spawns as a real character (can sit and lean)");
				if (body)
					SCR_EntityHelper.DeleteEntityAndChildren(body);
			}

			// Home and work are kinds this pack's places register (or its place templates name).
			ctx.True(IsPackKind(def.m_sHomeKind), string.Format("%1 home kind '%2' is a pack place kind", id, def.m_sHomeKind));
			if (!def.m_sWorkKind.IsEmpty())
				ctx.True(IsPackKind(def.m_sWorkKind), string.Format("%1 work kind '%2' is a pack place kind", id, def.m_sWorkKind));

			// The day parses: every line has a valid, distinct HH:MM start and a home or work place. The last line holds
			// across midnight to the first (LF_ResidentTimetable), so a parsed day covers all 24 h.
			ctx.True(def.m_aTimetable && def.m_aTimetable.Count() >= 2, id + " has a day of at least two lines");
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
			}

			foreach (LF_ResidentEntryDef entry : def.m_aTimetable)
			{
				// An empty anchor is the place origin: a site foreman stands there all day, a shop keeper at home when every anchor of the house is taken (#4560).
				bool siteStand = entry.m_sAnchor.IsEmpty();
				ctx.True(siteStand || entry.m_sAnchor == "counter" || entry.m_sAnchor == "bench" || entry.m_sAnchor.IndexOf("door") == 0, string.Format("%1 line %2 stands on a known anchor ('%3')", id, entry.m_sTime, entry.m_sAnchor));
				ctx.True(LF_ResidentWorkLoop.IsWork(entry.m_sActivity) || entry.m_sActivity == "serve" || entry.m_sActivity == "sit" || entry.m_sActivity == "loiter" || entry.m_sActivity == "idle", string.Format("%1 line %2 does a known activity ('%3')", id, entry.m_sTime, entry.m_sActivity));
			}

			// The name is a pack `#` key that resolves to words.
			string key = "#" + "LF-Resident_" + id + "_Name";
			string name = WidgetManager.Translate(key);
			ctx.True(!name.IsEmpty() && name != key && name.IndexOf("#") == -1, id + " name key resolves: " + key);
		}
	}
}

