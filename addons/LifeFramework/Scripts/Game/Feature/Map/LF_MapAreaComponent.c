//------------------------------------------------------------------------------------------------
enum LF_MapAreaShape
{
	POLYGON,
	CIRCLE
}

//------------------------------------------------------------------------------------------------
//! A map-only area on a layer entity (#2233, everon-map.md section 2): the town line, a gated yard,
//! any outline the map should draw that no rule needs. Safe and lawless areas are LF_ZoneComponent
//! entities and need no second one; the map reads LF_Zones for those. A gated area is a picture, not
//! a rule: the gate stays the owning system's job.
//! The polygon points are local to this entity (x and z are used), so the entity's origin and yaw place
//! them; a circle is centred on the entity. Core holds no coordinate; the pack's layer places it.
[ComponentEditorProps(category: "LifeFramework/Map", description: "A map-only area: a polygon or circle the map draws as a town line, gated area or custom area")]
class LF_MapAreaComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_MapAreaComponent : ScriptComponent
{
	[Attribute("0", UIWidgets.ComboBox, "How the area draws: town line, gated, custom (safe and lawless come from LF_ZoneComponent)", "", ParamEnumArray.FromEnum(LF_MapAreaStyle))]
	protected LF_MapAreaStyle m_eStyle;

	[Attribute("", UIWidgets.EditBox, "Label: a localization key (#LF-...) or plain text. Empty uses the style's own label")]
	protected string m_sLabel;

	[Attribute("0", UIWidgets.ComboBox, "Shape: polygon (the points below) or circle (radius below)", "", ParamEnumArray.FromEnum(LF_MapAreaShape))]
	protected LF_MapAreaShape m_eShape;

	[Attribute("30", UIWidgets.Auto, "Circle radius in metres, measured flat")]
	protected float m_fRadius;

	[Attribute("", UIWidgets.Auto, "Polygon points, local to this entity (x and z are used), in order, at least three")]
	protected ref array<vector> m_aPoints;

	protected string m_sAreaName;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		m_sAreaName = owner.GetName();
		if (m_sAreaName.IsEmpty())
			m_sAreaName = string.Format("maparea@%1", owner.GetOrigin());

		LF_MapArea area = new LF_MapArea();
		area.m_sName = m_sAreaName;
		area.m_eStyle = m_eStyle;
		area.m_sLabel = m_sLabel;

		bool made;
		if (m_eShape == LF_MapAreaShape.CIRCLE)
		{
			made = LF_MapAreas.CircleOutline(owner.GetOrigin(), m_fRadius, LF_MapAreas.DEFAULT_SEGMENTS, area.m_aOutline);
		}
		else
		{
			array<vector> world = {};
			if (m_aPoints)
			{
				foreach (vector point : m_aPoints)
					world.Insert(owner.CoordToParent(point));
			}

			made = LF_MapAreas.PolygonOutline(world, area.m_aOutline);
		}

		if (!made)
		{
			int pointCount;
			if (m_aPoints)
				pointCount = m_aPoints.Count();

			LF_Debug.Error(LF_MapAreas.FEATURE, string.Format("map area %1 has no usable shape (radius %2, %3 points), not on the map", m_sAreaName, m_fRadius, pointCount));
			m_sAreaName = string.Empty;
			return;
		}

		LF_MapAreas.Register(area);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (!m_sAreaName.IsEmpty())
			LF_MapAreas.Unregister(m_sAreaName);

		super.OnDelete(owner);
	}
}
