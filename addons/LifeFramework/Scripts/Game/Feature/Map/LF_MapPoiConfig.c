//------------------------------------------------------------------------------------------------
//! One POI row: a place kind the map may draw (docs/architecture/everon-map.md section 1). A place
//! is a POI when its LF_Places kind has a row here; a kind with no row never shows. The id is the
//! kind. Core ships neutral rows (Configs/Map/LF_MapPois.conf); a pack overlays icons, labels and
//! its hidden kinds through its mission header.
[BaseContainerProps()]
class LF_MapPoiKind : LF_ConfigRecord
{
	[Attribute("", UIWidgets.EditBox, "The LF_Places kind this row draws, e.g. Bank. The front kind of a system only (Bank, not BankVault)")]
	string m_sKind;

	[Attribute("civic", UIWidgets.EditBox, "Legend and filter group: civic, shop, work, law, health, travel, story, underworld")]
	string m_sCategory;

	[Attribute("", UIWidgets.EditBox, "Icon quad name in the map imageset (LF_MapIcons). Empty draws the category's fallback")]
	string m_sIcon;

	[Attribute("", UIWidgets.EditBox, "Localization key of the kind's name, e.g. #LF-MapPoi_Bank. The map adds the town")]
	string m_sLabel;

	[Attribute("0", UIWidgets.Auto, "Hide below this map zoom. 0 shows at every zoom", params: "0 100000")]
	float m_fMinZoom;

	[Attribute("0", UIWidgets.ComboBox, "When the POI shows", enums: ParamEnumArray.FromEnum(LF_EMapPoiVisibility))]
	LF_EMapPoiVisibility m_eVisibility;

	[Attribute("", UIWidgets.EditBox, "For ROLE rows: the role that sees it (police, medic)")]
	string m_sRole;

	//------------------------------------------------------------------------------------------------
	static LF_MapPoiKind Make(string kind, LF_EMapPoiVisibility visibility, string role = "")
	{
		LF_MapPoiKind row = new LF_MapPoiKind();
		row.m_sKind = kind;
		row.m_eVisibility = visibility;
		row.m_sRole = role;
		return row;
	}
}

//------------------------------------------------------------------------------------------------
//! The POI table: Configs/Map/LF_MapPois.conf with each pack overlay layered over it (LF_ConfigOverlay,
//! #962). A pack adds, replaces, patches or removes a row by kind. No owner section: the map is
//! read by every client and nothing here is a server tuning knob.
[BaseContainerProps(configRoot: true)]
class LF_MapPoiConfig
{
	static const string FEATURE = "MapPois";
	static const ResourceName CONFIG_PATH = "{46407C3917B5AD42}Configs/Map/LF_MapPois.conf";

	[Attribute("", UIWidgets.Object, "One row per place kind the map draws")]
	ref array<ref LF_MapPoiKind> m_aKinds;

	protected static ref LF_MapPoiConfig s_Config;
	protected static bool s_bLoadAttempted;

	//------------------------------------------------------------------------------------------------
	//! The loaded and layered table, or null (logged once) when the core file does not resolve.
	static LF_MapPoiConfig Get()
	{
		if (s_bLoadAttempted)
			return s_Config;

		s_bLoadAttempted = true;
		Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
		if (!container || !container.GetResource())
		{
			LF_Debug.Error(FEATURE, string.Format("POI table does not resolve, the map shows no places: %1", CONFIG_PATH));
			return null;
		}

		s_Config = LF_MapPoiConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!s_Config)
			LF_Debug.Error(FEATURE, string.Format("POI table has an unexpected root class: %1", CONFIG_PATH));
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
		if (!m_aKinds)
			m_aKinds = new array<ref LF_MapPoiKind>();

		LF_MapPoiList list = new LF_MapPoiList(this);
		array<ref Resource> resources = {};
		array<ResourceName> names = {};
		LF_ConfigOverlay.LoadPackOverlays(CONFIG_PATH, LF_MapPoiConfig, "mapPois", resources, names);
		LF_ConfigClaims claims = new LF_ConfigClaims();
		foreach (int i, Resource resource : resources)
		{
			int changed = LF_ConfigOverlay.ApplyContainerList(list, resource.GetResource().ToBaseContainer(), "m_aKinds", names[i], claims);
			LF_Debug.Info(FEATURE, string.Format("pack overlay %1 changed %2 POI rows", names[i], changed));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The row for a place kind, or null when the kind is not a POI.
	LF_MapPoiKind Find(string kind)
	{
		if (!m_aKinds)
			return null;

		foreach (LF_MapPoiKind row : m_aKinds)
		{
			if (row && row.m_sKind == kind)
				return row;
		}

		return null;
	}
}

//------------------------------------------------------------------------------------------------
//! The POI table's `m_aKinds` list for the shared merge (LF_ConfigOverlay), keyed by place kind.
class LF_MapPoiList : LF_ConfigList
{
	protected LF_MapPoiConfig m_Config;

	void LF_MapPoiList(LF_MapPoiConfig config)
	{
		m_Config = config;
	}

	override string GetName()
	{
		return "mapPois.kinds";
	}

	override int Count()
	{
		return m_Config.m_aKinds.Count();
	}

	override LF_ConfigRecord Get(int index)
	{
		return m_Config.m_aKinds[index];
	}

	override string IdOf(notnull LF_ConfigRecord record)
	{
		return LF_MapPoiKind.Cast(record).m_sKind;
	}

	override string IdOfContainer(notnull BaseContainer container)
	{
		string id;
		container.Get("m_sKind", id);
		return id;
	}

	override string IdHint()
	{
		return "a place kind";
	}

	override LF_ConfigRecord NewRecord(string id)
	{
		return LF_MapPoiKind.Make(id, LF_EMapPoiVisibility.DISCOVER);
	}

	override void Set(int index, notnull LF_ConfigRecord record)
	{
		LF_MapPoiKind row = LF_MapPoiKind.Cast(record);
		if (index < m_Config.m_aKinds.Count())
			m_Config.m_aKinds.Set(index, row);
		else
			m_Config.m_aKinds.Insert(row);
	}

	override void RemoveAt(int index)
	{
		m_Config.m_aKinds.RemoveOrdered(index);
	}

	override int PatchFromContainer(notnull LF_ConfigRecord record, notnull BaseContainer container)
	{
		LF_MapPoiKind row = LF_MapPoiKind.Cast(record);
		int changed;
		if (container.IsVariableSetDirectly("m_sCategory") && container.Get("m_sCategory", row.m_sCategory))
			changed++;
		if (container.IsVariableSetDirectly("m_sIcon") && container.Get("m_sIcon", row.m_sIcon))
			changed++;
		if (container.IsVariableSetDirectly("m_sLabel") && container.Get("m_sLabel", row.m_sLabel))
			changed++;
		if (container.IsVariableSetDirectly("m_fMinZoom") && container.Get("m_fMinZoom", row.m_fMinZoom))
			changed++;
		if (container.IsVariableSetDirectly("m_sRole") && container.Get("m_sRole", row.m_sRole))
			changed++;
		int visibility;
		if (container.IsVariableSetDirectly("m_eVisibility") && container.Get("m_eVisibility", visibility))
		{
			row.m_eVisibility = visibility;
			changed++;
		}
		return changed;
	}
}
