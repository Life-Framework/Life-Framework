//------------------------------------------------------------------------------------------------
//! One rung's ambulance in the medic car pool (#4432): the id of the medic rank
//! (`LF_MedicRank.m_sId`) and the vehicle prefab a medic of that rank draws.
[BaseContainerProps()]
class LF_MedicFleetRow
{
	[Attribute("", UIWidgets.EditBox, "Id of the medic rank (a row of the ladder in the medic config) that draws this ambulance")]
	string m_sRankId;

	[Attribute("", UIWidgets.ResourcePickerThumbnail, "The vehicle prefab the rank draws from the car pool", params: "et")]
	ResourceName m_sPrefab;

	//------------------------------------------------------------------------------------------------
	static LF_MedicFleetRow Create(string rankId, ResourceName prefab)
	{
		LF_MedicFleetRow row = new LF_MedicFleetRow();
		row.m_sRankId = rankId;
		row.m_sPrefab = prefab;
		return row;
	}
}

//------------------------------------------------------------------------------------------------
//! The medic fleet table: a pack overlay file of this root class lists one ambulance per medic rank.
//! The core ships no vehicle (the old M997 is not handed out): an empty table hands nobody a car.
[BaseContainerProps(configRoot: true)]
class LF_MedicFleetConfig
{
	[Attribute("", UIWidgets.Object, "One ambulance per medic rank id. A later overlay row for the same rank replaces an earlier one")]
	ref array<ref LF_MedicFleetRow> m_aRows;
}

//------------------------------------------------------------------------------------------------
//! Medic fleet (#4432): the clinic's car pool hands each medic rank its ambulance. A copy of the police
//! `LF_RankFleet` shape, kept separate on purpose (the ladders are separate): the Everon pack gives the
//! lower ranks the UAZ452 ambulance and the higher ranks the S1203 (`LF_MedicFleet_Everon1989.conf`).
//! A car is open to its rank and every rank above it; a lower rank is refused it in words naming the rank
//! it needs. Nothing here is saved or replicated.
class LF_MedicFleet
{
	static const string FEATURE = "MedicFleet";

	protected static ref array<ref LF_MedicFleetRow> s_aRows;
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
	static void UseRowsForTest(array<ref LF_MedicFleetRow> rows)
	{
		s_aRows = rows;
		s_bLoaded = true;
	}

	//------------------------------------------------------------------------------------------------
	//! The ambulance the medic rank at ladder index `rank` draws; empty when its row names none.
	static ResourceName CarFor(int rank)
	{
		string rankId = RankId(rank);
		if (rankId.IsEmpty())
			return ResourceName.Empty;

		foreach (LF_MedicFleetRow row : Rows())
		{
			if (row.m_sRankId == rankId)
				return row.m_sPrefab;
		}

		return ResourceName.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! The lowest ladder index whose row names `prefab`, or -1 when no rank draws it.
	static int RankNeeded(ResourceName prefab)
	{
		if (prefab.IsEmpty())
			return -1;

		array<ref LF_MedicRank> ranks = {};
		LF_MedicRanks.Ranks(ranks);
		foreach (int index, LF_MedicRank rank : ranks)
		{
			foreach (LF_MedicFleetRow row : Rows())
			{
				if (row.m_sRankId == rank.m_sId && LF_Utils.SamePrefab(row.m_sPrefab, prefab))
					return index;
			}
		}

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when a medic of rank `rank` may draw `prefab`: the car's own rank or above.
	//! \param[out] refusalKey Empty when allowed; else the localization key that says why.
	//! \param[out] neededTitleKey Title key of the rank the car needs, set with LF_RankFleet.REFUSED_NEEDS_RANK.
	static bool MayDraw(int rank, ResourceName prefab, out string refusalKey, out string neededTitleKey)
	{
		refusalKey = string.Empty;
		neededTitleKey = string.Empty;
		int needed = RankNeeded(prefab);
		if (needed < 0)
		{
			refusalKey = LF_RankFleet.REFUSED_NOT_ISSUED;
			LF_Debug.Warn(FEATURE, string.Format("draw refused: %1 is on no rank's row", prefab));
			return false;
		}

		if (rank >= needed)
			return true;

		refusalKey = LF_RankFleet.REFUSED_NEEDS_RANK;
		neededTitleKey = LF_MedicRanks.TitleKey(needed);
		LF_Debug.Warn(FEATURE, string.Format("draw refused: rank %1 is below rank %2 for %3", rank, needed, prefab));
		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected static string RankId(int rank)
	{
		array<ref LF_MedicRank> ranks = {};
		LF_MedicRanks.Ranks(ranks);
		if (ranks.IsEmpty())
			return string.Empty;

		return ranks[Math.ClampInt(rank, 0, ranks.Count() - 1)].m_sId;
	}

	//------------------------------------------------------------------------------------------------
	protected static array<ref LF_MedicFleetRow> Rows()
	{
		if (s_bLoaded && s_aRows)
			return s_aRows;

		s_bLoaded = true;
		s_aRows = new array<ref LF_MedicFleetRow>();
		array<ref Resource> resources = {};
		array<ResourceName> names = {};
		LF_ConfigOverlay.LoadPackOverlays(ResourceName.Empty, LF_MedicFleetConfig, FEATURE, resources, names);
		foreach (int i, Resource resource : resources)
		{
			LF_MedicFleetConfig overlay = LF_MedicFleetConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(resource.GetResource().ToBaseContainer()));
			if (!overlay || !overlay.m_aRows)
				continue;

			foreach (LF_MedicFleetRow row : overlay.m_aRows)
			{
				if (row && !row.m_sRankId.IsEmpty() && !row.m_sPrefab.IsEmpty())
					Put(s_aRows, row);
			}

			LF_Debug.Info(FEATURE, string.Format("medic fleet: overlay %1 gave %2 rows", names[i], overlay.m_aRows.Count()));
		}

		if (s_aRows.IsEmpty())
			LF_Debug.Warn(FEATURE, "medic fleet table is empty: no rank draws an ambulance");

		return s_aRows;
	}

	//------------------------------------------------------------------------------------------------
	//! Adds `row`, or replaces the earlier row for the same rank.
	protected static void Put(notnull array<ref LF_MedicFleetRow> rows, notnull LF_MedicFleetRow row)
	{
		foreach (int i, LF_MedicFleetRow existing : rows)
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
class LF_MedicFleetStaticReset : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_MedicFleet";
	}

	override void Apply()
	{
		LF_MedicFleet.ResetStatics();
	}
}
