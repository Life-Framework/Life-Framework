//------------------------------------------------------------------------------------------------
//! Tree felling rules (#3259, design docs/roadmap/lumber.md "Felling a real map tree"). Pure: no
//! world, no entities. The world action (#3260) measures the facts into an LF_FellFacts and asks
//! Check; it reads the tree from the base game's prefab path:
//!   Prefabs/Vegetation/Tree/<species folder>/<name>.et, size = the first digit of the name after
//!   the species stem (t_pinus_sylvestris_2f -> 2, T_Pinus_sylvestris_02_3d -> 3; the "02_" of the
//!   v2 trees is a model generation, not a size).
//! Stumps, branches, `base` prefabs and fallen trunks are never trees to fell; size 0 is a sapling
//! (the base game clears those with the e-tool).

enum LF_FellRefusal
{
	OK,
	NO_AXE,
	NOT_A_TREE,
	TOO_SMALL,
	ALREADY_FELLED,
	TOO_FAR,
	SAFE_ZONE,
	NEAR_ROAD,
	NEAR_BUILDING,
	TIRED
}

//------------------------------------------------------------------------------------------------
//! What the world measured for one fell attempt. Distances in metres; a negative road or building
//! distance means none was found in range.
class LF_FellFacts : Managed
{
	bool m_bHasAxe;
	ResourceName m_sPrefab;
	bool m_bFelled;
	float m_fDistanceM;
	bool m_bInSafeZone;
	float m_fRoadDistanceM = -1;
	float m_fBuildingDistanceM = -1;
	//! 0 to 1, the carrier's stamina.
	float m_fStamina = 1;
}

//------------------------------------------------------------------------------------------------
//! A tree as the rules read it from its prefab.
class LF_TreeKind : Managed
{
	LF_TreeWoodClass m_eClass = LF_TreeWoodClass.NONE;
	//! -1 when the name has no size digit.
	int m_iSize = -1;
	//! A stump, branch, base prefab or fallen trunk.
	bool m_bExcluded;
	string m_sFolder;
}

//------------------------------------------------------------------------------------------------
class LF_TreeFelling
{
	static const string FEATURE = "Lumber";
	static const string TREE_ROOT = "prefabs/vegetation/tree/";

	//------------------------------------------------------------------------------------------------
	//! Reads class, size and exclusion from a prefab path (with or without its {GUID}).
	static LF_TreeKind KindOf(ResourceName prefab, LF_TreeFellingConfig config)
	{
		LF_TreeKind kind = new LF_TreeKind();
		string path = prefab;
		path.ToLower();
		path.Replace("\\", "/");

		int root = path.IndexOf(TREE_ROOT);
		if (root < 0)
			return kind;

		string rest = path.Substring(root + TREE_ROOT.Length(), path.Length() - root - TREE_ROOT.Length());
		int slash = rest.IndexOf("/");
		if (slash <= 0)
			return kind;

		kind.m_sFolder = rest.Substring(0, slash);
		string name = rest.Substring(slash + 1, rest.Length() - slash - 1);
		if (name.Length() > 3 && name.Substring(name.Length() - 3, 3) == ".et")
			name = name.Substring(0, name.Length() - 3);

		kind.m_bExcluded = IsExcludedName(name);
		if (config)
			kind.m_eClass = config.ClassOfFolder(kind.m_sFolder);

		// The size digit comes after the species stem (the folder name), skipping a "02_" model generation.
		string tail = name;
		if (tail.StartsWith(kind.m_sFolder))
			tail = tail.Substring(kind.m_sFolder.Length(), tail.Length() - kind.m_sFolder.Length());
		if (tail.StartsWith("_02_"))
			tail = tail.Substring(4, tail.Length() - 4);

		kind.m_iSize = FirstDigit(tail);
		return kind;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsExcludedName(string lowerName)
	{
		array<string> words = { "stump", "branch", "base", "fallen" };
		foreach (string word : words)
		{
			if (lowerName.Contains(word))
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected static int FirstDigit(string text)
	{
		string digits = "0123456789";
		for (int i = 0; i < text.Length(); i++)
		{
			int at = digits.IndexOf(text.Get(i));
			if (at >= 0)
				return at;
		}

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! The first refusal that applies, in the design's order, or OK. A null config refuses NOT_A_TREE
	//! (a broken species file felled nothing rather than everything).
	static LF_FellRefusal Check(LF_FellFacts facts, LF_TreeFellingConfig config)
	{
		if (!facts.m_bHasAxe)
			return LF_FellRefusal.NO_AXE;

		if (!config)
			return LF_FellRefusal.NOT_A_TREE;

		LF_TreeKind kind = KindOf(facts.m_sPrefab, config);
		if (kind.m_eClass == LF_TreeWoodClass.NONE || kind.m_bExcluded || kind.m_iSize < 0)
			return LF_FellRefusal.NOT_A_TREE;

		LF_TreeYieldRow yield = config.YieldOf(kind.m_eClass, kind.m_iSize);
		if (kind.m_iSize < 1 || !yield)
			return LF_FellRefusal.TOO_SMALL;

		if (facts.m_bFelled)
			return LF_FellRefusal.ALREADY_FELLED;

		if (facts.m_fDistanceM > config.m_fReachM)
			return LF_FellRefusal.TOO_FAR;

		if (facts.m_bInSafeZone)
			return LF_FellRefusal.SAFE_ZONE;

		if (facts.m_fRoadDistanceM >= 0 && facts.m_fRoadDistanceM < config.m_fRoadClearM)
			return LF_FellRefusal.NEAR_ROAD;

		if (facts.m_fBuildingDistanceM >= 0 && facts.m_fBuildingDistanceM < config.m_fBuildingClearM)
			return LF_FellRefusal.NEAR_BUILDING;

		if (facts.m_fStamina * 100 < yield.m_iStaminaPct)
			return LF_FellRefusal.TIRED;

		return LF_FellRefusal.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! The yield row of a tree that passes Check, else null.
	static LF_TreeYieldRow YieldFor(ResourceName prefab, LF_TreeFellingConfig config)
	{
		if (!config)
			return null;

		LF_TreeKind kind = KindOf(prefab, config);
		if (kind.m_eClass == LF_TreeWoodClass.NONE || kind.m_bExcluded)
			return null;

		return config.YieldOf(kind.m_eClass, kind.m_iSize);
	}

	//------------------------------------------------------------------------------------------------
	//! The toast string key of a refusal (the Lumber rows of the core string table); empty for OK.
	static string RefusalKey(LF_FellRefusal reason)
	{
		switch (reason)
		{
			case LF_FellRefusal.NO_AXE: return "#LF-Lumber_NoAxe";
			case LF_FellRefusal.NOT_A_TREE: return "#LF-Lumber_NotATree";
			case LF_FellRefusal.TOO_SMALL: return "#LF-Lumber_TooSmall";
			case LF_FellRefusal.ALREADY_FELLED: return "#LF-Lumber_AlreadyFelled";
			case LF_FellRefusal.TOO_FAR: return "#LF-Lumber_TooFar";
			case LF_FellRefusal.SAFE_ZONE: return "#LF-Lumber_SafeZone";
			case LF_FellRefusal.NEAR_ROAD: return "#LF-Lumber_NearRoad";
			case LF_FellRefusal.NEAR_BUILDING: return "#LF-Lumber_NearBuilding";
			case LF_FellRefusal.TIRED: return "#LF-Lumber_Tired";
		}

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! The stable key of a map tree: its prefab path and its position to 0.1 m on x and z. The same on
	//! server, client and after a restart (a map tree never moves), so it is what replicates and saves.
	static string KeyOf(vector position, ResourceName prefab)
	{
		string path = prefab;
		path.ToLower();
		int dm = Math.Round(position[0] * 10);
		int dz = Math.Round(position[2] * 10);
		return string.Format("%1:%2:%3", dm, dz, path);
	}
}
