// LF_Capacity.c: the capacity ladder (0.3.0 W16, #4189). One table says what the body, packs, car, pickup,
// van and truck carry, in ore-sack units (a car 4, a van 8), and which vehicle each chain's best loop needs.
// Data: Configs/Vehicles/LF_Capacity.conf. The crime scope reads the same table. Native storage first, the cargo
// bed (Feature/Carry/Cargo) for bulky loads: a vehicle's total is its native sacks plus its bed units.
// tools/validation/validate-capacity-ladder.mjs checks the same ladder rule on the file text.

//------------------------------------------------------------------------------------------------
//! One rung of the ladder.
[BaseContainerProps()]
class LF_CapacityRow : Managed
{
	[Attribute("", UIWidgets.EditBox, "Class: body, packs, car, pickup, van or truck")]
	string m_sClass;

	[Attribute("0", UIWidgets.Auto, "Position on the ladder, 0 (body) up to 5 (truck)", params: "0 5")]
	int m_iRung;

	[Attribute("", UIWidgets.ResourcePickerThumbnail, "The vehicle prefab that stands for the class. Empty for body and packs", "et")]
	ResourceName m_Prefab;

	[Attribute("0", UIWidgets.Auto, "Sacks the native storage holds (body, packs, a car's boot)", params: "0 100")]
	int m_iNativeSacks;

	[Attribute("0", UIWidgets.Auto, "Units of the vehicle's cargo bed; must equal the prefab's LF_CargoBedComponent capacity", params: "0 100")]
	int m_iBedUnits;

	//------------------------------------------------------------------------------------------------
	//! Sacks the rung carries in all.
	int Total()
	{
		return m_iNativeSacks + m_iBedUnits;
	}

	//------------------------------------------------------------------------------------------------
	static LF_CapacityRow Make(string className, int rung, int nativeSacks, int bedUnits)
	{
		LF_CapacityRow row = new LF_CapacityRow();
		row.m_sClass = className;
		row.m_iRung = rung;
		row.m_iNativeSacks = nativeSacks;
		row.m_iBedUnits = bedUnits;
		return row;
	}
}

//------------------------------------------------------------------------------------------------
//! One chain's best loop: the load of a single run and the vehicle class it cannot be done without.
[BaseContainerProps()]
class LF_CapacityLoop : Managed
{
	[Attribute("", UIWidgets.EditBox, "The chain, such as coal, iron or lumber")]
	string m_sChain;

	[Attribute("0", UIWidgets.Auto, "Sacks of the chain's best single run", params: "1 100")]
	int m_iBestRun;

	[Attribute("", UIWidgets.EditBox, "The vehicle class the run needs: the smallest rung that holds it")]
	string m_sNeeds;

	//------------------------------------------------------------------------------------------------
	static LF_CapacityLoop Make(string chain, int bestRun, string needs)
	{
		LF_CapacityLoop loop = new LF_CapacityLoop();
		loop.m_sChain = chain;
		loop.m_iBestRun = bestRun;
		loop.m_sNeeds = needs;
		return loop;
	}
}

//------------------------------------------------------------------------------------------------
//! The ladder rule, pure: no config, no entities.
class LF_CapacityRules
{
	static const int RUNGS = 6;	// lf-fixed: body, packs, car, pickup, van, truck

	//------------------------------------------------------------------------------------------------
	//! The first thing wrong with a ladder, or "" when it is sound: six rows on rungs 0 to 5, each
	//! carrying strictly more than the one below it, and every loop needs the smallest class that holds
	//! its run (so it does not fit the rung below).
	static string Check(array<ref LF_CapacityRow> rows, array<ref LF_CapacityLoop> loops)
	{
		if (!rows || rows.Count() != RUNGS)
			return "the ladder needs exactly 6 rows (body, packs, car, pickup, van, truck)";

		for (int i = 0; i < rows.Count(); i++)
		{
			LF_CapacityRow row = rows[i];
			if (!row || row.m_iRung != i)
				return string.Format("row %1 is not on rung %1", i);

			if (i > 0 && row.Total() <= rows[i - 1].Total())
				return string.Format("%1 carries %2, no more than %3 (%4)", row.m_sClass, row.Total(), rows[i - 1].m_sClass, rows[i - 1].Total());
		}

		if (!loops)
			return string.Empty;

		foreach (LF_CapacityLoop loop : loops)
		{
			if (!loop)
				continue;

			int rung = RungOf(rows, loop.m_sNeeds);
			if (rung < 0)
				return string.Format("chain %1 needs %2, which is not a class", loop.m_sChain, loop.m_sNeeds);

			if (loop.m_iBestRun > rows[rung].Total())
				return string.Format("chain %1 best run %2 does not fit a %3 (%4)", loop.m_sChain, loop.m_iBestRun, loop.m_sNeeds, rows[rung].Total());

			if (rung > 0 && loop.m_iBestRun <= rows[rung - 1].Total())
				return string.Format("chain %1 best run %2 fits a %3 (%4): it must need the next vehicle up", loop.m_sChain, loop.m_iBestRun, rows[rung - 1].m_sClass, rows[rung - 1].Total());
		}

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! The rung of a class name, or -1.
	static int RungOf(array<ref LF_CapacityRow> rows, string className)
	{
		foreach (int i, LF_CapacityRow row : rows)
		{
			if (row && row.m_sClass == className)
				return i;
		}

		return -1;
	}
}

//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true)]
class LF_CapacityConfig
{
	static const string FEATURE = "Vehicle";
	static const ResourceName CONFIG_PATH = "{9C41D7E205B3A68F}Configs/Vehicles/LF_Capacity.conf";

	[Attribute("", UIWidgets.Object, "The ladder, one row per rung, lowest first")]
	ref array<ref LF_CapacityRow> m_aRows;

	[Attribute("", UIWidgets.Object, "Each chain's best loop and the vehicle it needs")]
	ref array<ref LF_CapacityLoop> m_aLoops;

	protected static ref LF_CapacityConfig s_Config;
	protected static bool s_bLoadAttempted;

	//------------------------------------------------------------------------------------------------
	//! The loaded table, or null (logged once) when the file does not resolve.
	static LF_CapacityConfig Get()
	{
		if (s_bLoadAttempted)
			return s_Config;

		s_bLoadAttempted = true;
		Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
		if (!container || !container.GetResource())
		{
			LF_Debug.Error(FEATURE, string.Format("capacity table does not resolve: %1", CONFIG_PATH));
			return null;
		}

		s_Config = LF_CapacityConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!s_Config)
		{
			LF_Debug.Error(FEATURE, string.Format("capacity table has an unexpected root class: %1", CONFIG_PATH));
			return null;
		}

		string problem = LF_CapacityRules.Check(s_Config.m_aRows, s_Config.m_aLoops);
		if (!problem.IsEmpty())
			LF_Debug.Error(FEATURE, "capacity table is not a ladder: " + problem);

		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	//! Drops the cached table so the next Get() loads it again.
	static void Reload()
	{
		s_Config = null;
		s_bLoadAttempted = false;
	}

	//------------------------------------------------------------------------------------------------
	//! The row of a class, or null.
	LF_CapacityRow Find(string className)
	{
		int rung = LF_CapacityRules.RungOf(m_aRows, className);
		if (rung < 0)
			return null;

		return m_aRows[rung];
	}

	//------------------------------------------------------------------------------------------------
	//! Sacks a class carries in all; 0 for an unknown class or no table (a caller never reads that as "carries nothing" by luck: it logs).
	static int SacksOf(string className)
	{
		LF_CapacityConfig config = Get();
		if (!config)
			return 0;

		LF_CapacityRow row = config.Find(className);
		if (!row)
		{
			LF_Debug.Warn(FEATURE, "capacity table has no class " + className);
			return 0;
		}

		return row.Total();
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of the capacity table cache (LF_StaticReset finds it by reflection).
class LF_CapacityConfig_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_CapacityConfig";
	}

	override void Apply()
	{
		LF_CapacityConfig.Reload();
	}
}
