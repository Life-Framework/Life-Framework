//------------------------------------------------------------------------------------------------
//! Placed convoy start or drop. The role is data on the marker (m_bDrop, m_bNearDrop), never an
//! entity name, so the world that places the markers decides where they are. The convoy finds
//! them through the static registry below.
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "LifeFramework/Feature/Crime", description: "Convoy start or drop marker.")]
class LF_ConvoyMarkerComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_ConvoyMarkerComponent : ScriptComponent
{
	[Attribute("", UIWidgets.EditBox, desc: "Marker id for logs. Empty uses the entity name.")]
	protected string m_sMarkerId;

	[Attribute("0", UIWidgets.CheckBox, desc: "True on the drop marker.")]
	protected bool m_bDrop;

	[Attribute("0", UIWidgets.CheckBox, desc: "True on the drop a short drive from the start, which test worlds use (LF_GameModeRoleplay.m_bConvoyNearDrop). Drop markers only.")]
	protected bool m_bNearDrop;

	//! Every marker in the world, so the convoy finds the start and the active drop without a name.
	protected static ref array<LF_ConvoyMarkerComponent> s_aMarkers;

	//------------------------------------------------------------------------------------------------
	//! Returns the first start marker (a marker that is not a drop), or null.
	static LF_ConvoyMarkerComponent FindStart()
	{
		if (!s_aMarkers)
			return null;

		foreach (LF_ConvoyMarkerComponent marker : s_aMarkers)
		{
			if (marker && !marker.m_bDrop)
				return marker;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Returns the first drop marker whose near-drop flag equals nearDrop, or null.
	static LF_ConvoyMarkerComponent FindDrop(bool nearDrop)
	{
		if (!s_aMarkers)
			return null;

		foreach (LF_ConvoyMarkerComponent marker : s_aMarkers)
		{
			if (marker && marker.m_bDrop && marker.m_bNearDrop == nearDrop)
				return marker;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!s_aMarkers)
			s_aMarkers = {};

		if (s_aMarkers.Find(this) == -1)
			s_aMarkers.Insert(this);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (s_aMarkers)
			s_aMarkers.RemoveItem(this);

		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	string GetMarkerId()
	{
		if (!m_sMarkerId.IsEmpty())
			return m_sMarkerId;

		if (GetOwner())
			return GetOwner().GetName();

		return "";
	}

	//------------------------------------------------------------------------------------------------
	bool IsDrop()
	{
		return m_bDrop;
	}

	//------------------------------------------------------------------------------------------------
	bool IsNearDrop()
	{
		return m_bNearDrop;
	}
}
