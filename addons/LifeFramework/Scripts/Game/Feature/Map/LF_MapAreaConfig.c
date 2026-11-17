//------------------------------------------------------------------------------------------------
//! One map area row: an outline the map draws that a conf, not a layer entity, holds (#2233 split
//! M5b, docs/architecture/everon-map.md section 2). The id is the area's name. Core ships no rows
//! (core holds no coordinate: Configs/Map/LF_MapAreas.conf is empty); a pack adds its town line and
//! squares through its mission header, positions in world x and z. A zone entity (LF_ZoneComponent)
//! of the same name as a row wins: it is the rule, the row is only its picture.
[BaseContainerProps()]
class LF_MapAreaRow : LF_ConfigRecord
{
	[Attribute("", UIWidgets.EditBox, "The area's name, the key a pack overlay patches. A zone entity with this name replaces the row")]
	string m_sId;

	[Attribute("", UIWidgets.EditBox, "Label: a localization key (#LF-...) or plain text. Empty uses the style's own label")]
	string m_sLabel;

	[Attribute("0", UIWidgets.ComboBox, "How the area draws", "", ParamEnumArray.FromEnum(LF_MapAreaStyle))]
	LF_MapAreaStyle m_eStyle;

	[Attribute("0", UIWidgets.ComboBox, "Shape: polygon (the points below) or circle (centre and radius)", "", ParamEnumArray.FromEnum(LF_MapAreaShape))]
	LF_MapAreaShape m_eShape;

	[Attribute("0 0 0", UIWidgets.Auto, "Circle centre, world (x and z are used)")]
	vector m_vCentre;

	[Attribute("30", UIWidgets.Auto, "Circle radius in metres, measured flat", params: "0 100000")]
	float m_fRadius;

	[Attribute("", UIWidgets.Auto, "Polygon points, world (x and z are used), in order, at least three")]
	ref array<vector> m_aPoints;

	//------------------------------------------------------------------------------------------------
	static LF_MapAreaRow MakeCircle(string id, LF_MapAreaStyle style, vector centre, float radius)
	{
		LF_MapAreaRow row = new LF_MapAreaRow();
		row.m_sId = id;
		row.m_eStyle = style;
		row.m_eShape = LF_MapAreaShape.CIRCLE;
		row.m_vCentre = centre;
		row.m_fRadius = radius;
		return row;
	}

	//------------------------------------------------------------------------------------------------
	static LF_MapAreaRow MakePolygon(string id, LF_MapAreaStyle style, notnull array<vector> points)
	{
		LF_MapAreaRow row = new LF_MapAreaRow();
		row.m_sId = id;
		row.m_eStyle = style;
		row.m_eShape = LF_MapAreaShape.POLYGON;
		row.m_aPoints = new array<vector>();
		row.m_aPoints.Copy(points);
		return row;
	}
}

//------------------------------------------------------------------------------------------------
//! The map area table: Configs/Map/LF_MapAreas.conf with each pack overlay layered over it
//! (LF_ConfigOverlay, #962). A pack adds, replaces, patches or removes a row by id. No owner
//! section: the map is read by every client and nothing here is a server tuning knob.
[BaseContainerProps(configRoot: true)]
class LF_MapAreaConfig
{
	static const string FEATURE = "MapAreas";
	static const ResourceName CONFIG_PATH = "{5B1D6E2A8C47F093}Configs/Map/LF_MapAreas.conf";

	[Attribute("", UIWidgets.Object, "One row per area the map draws from data")]
	ref array<ref LF_MapAreaRow> m_aAreas;

	protected static ref LF_MapAreaConfig s_Config;
	protected static bool s_bLoadAttempted;

	//------------------------------------------------------------------------------------------------
	//! The core file alone, no pack overlay applied, or null (logged) when it does not resolve.
	static LF_MapAreaConfig LoadCore()
	{
		Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
		if (!container || !container.GetResource())
		{
			LF_Debug.Error(FEATURE, string.Format("area table does not resolve, the map shows no data areas: %1", CONFIG_PATH));
			return null;
		}

		LF_MapAreaConfig config = LF_MapAreaConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!config)
		{
			LF_Debug.Error(FEATURE, string.Format("area table has an unexpected root class: %1", CONFIG_PATH));
			return null;
		}

		if (!config.m_aAreas)
			config.m_aAreas = new array<ref LF_MapAreaRow>();

		return config;
	}

	//------------------------------------------------------------------------------------------------
	//! The loaded and layered table, or null (logged once) when the core file does not resolve.
	static LF_MapAreaConfig Get()
	{
		if (s_bLoadAttempted)
			return s_Config;

		s_bLoadAttempted = true;
		s_Config = LoadCore();
		if (s_Config)
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
	//! Merges each pack overlay of this class, in load order, over this instance. \return Rows changed.
	int ApplyLayers()
	{
		if (!m_aAreas)
			m_aAreas = new array<ref LF_MapAreaRow>();

		LF_MapAreaList list = new LF_MapAreaList(this);
		array<ref Resource> resources = {};
		array<ResourceName> names = {};
		LF_ConfigOverlay.LoadPackOverlays(CONFIG_PATH, LF_MapAreaConfig, "mapAreas", resources, names);
		LF_ConfigClaims claims = new LF_ConfigClaims();
		int total;
		foreach (int i, Resource resource : resources)
		{
			int changed = LF_ConfigOverlay.ApplyContainerList(list, resource.GetResource().ToBaseContainer(), "m_aAreas", names[i], claims);
			total += changed;
			LF_Debug.Info(FEATURE, string.Format("pack overlay %1 changed %2 area rows", names[i], changed));
		}

		return total;
	}

	//------------------------------------------------------------------------------------------------
	//! The row with this id, or null.
	LF_MapAreaRow Find(string id)
	{
		if (!m_aAreas)
			return null;

		foreach (LF_MapAreaRow row : m_aAreas)
		{
			if (row && row.m_sId == id)
				return row;
		}

		return null;
	}
}

//------------------------------------------------------------------------------------------------
//! The area table's `m_aAreas` list for the shared merge (LF_ConfigOverlay), keyed by id.
class LF_MapAreaList : LF_ConfigList
{
	protected LF_MapAreaConfig m_Config;

	void LF_MapAreaList(LF_MapAreaConfig config)
	{
		m_Config = config;
	}

	override string GetName()
	{
		return "mapAreas.areas";
	}

	override int Count()
	{
		return m_Config.m_aAreas.Count();
	}

	override LF_ConfigRecord Get(int index)
	{
		return m_Config.m_aAreas[index];
	}

	override string IdOf(notnull LF_ConfigRecord record)
	{
		return LF_MapAreaRow.Cast(record).m_sId;
	}

	override string IdOfContainer(notnull BaseContainer container)
	{
		string id;
		container.Get("m_sId", id);
		return id;
	}

	override string IdHint()
	{
		return "an area name";
	}

	override LF_ConfigRecord NewRecord(string id)
	{
		LF_MapAreaRow row = new LF_MapAreaRow();
		row.m_sId = id;
		return row;
	}

	override void Set(int index, notnull LF_ConfigRecord record)
	{
		LF_MapAreaRow row = LF_MapAreaRow.Cast(record);
		if (index < m_Config.m_aAreas.Count())
			m_Config.m_aAreas.Set(index, row);
		else
			m_Config.m_aAreas.Insert(row);
	}

	override void RemoveAt(int index)
	{
		m_Config.m_aAreas.RemoveOrdered(index);
	}

	override int PatchFromContainer(notnull LF_ConfigRecord record, notnull BaseContainer container)
	{
		LF_MapAreaRow row = LF_MapAreaRow.Cast(record);
		int changed;
		if (container.IsVariableSetDirectly("m_sLabel") && container.Get("m_sLabel", row.m_sLabel))
			changed++;
		if (container.IsVariableSetDirectly("m_vCentre") && container.Get("m_vCentre", row.m_vCentre))
			changed++;
		if (container.IsVariableSetDirectly("m_fRadius") && container.Get("m_fRadius", row.m_fRadius))
			changed++;
		int style;
		if (container.IsVariableSetDirectly("m_eStyle") && container.Get("m_eStyle", style))
		{
			row.m_eStyle = style;
			changed++;
		}

		int shape;
		if (container.IsVariableSetDirectly("m_eShape") && container.Get("m_eShape", shape))
		{
			row.m_eShape = shape;
			changed++;
		}

		if (container.IsVariableSetDirectly("m_aPoints"))
		{
			array<vector> points = {};
			if (container.Get("m_aPoints", points))
			{
				row.m_aPoints = new array<vector>();
				row.m_aPoints.Copy(points);
				changed++;
			}
		}

		return changed;
	}
}
