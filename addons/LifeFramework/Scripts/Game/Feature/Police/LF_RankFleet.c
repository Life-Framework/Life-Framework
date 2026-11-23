//------------------------------------------------------------------------------------------------
//! One rung's car in the police car pool (PO-R1, #4205): the rank's id on the ladder
//! (`LF_PoliceRank.m_sId`) and the vehicle prefab an officer of that rank draws.
[BaseContainerProps()]
class LF_RankFleetRow
{
	[Attribute("", UIWidgets.EditBox, "Id of the police rank (a row of the ladder in the police config) that draws this car")]
	string m_sRankId;

	[Attribute("", UIWidgets.ResourcePickerThumbnail, "The vehicle prefab the rank draws from the car pool", params: "et")]
	ResourceName m_sPrefab;

	//------------------------------------------------------------------------------------------------
	static LF_RankFleetRow Create(string rankId, ResourceName prefab)
	{
		LF_RankFleetRow row = new LF_RankFleetRow();
		row.m_sRankId = rankId;
		row.m_sPrefab = prefab;
		return row;
	}
}

//------------------------------------------------------------------------------------------------
//! The rank fleet table: a pack overlay file of this root class lists one car per rank. The core ships
//! no car (a core slot names no vehicle); an empty table hands nobody a car and says so in the log.
[BaseContainerProps(configRoot: true)]
class LF_RankFleetConfig
{
	[Attribute("", UIWidgets.Object, "One car per police rank id. A later overlay row for the same rank replaces an earlier one")]
	ref array<ref LF_RankFleetRow> m_aRows;
}

//------------------------------------------------------------------------------------------------
//! Rank fleet (PO-R1, #4205): the police car pool hands each rank its car. A Cadet draws the small
//! beige patrol car, Patrol and up draw the UAZ469 (the pack's `LF_RankFleet_Everon1989.conf`). A car is
//! open to its rank and every rank above it; a lower rank is refused it, with the rank it needs named
//! in the refusal. Server and client read the same table; nothing here is saved or replicated.
//! The pool calls `Request` with the officer's rank and spawns the prefab it returns.
class LF_RankFleet
{
	static const string FEATURE = "RankFleet";
	//! Toast: the rank is below the car's. The argument is the title key of the rank that may draw it.
	static const string REFUSED_NEEDS_RANK = "#LF-RankFleet_NeedsRank";
	//! Toast: no car on the table for this rank, or a car that is not on it.
	static const string REFUSED_NOT_ISSUED = "#LF-RankFleet_NotIssued";

	protected static ref array<ref LF_RankFleetRow> s_aRows;
	protected static bool s_bLoaded;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_aRows = null;
		s_bLoaded = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Drops the loaded table so the next read loads the overlays again.
	static void Reload()
	{
		ResetStatics();
	}

	//------------------------------------------------------------------------------------------------
	//! Tests: makes `rows` the table until Reload.
	static void UseRowsForTest(array<ref LF_RankFleetRow> rows)
	{
		s_aRows = rows;
		s_bLoaded = true;
	}

	//------------------------------------------------------------------------------------------------
	//! The car the rank at ladder index `rank` draws; empty when its row names none. A rank past the
	//! top of the ladder reads as the top rung.
	static ResourceName CarFor(int rank)
	{
		string rankId = RankId(rank);
		if (rankId.IsEmpty())
			return ResourceName.Empty;

		foreach (LF_RankFleetRow row : Rows())
		{
			if (row.m_sRankId == rankId)
				return row.m_sPrefab;
		}

		return ResourceName.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! The lowest ladder index whose row names `prefab`, or -1 when no rank draws it. A rank with a
	//! row naming a rung that is not on this ladder is skipped.
	static int RankNeeded(ResourceName prefab)
	{
		if (prefab.IsEmpty())
			return -1;

		array<ref LF_PoliceRank> ranks = {};
		LF_PoliceConfig.Ranks(ranks);
		foreach (int index, LF_PoliceRank rank : ranks)
		{
			foreach (LF_RankFleetRow row : Rows())
			{
				if (row.m_sRankId == rank.m_sId && LF_Utils.SamePrefab(row.m_sPrefab, prefab))
					return index;
			}
		}

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when an officer of rank `rank` may draw `prefab`: the car's own rank or above.
	//! \param[out] refusalKey Empty when allowed; else the localization key that says why.
	//! \param[out] neededTitleKey Title key of the rank the car needs, set with REFUSED_NEEDS_RANK.
	static bool MayDraw(int rank, ResourceName prefab, out string refusalKey, out string neededTitleKey)
	{
		refusalKey = string.Empty;
		neededTitleKey = string.Empty;
		int needed = RankNeeded(prefab);
		if (needed < 0)
		{
			refusalKey = REFUSED_NOT_ISSUED;
			LF_Debug.Warn(FEATURE, string.Format("draw refused: %1 is on no rank's row", prefab));
			return false;
		}

		if (rank >= needed)
			return true;

		refusalKey = REFUSED_NEEDS_RANK;
		neededTitleKey = LF_PoliceRanks.TitleKey(needed);
		LF_Debug.Warn(FEATURE, string.Format("draw refused: rank %1 is below rank %2 for %3", rank, needed, prefab));
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! What the pool calls: the car for an officer of rank `rank`.
	//! \param[out] prefab The car to spawn, set only when the draw is allowed.
	//! \param[out] refusalKey Empty when allowed; else the localization key of the refusal.
	static bool Request(int rank, out ResourceName prefab, out string refusalKey)
	{
		prefab = ResourceName.Empty;
		refusalKey = string.Empty;
		ResourceName car = CarFor(rank);
		if (car.IsEmpty())
		{
			refusalKey = REFUSED_NOT_ISSUED;
			LF_Debug.Warn(FEATURE, string.Format("draw refused: rank %1 has no car on the fleet table", rank));
			return false;
		}

		prefab = car;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static string RankId(int rank)
	{
		array<ref LF_PoliceRank> ranks = {};
		LF_PoliceConfig.Ranks(ranks);
		if (ranks.IsEmpty())
			return string.Empty;

		return ranks[Math.ClampInt(rank, 0, ranks.Count() - 1)].m_sId;
	}

	//------------------------------------------------------------------------------------------------
	protected static array<ref LF_RankFleetRow> Rows()
	{
		if (s_bLoaded && s_aRows)
			return s_aRows;

		s_bLoaded = true;
		s_aRows = new array<ref LF_RankFleetRow>();
		array<ref Resource> resources = {};
		array<ResourceName> names = {};
		LF_ConfigOverlay.LoadPackOverlays(ResourceName.Empty, LF_RankFleetConfig, FEATURE, resources, names);
		foreach (int i, Resource resource : resources)
		{
			LF_RankFleetConfig overlay = LF_RankFleetConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(resource.GetResource().ToBaseContainer()));
			if (!overlay || !overlay.m_aRows)
				continue;

			foreach (LF_RankFleetRow row : overlay.m_aRows)
			{
				if (row && !row.m_sRankId.IsEmpty() && !row.m_sPrefab.IsEmpty())
					Put(s_aRows, row);
			}

			LF_Debug.Info(FEATURE, string.Format("rank fleet: overlay %1 gave %2 rows", names[i], overlay.m_aRows.Count()));
		}

		if (s_aRows.IsEmpty())
			LF_Debug.Warn(FEATURE, "rank fleet table is empty: no rank draws a car");

		return s_aRows;
	}

	//------------------------------------------------------------------------------------------------
	//! Adds `row`, or replaces the earlier row for the same rank.
	protected static void Put(notnull array<ref LF_RankFleetRow> rows, notnull LF_RankFleetRow row)
	{
		foreach (int i, LF_RankFleetRow existing : rows)
		{
			if (existing.m_sRankId == row.m_sRankId)
			{
				rows[i] = row;
				return;
			}
		}

		rows.Insert(row);
	}
}

//------------------------------------------------------------------------------------------------
class LF_RankFleetStaticReset : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_RankFleet";
	}

	override void Apply()
	{
		LF_RankFleet.ResetStatics();
	}
}
