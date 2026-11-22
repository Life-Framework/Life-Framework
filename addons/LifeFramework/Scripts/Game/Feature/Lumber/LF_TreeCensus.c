//------------------------------------------------------------------------------------------------
//! One prefab family of the tree census: how many map trees stand from it, and how many of them the
//! rules read as a tree to fell (a species with a yield row, not a stump or sapling) by the placed
//! prefab alone and through the prefab ancestry.
class LF_TreeCensusRow : Managed
{
	string m_sFamily;
	int m_iTrees;
	//! Fellable when the rules read the placed prefab's own path (the rule before the ancestry fix).
	int m_iFellableByPath;
	//! Fellable through LF_FellAction.TreePrefabOf (what an axe does now).
	int m_iFellable;
}

//------------------------------------------------------------------------------------------------
//! Counts the map trees of the loaded world by prefab family (`treefall census`, a dev harness verb):
//! the measure behind "which trees can an axe fell". The sweep walks the world in square tiles, one
//! per tick, so a big island never stalls the server; the summary goes to the log as
//! `[LFDebug:Lumber] tree census: ...` lines, one per family.
class LF_TreeCensus
{
	static const float TILE_M = 400; // lf-fixed: sweep tile of a dev harness tool, not a game tunable
	static const int TICK_MS = 30; // lf-fixed: pacing of a dev harness sweep, not a game tunable

	protected static ref array<ref LF_TreeCensusRow> s_aRows;
	protected static ref LF_FelledTreeQuery s_Query;
	protected static vector s_vMin;
	protected static vector s_vMax;
	protected static float s_fX;
	protected static float s_fZ;
	protected static int s_iNoPrefab;
	protected static bool s_bRunning;

	//------------------------------------------------------------------------------------------------
	//! The folder of a prefab path with its GUID and file name removed, lower case; the family a tree is
	//! counted under (prefabs/vegetation/tree/t_betula_pendula, prefablibrary/defaultlibrary/vegetation/trees).
	static string FamilyOf(string prefab)
	{
		string path = prefab;
		path.ToLower();
		path.Replace("\\", "/");
		int brace = path.IndexOf("}");
		if (brace >= 0)
			path = path.Substring(brace + 1, path.Length() - brace - 1);

		int slash = path.LastIndexOf("/");
		if (slash <= 0)
			return path;

		return path.Substring(0, slash);
	}

	//------------------------------------------------------------------------------------------------
	//! True while a sweep runs.
	static bool IsRunning()
	{
		return s_bRunning;
	}

	//------------------------------------------------------------------------------------------------
	//! Starts a sweep of the whole world; false (logged) when one already runs or there is no world.
	static bool Start()
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world || s_bRunning)
		{
			LF_Debug.Warn(LF_TreeFelling.FEATURE, "tree census not started (no world, or one is running)");
			return false;
		}

		world.GetBoundBox(s_vMin, s_vMax);
		s_aRows = new array<ref LF_TreeCensusRow>();
		s_Query = new LF_FelledTreeQuery();
		s_fX = s_vMin[0];
		s_fZ = s_vMin[2];
		s_iNoPrefab = 0;
		s_bRunning = true;
		LF_Debug.Info(LF_TreeFelling.FEATURE, string.Format("tree census started over %1 to %2", s_vMin, s_vMax));
		GetGame().GetCallqueue().CallLater(Tick, TICK_MS, true);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Tick()
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world || s_fZ >= s_vMax[2])
		{
			Finish();
			return;
		}

		s_Query.m_aEntities.Clear();
		world.QueryEntitiesByAABB(Vector(s_fX, s_vMin[1], s_fZ), Vector(s_fX + TILE_M, s_vMax[1], s_fZ + TILE_M), s_Query.Collect, null, EQueryEntitiesFlags.ALL);
		LF_TreeFellingConfig config = LF_TreeFellingConfig.Get();
		foreach (IEntity entity : s_Query.m_aEntities)
		{
			if (!Tree.Cast(entity))
				continue;

			Count(entity, config);
		}

		s_fX += TILE_M;
		if (s_fX >= s_vMax[0])
		{
			s_fX = s_vMin[0];
			s_fZ += TILE_M;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void Count(IEntity tree, LF_TreeFellingConfig config)
	{
		ResourceName own = LF_FellAction.PrefabOf(tree);
		if (own.IsEmpty())
		{
			s_iNoPrefab++;
			return;
		}

		string family = FamilyOf(own);
		LF_TreeCensusRow row;
		foreach (LF_TreeCensusRow candidate : s_aRows)
		{
			if (candidate.m_sFamily == family)
			{
				row = candidate;
				break;
			}
		}

		if (!row)
		{
			row = new LF_TreeCensusRow();
			row.m_sFamily = family;
			s_aRows.Insert(row);
		}

		row.m_iTrees++;
		if (IsFellablePrefab(own, config))
			row.m_iFellableByPath++;

		if (IsFellablePrefab(LF_FellAction.TreePrefabOf(tree), config))
			row.m_iFellable++;
	}

	//------------------------------------------------------------------------------------------------
	//! True when the rules give a prefab a yield row (a species, not a stump or fallen trunk, size 1 or more).
	static bool IsFellablePrefab(ResourceName prefab, LF_TreeFellingConfig config)
	{
		if (!config)
			return false;

		LF_TreeKind kind = LF_TreeFelling.KindOf(prefab, config);
		if (kind.m_eClass == LF_TreeWoodClass.NONE || kind.m_bExcluded || kind.m_iSize < 1)
			return false;

		return config.YieldOf(kind.m_eClass, kind.m_iSize) != null;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Finish()
	{
		GetGame().GetCallqueue().Remove(Tick);
		s_bRunning = false;
		int trees;
		int byPath;
		int now;
		foreach (LF_TreeCensusRow row : s_aRows)
		{
			trees += row.m_iTrees;
			byPath += row.m_iFellableByPath;
			now += row.m_iFellable;
			LF_Debug.Info(LF_TreeFelling.FEATURE, string.Format("tree census: %1 trees=%2 fellable-by-path=%3 fellable=%4", row.m_sFamily, row.m_iTrees, row.m_iFellableByPath, row.m_iFellable));
		}

		LF_Debug.Info(LF_TreeFelling.FEATURE, string.Format("tree census done: trees=%1 no-prefab=%2 fellable-by-path=%3 fellable=%4 unfellable-by-path-only=%5", trees, s_iNoPrefab, byPath, now, now - byPath));
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		if (s_bRunning && GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(Tick);

		s_bRunning = false;
		s_aRows = null;
		s_Query = null;
	}
}

//------------------------------------------------------------------------------------------------
class LF_TreeCensus_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_TreeCensus";
	}

	override void Apply()
	{
		LF_TreeCensus.ResetStatics();
	}
}
