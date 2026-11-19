//------------------------------------------------------------------------------------------------
//! One vehicle that takes a hidden compartment (#3436): the car prefab, the compartment size
//! (a level 0-3, LF_VehicleOwnership.COMPARTMENT_MAX) and the fitting price at the chop shop.
[BaseContainerProps()]
class LF_VehicleCompartmentRow : LF_ConfigRecord
{
	[Attribute("", UIWidgets.ResourceNamePicker, "The vehicle prefab. A vehicle with no row takes no compartment", "et")]
	ResourceName m_sPrefab;

	[Attribute("1", UIWidgets.Auto, "Compartment size level, 1 to LF_VehicleOwnership.COMPARTMENT_MAX", params: "1 3")]
	int m_iSize;

	[Attribute("0", UIWidgets.Auto, "Fitting price in cash at the chop shop", params: "0 1000000")]
	int m_iFitPrice;

	//------------------------------------------------------------------------------------------------
	static LF_VehicleCompartmentRow Make(string prefab)
	{
		LF_VehicleCompartmentRow row = new LF_VehicleCompartmentRow();
		row.m_sPrefab = prefab;
		return row;
	}
}

//------------------------------------------------------------------------------------------------
//! Which vehicles take a hidden compartment: Configs/Vehicles/LF_VehicleCompartments.conf (core,
//! empty: no car takes one) with each pack overlay layered over it (LF_ConfigOverlay). The pack
//! names its period cars. LF_VehicleCompartment.Fit does the fitting; this says who may be fitted.
[BaseContainerProps(configRoot: true)]
class LF_VehicleCompartmentConfig
{
	static const string FEATURE = "Vehicle";
	static const ResourceName CONFIG_PATH = "{7D3B5E19A4C2680F}Configs/Vehicles/LF_VehicleCompartments.conf";

	[Attribute("", UIWidgets.Object, "One row per vehicle prefab that takes a compartment")]
	ref array<ref LF_VehicleCompartmentRow> m_aRows;

	protected static ref LF_VehicleCompartmentConfig s_Config;
	protected static bool s_bLoadAttempted;

	//------------------------------------------------------------------------------------------------
	//! The loaded and layered table, or null (logged once) when the core file does not resolve.
	static LF_VehicleCompartmentConfig Get()
	{
		if (s_bLoadAttempted)
			return s_Config;

		s_bLoadAttempted = true;
		Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
		if (!container || !container.GetResource())
		{
			LF_Debug.Error(FEATURE, string.Format("compartment table does not resolve, no car takes one: %1", CONFIG_PATH));
			return null;
		}

		s_Config = LF_VehicleCompartmentConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!s_Config)
			LF_Debug.Error(FEATURE, string.Format("compartment table has an unexpected root class: %1", CONFIG_PATH));
		else
			s_Config.ApplyLayers();

		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	//! Drops the cached table so the next Get() loads and layers it again.
	static void Reload()
	{
		s_Config = null;
		s_bLoadAttempted = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Merges each pack overlay of this class, in load order, over this instance.
	void ApplyLayers()
	{
		if (!m_aRows)
			m_aRows = new array<ref LF_VehicleCompartmentRow>();

		LF_VehicleCompartmentList list = new LF_VehicleCompartmentList(this);
		array<ref Resource> resources = {};
		array<ResourceName> names = {};
		LF_ConfigOverlay.LoadPackOverlays(CONFIG_PATH, LF_VehicleCompartmentConfig, "vehicleCompartments", resources, names);
		LF_ConfigClaims claims = new LF_ConfigClaims();
		foreach (int i, Resource resource : resources)
		{
			int changed = LF_ConfigOverlay.ApplyContainerList(list, resource.GetResource().ToBaseContainer(), "m_aRows", names[i], claims);
			LF_Debug.Info(FEATURE, string.Format("pack overlay %1 changed %2 compartment rows", names[i], changed));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The row for this vehicle prefab, or null: the vehicle takes no compartment.
	LF_VehicleCompartmentRow Find(string prefab)
	{
		if (!m_aRows || prefab.IsEmpty())
			return null;

		foreach (LF_VehicleCompartmentRow row : m_aRows)
		{
			if (row && row.m_sPrefab == prefab) // lf-path-ok: both sides are the full {GUID}path
				return row;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! The compartment size level this prefab takes, 0 for a vehicle with no row.
	static int SizeFor(string prefab)
	{
		LF_VehicleCompartmentConfig config = Get();
		if (!config)
			return 0;

		LF_VehicleCompartmentRow row = config.Find(prefab);
		if (!row)
			return 0;

		return Math.ClampInt(row.m_iSize, 0, LF_VehicleOwnership.COMPARTMENT_MAX);
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of the compartment table cache (LF_StaticReset finds it by reflection).
class LF_VehicleCompartmentConfig_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_VehicleCompartmentConfig";
	}

	override void Apply()
	{
		LF_VehicleCompartmentConfig.Reload();
	}
}

//------------------------------------------------------------------------------------------------
//! The compartment table list `m_aRows` for the shared merge (LF_ConfigOverlay), keyed by prefab.
class LF_VehicleCompartmentList : LF_ConfigList
{
	protected LF_VehicleCompartmentConfig m_Config;

	void LF_VehicleCompartmentList(LF_VehicleCompartmentConfig config)
	{
		m_Config = config;
	}

	override string GetName()
	{
		return "vehicleCompartments.rows";
	}

	override int Count()
	{
		return m_Config.m_aRows.Count();
	}

	override LF_ConfigRecord Get(int index)
	{
		return m_Config.m_aRows[index];
	}

	override string IdOf(notnull LF_ConfigRecord record)
	{
		return LF_VehicleCompartmentRow.Cast(record).m_sPrefab;
	}

	override string IdOfContainer(notnull BaseContainer container)
	{
		ResourceName prefab;
		container.Get("m_sPrefab", prefab);
		return prefab;
	}

	override string IdHint()
	{
		return "a vehicle prefab as {GUID}path";
	}

	override LF_ConfigRecord NewRecord(string id)
	{
		return LF_VehicleCompartmentRow.Make(id);
	}

	override void Set(int index, notnull LF_ConfigRecord record)
	{
		LF_VehicleCompartmentRow row = LF_VehicleCompartmentRow.Cast(record);
		if (index < m_Config.m_aRows.Count())
			m_Config.m_aRows.Set(index, row);
		else
			m_Config.m_aRows.Insert(row);
	}

	override void RemoveAt(int index)
	{
		m_Config.m_aRows.RemoveOrdered(index);
	}

	override int PatchFromContainer(notnull LF_ConfigRecord record, notnull BaseContainer container)
	{
		LF_VehicleCompartmentRow row = LF_VehicleCompartmentRow.Cast(record);
		int changed;
		if (container.IsVariableSetDirectly("m_iSize") && container.Get("m_iSize", row.m_iSize))
			changed++;
		if (container.IsVariableSetDirectly("m_iFitPrice") && container.Get("m_iFitPrice", row.m_iFitPrice))
			changed++;
		return changed;
	}
}
