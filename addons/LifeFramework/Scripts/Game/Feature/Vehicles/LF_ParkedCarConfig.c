//------------------------------------------------------------------------------------------------
//! One car a parked-car marker may put out (#3365). The record key is the prefab GUID. The prefab is the
//! thin locked child (it carries LF_VehicleLockComponent), never a bare vanilla car, so the lock is the
//! normal one.
[BaseContainerProps()]
class LF_ParkedCarDef : LF_ConfigRecord
{
	[Attribute("", UIWidgets.ResourcePickerThumbnail, "Locked civilian car prefab (a thin child carrying LF_VehicleLockComponent). The record key", "et")]
	ResourceName m_sPrefab;

	//------------------------------------------------------------------------------------------------
	//! \return Empty when the row is usable, else why not.
	string Problem()
	{
		if (m_sPrefab.IsEmpty())
			return "no prefab";

		return "";
	}
}

//------------------------------------------------------------------------------------------------
//! The cars the parked-car markers choose from. Loaded from Configs/Vehicles/LF_ParkedCars.conf. The core
//! list is two neutral vanilla civilian cars; a pack overlay (`m_aLF_ConfigOverlays`) adds, replaces,
//! patches or removes rows by prefab GUID to name its period cars.
[BaseContainerProps(configRoot: true)]
class LF_ParkedCarConfig
{
	static const string FEATURE = "ParkedCar";
	static const ResourceName CONFIG_PATH = "{D9D0007A84D03959}Configs/Vehicles/LF_ParkedCars.conf";

	[Attribute("", UIWidgets.Object, "Cars a marker may put out; each spawn picks one at random")]
	ref array<ref LF_ParkedCarDef> m_aCars;

	protected static ref LF_ParkedCarConfig s_Config;
	protected static bool s_bLoadAttempted;

	//------------------------------------------------------------------------------------------------
	//! The loaded config with unusable rows removed, or null (logged once) when the file does not resolve.
	static LF_ParkedCarConfig Get()
	{
		if (s_bLoadAttempted)
			return s_Config;

		s_bLoadAttempted = true;
		Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
		if (!container || !container.GetResource())
		{
			LF_Debug.Error(FEATURE, string.Format("parked car config does not resolve: %1", CONFIG_PATH));
			return null;
		}

		s_Config = LF_ParkedCarConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!s_Config)
		{
			LF_Debug.Error(FEATURE, string.Format("parked car config has an unexpected root class: %1", CONFIG_PATH));
			return null;
		}

		s_Config.ApplyLayers();
		s_Config.Sanitize();
		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	//! Merges each pack overlay (in load order) over this instance, which holds the core `.conf`.
	void ApplyLayers()
	{
		array<ref Resource> resources = {};
		array<ResourceName> names = {};
		LF_ConfigOverlay.LoadPackOverlays(CONFIG_PATH, LF_ParkedCarConfig, "parkedcars", resources, names);
		LF_ConfigClaims claims = new LF_ConfigClaims();
		foreach (int i, Resource resource : resources)
		{
			int changed = ApplyPackContainer(resource.GetResource().ToBaseContainer(), names[i], claims);
			LF_Debug.Info(FEATURE, string.Format("parked cars: pack overlay %1 changed %2 entries", names[i], changed));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Applies one pack overlay's `m_aCars` by prefab. \return Entries changed.
	int ApplyPackContainer(notnull BaseContainer root, string source, LF_ConfigClaims claims = null)
	{
		if (!m_aCars)
			m_aCars = {};

		return LF_ConfigOverlay.ApplyContainerList(new LF_ParkedCarList(this), root, "m_aCars", source, claims);
	}

	//------------------------------------------------------------------------------------------------
	//! Drops the cached instance; the next Get loads the `.conf` again (world end, tests).
	static void Reload()
	{
		s_Config = null;
		s_bLoadAttempted = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Removes unusable or repeated rows with `LF_Debug.Error`, so one bad row never degrades the rest.
	//! \return How many rows were removed.
	int Sanitize()
	{
		if (!m_aCars)
			m_aCars = {};

		int removed = 0;
		for (int i = m_aCars.Count() - 1; i >= 0; i--)
		{
			LF_ParkedCarDef def = m_aCars[i];
			string problem;
			if (!def)
				problem = "empty row";
			else
				problem = def.Problem();

			if (problem.IsEmpty() && FirstIndexOf(def.m_sPrefab) != i)
				problem = "the prefab is listed twice";

			if (problem.IsEmpty())
				continue;

			string name = "(empty)";
			if (def)
				name = def.m_sPrefab;

			LF_Debug.Error(FEATURE, string.Format("parked car '%1' refused at load: %2", name, problem));
			m_aCars.Remove(i);
			removed++;
		}

		return removed;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the list holds this car prefab (by GUID).
	bool Lists(ResourceName prefab)
	{
		return m_aCars && FirstIndexOf(prefab) >= 0;
	}

	//------------------------------------------------------------------------------------------------
	protected int FirstIndexOf(ResourceName prefab)
	{
		for (int i = 0; i < m_aCars.Count(); i++)
		{
			if (m_aCars[i] && LF_Utils.SamePrefab(m_aCars[i].m_sPrefab, prefab))
				return i;
		}

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! \param failed Car ids (LF_ParkedCarList.CarId) that did not spawn this session; those rows are skipped.
	//! \return A random car prefab from the rows not in `failed`, or empty when there is none left.
	ResourceName PickPrefabAvoiding(array<string> failed)
	{
		array<ResourceName> usable = {};
		if (m_aCars)
		{
			foreach (LF_ParkedCarDef def : m_aCars)
			{
				if (!def)
					continue;

				if (failed && failed.Find(LF_ParkedCarList.CarId(def.m_sPrefab)) >= 0)
					continue;

				usable.Insert(def.m_sPrefab);
			}
		}

		if (usable.IsEmpty())
			return ResourceName.Empty;

		return usable[Math.RandomInt(0, usable.Count())];
	}
}

//------------------------------------------------------------------------------------------------
//! The cars list for the shared merge (LF_ConfigOverlay), keyed by the prefab GUID. Pack overlays only.
class LF_ParkedCarList : LF_ConfigList
{
	protected LF_ParkedCarConfig m_Config;

	void LF_ParkedCarList(LF_ParkedCarConfig config)
	{
		m_Config = config;
		if (!m_Config.m_aCars)
			m_Config.m_aCars = {};
	}

	//------------------------------------------------------------------------------------------------
	//! "{GUID}path" to "GUID" in upper case; a bare path comes back upper-cased whole.
	static string CarId(ResourceName prefab)
	{
		string text = prefab;
		if (text.StartsWith("{"))
		{
			int close = text.IndexOf("}");
			if (close > 1)
				text = text.Substring(1, close - 1);
		}

		text.ToUpper();
		return text;
	}

	override string GetName()
	{
		return "parkedcars.cars";
	}

	override int Count()
	{
		return m_Config.m_aCars.Count();
	}

	override LF_ConfigRecord Get(int index)
	{
		return m_Config.m_aCars[index];
	}

	override string IdOf(notnull LF_ConfigRecord record)
	{
		return CarId(LF_ParkedCarDef.Cast(record).m_sPrefab);
	}

	override string IdOfContainer(notnull BaseContainer container)
	{
		ResourceName prefab;
		container.Get("m_sPrefab", prefab);
		return CarId(prefab);
	}

	override string IdHint()
	{
		return "a car prefab's GUID";
	}

	override void Set(int index, notnull LF_ConfigRecord record)
	{
		LF_ParkedCarDef def = LF_ParkedCarDef.Cast(record);
		if (index < m_Config.m_aCars.Count())
			m_Config.m_aCars.Set(index, def);
		else
			m_Config.m_aCars.Insert(def);
	}

	override void RemoveAt(int index)
	{
		m_Config.m_aCars.RemoveOrdered(index);
	}

	override string Incomplete(notnull LF_ConfigRecord record)
	{
		return LF_ParkedCarDef.Cast(record).Problem();
	}

	override int PatchFromContainer(notnull LF_ConfigRecord record, notnull BaseContainer container)
	{
		// The prefab is the key: a patch has nothing else to change.
		return 0;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_ParkedCarConfig's statics.
class LF_ParkedCarConfig_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_ParkedCarConfig";
	}

	override void Apply()
	{
		LF_ParkedCarConfig.Reload();
	}
}
