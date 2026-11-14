//------------------------------------------------------------------------------------------------
//! One rename: a save that names `from` loads as if it named `to` (#965). A pack ships these when
//! it renames, moves or replaces content that saves refer to.
[BaseContainerProps(), LF_SaveAliasTitle()]
class LF_SaveAlias
{
	[Attribute("prefab", UIWidgets.EditBox, "What the id names: prefab (an item, character or vehicle prefab), job, quest, place (a store, station, plot, hideout or storage key), chain or crop")]
	string m_sKind;

	[Attribute("", UIWidgets.EditBox, "The id an older save holds. A prefab matches by its {GUID}, so the path part may be anything")]
	string m_sFrom;

	[Attribute("", UIWidgets.EditBox, "The id it loads as now. A prefab is a full {GUID}path")]
	string m_sTo;
}

//------------------------------------------------------------------------------------------------
class LF_SaveAliasTitle : BaseContainerCustomTitle
{
	//------------------------------------------------------------------------------------------------
	override bool _WB_GetCustomTitle(BaseContainer source, out string title)
	{
		string kind;
		string from;
		string to;
		source.Get("m_sKind", kind);
		source.Get("m_sFrom", from);
		source.Get("m_sTo", to);
		title = string.Format("%1: %2 -> %3", kind, from, to);
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The alias table (#965). The core file ships empty; a pack lists its own file in the mission
//! header's config overlays (LF_MissionHeader.m_aLF_ConfigOverlays), later files winning.
[BaseContainerProps(configRoot: true)]
class LF_SaveAliasesConfig
{
	[Attribute("", UIWidgets.Object, "Renamed ids: an older save's id and the id it loads as now")]
	ref array<ref LF_SaveAlias> m_aAliases;
}

//------------------------------------------------------------------------------------------------
//! Saves keep content by stable id; renamed content still loads (#965).
//!
//! IDS. A save names content by an id that survives a move: a prefab by its {GUID} (a moved
//! resource keeps its GUID, so LF_Utils.SamePrefab and this table ignore the path), a job, quest,
//! chain or crop by its config id, a place by the key its component registers. Never by an index.
//!
//! ALIASES. LF_SaveAliasesConfig rows map an old id to a new one per kind. Every load runs the
//! records it read through Resolve before the feature applies them, so the next save writes the
//! new id. Chains follow (a -> b -> c); a loop is refused and logged. Each alias logs once per
//! session the first time a load uses it.
//!
//! MISSING. A prefab that does not load after the aliases is missing: logged once per id, and
//! the record that names it keeps the old id. A stored inventory holds such an item (and what
//! was inside it) through every capture (LF_StorageSnapshot.Set), so it comes back the first load
//! after the content returns or a pack adds its alias. Ids of the other kinds are kept by the
//! features themselves (job XP of an unknown job, a station, plot or store row with no live
//! component waits for it).
//!
//! Server only. Aliases load once per world (LF_StaticReset drops them).
class LF_SaveIds
{
	static const string FEATURE = "Persistence";
	static const ResourceName CORE_CONF = "{3622B1943ECEDE8E}Configs/Systems/Persistence/LF_SaveAliases.conf";

	static const string KIND_PREFAB = "prefab";
	static const string KIND_JOB = "job";
	static const string KIND_QUEST = "quest";
	static const string KIND_PLACE = "place";
	static const string KIND_CHAIN = "chain";
	static const string KIND_CROP = "crop";

	//! The e2e content-id proof (`tools\cli wt e2e <slug> --content-ids`): the reload boot reads the
	//! rename, the third boot also the restore. Test resources in the core test annex.
	static const ResourceName E2E_RENAMED_CONF = "{831A085EFC734058}Configs/Tests/LF_E2E_SaveAliases_Renamed.conf";
	static const ResourceName E2E_RESTORED_CONF = "{22A199757155FA90}Configs/Tests/LF_E2E_SaveAliases_Restored.conf";

	//! A chain longer than this is treated as a loop.
	protected static const int MAX_HOPS = 16;	// lf-fixed: loop guard, not a tuning value

	//! Key (kind:normalized from) -> to.
	protected static ref map<string, string> s_mAliases;
	//! Keys of the aliases a load used this session (each logs once).
	protected static ref map<string, bool> s_mLoggedAliases;
	//! Keys of the missing ids logged this session.
	protected static ref map<string, bool> s_mLoggedMissing;
	//! Prefab GUID key -> whether it loads. Content does not come back inside a session.
	protected static ref map<string, bool> s_mPrefabLoads;
	protected static int s_iAliasesUsed;
	protected static int s_iMissing;

	//------------------------------------------------------------------------------------------------
	//! World teardown (LF_StaticReset): the next world reads the tables again.
	static void ResetStatics()
	{
		s_mAliases = null;
		s_mLoggedAliases = null;
		s_mLoggedMissing = null;
		s_mPrefabLoads = null;
		s_iAliasesUsed = 0;
		s_iMissing = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The table key of an id: a prefab by its upper-case GUID, anything else as written.
	static string Key(string kind, string id)
	{
		if (kind == KIND_PREFAB)
			return kind + ":" + LF_Utils.PrefabGuid(id);

		return kind + ":" + id;
	}

	//------------------------------------------------------------------------------------------------
	//! Adds or replaces one alias (a later file wins). Tests call it after ResetStatics.
	//! \return False when the row is incomplete or maps an id to itself.
	static bool Add(string kind, string from, string to, string source = "test")
	{
		EnsureLoaded();
		return AddRow(kind, from, to, source);
	}

	//------------------------------------------------------------------------------------------------
	//! \return Number of aliases in the table.
	static int Count()
	{
		EnsureLoaded();
		return s_mAliases.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! The id a save's id loads as: followed through every alias, or the id itself when no alias
	//! names it. A loop logs an error and keeps the id.
	static string Resolve(string kind, string id)
	{
		if (id.IsEmpty())
			return id;

		EnsureLoaded();
		if (s_mAliases.IsEmpty())
			return id;

		string current = id;
		array<string> seen = {};
		for (int hop = 0; hop < MAX_HOPS; hop++)
		{
			string key = Key(kind, current);
			string next;
			if (!s_mAliases.Find(key, next))
				break;

			if (seen.Find(key) != -1)
			{
				LogOnce(s_mLoggedAliases, "loop:" + Key(kind, id), LogLevel.ERROR, string.Format("save alias loop for %1 %2: the aliases lead back to %3; it loads unchanged", kind, id, current));
				return id;
			}

			seen.Insert(key);
			current = next;
		}

		if (seen.Count() >= MAX_HOPS)
		{
			LogOnce(s_mLoggedAliases, "loop:" + Key(kind, id), LogLevel.ERROR, string.Format("save alias chain for %1 %2 is longer than %3 steps; it loads unchanged", kind, id, MAX_HOPS));
			return id;
		}

		if (seen.IsEmpty())
			return id;

		s_iAliasesUsed++;
		LogOnce(s_mLoggedAliases, Key(kind, id), LogLevel.NORMAL, string.Format("save alias used: %1 %2 -> %3", kind, id, current));
		return current;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Whether the prefab loads now. Cached per GUID for the session.
	static bool PrefabLoads(string prefab)
	{
		if (prefab.IsEmpty())
			return false;

		if (!s_mPrefabLoads)
			s_mPrefabLoads = new map<string, bool>();

		string key = Key(KIND_PREFAB, prefab);
		bool loads;
		if (s_mPrefabLoads.Find(key, loads))
			return loads;

		Resource resource = Resource.Load(prefab);
		loads = resource && resource.IsValid();
		s_mPrefabLoads.Set(key, loads);
		return loads;
	}

	//------------------------------------------------------------------------------------------------
	//! A saved prefab id as it loads now: the alias target when it loads, otherwise the saved id,
	//! kept so it resolves again once the content (or its alias) is back. A missing id logs once.
	static string ResolvePrefab(string prefab)
	{
		if (prefab.IsEmpty())
			return prefab;

		string resolved = Resolve(KIND_PREFAB, prefab);
		if (PrefabLoads(resolved))
			return resolved;

		if (resolved != prefab) // lf-path-ok: did the alias table change this id (same string in, same out)
			NoteMissing(KIND_PREFAB, prefab, string.Format(" (its alias leads to %1, which does not load either)", resolved));
		else
			NoteMissing(KIND_PREFAB, prefab, "");

		return prefab;
	}

	//------------------------------------------------------------------------------------------------
	//! The prefab to spawn for a saved id: the id itself when it loads, otherwise the first
	//! fallback that loads (a character body whose content is gone still gets a body; the
	//! account keeps the saved id, so the old body comes back with its content). Empty when
	//! nothing loads.
	static ResourceName SpawnablePrefab(ResourceName saved, array<ResourceName> fallbacks)
	{
		if (PrefabLoads(saved))
			return saved;

		if (!saved.IsEmpty())
			NoteMissing(KIND_PREFAB, saved, " (it spawns from a default prefab until it returns)");

		if (fallbacks)
		{
			foreach (ResourceName fallback : fallbacks)
			{
				if (PrefabLoads(fallback))
					return fallback;
			}
		}

		return ResourceName.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! Logs a missing id once per session; counts it for the load summary.
	static void NoteMissing(string kind, string id, string detail = "")
	{
		if (!s_mLoggedMissing)
			s_mLoggedMissing = new map<string, bool>();

		string key = Key(kind, id);
		if (s_mLoggedMissing.Contains(key))
			return;

		s_mLoggedMissing.Set(key, true);
		s_iMissing++;
		LF_Debug.Warn(FEATURE, string.Format("save content missing: %1 %2 does not load%3; what names it is held until it returns (a pack that renamed it adds an alias, docs/persistence.md)", kind, id, detail));
	}

	//------------------------------------------------------------------------------------------------
	//! Logs once per stored inventory per session that a capture held missing items (every save
	//! point re-captures, so the line would repeat each autosave).
	static void NoteHeld(string snapshotKey, string message)
	{
		if (!s_mLoggedMissing)
			s_mLoggedMissing = new map<string, bool>();

		string key = "held:" + snapshotKey;
		if (s_mLoggedMissing.Contains(key))
			return;

		s_mLoggedMissing.Set(key, true);
		LF_Debug.Info("CarriedInventory", message);
	}

	//------------------------------------------------------------------------------------------------
	//! \return Whether the session logged this id as missing.
	static bool WasMissing(string kind, string id)
	{
		return s_mLoggedMissing && s_mLoggedMissing.Contains(Key(kind, id));
	}

	//------------------------------------------------------------------------------------------------
	//! Start of a store read: the summary counts this load only.
	static void BeginLoad()
	{
		s_iAliasesUsed = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! End of a store read: one line when the load used an alias or met a missing id.
	static void ReportLoad()
	{
		if (s_iAliasesUsed == 0 && s_iMissing == 0)
			return;

		LF_Debug.Info(FEATURE, string.Format("save ids: %1 reference(s) loaded through an alias, %2 missing id(s) held this session", s_iAliasesUsed, s_iMissing));
	}

	//------------------------------------------------------------------------------------------------
	// Record helpers: each rewrites the ids of what one payload read, before the feature applies it.
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	static void ResolveAccounts(array<ref LF_PlayerAccountRecord> records)
	{
		if (!records)
			return;

		foreach (LF_PlayerAccountRecord record : records)
		{
			if (!record || !record.m_aCharacters)
				continue;

			foreach (LF_PlayerCharacterRecord character : record.m_aCharacters)
			{
				if (character)
					character.m_rPrefab = ResolvePrefab(character.m_rPrefab);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	static void ResolveHideouts(array<ref LF_HideoutRecord> records)
	{
		if (!records)
			return;

		foreach (LF_HideoutRecord record : records)
		{
			if (record)
				record.m_sHideoutId = Resolve(KIND_PLACE, record.m_sHideoutId);
		}
	}

	//------------------------------------------------------------------------------------------------
	static void ResolveVehicles(array<ref LF_VehicleRecord> records)
	{
		if (!records)
			return;

		foreach (LF_VehicleRecord record : records)
		{
			if (!record)
				continue;

			record.m_sPrefab = ResolvePrefab(record.m_sPrefab);
			record.m_sSpawnPrefab = ResolvePrefab(record.m_sSpawnPrefab);
			record.m_sShopId = Resolve(KIND_PLACE, record.m_sShopId);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Snapshot owner keys are places (a stash key) or character ids (never aliased); items are prefabs.
	static void ResolveSnapshots(array<ref LF_StorageSnapshotRecord> records)
	{
		if (!records)
			return;

		foreach (LF_StorageSnapshotRecord record : records)
		{
			if (!record)
				continue;

			record.m_sOwnerKey = Resolve(KIND_PLACE, record.m_sOwnerKey);
			if (!record.m_aItems)
				continue;

			foreach (LF_StorageItemRecord item : record.m_aItems)
			{
				if (item)
					item.m_sPrefab = ResolvePrefab(item.m_sPrefab);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	static void ResolveItemState(array<ref LF_StorageItemStateRecord> records)
	{
		if (!records)
			return;

		foreach (LF_StorageItemStateRecord record : records)
		{
			if (record)
				record.m_sOwnerKey = Resolve(KIND_PLACE, record.m_sOwnerKey);
		}
	}

	//------------------------------------------------------------------------------------------------
	static void ResolveItemSerials(array<ref LF_StorageItemSerialRecord> records)
	{
		if (!records)
			return;

		foreach (LF_StorageItemSerialRecord record : records)
		{
			if (record)
				record.m_sOwnerKey = Resolve(KIND_PLACE, record.m_sOwnerKey);
		}
	}

	//------------------------------------------------------------------------------------------------
	static void ResolveProgression(array<ref LF_ProgressionRecord> records)
	{
		if (!records)
			return;

		foreach (LF_ProgressionRecord record : records)
		{
			if (!record || !record.m_aTurnedInQuests)
				continue;

			for (int index = 0; index < record.m_aTurnedInQuests.Count(); index++)
				record.m_aTurnedInQuests[index] = Resolve(KIND_QUEST, record.m_aTurnedInQuests[index]);
		}
	}

	//------------------------------------------------------------------------------------------------
	static void ResolveSignUps(array<ref LF_JobSignUpRecord> records)
	{
		if (!records)
			return;

		foreach (LF_JobSignUpRecord record : records)
		{
			if (record)
				record.m_sJob = Resolve(KIND_JOB, record.m_sJob);
		}
	}

	//------------------------------------------------------------------------------------------------
	static void ResolveFenceSerials(array<ref LF_FenceSerialRow> rows)
	{
		if (!rows)
			return;

		foreach (LF_FenceSerialRow row : rows)
		{
			if (row)
				row.m_sPrefab = ResolvePrefab(row.m_sPrefab);
		}
	}

	//------------------------------------------------------------------------------------------------
	static void ResolveEvidenceLocker(array<ref LF_EvidenceLockerRow> rows)
	{
		if (!rows)
			return;

		foreach (LF_EvidenceLockerRow row : rows)
		{
			if (row)
				row.m_sPrefab = ResolvePrefab(row.m_sPrefab);
		}
	}

	//------------------------------------------------------------------------------------------------
	static void ResolveChainStations(array<ref LF_ChainStationRow> rows)
	{
		if (!rows)
			return;

		foreach (LF_ChainStationRow row : rows)
		{
			if (!row)
				continue;

			row.m_sStationKey = Resolve(KIND_PLACE, row.m_sStationKey);
			row.m_sChainId = Resolve(KIND_CHAIN, row.m_sChainId);
		}
	}

	//------------------------------------------------------------------------------------------------
	static void ResolveStoreStock(array<ref LF_StoreStockRow> rows)
	{
		if (!rows)
			return;

		foreach (LF_StoreStockRow row : rows)
		{
			if (!row)
				continue;

			row.m_sStoreKey = Resolve(KIND_PLACE, row.m_sStoreKey);
			row.m_sPrefab = ResolvePrefab(row.m_sPrefab);
		}
	}

	//------------------------------------------------------------------------------------------------
	static void ResolveFarmPlots(array<ref LF_FarmPlotRow> rows)
	{
		if (!rows)
			return;

		foreach (LF_FarmPlotRow row : rows)
		{
			if (!row)
				continue;

			row.m_sPlotKey = Resolve(KIND_PLACE, row.m_sPlotKey);
			row.m_sCropId = Resolve(KIND_CROP, row.m_sCropId);
		}
	}

	//------------------------------------------------------------------------------------------------
	static void ResolveStoreUnpaid(array<ref LF_StoreUnpaidRow> rows)
	{
		if (!rows)
			return;

		foreach (LF_StoreUnpaidRow row : rows)
		{
			if (!row)
				continue;

			row.m_sStoreKey = Resolve(KIND_PLACE, row.m_sStoreKey);
			row.m_sPrefab = ResolvePrefab(row.m_sPrefab);
		}
	}

	//------------------------------------------------------------------------------------------------
	static void ResolvePlaytime(array<ref LF_PlaytimeRecord> rows)
	{
		if (!rows)
			return;

		foreach (LF_PlaytimeRecord row : rows)
		{
			if (row)
				row.m_sJob = Resolve(KIND_JOB, row.m_sJob);
		}
	}

	//------------------------------------------------------------------------------------------------
	static void ResolveJobXp(array<ref LF_JobXpRow> rows)
	{
		if (!rows)
			return;

		foreach (LF_JobXpRow row : rows)
		{
			if (row)
				row.m_sJob = Resolve(KIND_JOB, row.m_sJob);
		}
	}

	//------------------------------------------------------------------------------------------------
	// Loading the table
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	protected static void EnsureLoaded()
	{
		if (!s_mLoggedAliases)
			s_mLoggedAliases = new map<string, bool>();

		if (s_mAliases)
			return;

		s_mAliases = new map<string, string>();
		array<ResourceName> files = {};
		files.Insert(CORE_CONF);
		array<ResourceName> overlays = {};
		LF_ConfigService.GetPackOverlays(CORE_CONF, overlays);
		files.InsertAll(overlays);
#ifdef LF_E2E_IDS_RENAMED
		files.Insert(E2E_RENAMED_CONF);
#endif
#ifdef LF_E2E_IDS_RESTORED
		files.Insert(E2E_RESTORED_CONF);
#endif

		int read;
		foreach (ResourceName file : files)
		{
			if (LoadFile(file, file == CORE_CONF))
				read++;
		}

		if (!s_mAliases.IsEmpty())
			LF_Debug.Info(FEATURE, string.Format("save aliases: %1 from %2 file(s)", s_mAliases.Count(), read));
	}

	//------------------------------------------------------------------------------------------------
	//! Reads one alias file. An overlay of another class (a shops overlay in the same header list) is
	//! not ours and is passed over quietly.
	//! \return True when the file was an alias table.
	protected static bool LoadFile(ResourceName file, bool isCore)
	{
		Resource resource = BaseContainerTools.LoadContainer(file);
		if (!resource || !resource.IsValid() || !resource.GetResource())
		{
			LF_Debug.Error(FEATURE, string.Format("save aliases: %1 does not resolve, skipped", file));
			return false;
		}

		BaseContainer container = resource.GetResource().ToBaseContainer();
		typename type;
		if (container)
			type = container.GetClassName().ToType();
		if (!type || !type.IsInherited(LF_SaveAliasesConfig))
		{
			if (isCore)
				LF_Debug.Error(FEATURE, string.Format("save aliases: %1 is not an LF_SaveAliasesConfig, skipped", file));
			return false;
		}

		LF_SaveAliasesConfig config = LF_SaveAliasesConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container));
		if (!config || !config.m_aAliases)
			return true;

		foreach (LF_SaveAlias alias : config.m_aAliases)
		{
			if (alias)
				AddRow(alias.m_sKind, alias.m_sFrom, alias.m_sTo, file);
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool AddRow(string kind, string from, string to, string source)
	{
		if (kind.IsEmpty() || from.IsEmpty() || to.IsEmpty())
		{
			LF_Debug.Error(FEATURE, string.Format("save aliases: a row in %1 has no kind, from or to (%2: %3 -> %4), skipped", source, kind, from, to));
			return false;
		}

		if (!IsKnownKind(kind))
		{
			LF_Debug.Error(FEATURE, string.Format("save aliases: unknown kind '%1' in %2 (prefab, job, quest, place, chain, crop), skipped", kind, source));
			return false;
		}

		string key = Key(kind, from);
		if (key == Key(kind, to))
		{
			LF_Debug.Warn(FEATURE, string.Format("save aliases: %1 %2 maps to itself in %3, skipped", kind, from, source));
			return false;
		}

		s_mAliases.Set(key, to);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsKnownKind(string kind)
	{
		return kind == KIND_PREFAB || kind == KIND_JOB || kind == KIND_QUEST || kind == KIND_PLACE || kind == KIND_CHAIN || kind == KIND_CROP;
	}

	//------------------------------------------------------------------------------------------------
	protected static void LogOnce(map<string, bool> logged, string key, LogLevel level, string message)
	{
		if (logged && logged.Contains(key))
			return;

		if (logged)
			logged.Set(key, true);

		LF_Debug.Log(FEATURE, message, level);
	}
}
